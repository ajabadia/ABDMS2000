#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Core/SynthEngine.h"
#include "../State/ParameterRegistry.gen.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "BridgeHost.h"

namespace ABDMS2000 {


class ABDMS2000AudioProcessor : public juce::AudioProcessor,
                                public BridgeHost {
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

    // --- BridgeHost (puerto que consume BridgeActions) ---
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept override { return apvts_; }
    SynthEngine& getEngine() noexcept override { return engine_; }
    class MIDITelemetryManager& getMIDITelemetry() noexcept override;
    class SysExManager& getSysExManager() noexcept override { return *sysexManager_; }
    HardwareMidiTransport& getHardwareMidiTransport() noexcept override { return hardwareMidiTransport_; }
    // getCurrentProgram/setCurrentProgram/changeProgramName ya se declaran más
    // abajo como overrides de juce::AudioProcessor: cumplen también BridgeHost.

private:
    juce::AudioProcessorValueTreeState apvts_;
    SynthEngine engine_;
    std::unique_ptr<class MIDITelemetryManager> midiTelemetry_;
    std::unique_ptr<class SysExManager> sysexManager_;

    // Hardware MIDI para el puente del Bank Manager embebido (los dispositivos
    // reales los enlaza el Editor; ver Source/Plugin/HardwareMidiTransport.h).
    HardwareMidiTransport hardwareMidiTransport_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ABDMS2000AudioProcessor)
};


} // namespace ABDMS2000

