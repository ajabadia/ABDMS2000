#include "Voice.h"
#include <cmath>

namespace ABDMS2000 {

void Voice::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    osc1VA_.prepare(sampleRate_);
    osc1DWGS_.prepare(sampleRate_);
    osc1VoxWave_.prepare(sampleRate_);
    osc2_.prepare(sampleRate_);
    noiseGen_.reset();
    filter_.prepare(sampleRate_);
    eg1_.prepare(sampleRate_);
    eg2_.prepare(sampleRate_);
    glide_.prepare(sampleRate_);
    lfo1_.prepare(sampleRate_);
    lfo2_.prepare(sampleRate_);
    reset();
}

void Voice::reset() noexcept
{
    osc1VA_.reset();
    osc1DWGS_.reset();
    osc1VoxWave_.reset();
    osc2_.reset();
    filter_.reset();
    eg1_.reset();
    eg2_.reset();
    glide_.reset(static_cast<float>(currentMidiNote_));
    lfo1_.reset();
    lfo2_.reset();
    noteAge_ = 0;
    prevMasterPhase_ = 0.0f;
}

void Voice::noteOn(int midiNote, float velocity, bool glideEnabled, bool isFirstTimbreNote, bool isLegato) noexcept
{
    currentMidiNote_ = midiNote;
    velocity_ = velocity;
    noteAge_ = 0;

    glide_.setTargetNote(static_cast<float>(midiNote), glideEnabled);

    if (!isLegato)
    {
        osc1VA_.reset();
        osc1DWGS_.reset();
        osc1VoxWave_.reset();
        osc2_.reset();
        filter_.reset();

        eg1_.noteOn(velocity);
        eg2_.noteOn(velocity);
    }

    lfo1_.triggerKeySync(isFirstTimbreNote);
    lfo2_.triggerKeySync(isFirstTimbreNote);
}

void Voice::noteOff() noexcept
{
    eg1_.noteOff();
    eg2_.noteOff();
}

void Voice::stopImmediately() noexcept
{
    eg1_.reset();
    eg2_.reset();
}

bool Voice::isActive() const noexcept
{
    return !eg2_.isIdle();
}

float Voice::getCurrentAmpLevel() const noexcept
{
    return eg2_.getCurrentLevel();
}

void Voice::applyBlockParams(const VoiceParameters& params) noexcept
{
    cachedParams_ = params;

    // ── Static DSP settings: applied once per audio block ──

    glide_.setGlideTime(params.portamentoTime / 127.0f);

    // LFO1: Saw / Square / Triangle / S&H
    lfo1_.setBpm(params.bpm);
    lfo1_.setTempoSync(params.lfo1TempoSync, params.lfo1SyncNote);
    if (!params.lfo1TempoSync)
        lfo1_.setFrequencyHz(params.lfo1FreqHz);
    lfo1_.setWaveformLFO1(params.lfo1Wave);
    lfo1_.setKeySyncMode(static_cast<int>(params.lfo1KeySync));

    // LFO2: Saw / Square+ / Sine / S&H
    lfo2_.setBpm(params.bpm);
    lfo2_.setTempoSync(params.lfo2TempoSync, params.lfo2SyncNote);
    if (!params.lfo2TempoSync)
        lfo2_.setFrequencyHz(params.lfo2FreqHz);
    lfo2_.setWaveformLFO2(params.lfo2Wave);
    lfo2_.setKeySyncMode(static_cast<int>(params.lfo2KeySync));

    // Envelope ADSR times
    eg1_.setAttack(params.eg1Attack);
    eg1_.setDecay(params.eg1Decay);
    eg1_.setSustain(params.eg1Sustain);
    eg1_.setRelease(params.eg1Release);

    eg2_.setAttack(params.eg2Attack);
    eg2_.setDecay(params.eg2Decay);
    eg2_.setSustain(params.eg2Sustain);
    eg2_.setRelease(params.eg2Release);

    // Virtual Patch Matrix: 4 mod slot routings
    for (size_t s = 0; s < 4; ++s)
    {
        patchMatrix_.setSlot(s, params.patchSlots[s].source,
                             params.patchSlots[s].destination,
                             params.patchSlots[s].intensity);
    }

    // OSC 1: waveform type and DWGS index
    if (params.osc1Type <= OSC1Type::Sine)
        osc1VA_.setWaveform(static_cast<VAWaveform>(params.osc1Type));
    osc1DWGS_.setWaveIndex(params.osc1DwgsIndex);

    // OSC 2: waveform type
    osc2_.setWaveform(params.osc2Wave);

    // Filter: type and resonance (cutoff varies per-sample due to EG sweep)
    filter_.setType(params.filterType);
    filter_.setResonance(params.filterResonance);
}

