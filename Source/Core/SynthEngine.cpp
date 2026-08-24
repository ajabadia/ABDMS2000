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
    reset();
}

void SynthEngine::reset()
{
    currentSnapshot_ = AudioThreadSnapshot{};
}

void SynthEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    if (numSamples == 0 || numChannels == 0)
        return;

    // Fetch master volume param
    if (auto* param = apvts_.getRawParameterValue(ParamIDs::masterVolume))
    {
        masterVolume_.store(param->load(std::memory_order_relaxed), std::memory_order_relaxed);
    }
    smoothedMasterGain_.setTargetValue(masterVolume_.load(std::memory_order_relaxed));

    // Clear output buffer for base phase 0
    buffer.clear();

    // Process MIDI messages (parse note ons/offs for snapshot)
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

    // Apply smoothed master volume
    for (int s = 0; s < numSamples; ++s)
    {
        const float gain = smoothedMasterGain_.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.setSample(ch, s, buffer.getSample(ch, s) * gain);
        }

        // Fill oscilloscope buffer
        if (s < static_cast<int>(AudioThreadSnapshot::kScopeBufferSize))
        {
            currentSnapshot_.scopeBuffer[currentSnapshot_.scopeWriteIndex] = buffer.getSample(0, s);
            currentSnapshot_.scopeWriteIndex = (currentSnapshot_.scopeWriteIndex + 1) % AudioThreadSnapshot::kScopeBufferSize;
        }
    }

    // Update VU levels in snapshot
    float peakL = buffer.getMagnitude(0, 0, numSamples);
    float peakR = (numChannels > 1) ? buffer.getMagnitude(1, 0, numSamples) : peakL;
    currentSnapshot_.vuLeft = peakL;
    currentSnapshot_.vuRight = peakR;
}

void SynthEngine::noteOn(int midiChannel, int midiNoteNumber, float velocity)
{
    juce::ignoreUnused(midiChannel);
    if (currentSnapshot_.activeVoiceCount < AudioThreadSnapshot::kMaxVoices)
    {
        currentSnapshot_.voiceActive[currentSnapshot_.activeVoiceCount] = true;
        currentSnapshot_.voiceNote[currentSnapshot_.activeVoiceCount] = static_cast<uint8_t>(midiNoteNumber);
        currentSnapshot_.voiceVelocity[currentSnapshot_.activeVoiceCount] = velocity;
        currentSnapshot_.activeVoiceCount++;
    }
}

void SynthEngine::noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff)
{
    juce::ignoreUnused(midiChannel, velocity, allowTailOff);
    for (size_t i = 0; i < currentSnapshot_.activeVoiceCount; ++i)
    {
        if (currentSnapshot_.voiceNote[i] == midiNoteNumber)
        {
            currentSnapshot_.voiceActive[i] = false;
        }
    }
    if (currentSnapshot_.activeVoiceCount > 0)
        currentSnapshot_.activeVoiceCount--;
}

void SynthEngine::allNotesOff()
{
    currentSnapshot_.activeVoiceCount = 0;
    currentSnapshot_.voiceActive.fill(false);
}

} // namespace ABDMS2000
