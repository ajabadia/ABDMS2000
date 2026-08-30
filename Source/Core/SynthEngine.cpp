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

    int vMode = static_cast<int>(getParam(ParamIDs::voiceMode, 1.0f));
    voiceAssignMode_ = vMode;

    if (vMode == 0) // Mono
    {
        voiceManagerA_.setAssignMode(VoiceAssignMode::Mono);
        voiceManagerA_.setMaxPolyphony(1);
        voiceManagerB_.setAssignMode(VoiceAssignMode::Mono);
        voiceManagerB_.setMaxPolyphony(1);
    }
    else if (vMode == 2) // Unison
    {
        voiceManagerA_.setAssignMode(VoiceAssignMode::Unison);
        voiceManagerA_.setMaxPolyphony(totalPolyphony);
        voiceManagerB_.setAssignMode(VoiceAssignMode::Unison);
        voiceManagerB_.setMaxPolyphony(totalPolyphony);
    }
    else // Poly
    {
        voiceManagerA_.setAssignMode(VoiceAssignMode::Poly);
        voiceManagerA_.setMaxPolyphony(totalPolyphony);
        voiceManagerB_.setAssignMode(VoiceAssignMode::Poly);
        voiceManagerB_.setMaxPolyphony(totalPolyphony);
    }

    voiceManagerA_.setUnisonDetune(getParam(ParamIDs::unisonDetune, 10.0f));
    voiceManagerA_.setUnisonSpread(getParam(ParamIDs::unisonSpread, 0.5f));

    voiceManagerB_.setUnisonDetune(getParam(ParamIDs::unisonDetune, 10.0f));
    voiceManagerB_.setUnisonSpread(getParam(ParamIDs::unisonSpread, 0.5f));

    bool pOn = (getParam(ParamIDs::portamentoOn, 1.0f) > 0.5f);
    float pTime = pOn ? getParam(ParamIDs::portamentoTime, 0.0f) : 0.0f;
    voiceParamsA_.portamentoTime = pTime;
    voiceParamsB_.portamentoTime = pTime;

    // OSC 1 — 8 algorithms: Saw=0, Pulse=1, Tri=2, Sine=3, VoxWave=4, DWGS=5, Noise=6, AudioIn=7
    int o1w = static_cast<int>(getParam(ParamIDs::osc1Wave, 0.0f));
    voiceParamsA_.osc1Type = static_cast<OSC1Type>(std::min(o1w, 7));
    voiceParamsA_.osc1DwgsIndex = static_cast<int>(getParam(ParamIDs::osc1DwgsWave, 0.0f));
    voiceParamsA_.osc1Ctrl1 = getParam(ParamIDs::osc1Ctrl1, 0.0f) / 127.0f;
    voiceParamsA_.osc1Ctrl2 = getParam(ParamIDs::osc1Ctrl2, 0.0f) / 127.0f;
    voiceParamsA_.osc1Level = getParam(ParamIDs::mixOsc1Level, 127.0f) / 127.0f;

    // OSC 2
    int o2w = static_cast<int>(getParam(ParamIDs::osc2Wave, 0.0f));
    voiceParamsA_.osc2Wave = static_cast<VAWaveform>(std::min(o2w, 2));
    voiceParamsA_.osc2Semitone = getParam(ParamIDs::osc2Semitone, 0.0f);
    voiceParamsA_.osc2Tune = getParam(ParamIDs::osc2Tune, 0.0f);
    int modMode = static_cast<int>(getParam(ParamIDs::osc2ModType, 0.0f));
    voiceParamsA_.osc2ModMode = static_cast<OSC2ModulationMode>(std::min(modMode, 3));
    voiceParamsA_.osc2Level = getParam(ParamIDs::mixOsc2Level, 0.0f) / 127.0f;

    // Noise
    voiceParamsA_.noiseLevel = getParam(ParamIDs::mixNoiseLevel, 0.0f) / 127.0f;

    // Filter
    int fType = static_cast<int>(getParam(ParamIDs::filterType, 0.0f));
    voiceParamsA_.filterType = static_cast<FilterType>(fType);
    voiceParamsA_.filterCutoffNorm = getParam(ParamIDs::filterCutoff, 127.0f) / 127.0f;
    voiceParamsA_.filterResonance = getParam(ParamIDs::filterResonance, 0.0f) / 127.0f;
    voiceParamsA_.eg1FilterIntensity = getParam(ParamIDs::filterEg1Int, 0.0f) / 63.0f;
    voiceParamsA_.filterKbdTrack = getParam(ParamIDs::filterKeyTrack, 0.0f) / 63.0f;

    // EG1
    voiceParamsA_.eg1Attack = getParam(ParamIDs::eg1Attack, 0.0f) / 127.0f;
    voiceParamsA_.eg1Decay = getParam(ParamIDs::eg1Decay, 40.0f) / 127.0f;
    voiceParamsA_.eg1Sustain = getParam(ParamIDs::eg1Sustain, 0.0f) / 127.0f;
    voiceParamsA_.eg1Release = getParam(ParamIDs::eg1Release, 40.0f) / 127.0f;

    // EG2
    voiceParamsA_.eg2Attack = getParam(ParamIDs::eg2Attack, 0.0f) / 127.0f;
    voiceParamsA_.eg2Decay = getParam(ParamIDs::eg2Decay, 40.0f) / 127.0f;
    voiceParamsA_.eg2Sustain = getParam(ParamIDs::eg2Sustain, 127.0f) / 127.0f;
    voiceParamsA_.eg2Release = getParam(ParamIDs::eg2Release, 40.0f) / 127.0f;

    // LFO1: Saw=0, Square=1, Triangle=2, SampleHold=3
    int l1w = static_cast<int>(getParam(ParamIDs::lfo1Wave, 2.0f));
    voiceParamsA_.lfo1Wave = static_cast<LFOWaveform>(std::min(l1w, 3));
    voiceParamsA_.lfo1KeySync = static_cast<KeySyncMode>(static_cast<int>(getParam(ParamIDs::lfo1KeySync, 2.0f)));
    float l1FreqNorm = getParam(ParamIDs::lfo1Freq, 40.0f) / 127.0f;
    voiceParamsA_.lfo1FreqHz = 0.01f * std::pow(2000.0f, std::pow(l1FreqNorm, 0.8f));
    voiceParamsA_.lfo1TempoSync = (getParam(ParamIDs::lfo1TempoSync, 0.0f) > 0.5f);
    voiceParamsA_.lfo1SyncNote = static_cast<int>(getParam(ParamIDs::lfo1SyncNote, 4.0f));

    // LFO2: Saw=0, SquarePlus=1, Sine=2, SampleHold=3
    int l2w = static_cast<int>(getParam(ParamIDs::lfo2Wave, 2.0f));
    voiceParamsA_.lfo2Wave = static_cast<LFOWaveformLFO2>(std::min(l2w, 3));
    voiceParamsA_.lfo2KeySync = static_cast<KeySyncMode>(static_cast<int>(getParam(ParamIDs::lfo2KeySync, 2.0f)));
    float l2FreqNorm = getParam(ParamIDs::lfo2Freq, 40.0f) / 127.0f;
    voiceParamsA_.lfo2FreqHz = 0.01f * std::pow(2000.0f, std::pow(l2FreqNorm, 0.8f));
    voiceParamsA_.lfo2TempoSync = (getParam(ParamIDs::lfo2TempoSync, 0.0f) > 0.5f);
    voiceParamsA_.lfo2SyncNote = static_cast<int>(getParam(ParamIDs::lfo2SyncNote, 4.0f));

    // Amp
    voiceParamsA_.ampLevel = getParam(ParamIDs::ampLevel, 100.0f) / 127.0f;
    voiceParamsA_.panpot = getParam(ParamIDs::ampPan, 0.0f) / 64.0f;
    voiceParamsA_.distortionOn = (getParam(ParamIDs::ampDistortion, 0.0f) > 0.5f);
    voiceParamsA_.ampKeyTrack = getParam(ParamIDs::ampKeyTrack, 0.0f) / 63.0f;

    // Wheel inputs
    voiceParamsA_.pitchBendValue = pitchBendValue_.load(std::memory_order_relaxed);
    voiceParamsA_.modWheelValue = modWheelValue_.load(std::memory_order_relaxed);

    // Virtual Patch Slots 1-4
    voiceParamsA_.patchSlots[0].source = static_cast<PatchSource>(static_cast<int>(getParam(ParamIDs::patch1Source, 0.0f)));
    voiceParamsA_.patchSlots[0].destination = static_cast<PatchDestination>(static_cast<int>(getParam(ParamIDs::patch1Destination, 4.0f)));
    voiceParamsA_.patchSlots[0].intensity = getParam(ParamIDs::patch1Intensity, 0.0f) / 63.0f;

    voiceParamsA_.patchSlots[1].source = static_cast<PatchSource>(static_cast<int>(getParam(ParamIDs::patch2Source, 1.0f)));
    voiceParamsA_.patchSlots[1].destination = static_cast<PatchDestination>(static_cast<int>(getParam(ParamIDs::patch2Destination, 4.0f)));
    voiceParamsA_.patchSlots[1].intensity = getParam(ParamIDs::patch2Intensity, 0.0f) / 63.0f;

    voiceParamsA_.patchSlots[2].source = static_cast<PatchSource>(static_cast<int>(getParam(ParamIDs::patch3Source, 2.0f)));
    voiceParamsA_.patchSlots[2].destination = static_cast<PatchDestination>(static_cast<int>(getParam(ParamIDs::patch3Destination, 0.0f)));
    voiceParamsA_.patchSlots[2].intensity = getParam(ParamIDs::patch3Intensity, 0.0f) / 63.0f;

    voiceParamsA_.patchSlots[3].source = static_cast<PatchSource>(static_cast<int>(getParam(ParamIDs::patch4Source, 3.0f)));
    voiceParamsA_.patchSlots[3].destination = static_cast<PatchDestination>(static_cast<int>(getParam(ParamIDs::patch4Destination, 7.0f)));
    voiceParamsA_.patchSlots[3].intensity = getParam(ParamIDs::patch4Intensity, 0.0f) / 63.0f;

    // Timbre B mirroring / offsets
    voiceParamsB_ = voiceParamsA_;

    // Arpeggiator & Mod Sequencer BPM
    float tempoBpm = (voiceParamsA_.bpm > 20.0f) ? voiceParamsA_.bpm : 120.0f;
    arpeggiator_.setTempoBPM(tempoBpm);
    arpeggiator_.setEnabled(getParam(ParamIDs::arpOn, 0.0f) > 0.5f);
    arpeggiator_.setType(static_cast<ArpType>(static_cast<int>(getParam(ParamIDs::arpType, 0.0f))));
    arpeggiator_.setOctaveRange(static_cast<int>(getParam(ParamIDs::arpRange, 1.0f)));
    arpeggiator_.setGateTime(getParam(ParamIDs::arpGate, 100.0f) / 127.0f);
    arpeggiator_.setLatch(getParam(ParamIDs::arpLatch, 0.0f) > 0.5f);
    arpeggiator_.setSyncResolution(static_cast<int>(getParam(ParamIDs::arpResolution, 3.0f)));

    // Mod Sequencer
    bool modSeqActive = getParam(ParamIDs::modSeqOn, 0.0f) > 0.5f;
    modSeq_.setEnabled(modSeqActive);
    modSeq_.setTempoBPM(tempoBpm);
    modSeq_.setSyncResolution(static_cast<int>(getParam(ParamIDs::modSeqResolution, 3.0f)));
    int seqType = static_cast<int>(getParam(ParamIDs::modSeqType, 0.0f));
    int seqSmooth = static_cast<int>(getParam(ParamIDs::modSeqSmooth, 0.0f));
    for (size_t t = 0; t < ModSequencer::NUM_TRACKS; ++t)
    {
        modSeq_.getTrack(t).mode = static_cast<ModSeqMode>(std::min(3, seqType));
        modSeq_.getTrack(t).motion = (seqSmooth > 0) ? ModSeqMotion::Smooth : ModSeqMotion::Step;
    }

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

    // 1. Advance Modulation Sequencer Clock and apply to voice parameters
    modSeq_.advanceClock(numSamples);
    if (modSeq_.isEnabled())
    {
        float modCutoff = (modSeq_.getTrackOutput(0) - 0.5f) * 0.8f;
        voiceParamsA_.filterCutoffNorm = DSPUtils::clamp(voiceParamsA_.filterCutoffNorm + modCutoff, 0.0f, 1.0f);
        voiceParamsB_.filterCutoffNorm = voiceParamsA_.filterCutoffNorm;
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

        currentSnapshot_.scopeBuffer[currentSnapshot_.scopeWriteIndex] = leftSample;
        currentSnapshot_.scopeWriteIndex = (currentSnapshot_.scopeWriteIndex + 1) % AudioThreadSnapshot::kScopeBufferSize;
    }

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
    modSeq_.triggerKeySync();
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