#include "SynthEngine.h"
#include <algorithm>

namespace ABDMS2000 {

SynthEngine::SynthEngine(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
}

void SynthEngine::prepare(double sampleRate, int samplesPerBlock)
{
    sampleRate_ = sampleRate;
    samplesPerBlock_ = samplesPerBlock;
    smoothedMasterGain_.reset(sampleRate, 0.02); // 20ms smoothing
    voiceManager_.prepare(sampleRate);
    reset();
}

void SynthEngine::reset()
{
    voiceManager_.reset();
    currentSnapshot_ = AudioThreadSnapshot{};
}

void SynthEngine::updateParametersFromAPVTS() noexcept
{
    auto getParam = [this](const char* id, float defaultVal = 0.0f) -> float {
        if (auto* p = apvts_.getRawParameterValue(id))
            return p->load(std::memory_order_relaxed);
        return defaultVal;
    };

    // Master
    masterVolume_.store(getParam(ParamIDs::masterVolume, 0.8f), std::memory_order_relaxed);
    smoothedMasterGain_.setTargetValue(masterVolume_.load(std::memory_order_relaxed));

    // Voice Architecture
    int vMode = static_cast<int>(getParam(ParamIDs::voiceMode, 1.0f)); // 0: Mono, 1: Poly, 2: Unison
    if (vMode == 0) voiceManager_.setAssignMode(VoiceAssignMode::Mono);
    else if (vMode == 2) voiceManager_.setAssignMode(VoiceAssignMode::Unison);
    else voiceManager_.setAssignMode(VoiceAssignMode::Poly);

    voiceManager_.setUnisonDetune(getParam(ParamIDs::unisonDetune, 10.0f));
    voiceManager_.setUnisonSpread(getParam(ParamIDs::unisonSpread, 0.5f));
    voiceParams_.portamentoTime = getParam(ParamIDs::portamentoTime, 0.0f);

    // OSC 1
    int o1w = static_cast<int>(getParam(ParamIDs::osc1Wave, 0.0f));
    voiceParams_.osc1Wave = static_cast<VAWaveform>(std::min(o1w, 3));
    voiceParams_.osc1Ctrl1 = getParam(ParamIDs::osc1Ctrl1, 0.0f) / 127.0f;
    voiceParams_.osc1Level = getParam(ParamIDs::mixOsc1Level, 127.0f) / 127.0f;

    // OSC 2
    int o2w = static_cast<int>(getParam(ParamIDs::osc2Wave, 0.0f));
    voiceParams_.osc2Wave = static_cast<VAWaveform>(std::min(o2w, 2));
    voiceParams_.osc2Semitone = getParam(ParamIDs::osc2Semitone, 0.0f);
    voiceParams_.osc2Tune = getParam(ParamIDs::osc2Tune, 0.0f);
    int modMode = static_cast<int>(getParam(ParamIDs::osc2ModType, 0.0f));
    voiceParams_.osc2ModMode = static_cast<OSC2ModulationMode>(modMode);
    voiceParams_.osc2Level = getParam(ParamIDs::mixOsc2Level, 0.0f) / 127.0f;

    // Noise
    voiceParams_.noiseLevel = getParam(ParamIDs::mixNoiseLevel, 0.0f) / 127.0f;

    // Filter
    int fType = static_cast<int>(getParam(ParamIDs::filterType, 0.0f));
    voiceParams_.filterType = static_cast<FilterType>(fType);
    voiceParams_.filterCutoffNorm = getParam(ParamIDs::filterCutoff, 127.0f) / 127.0f;
    voiceParams_.filterResonance = getParam(ParamIDs::filterResonance, 0.0f) / 127.0f;
    voiceParams_.eg1FilterIntensity = (getParam(ParamIDs::filterEg1Int, 64.0f) - 64.0f) / 64.0f;
    voiceParams_.filterKbdTrack = (getParam(ParamIDs::filterKeyTrack, 64.0f) - 64.0f) / 64.0f;

    // EG1
    voiceParams_.eg1Attack = getParam(ParamIDs::eg1Attack, 0.0f) / 127.0f;
    voiceParams_.eg1Decay = getParam(ParamIDs::eg1Decay, 40.0f) / 127.0f;
    voiceParams_.eg1Sustain = getParam(ParamIDs::eg1Sustain, 0.0f) / 127.0f;
    voiceParams_.eg1Release = getParam(ParamIDs::eg1Release, 40.0f) / 127.0f;

    // EG2
    voiceParams_.eg2Attack = getParam(ParamIDs::eg2Attack, 0.0f) / 127.0f;
    voiceParams_.eg2Decay = getParam(ParamIDs::eg2Decay, 40.0f) / 127.0f;
    voiceParams_.eg2Sustain = getParam(ParamIDs::eg2Sustain, 127.0f) / 127.0f;
    voiceParams_.eg2Release = getParam(ParamIDs::eg2Release, 40.0f) / 127.0f;

    // Amp
    voiceParams_.ampLevel = getParam(ParamIDs::ampLevel, 100.0f) / 127.0f;
    voiceParams_.panpot = (getParam(ParamIDs::ampPan, 64.0f) - 64.0f) / 64.0f;
    voiceParams_.distortionOn = (getParam(ParamIDs::ampDistortion, 0.0f) > 0.5f);
}

void SynthEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Update voice parameters from APVTS before block processing
    updateParametersFromAPVTS();

    // Clear output buffer
    buffer.clear();

    // Process incoming MIDI events
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
    }

    float* channelL = buffer.getWritePointer(0);
    float* channelR = (numChannels > 1) ? buffer.getWritePointer(1) : nullptr;

    // Render voices sample-by-sample
    for (int s = 0; s < numSamples; ++s)
    {
        float leftSample = 0.0f;
        float rightSample = 0.0f;

        voiceManager_.process(leftSample, rightSample, voiceParams_);

        const float masterGain = smoothedMasterGain_.getNextValue();
        leftSample *= masterGain;
        rightSample *= masterGain;

        channelL[s] = leftSample;
        if (channelR != nullptr)
        {
            channelR[s] = rightSample;
        }

        // Fill oscilloscope visual snapshot buffer
        currentSnapshot_.scopeBuffer[currentSnapshot_.scopeWriteIndex] = leftSample;
        currentSnapshot_.scopeWriteIndex = (currentSnapshot_.scopeWriteIndex + 1) % AudioThreadSnapshot::kScopeBufferSize;
    }

    // Update telemetry in snapshot
    currentSnapshot_.activeVoiceCount = static_cast<uint32_t>(voiceManager_.getActiveVoiceCount());
    currentSnapshot_.vuLeft = buffer.getMagnitude(0, 0, numSamples);
    currentSnapshot_.vuRight = (numChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : currentSnapshot_.vuLeft;
}

void SynthEngine::noteOn(int /*midiChannel*/, int midiNoteNumber, float velocity)
{
    voiceManager_.noteOn(midiNoteNumber, velocity);
}

void SynthEngine::noteOff(int /*midiChannel*/, int midiNoteNumber, float /*velocity*/, bool /*allowTailOff*/)
{
    voiceManager_.noteOff(midiNoteNumber);
}

void SynthEngine::allNotesOff()
{
    voiceManager_.allNotesOff();
}

} // namespace ABDMS2000