void Voice::renderNextSample(float& leftOut, float& rightOut, int diagPoint, float diagTone) noexcept
{
    const VoiceParameters& p = cachedParams_;
    if (!isActive() && !p.diagBypassVCA) return;

    noteAge_++;

    // 1. Portamento glide → per-sample base pitch
    float basePitch = glide_.getNextPitchSemitones();

    // 2. Step LFO1 and Envelopes
    float lfo1Val = lfo1_.getNextSample();
    float eg1Val  = eg1_.getNextSample();
    float eg2Val  = eg2_.getNextSample();

    // 3. Evaluate LFO2 with potential Virtual Patch frequency cross-modulation
    PatchModulationSources prelimSources;
    prelimSources.eg1       = eg1Val;
    prelimSources.eg2       = eg2Val;
    prelimSources.lfo1      = lfo1Val;
    prelimSources.lfo2      = lfo2_.getCurrentValue();
    prelimSources.velocity  = velocity_;
    prelimSources.kbdTrack  = (basePitch - 60.0f) / 64.0f;
    prelimSources.pitchBend = p.pitchBendValue;
    prelimSources.modWheel  = p.modWheelValue;

    PatchModulationOutputs prelimMod = patchMatrix_.evaluate(prelimSources);
    float lfo2Freq = p.lfo2FreqHz * std::pow(2.0f, prelimMod.lfo2FreqMod * 4.0f);
    lfo2_.setFrequencyHz(lfo2Freq);
    float lfo2Val = lfo2_.getNextSample();

    // 4. Full modulation matrix evaluation
    PatchModulationSources sources = prelimSources;
    sources.lfo2 = lfo2Val;
    PatchModulationOutputs mod = patchMatrix_.evaluate(sources);

    // 5. Portamento + LFO + Virtual Patch Pitch Modulation + Voice Detune
    float voiceDetuneSemitones = p.voiceDetuneCents / 100.0f;
    float osc1PitchMod = (p.pitchBendValue * 2.0f) + (mod.pitchMod * 24.0f) + voiceDetuneSemitones;
    float osc1FinalPitch = basePitch + osc1PitchMod;
    float osc1Freq = DSPUtils::midiNoteToFrequency(osc1FinalPitch);

    float osc2PitchMod = (p.pitchBendValue * 2.0f) + (mod.pitchMod * 24.0f) + voiceDetuneSemitones
                       + p.osc2Semitone + (p.osc2Tune / 100.0f);
    float osc2FinalPitch = basePitch + osc2PitchMod;
    float osc2Freq = DSPUtils::midiNoteToFrequency(osc2FinalPitch);

    // 6. Set oscillator frequencies
    osc1VA_.setFrequency(osc1Freq);
    osc1DWGS_.setFrequency(osc1Freq);
    osc1VoxWave_.setFrequency(osc1Freq);
    osc2_.setFrequency(osc2Freq);

    // 7. Render OSC 1
    float osc1Sig = 0.0f;
    switch (p.osc1Type)
    {
        case OSC1Type::Saw:
        case OSC1Type::Pulse:
        case OSC1Type::Triangle:
        case OSC1Type::Sine:
            osc1VA_.setWaveform(static_cast<VAWaveform>(p.osc1Type));
            osc1VA_.setControl1(DSPUtils::clamp(p.osc1Ctrl1 + mod.osc1Ctrl1Mod, 0.0f, 1.0f));
            osc1Sig = osc1VA_.getNextSample();
            break;

        case OSC1Type::DWGS:
            osc1DWGS_.setWaveIndex(p.osc1DwgsIndex);
            osc1Sig = osc1DWGS_.getNextSample();
            break;

        case OSC1Type::VoxWave:
            osc1VoxWave_.setVowel(DSPUtils::clamp(p.osc1Ctrl1 + mod.osc1Ctrl1Mod, 0.0f, 1.0f));
            osc1Sig = osc1VoxWave_.getNextSample();
            break;

        case OSC1Type::Noise:
            osc1Sig = noiseGen_.getWhiteNoise();
            break;

        case OSC1Type::AudioIn:
        default:
            osc1Sig = 0.0f;
            break;
    }

    // 8. Render OSC 2 with modulation mode
    float osc2Sig = 0.0f;
    if (p.osc2Level > 0.001f || p.osc2ModMode != OSC2ModulationMode::Off)
    {
        if (p.osc2ModMode == OSC2ModulationMode::Sync)
        {
            if (osc1VA_.getPhase() < prevMasterPhase_) osc2_.reset();
        }
        osc2Sig = osc2_.getNextSample();
    }
    prevMasterPhase_ = osc1VA_.getPhase();

    if (diagPoint == 5) // Diagnostic Point 5: Override OSC1
    {
        osc1Sig = diagTone;
    }

    // 9. Noise
    float noiseLevel = DSPUtils::clamp(p.noiseLevel + mod.noiseLevelMod, 0.0f, 1.0f);
    float noiseSig   = (noiseLevel > 0.001f) ? noiseGen_.getWhiteNoise() : 0.0f;

    // 10. Mixer stage — sum OSC1, OSC2, Noise
    float osc1Level = p.diagBypassOscMixer ? 1.0f : p.osc1Level;
    float osc1Mixed = osc1Sig * osc1Level;
    float osc2Mixed = osc2Sig * p.osc2Level;
    float mixedAudio = osc1Mixed + osc2Mixed + (noiseSig * noiseLevel);

    if (diagPoint == 4) // Diagnostic Point 4: PreFilter (Bypasses OSC & Mixer)
    {
        mixedAudio = diagTone;
    }

    // 11. Filter stage
    float filtered = mixedAudio;
    if (!p.diagBypassFilter)
    {
        float baseHz       = DSPUtils::convertSysExToCutoffHz(p.filterCutoffNorm);
        float egOctaves    = eg1Val * (p.eg1FilterIntensity * 5.0f);
        float kbdOctaves   = ((basePitch - 60.0f) / 12.0f) * p.filterKbdTrack;
        float patchOctaves = mod.cutoffMod * 5.0f;
        float cutoffHz     = baseHz * std::pow(2.0f, egOctaves + kbdOctaves + patchOctaves);

        filter_.setCutoff(cutoffHz);
        filtered = filter_.process(mixedAudio);
    }

    // 12. Distortion — between Filter and VCA per MS2000 block diagram (Section 5.1)
    //     MS2000 distortion is a fixed-drive on/off toggle that saturates the post-filter signal.
    if (p.distortionOn && !p.diagBypassDistortion)
    {
        filtered = DSPUtils::ampDistortion(filtered, 0.85f);
    }

    // 13. VCA (Amp) stage
    float vcaOut = filtered;
    if (!p.diagBypassVCA)
    {
        float amp = DSPUtils::clamp(p.ampLevel + mod.ampMod, 0.0f, 1.0f);
        vcaOut = filtered * eg2Val * amp;
    }

    // 14. Stereo pan — Constant Equal-Power law (trigonometric -3dB quadrant)
    float effPan = DSPUtils::clamp(p.panpot + (mod.panMod * 0.5f), -1.0f, 1.0f);
    float angle  = (effPan * 0.5f + 0.5f) * (DSPUtils::PI * 0.5f); // [0, PI/2]
    float lGain  = std::cos(angle);
    float rGain  = std::sin(angle);

    leftOut  += vcaOut * lGain;
    rightOut += vcaOut * rGain;

    lastDiagStats_ = { basePitch, osc1Sig, mixedAudio, filtered, eg1Val, eg2Val, vcaOut, lGain, rGain, vcaOut * lGain, vcaOut * rGain };
}

