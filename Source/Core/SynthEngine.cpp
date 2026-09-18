#include "SynthEngine.h"
#include "AppLogger.h"
#include <algorithm>
#include <cmath>

namespace ABDMS2000 {

SynthEngine::SynthEngine(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    ABD_LOG("[ENGINE] SynthEngine constructor called.");
}

void SynthEngine::prepare(double sampleRate, int samplesPerBlock)
{
    ABD_LOG(juce::String("[ENGINE] SynthEngine::prepare start. SR: ") + juce::String(sampleRate) + " Block: " + juce::String(samplesPerBlock));
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    samplesPerBlock_ = samplesPerBlock;
    smoothedMasterGain_.reset(sampleRate_, 0.02);
    smoothedMasterGain_.setCurrentAndTargetValue(masterVolume_.load(std::memory_order_relaxed));

    // Size the per-block scope capture scratch buffers to the current block.
    const int bs = std::max(1, samplesPerBlock);
    scopeBufMasterL_.assign(bs, 0.0f);
    scopeBufMasterR_.assign(bs, 0.0f);
    scopeBufPreFxL_.assign(bs, 0.0f);
    scopeBufPreFxR_.assign(bs, 0.0f);
    scopeBufOscMix_.assign(bs, 0.0f);
    scopeBufPostFilter_.assign(bs, 0.0f);
    scopeBufPostVca_.assign(bs, 0.0f);
    scopeBufLfo1_.assign(bs, 0.0f);
    registerScopeTaps();

    voiceManagerA_.prepare(sampleRate_);
    voiceManagerB_.prepare(sampleRate_);
    modSeq_.prepare(sampleRate_);
    arpeggiator_.prepare(sampleRate_);
    modFX_.prepare(sampleRate_);
    delayFX_.prepare(sampleRate_);
    masterEQ_.prepare(sampleRate_);
    vocoder_.prepare(sampleRate_);
    debugFilter_.prepare(sampleRate_);

    arpEventsBuffer_.reserve(32);
    reset();
    ABD_LOG("[ENGINE] SynthEngine::prepare completed.");
}

void SynthEngine::reset()
{
    voiceManagerA_.reset();
    voiceManagerB_.reset();
    modSeq_.reset();
    arpeggiator_.reset();
    modFX_.reset();
    delayFX_.reset();
    masterEQ_.reset();
    vocoder_.reset();
    debugFilter_.reset();
    testTonePhase_ = 0.0;
    currentSnapshot_ = AudioThreadSnapshot{};
}

void SynthEngine::registerScopeTaps()
{
    if (scopeTapsRegistered_)
        return;
    scopeTapsRegistered_ = true;

    // Fase A tap set: Master out, Pre-FX, and Voice 0 stage taps
    // (LFO1 control, Osc.Mix, Post-Filter, Post-VCA). Capacity = block size,
    // enough to satisfy the 512-sample frame pull at 1:1 audio rate.
    const size_t cap = static_cast<size_t>(std::max(1, samplesPerBlock_));
    tapMaster_ = scopeCollector_.registerTap("Master Output", abd::scope::ScopeTapType::StereoAudio, cap, "master_out");
    tapPreFx_  = scopeCollector_.registerTap("Pre FX",       abd::scope::ScopeTapType::StereoAudio, cap, "pre_fx");
    tapOscMix_ = scopeCollector_.registerTap("Voice 1 Osc Mix", abd::scope::ScopeTapType::MonoAudio, cap, "osc_mix");
    tapPostFilter_ = scopeCollector_.registerTap("Voice 1 Post Filter", abd::scope::ScopeTapType::MonoAudio, cap, "post_filter");
    tapPostVca_ = scopeCollector_.registerTap("Voice 1 Post VCA",   abd::scope::ScopeTapType::MonoAudio, cap, "post_vca");
    tapLfo1_   = scopeCollector_.registerTap("Voice 1 LFO1", abd::scope::ScopeTapType::ControlSignal, cap, "lfo1");
}

void SynthEngine::flushScopeTaps(int numSamples) noexcept
{
    if (numSamples <= 0)
        return;
    const auto n = static_cast<size_t>(numSamples);

    if (tapMaster_ != nullptr && tapMaster_->isActive())
        tapMaster_->writeStereo(scopeBufMasterL_.data(), scopeBufMasterR_.data(), n);
    if (tapPreFx_ != nullptr && tapPreFx_->isActive())
        tapPreFx_->writeStereo(scopeBufPreFxL_.data(), scopeBufPreFxR_.data(), n);
    if (tapOscMix_ != nullptr && tapOscMix_->isActive())
        tapOscMix_->write(scopeBufOscMix_.data(), n);
    if (tapPostFilter_ != nullptr && tapPostFilter_->isActive())
        tapPostFilter_->write(scopeBufPostFilter_.data(), n);
    if (tapPostVca_ != nullptr && tapPostVca_->isActive())
        tapPostVca_->write(scopeBufPostVca_.data(), n);
    if (tapLfo1_ != nullptr && tapLfo1_->isActive())
        tapLfo1_->write(scopeBufLfo1_.data(), n);
}


namespace
{
/** `osc1Wave` → `t2Osc1Wave`: el id del mismo parámetro en el Timbre 2. */
juce::String timbre2ParamId(juce::String id)
{
    if (id.isNotEmpty())
        id = id.substring(0, 1).toUpperCase() + id.substring(1);
    return "t2" + id;
}
} // namespace

float SynthEngine::paramValueById(const juce::String& id, float defaultVal) const noexcept
{
    if (auto* p = apvts_.getRawParameterValue(id))
        return p->load(std::memory_order_relaxed);
    juce::ignoreUnused(defaultVal);
    return defaultVal;
}

float SynthEngine::paramValue(bool second, const char* id, float defaultVal) const noexcept
{
    juce::String key(id);
    if (second) key = timbre2ParamId(key);
    return paramValueById(key, defaultVal);
}

/**
 * Parámetros de un timbre. Los dos timbres leen **el mismo** vocabulario: uno con los
 * ids tal cual y el otro con el prefijo `t2`, que es exactamente cómo el programa real
 * guarda dos bloques de 108 B idénticos en estructura.
 */
void SynthEngine::readTimbreParams(bool second, VoiceParameters& vp) noexcept
{
    auto P = [this, second](const char* id, float def) { return paramValue(second, id, def); };

    const bool portaOn = (paramValueById(ParamIDs::portamentoOn, 1.0f) > 0.5f);
    vp.portamentoTime = portaOn ? P(ParamIDs::portamentoTime, 0.0f) : 0.0f;

    // OSC 1 — 8 algoritmos: Saw=0, Pulse=1, Tri=2, Sine=3, VoxWave=4, DWGS=5, Noise=6, AudioIn=7
    int o1w = static_cast<int>(P(ParamIDs::osc1Wave, 0.0f));
    vp.osc1Type = static_cast<OSC1Type>(std::min(o1w, 7));
    vp.osc1DwgsIndex = static_cast<int>(P(ParamIDs::osc1DwgsWave, 0.0f));
    vp.osc1Ctrl1 = P(ParamIDs::osc1Ctrl1, 0.0f) / 127.0f;
    vp.osc1Ctrl2 = P(ParamIDs::osc1Ctrl2, 0.0f) / 127.0f;
    vp.osc1Level = P(ParamIDs::mixOsc1Level, 127.0f) / 127.0f;

    // OSC 2
    int o2w = static_cast<int>(P(ParamIDs::osc2Wave, 0.0f));
    vp.osc2Wave = static_cast<VAWaveform>(std::min(o2w, 2));
    vp.osc2Semitone = P(ParamIDs::osc2Semitone, 0.0f);
    vp.osc2Tune = P(ParamIDs::osc2Tune, 0.0f);
    int modMode = static_cast<int>(P(ParamIDs::osc2ModType, 0.0f));
    vp.osc2ModMode = static_cast<OSC2ModulationMode>(std::min(modMode, 3));
    vp.osc2Level = P(ParamIDs::mixOsc2Level, 0.0f) / 127.0f;

    // Noise
    vp.noiseLevel = P(ParamIDs::mixNoiseLevel, 0.0f) / 127.0f;

    // Filter
    int fType = static_cast<int>(P(ParamIDs::filterType, 0.0f));
    vp.filterType = static_cast<FilterType>(fType);
    vp.filterCutoffNorm = P(ParamIDs::filterCutoff, 127.0f) / 127.0f;
    vp.filterResonance = P(ParamIDs::filterResonance, 0.0f) / 127.0f;
    vp.eg1FilterIntensity = P(ParamIDs::filterEg1Int, 0.0f) / 63.0f;
    vp.filterKbdTrack = P(ParamIDs::filterKeyTrack, 0.0f) / 63.0f;
    vp.filterVeloSens = P(ParamIDs::filterVelo, 0.0f) / 63.0f;

    // EG1
    vp.eg1Attack = P(ParamIDs::eg1Attack, 0.0f) / 127.0f;
    vp.eg1Decay = P(ParamIDs::eg1Decay, 40.0f) / 127.0f;
    vp.eg1Sustain = P(ParamIDs::eg1Sustain, 0.0f) / 127.0f;
    vp.eg1Release = P(ParamIDs::eg1Release, 40.0f) / 127.0f;

    // EG2
    vp.eg2Attack = P(ParamIDs::eg2Attack, 0.0f) / 127.0f;
    vp.eg2Decay = P(ParamIDs::eg2Decay, 40.0f) / 127.0f;
    vp.eg2Sustain = P(ParamIDs::eg2Sustain, 127.0f) / 127.0f;
    vp.eg2Release = P(ParamIDs::eg2Release, 40.0f) / 127.0f;

    // LFO1: Saw=0, Square=1, Triangle=2, SampleHold=3
    int l1w = static_cast<int>(P(ParamIDs::lfo1Wave, 2.0f));
    vp.lfo1Wave = static_cast<LFOWaveform>(std::min(l1w, 3));
    vp.lfo1KeySync = static_cast<KeySyncMode>(static_cast<int>(P(ParamIDs::lfo1KeySync, 2.0f)));
    float l1FreqNorm = P(ParamIDs::lfo1Freq, 40.0f) / 127.0f;
    vp.lfo1FreqHz = 0.01f * std::pow(2000.0f, std::pow(l1FreqNorm, 0.8f));
    vp.lfo1TempoSync = (P(ParamIDs::lfo1TempoSync, 0.0f) > 0.5f);
    vp.lfo1SyncNote = static_cast<int>(P(ParamIDs::lfo1SyncNote, 4.0f));

    // LFO2: Saw=0, SquarePlus=1, Sine=2, SampleHold=3
    int l2w = static_cast<int>(P(ParamIDs::lfo2Wave, 2.0f));
    vp.lfo2Wave = static_cast<LFOWaveformLFO2>(std::min(l2w, 3));
    vp.lfo2KeySync = static_cast<KeySyncMode>(static_cast<int>(P(ParamIDs::lfo2KeySync, 2.0f)));
    float l2FreqNorm = P(ParamIDs::lfo2Freq, 40.0f) / 127.0f;
    vp.lfo2FreqHz = 0.01f * std::pow(2000.0f, std::pow(l2FreqNorm, 0.8f));
    vp.lfo2TempoSync = (P(ParamIDs::lfo2TempoSync, 0.0f) > 0.5f);
    vp.lfo2SyncNote = static_cast<int>(P(ParamIDs::lfo2SyncNote, 4.0f));

    // Amp
    vp.ampLevel = P(ParamIDs::ampLevel, 100.0f) / 127.0f;
    vp.panpot = P(ParamIDs::ampPan, 0.0f) / 64.0f;
    vp.distortionOn = (P(ParamIDs::ampDistortion, 0.0f) > 0.5f);
    vp.ampKeyTrack = P(ParamIDs::ampKeyTrack, 0.0f) / 63.0f;
    vp.ampVeloSens = P(ParamIDs::ampVelo, 0.0f) / 63.0f;

    // Wheels (comunes a los dos timbres)
    vp.pitchBendValue = pitchBendValue_.load(std::memory_order_relaxed);
    vp.modWheelValue = modWheelValue_.load(std::memory_order_relaxed);

    // Virtual Patch Slots 1-4
    const char* slotSource[4] = { ParamIDs::patch1Source, ParamIDs::patch2Source,
                                  ParamIDs::patch3Source, ParamIDs::patch4Source };
    const char* slotDestination[4] = { ParamIDs::patch1Destination, ParamIDs::patch2Destination,
                                       ParamIDs::patch3Destination, ParamIDs::patch4Destination };
    const char* slotIntensity[4] = { ParamIDs::patch1Intensity, ParamIDs::patch2Intensity,
                                     ParamIDs::patch3Intensity, ParamIDs::patch4Intensity };
    const float sourceDefault[4] = { 0.0f, 1.0f, 2.0f, 3.0f };
    const float destinationDefault[4] = { 4.0f, 4.0f, 0.0f, 7.0f };
    for (size_t s = 0; s < 4; ++s)
    {
        vp.patchSlots[s].source = static_cast<PatchSource>(static_cast<int>(P(slotSource[s], sourceDefault[s])));
        vp.patchSlots[s].destination = static_cast<PatchDestination>(static_cast<int>(P(slotDestination[s], destinationDefault[s])));
        vp.patchSlots[s].intensity = P(slotIntensity[s], 0.0f) / 63.0f;
    }
}

/**
 * Vuelca las 3 filas del timbre en sus pistas del secuenciador. Los pasos viajan como
 * byte real (−63..+63) y aquí se guardan normalizados 0..1, que es como los consume
 * `ModSequencer`; a la salida se vuelven a bipolar con el rango del destino.
 */
void SynthEngine::configureTimbreSeq(bool second, size_t firstTrack) noexcept
{
    const int mode = static_cast<int>(paramValue(second, ParamIDs::modSeqType, 0.0f));
    const int lastStep = static_cast<int>(std::max(1.0f, std::min(16.0f,
                              paramValue(second, ParamIDs::seqLastStep, 16.0f))));

    for (size_t row = 0; row < ModSequencer::TRACKS_PER_TIMBRE; ++row)
    {
        ModSeqTrack& track = modSeq_.getTrack(firstTrack + row);
        const juce::String base("seq" + juce::String(static_cast<int>(row) + 1));
        const juce::String destId = base + "Dest";
        const juce::String motionId = base + "Motion";

        const int destIndex = static_cast<int>(std::max(0.0f, std::min(30.0f,
            paramValueById(second ? timbre2ParamId(destId) : destId, 0.0f))));
        track.destination = static_cast<ModSeqDest>(destIndex);
        track.motion = static_cast<ModSeqMotion>(static_cast<int>(
            paramValueById(second ? timbre2ParamId(motionId) : motionId, 0.0f)));
        track.mode = static_cast<ModSeqMode>(std::min(3, mode));
        track.length = lastStep;

        for (size_t s = 0; s < ModSequencer::NUM_STEPS; ++s)
        {
            const juce::String stepId = base + "Step" + juce::String(static_cast<int>(s) + 1);
            const float raw = paramValueById(second ? timbre2ParamId(stepId) : stepId, 0.0f);
            track.steps[s] = DSPUtils::clamp((raw + 63.0f) / 126.0f, 0.0f, 1.0f);
        }
    }
}

/**
 * Aplica la salida de las 3 filas del timbre.
 *
 * Los destinos que el motor calcula **por muestra** (pitch, niveles, cutoff, amp, pan,
 * LFO2…) viajan en `SeqModulation` y los aplica `Voice`; los que son de bloque
 * (resonancia, EG, portamento, intensidad de los patch, LFO1) se aplican aquí. El
 * alcance de cada uno es el del mejor equivalente del motor y las escalas siguen las
 * mismas que la matriz de patch (±24 st de pitch, ±5 octavas de cutoff, ±1 de amp).
 * `OSC1 CTRL2` y `STEP RUN` no tienen equivalente y se declaran no modelados.
 */
void SynthEngine::applySeqModulation(bool second, VoiceParameters& vp) noexcept
{
    const size_t firstTrack = second ? ModSequencer::TIMBRE2_TRACK : ModSequencer::TIMBRE1_TRACK;
    bool lengthModulated = false;
    float lengthAmount = 0.0f;

    for (size_t row = 0; row < ModSequencer::TRACKS_PER_TIMBRE; ++row)
    {
        const ModSeqDest dest = modSeq_.getTrack(firstTrack + row).destination;
        if (dest == ModSeqDest::None) continue;

        const float value = DSPUtils::clamp(
            (modSeq_.getTrackOutput(firstTrack + row) - 0.5f) * 2.0f, -1.0f, 1.0f);

        switch (dest)
        {
            case ModSeqDest::Pitch:      vp.seq.pitch = value; break;
            case ModSeqDest::OSC2Semi:   vp.seq.osc2Pitch = value; break;
            case ModSeqDest::OSC2Tune:   vp.seq.osc2Tune = value * 0.5f; break;   // ±50 cents
            case ModSeqDest::OSC1Ctrl1:  vp.seq.osc1Ctrl1 = value; break;
            case ModSeqDest::OSC1Level:  vp.seq.osc1Level = value; break;
            case ModSeqDest::OSC2Level:  vp.seq.osc2Level = value; break;
            case ModSeqDest::NoiseLevel: vp.seq.noiseLevel = value; break;
            case ModSeqDest::Cutoff:     vp.seq.cutoff = value; break;
            case ModSeqDest::AmpLevel:   vp.seq.amp = value; break;
            case ModSeqDest::Panpot:     vp.seq.pan = value; break;
            case ModSeqDest::LFO2Freq:   vp.seq.lfo2Freq = value; break;

            case ModSeqDest::Resonance:
                vp.filterResonance = DSPUtils::clamp(vp.filterResonance + value, 0.0f, 1.0f);
                break;
            case ModSeqDest::EG1Int:
                vp.eg1FilterIntensity = DSPUtils::clamp(vp.eg1FilterIntensity + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::KbdTrk:
                vp.filterKbdTrack = DSPUtils::clamp(vp.filterKbdTrack + value, -1.0f, 1.0f);
                break;
            case ModSeqDest::Portamento:
                vp.portamentoTime = DSPUtils::clamp(vp.portamentoTime + (value * 64.0f), 0.0f, 127.0f);
                break;
            case ModSeqDest::LFO1Freq:
                vp.lfo1FreqHz = DSPUtils::clamp(vp.lfo1FreqHz * std::pow(2.0f, value * 4.0f), 0.01f, 20000.0f);
                break;

            // Tiempos de envolvente: escalan el valor normalizado (factor hasta ×2 / ×0).
            case ModSeqDest::EG1Attack:  vp.eg1Attack  = DSPUtils::clamp(vp.eg1Attack  * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG1Decay:   vp.eg1Decay   = DSPUtils::clamp(vp.eg1Decay   * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG1Sustain: vp.eg1Sustain = DSPUtils::clamp(vp.eg1Sustain * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG1Release: vp.eg1Release = DSPUtils::clamp(vp.eg1Release * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG2Attack:  vp.eg2Attack  = DSPUtils::clamp(vp.eg2Attack  * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG2Decay:   vp.eg2Decay   = DSPUtils::clamp(vp.eg2Decay   * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG2Sustain: vp.eg2Sustain = DSPUtils::clamp(vp.eg2Sustain * (1.0f + value), 0.0f, 1.0f); break;
            case ModSeqDest::EG2Release: vp.eg2Release = DSPUtils::clamp(vp.eg2Release * (1.0f + value), 0.0f, 1.0f); break;

            case ModSeqDest::Patch1Int: vp.patchSlots[0].intensity = DSPUtils::clamp(vp.patchSlots[0].intensity + value, -1.0f, 1.0f); break;
            case ModSeqDest::Patch2Int: vp.patchSlots[1].intensity = DSPUtils::clamp(vp.patchSlots[1].intensity + value, -1.0f, 1.0f); break;
            case ModSeqDest::Patch3Int: vp.patchSlots[2].intensity = DSPUtils::clamp(vp.patchSlots[2].intensity + value, -1.0f, 1.0f); break;
            case ModSeqDest::Patch4Int: vp.patchSlots[3].intensity = DSPUtils::clamp(vp.patchSlots[3].intensity + value, -1.0f, 1.0f); break;

            case ModSeqDest::StepLength:
                lengthModulated = true;
                lengthAmount = value;
                break;

            case ModSeqDest::OSC1Ctrl2:
            case ModSeqDest::None:
            default:
                break; // declarado no modelado (ver `MS2000HardwareProgram::unmodelled()`)
        }
    }

    // "STEP LENGTH" desplaza el último paso de las tres filas del timbre (±6, real).
    if (lengthModulated)
    {
        const int lastStep = static_cast<int>(std::max(1.0f, std::min(16.0f,
                                  paramValue(second, ParamIDs::seqLastStep, 16.0f))));
        const int shifted = static_cast<int>(std::round(lastStep + (lengthAmount * 6.0f)));
        const int length = std::max(1, std::min(16, shifted));
        for (size_t row = 0; row < ModSequencer::TRACKS_PER_TIMBRE; ++row)
            modSeq_.getTrack(firstTrack + row).length = length;
    }
}

void SynthEngine::updateParametersFromAPVTS() noexcept
{
    auto getParam = [this](const char* id, float defaultVal = 0.0f) -> float {
        if (auto* p = apvts_.getRawParameterValue(id))
            return p->load(std::memory_order_relaxed);
        return defaultVal;
    };

    // Master volume
    masterVolume_.store(getParam(ParamIDs::masterVolume, 0.8f), std::memory_order_relaxed);
    smoothedMasterGain_.setTargetValue(masterVolume_.load(std::memory_order_relaxed));

    // Voice Architecture & Multitimbric Mode
    int synthMode = static_cast<int>(getParam(ParamIDs::synthMode, 0.0f));
    const size_t totalPolyphony = (synthMode == 2) ? VoiceManager::MAX_EXPANDED_VOICES : VoiceManager::NUM_HW_VOICES;

    const int vMode = static_cast<int>(getParam(ParamIDs::voiceMode, 1.0f));


    // Unison: el Timbre 2 tiene su propio detune; el reparto estéreo es común.
    voiceManagerA_.setUnisonDetune(getParam(ParamIDs::unisonDetune, 10.0f));
    voiceManagerB_.setUnisonDetune(paramValue(true, ParamIDs::unisonDetune, 10.0f));
    voiceManagerA_.setUnisonSpread(getParam(ParamIDs::unisonSpread, 0.5f));
    voiceManagerB_.setUnisonSpread(getParam(ParamIDs::unisonSpread, 0.5f));

    // ── Timbre 1 y Timbre 2, cada uno con sus parámetros ──
    readTimbreParams(false, voiceParamsA_);
    readTimbreParams(true,  voiceParamsB_);

    // ── Modo de programa (Single / Split / Layer) y reparto de voces ──
    timbreMode_ = static_cast<int>(std::max(0.0f, std::min(2.0f, getParam(ParamIDs::timbreMode, 0.0f))));
    splitKey_ = static_cast<int>(std::max(0.0f, std::min(127.0f, getParam(ParamIDs::splitPoint, 60.0f))));

    // "Timbre Voice" (byte 0x10 bits 6,7): 1+3, 2+2 o 3+1 de las 4 voces del equipo
    // entre los dos timbres. En Advanced Mode se reparte por mitades.
    const int voiceSplit = static_cast<int>(std::max(0.0f, std::min(2.0f, getParam(ParamIDs::timbreVoices, 0.0f))));
    const size_t polyA = (synthMode == 2) ? (totalPolyphony / 2)
                                          : static_cast<size_t>(1 + voiceSplit);
    const size_t polyB = (synthMode == 2) ? (totalPolyphony - totalPolyphony / 2)
                                         : static_cast<size_t>(3 - voiceSplit);
    voiceManagerA_.setMaxPolyphony(std::max<size_t>(1, polyA));
    voiceManagerB_.setMaxPolyphony(std::max<size_t>(1, polyB));

    // Asignación por timbre (`voiceMode` y `t2VoiceMode`: Mono/Poly/Unison)
    const auto assignModeOf = [](int mode) {
        if (mode == 0) return VoiceAssignMode::Mono;
        if (mode == 2) return VoiceAssignMode::Unison;
        return VoiceAssignMode::Poly;
    };
    voiceManagerA_.setAssignMode(assignModeOf(vMode));
    voiceManagerB_.setAssignMode(assignModeOf(static_cast<int>(paramValue(true, ParamIDs::voiceMode, 1.0f))));

    // Arpeggiator & Mod Sequencer BPM
    float tempoBpm = (voiceParamsA_.bpm > 20.0f) ? voiceParamsA_.bpm : 120.0f;
    arpeggiator_.setTempoBPM(tempoBpm);
    arpeggiator_.setEnabled(getParam(ParamIDs::arpOn, 0.0f) > 0.5f);
    arpeggiator_.setType(static_cast<ArpType>(static_cast<int>(getParam(ParamIDs::arpType, 0.0f))));
    arpeggiator_.setOctaveRange(static_cast<int>(getParam(ParamIDs::arpRange, 1.0f)));
    arpeggiator_.setGateTime(getParam(ParamIDs::arpGate, 100.0f) / 127.0f);
    arpeggiator_.setLatch(getParam(ParamIDs::arpLatch, 0.0f) > 0.5f);
    arpeggiator_.setSyncResolution(static_cast<int>(getParam(ParamIDs::arpResolution, 3.0f)));

    // Mod Sequencer: tres filas por timbre (el reloj —tempo y resolución— es común;
    // en el equipo real la resolución también vive dentro de cada bloque de timbre).
    configureTimbreSeq(false, ModSequencer::TIMBRE1_TRACK);
    configureTimbreSeq(true,  ModSequencer::TIMBRE2_TRACK);
    modSeq_.setTempoBPM(tempoBpm);
    modSeq_.setEnabled((getParam(ParamIDs::modSeqOn, 0.0f) > 0.5f)
                       || (paramValue(true, ParamIDs::modSeqOn, 0.0f) > 0.5f));

    // ── Global FX parameters ──
    int eqLRaw = static_cast<int>(getParam(ParamIDs::eqLowFreq, 1.0f));
    int eqLIdx = (eqLRaw > 3) ? (eqLRaw / 32) : eqLRaw;
    float eqLGain = ((getParam(ParamIDs::eqLowGain, 64.0f) - 64.0f) / 64.0f) * 12.0f;

    int eqHRaw = static_cast<int>(getParam(ParamIDs::eqHighFreq, 2.0f));
    int eqHIdx = (eqHRaw > 3) ? (eqHRaw / 32) : eqHRaw;
    float eqHGain = ((getParam(ParamIDs::eqHighGain, 64.0f) - 64.0f) / 64.0f) * 12.0f;

    masterEQ_.setLowFreqIndex(eqLIdx);
    masterEQ_.setLowGainDB(eqLGain);
    masterEQ_.setHighFreqIndex(eqHIdx);
    masterEQ_.setHighGainDB(eqHGain);

    int mType = static_cast<int>(getParam(ParamIDs::modFxType, 0.0f));
    modFX_.setEnabled(getParam(ParamIDs::modFxOn, 1.0f) > 0.5f);
    modFX_.setType(static_cast<ModFXType>(std::min(mType, 2)));
    modFX_.setSpeed(getParam(ParamIDs::modFxSpeed, 40.0f) / 127.0f);
    modFX_.setDepth(getParam(ParamIDs::modFxDepth, 64.0f) / 127.0f);
    modFX_.setFeedback(getParam(ParamIDs::modFxFeedback, 0.0f));

    int dType = static_cast<int>(getParam(ParamIDs::delayType, 0.0f));
    delayFX_.setEnabled(getParam(ParamIDs::delayOn, 1.0f) > 0.5f);
    delayFX_.setType(static_cast<DelayType>(std::min(dType, 2)));
    float dTimeNorm = getParam(ParamIDs::delayTime, 40.0f) / 127.0f;
    float dTimeSec = 0.020f + (0.980f * dTimeNorm);
    delayFX_.setTimeSeconds(dTimeSec);
    delayFX_.setDepth(getParam(ParamIDs::delayDepth, 50.0f) / 127.0f);
    delayFX_.setFeedback(getParam(ParamIDs::delayFeedback, 40.0f) / 127.0f);

    const bool vocMode = getParam(ParamIDs::synthVocoderMode, 0.0f) > 0.5f;
    vocoder_.setEnabled(vocMode);

    if (vocMode)
    {
        int shift = static_cast<int>(getParam(ParamIDs::vocoderFormantShift, 2.0f)) - 2;
        vocoder_.setFormantShift(shift);
        vocoder_.setHPFLevel(getParam(ParamIDs::vocoderHpfLevel, 64.0f) / 127.0f);
        vocoder_.setGateSense(getParam(ParamIDs::vocoderGateSense, 50.0f) / 127.0f);
        vocoder_.setHPFThreshold(getParam(ParamIDs::vocoderGateSense, 50.0f) / 127.0f);
        vocoder_.setDirectLevel(getParam(ParamIDs::vocoderDirectLevel, 0.0f) / 127.0f);

        const char* bandParamIds[16] = {
            ParamIDs::vocoderBandLevel1,  ParamIDs::vocoderBandLevel2,
            ParamIDs::vocoderBandLevel3,  ParamIDs::vocoderBandLevel4,
            ParamIDs::vocoderBandLevel5,  ParamIDs::vocoderBandLevel6,
            ParamIDs::vocoderBandLevel7,  ParamIDs::vocoderBandLevel8,
            ParamIDs::vocoderBandLevel9,  ParamIDs::vocoderBandLevel10,
            ParamIDs::vocoderBandLevel11, ParamIDs::vocoderBandLevel12,
            ParamIDs::vocoderBandLevel13, ParamIDs::vocoderBandLevel14,
            ParamIDs::vocoderBandLevel15, ParamIDs::vocoderBandLevel16
        };
        for (size_t b = 0; b < 16; ++b)
        {
            vocoder_.setBandLevel(b, getParam(bandParamIds[b], 127.0f) / 127.0f);
        }
    }
}


void SynthEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                               juce::AudioPlayHead* playHead)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (playHead != nullptr)
    {
        if (auto pos = playHead->getPosition())
        {
            if (pos->getBpm().hasValue())
            {
                float hostBpm = static_cast<float>(*pos->getBpm());
                if (hostBpm >= 20.0f && hostBpm <= 300.0f)
                {
                    voiceParamsA_.bpm = hostBpm;
                    voiceParamsB_.bpm = hostBpm;
                    arpeggiator_.setTempoBPM(hostBpm);
                    modSeq_.setTempoBPM(hostBpm);
                }
            }
        }
    }

    updateParametersFromAPVTS();

    // Copy diagnostic bypass flags into voice parameter structs
    voiceParamsA_.diagBypassFilter = diagBypassFilter_.load(std::memory_order_relaxed);
    voiceParamsA_.diagBypassVCA = diagBypassVCA_.load(std::memory_order_relaxed);
    voiceParamsA_.diagBypassOscMixer = diagBypassOscMixer_.load(std::memory_order_relaxed);
    voiceParamsA_.diagBypassDistortion = diagBypassDistortion_.load(std::memory_order_relaxed);
    voiceParamsB_.diagBypassFilter = voiceParamsA_.diagBypassFilter;
    voiceParamsB_.diagBypassVCA = voiceParamsA_.diagBypassVCA;
    voiceParamsB_.diagBypassOscMixer = voiceParamsA_.diagBypassOscMixer;
    voiceParamsB_.diagBypassDistortion = voiceParamsA_.diagBypassDistortion;

    // 1. Avanza el reloj del Mod Sequencer y aplica cada timbre a **sus** parámetros
    modSeq_.advanceClock(numSamples);
    if (modSeq_.isEnabled())
    {
        if (paramValue(false, ParamIDs::modSeqOn, 0.0f) > 0.5f)
            applySeqModulation(false, voiceParamsA_);
        if (paramValue(true, ParamIDs::modSeqOn, 0.0f) > 0.5f)
            applySeqModulation(true, voiceParamsB_);
    }

    voiceManagerA_.applyBlockParams(voiceParamsA_);
    voiceManagerB_.applyBlockParams(voiceParamsB_);

    // 2. Process MIDI Messages
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            noteOn(msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            noteOff(msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            allNotesOff();
        }
        else if (msg.isPitchWheel())
        {
            float pb = (msg.getPitchWheelValue() - 8192) / 8192.0f;
            setPitchBend(pb);
        }
        else if (msg.isController() && msg.getControllerNumber() == 1)
        {
            float mw = msg.getControllerValue() / 127.0f;
            setModWheel(mw);
        }
    }

    // 3. Advance Arpeggiator Clock
    if (arpeggiator_.isEnabled())
    {
        arpEventsBuffer_.clear();
        if (arpeggiator_.processStep(numSamples, arpEventsBuffer_))
        {
            for (const auto& ev : arpEventsBuffer_)
            {
                triggerArpNote(ev.midiNote, ev.velocity, ev.isNoteOn);
            }
        }
    }

    const float* inL = (numChannels > 0) ? buffer.getReadPointer(0) : nullptr;
    const float* inR = (numChannels > 1) ? buffer.getReadPointer(1) : inL;

    buffer.clear();

    float* channelL = buffer.getWritePointer(0);
    float* channelR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    const bool isVocoderActive = vocoder_.isEnabled();
    const bool isExternalCarrier = apvts_.getRawParameterValue("vocoderCarrierSrc") &&
                                   apvts_.getRawParameterValue("vocoderCarrierSrc")->load() > 0.5f;

    const DiagnosticTonePoint diagPoint = diagTonePoint_.load(std::memory_order_relaxed);
    const float diagFreq = testToneFrequency_.load(std::memory_order_relaxed);
    const float diagLevel = testToneLevel_.load(std::memory_order_relaxed);

    // 4. Render Voices (Timbre A + Timbre B)
    for (int s = 0; s < numSamples; ++s)
    {
        // Compute Diagnostic Tone sample if enabled
        float diagTone = 0.0f;
        if (diagPoint != DiagnosticTonePoint::Disabled)
        {
            diagTone = static_cast<float>(std::sin(DSPUtils::TWO_PI * testTonePhase_)) * diagLevel;
            testTonePhase_ += diagFreq / sampleRate_;
            if (testTonePhase_ >= 1.0) testTonePhase_ -= 1.0;
        }

        // Forward diagnostic tone directly into voice if pre-filter or oscillator-override is selected
        voiceParamsA_.diagnosticTonePoint = static_cast<int>(diagPoint);
        voiceParamsA_.diagnosticToneValue = diagTone;
        voiceParamsB_.diagnosticTonePoint = static_cast<int>(diagPoint);
        voiceParamsB_.diagnosticToneValue = diagTone;

        float leftSample = 0.0f;
        float rightSample = 0.0f;

        const int diagPt = static_cast<int>(diagPoint);
        voiceManagerA_.process(leftSample, rightSample, diagPt, diagTone);

        // ── ABDScope taps: capture Voice-0 stage signals per sample ──
        {
            const auto& v0 = voiceManagerA_.getVoice(0).getDiagnosticStats();
            scopeBufOscMix_[s] = v0.mixedAudio;
            scopeBufPostFilter_[s] = v0.filtered;
            scopeBufPostVca_[s] = v0.vcaOut;
            scopeBufLfo1_[s] = voiceManagerA_.getVoice(0).getLfo1().getCurrentValue();
        }

        if (timbreMode_ == 1 || timbreMode_ == 2) // Split or Dual
        {
            float leftB = 0.0f, rightB = 0.0f;
            voiceManagerB_.process(leftB, rightB, diagPt, diagTone);
            leftSample += leftB;
            rightSample += rightB;
        }

        // Multimode Vocoder processing (only if active and not in diagnostic tone mode)
        if (isVocoderActive && diagPoint == DiagnosticTonePoint::Disabled)
        {
            float modSample = (inL != nullptr) ? inL[s] : 0.0f;
            float carrierSample = (leftSample + rightSample) * 0.5f;

            if (isExternalCarrier && inR != nullptr)
            {
                carrierSample = inR[s];
            }

            vocoder_.process(modSample, carrierSample, leftSample, rightSample);
        }

        // ── Point 3: Pre-Effects (Injects after voices/vocoder, before ModFX/DelayFX/EQ) ──
        if (diagPoint == DiagnosticTonePoint::PreEffects)
        {
            leftSample += diagTone;
            rightSample += diagTone;
        }

        // FX chain: Mod FX → Delay FX → Master EQ (with diagnostic bypass checks)
        scopeBufPreFxL_[s] = leftSample;
        scopeBufPreFxR_[s] = rightSample;
        if (!diagBypassModFX_.load(std::memory_order_relaxed))
            modFX_.process(leftSample, rightSample);
        if (!diagBypassDelayFX_.load(std::memory_order_relaxed))
            delayFX_.process(leftSample, rightSample);
        if (!diagBypassMasterEQ_.load(std::memory_order_relaxed))
            masterEQ_.process(leftSample, rightSample);

        // ── Point 2: Pre-Master Volume (Injects before master volume attenuation) ──
        if (diagPoint == DiagnosticTonePoint::PreMasterVolume)
        {
            leftSample += diagTone;
            rightSample += diagTone;
        }

        const float masterGain = smoothedMasterGain_.getNextValue();
        leftSample *= masterGain;
        rightSample *= masterGain;

        // ── Point 1: Post-Master Volume (Injects directly to the final DAC bus) ──
        if (diagPoint == DiagnosticTonePoint::PostMasterVolume)
        {
            leftSample += diagTone;
            rightSample += diagTone;
        }

        channelL[s] = leftSample;
        if (channelR != nullptr)
        {
            channelR[s] = rightSample;
        }

        // ── ABDScope taps: capture post-master DAC bus ──
        scopeBufMasterL_[s] = leftSample;
        scopeBufMasterR_[s] = rightSample;

        currentSnapshot_.scopeBuffer[currentSnapshot_.scopeWriteIndex] = leftSample;
        currentSnapshot_.scopeWriteIndex = (currentSnapshot_.scopeWriteIndex + 1) % AudioThreadSnapshot::kScopeBufferSize;
    }

    // Flush the captured per-sample scratch buffers into the ABDScope taps once per block.
    flushScopeTaps(numSamples);

    currentSnapshot_.activeVoiceCount = static_cast<uint32_t>(voiceManagerA_.getActiveVoiceCount() + voiceManagerB_.getActiveVoiceCount());
    currentSnapshot_.vuLeft = buffer.getMagnitude(0, 0, numSamples);
    currentSnapshot_.vuRight = (numChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : currentSnapshot_.vuLeft;

    static int s_blockCounter = 0;
    if (++s_blockCounter >= 50)
    {
        s_blockCounter = 0;
        uint32_t activeV = currentSnapshot_.activeVoiceCount;
        float peak = currentSnapshot_.vuLeft;
        if (activeV > 0 || diagPoint != DiagnosticTonePoint::Disabled || peak > 0.0001f || diagBypassVCA_.load(std::memory_order_relaxed))
        {
            const auto& v0 = voiceManagerA_.getVoice(0).getDiagnosticStats();
            ABD_LOG(juce::String("[DSP-AUDIT] ActiveVoices: ") + juce::String(activeV)
                    + " DiagPt: " + juce::String(static_cast<int>(diagPoint))
                    + " PeakOut: " + juce::String(peak)
                    + " MasterGain: " + juce::String(masterVolume_.load())
                    + " VocoderActive: " + juce::String(isVocoderActive ? "1" : "0")
                    + " Bypasses [Filt:" + juce::String(diagBypassFilter_.load() ? "1" : "0")
                    + " VCA:" + juce::String(diagBypassVCA_.load() ? "1" : "0")
                    + " Mix:" + juce::String(diagBypassOscMixer_.load() ? "1" : "0") + "]"
                    + " Voice0 [Pitch:" + juce::String(v0.basePitch, 1)
                    + " Osc1:" + juce::String(v0.osc1Sig, 4)
                    + " EG2:" + juce::String(v0.eg2Val, 4)
                    + " Filt:" + juce::String(v0.filtered, 4)
                    + " VCA:" + juce::String(v0.vcaOut, 4)
                    + " LOut:" + juce::String(v0.leftOutSample, 4) + "]"
                    + " Params [CutoffNorm:" + juce::String(voiceParamsA_.filterCutoffNorm)
                    + " AmpLvl:" + juce::String(voiceParamsA_.ampLevel)
                    + " Osc1Lvl:" + juce::String(voiceParamsA_.osc1Level)
                    + " Osc1Wave:" + juce::String(static_cast<int>(voiceParamsA_.osc1Type)) + "]");
        }
    }
}

void SynthEngine::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
    ABD_LOG(juce::String("[ENGINE] noteOn: ch=") + juce::String(midiChannel)
            + " note=" + juce::String(midiNoteNumber)
            + " vel=" + juce::String(velocity)
            + " arp=" + juce::String(arpeggiator_.isEnabled() ? "ON" : "OFF")
            + " timbreMode=" + juce::String(timbreMode_)
            + " maxPoly=" + juce::String(voiceManagerA_.getMaxPolyphony()));

    // ¿Es la primera nota de cada timbre? El Key Sync "Timbre" reinicia la fila al
    // primer note-on del timbre, y "Voice" en cada uno (byte 53 bits 0,1 reales).
    const bool firstNoteOfA = (voiceManagerA_.getActiveVoiceCount() == 0);
    const bool firstNoteOfB = (voiceManagerB_.getActiveVoiceCount() == 0);
    const bool playsA = (timbreMode_ != 1) || (midiNoteNumber < splitKey_);
    const bool playsB = (timbreMode_ == 2) || (timbreMode_ == 1 && midiNoteNumber >= splitKey_);

    if (arpeggiator_.isEnabled())
    {
        arpeggiator_.noteOn(midiNoteNumber, velocity);
    }
    else
    {
        if (timbreMode_ == 1) // Split
        {
            if (midiNoteNumber < splitKey_) voiceManagerA_.noteOn(midiNoteNumber, velocity);
            else voiceManagerB_.noteOn(midiNoteNumber, velocity);
        }
        else if (timbreMode_ == 2) // Dual / Layer
        {
            voiceManagerA_.noteOn(midiNoteNumber, velocity);
            voiceManagerB_.noteOn(midiNoteNumber, velocity);
        }
        else // Single / Vocoder
        {
            voiceManagerA_.noteOn(midiNoteNumber, velocity);
        }
    }

    const auto keySyncTimbre = [this](bool second, bool plays, bool firstOfTimbre) {
        if (!plays) return;
        const int sync = static_cast<int>(paramValue(second, ParamIDs::seqKeySync, 0.0f));
        if (sync == static_cast<int>(ModSeqKeySync::Off)) return;
        if (sync == static_cast<int>(ModSeqKeySync::Timbre) && !firstOfTimbre) return;
        const size_t first = second ? ModSequencer::TIMBRE2_TRACK : ModSequencer::TIMBRE1_TRACK;
        modSeq_.triggerKeySync(first, ModSequencer::TRACKS_PER_TIMBRE);
    };
    keySyncTimbre(false, playsA, firstNoteOfA);
    keySyncTimbre(true,  playsB, firstNoteOfB);
}

void SynthEngine::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
    ABD_LOG(juce::String("[ENGINE] noteOff: ch=") + juce::String(midiChannel)
            + " note=" + juce::String(midiNoteNumber));
    if (arpeggiator_.isEnabled())
    {
        arpeggiator_.noteOff(midiNoteNumber);
    }
    else
    {
        if (timbreMode_ == 1) // Split
        {
            if (midiNoteNumber < splitKey_) voiceManagerA_.noteOff(midiNoteNumber);
            else voiceManagerB_.noteOff(midiNoteNumber);
        }
        else if (timbreMode_ == 2) // Dual / Layer
        {
            voiceManagerA_.noteOff(midiNoteNumber);
            voiceManagerB_.noteOff(midiNoteNumber);
        }
        else // Single / Vocoder
        {
            voiceManagerA_.noteOff(midiNoteNumber);
        }
    }
}

void SynthEngine::triggerArpNote(int note, float velocity, bool isNoteOn) noexcept
{
    if (isNoteOn)
    {
        if (timbreMode_ == 1)
        {
            if (note < splitKey_) voiceManagerA_.noteOn(note, velocity);
            else voiceManagerB_.noteOn(note, velocity);
        }
        else if (timbreMode_ == 2)
        {
            voiceManagerA_.noteOn(note, velocity);
            voiceManagerB_.noteOn(note, velocity);
        }
        else
        {
            voiceManagerA_.noteOn(note, velocity);
        }
    }
    else
    {
        if (timbreMode_ == 1)
        {
            if (note < splitKey_) voiceManagerA_.noteOff(note);
            else voiceManagerB_.noteOff(note);
        }
        else if (timbreMode_ == 2)
        {
            voiceManagerA_.noteOff(note);
            voiceManagerB_.noteOff(note);
        }
        else
        {
            voiceManagerA_.noteOff(note);
        }
    }
}

void SynthEngine::allNotesOff()
{
    voiceManagerA_.allNotesOff();
    voiceManagerB_.allNotesOff();
    arpeggiator_.allNotesOff();
}

} // namespace ABDMS2000