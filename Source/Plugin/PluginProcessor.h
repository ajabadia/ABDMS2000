#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/SynthEngine.h"
#include "../State/ParameterRegistry.gen.h"

namespace ABDMS2000 {

class ABDMS2000AudioProcessor : public juce::AudioProcessor {
public:
    ABDMS2000AudioProcessor();
    ~ABDMS2000AudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts_; }
    SynthEngine& getEngine() noexcept { return engine_; }

private:
    juce::AudioProcessorValueTreeState apvts_;
    SynthEngine engine_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ABDMS2000AudioProcessor)
};

} // namespace ABDMS2000
