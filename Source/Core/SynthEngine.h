#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioThreadSnapshot.h"
#include "HardwareConstants.h"
#include "../State/ParameterRegistry.gen.h"
#include <memory>
#include <atomic>

namespace ABDMS2000 {

class SynthEngine {
public:
    SynthEngine(juce::AudioProcessorValueTreeState& apvts);
    ~SynthEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);

    const AudioThreadSnapshot& getSnapshot() const noexcept { return currentSnapshot_; }

    void noteOn(int midiChannel, int midiNoteNumber, float velocity);
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff = true);
    void allNotesOff();

private:
    juce::AudioProcessorValueTreeState& apvts_;
    double sampleRate_{44100.0};
    int samplesPerBlock_{512};

    AudioThreadSnapshot currentSnapshot_{};
    std::atomic<float> masterVolume_{0.8f};

    // Smooth gain
    juce::LinearSmoothedValue<float> smoothedMasterGain_{0.8f};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEngine)
};

} // namespace ABDMS2000