void Voice::renderDiagnosticSample(float& leftOut, float& rightOut, int diagPoint, float diagTone) noexcept
{
    const auto& p = cachedParams_;
    float mixedAudio = diagTone;
    if (diagPoint == 5) // OscillatorDirect: apply osc1Level
    {
        float osc1Level = p.diagBypassOscMixer ? 1.0f : p.osc1Level;
        mixedAudio = diagTone * osc1Level;
    }

    // Filter stage
    float filtered = mixedAudio;
    if (!p.diagBypassFilter)
    {
        float baseHz = DSPUtils::convertSysExToCutoffHz(p.filterCutoffNorm);
        filter_.setCutoff(baseHz);
        filtered = filter_.process(mixedAudio);
    }

    // VCA stage
    float vcaOut = filtered;
    if (!p.diagBypassVCA)
    {
        float amp = DSPUtils::clamp(p.ampLevel, 0.0f, 1.0f);
        vcaOut = filtered * amp;
    }

    // Pan
    float angle = (p.panpot * 0.5f + 0.5f) * (DSPUtils::PI * 0.5f);
    float lGain = std::cos(angle);
    float rGain = std::sin(angle);
    leftOut += vcaOut * lGain;
    rightOut += vcaOut * rGain;

    lastDiagStats_ = { 60.0f, 0.0f, mixedAudio, filtered, 0.0f, 1.0f, vcaOut, lGain, rGain, vcaOut * lGain, vcaOut * rGain };
}

} // namespace ABDMS2000