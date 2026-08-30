#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "AudioThreadSnapshot.h"
#include "HardwareConstants.h"
#include "VoiceManager.h"
#include "../DSP/Sequencer/ModSequencer.h"
#include "../DSP/Sequencer/Arpeggiator.h"
#include "../DSP/Effects/ModFX.h"
#include "../DSP/Effects/DelayFX.h"
#include "../DSP/Effects/Equalizer.h"
#include "../DSP/Vocoder/Vocoder16Band.h"
#include "../State/ParameterRegistry.gen.h"
#include <memory>
#include <atomic>
#include <vector>

namespace ABDMS2000 {

enum class DiagnosticTonePoint {
    Disabled = 0,
    PostMasterVolume = 1, // At the very end of processBlock (after master volume)
    PreMasterVolume  = 2, // Before master volume attenuation
    PreEffects       = 3, // After Voices/Vocoder, before ModFX / DelayFX / MasterEQ
    PreFilter        = 4, // Inside Voice: replaces mixedAudio before filter_
    OscillatorDirect = 5  // Inside Voice: replaces OSC1 output before mixer & VCF
};

class SynthEngine {
public:
    explicit SynthEngine(juce::AudioProcessorValueTreeState& apvts);
    ~SynthEngine() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void reset();
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages,
                      juce::AudioPlayHead* playHead = nullptr);

    const AudioThreadSnapshot& getSnapshot() const noexcept { return currentSnapshot_; }

    void noteOn(int midiChannel, int midiNoteNumber, float velocity);
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff = true);
    void allNotesOff();
    void setPitchBend(float bendMinus1to1) noexcept { pitchBendValue_.store(bendMinus1to1, std::memory_order_relaxed); }
    void setModWheel(float mod0to1) noexcept { modWheelValue_.store(mod0to1, std::memory_order_relaxed); }
    void setTestToneEnabled(bool enabled) noexcept { 
        diagTonePoint_.store(enabled ? DiagnosticTonePoint::PreMasterVolume : DiagnosticTonePoint::Disabled, std::memory_order_relaxed); 
    }
    void setDiagnosticTone(int point, float freqHz = 440.0f, float level = 0.25f) noexcept {
        diagTonePoint_.store(static_cast<DiagnosticTonePoint>(std::max(0, std::min(5, point))), std::memory_order_relaxed);
        testToneFrequency_.store(std::max(20.0f, std::min(10000.0f, freqHz)), std::memory_order_relaxed);
        testToneLevel_.store(std::max(0.0f, std::min(1.0f, level)), std::memory_order_relaxed);
    }
    int getDiagnosticTonePoint() const noexcept { return static_cast<int>(diagTonePoint_.load(std::memory_order_relaxed)); }

    void setDiagnosticBypass(const juce::String& stage, bool enabled) noexcept {
        if (stage == "filter") diagBypassFilter_.store(enabled, std::memory_order_relaxed);
        else if (stage == "vca") diagBypassVCA_.store(enabled, std::memory_order_relaxed);
        else if (stage == "mixer" || stage == "osc") diagBypassOscMixer_.store(enabled, std::memory_order_relaxed);
        else if (stage == "distortion") diagBypassDistortion_.store(enabled, std::memory_order_relaxed);
        else if (stage == "modfx") diagBypassModFX_.store(enabled, std::memory_order_relaxed);
        else if (stage == "delayfx") diagBypassDelayFX_.store(enabled, std::memory_order_relaxed);
        else if (stage == "eq") diagBypassMasterEQ_.store(enabled, std::memory_order_relaxed);
    }

    void resetAllDiagnosticBypasses() noexcept {
        diagBypassFilter_.store(false, std::memory_order_relaxed);
        diagBypassVCA_.store(false, std::memory_order_relaxed);
        diagBypassOscMixer_.store(false, std::memory_order_relaxed);
        diagBypassDistortion_.store(false, std::memory_order_relaxed);
        diagBypassModFX_.store(false, std::memory_order_relaxed);
        diagBypassDelayFX_.store(false, std::memory_order_relaxed);
        diagBypassMasterEQ_.store(false, std::memory_order_relaxed);
    }

    // Accessors for DSP modules
    ModSequencer& getModSequencer() noexcept { return modSeq_; }
    Arpeggiator& getArpeggiator() noexcept { return arpeggiator_; }
    ModFX& getModFX() noexcept { return modFX_; }
    DelayFX& getDelayFX() noexcept { return delayFX_; }
    Equalizer& getMasterEQ() noexcept { return masterEQ_; }
    Vocoder16Band& getVocoder() noexcept { return vocoder_; }

    void updateParametersFromAPVTS() noexcept;
    void triggerArpNote(int note, float velocity, bool isNoteOn) noexcept;

    juce::AudioProcessorValueTreeState& apvts_;
    double sampleRate_{ 44100.0 };
    int samplesPerBlock_{ 512 };

    // Dual-Timbre Voice Architecture (Single / Split / Dual Layer / Vocoder)
    VoiceParameters voiceParamsA_{};
    VoiceParameters voiceParamsB_{};
    VoiceManager voiceManagerA_;
    VoiceManager voiceManagerB_;
    int timbreMode_{ 0 };      // 0: Single, 1: Split, 2: Dual
    int voiceAssignMode_{ 1 }; // 0: Mono, 1: Poly, 2: Unison
    int splitKey_{ 60 };       // C4 Split Point
    float timbreBalance_{ 0.5f };

    ModSequencer modSeq_;
    Arpeggiator arpeggiator_;
    ModFX modFX_;
    DelayFX delayFX_;
    Equalizer masterEQ_;
    Vocoder16Band vocoder_;
    MultiModeFilter debugFilter_;

    std::vector<ArpNoteEvent> arpEventsBuffer_;

    AudioThreadSnapshot currentSnapshot_{};
    std::atomic<float> masterVolume_{ 0.8f };
    std::atomic<float> pitchBendValue_{ 0.0f };
    std::atomic<float> modWheelValue_{ 0.0f };
    std::atomic<DiagnosticTonePoint> diagTonePoint_{ DiagnosticTonePoint::Disabled };
    std::atomic<float> testToneFrequency_{ 440.0f };
    std::atomic<float> testToneLevel_{ 0.25f };
    std::atomic<bool> diagBypassFilter_{ false };
    std::atomic<bool> diagBypassVCA_{ false };
    std::atomic<bool> diagBypassOscMixer_{ false };
    std::atomic<bool> diagBypassDistortion_{ false };
    std::atomic<bool> diagBypassModFX_{ false };
    std::atomic<bool> diagBypassDelayFX_{ false };
    std::atomic<bool> diagBypassMasterEQ_{ false };
    double testTonePhase_{ 0.0 };
    juce::LinearSmoothedValue<float> smoothedMasterGain_{ 0.8f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEngine)
};


} // namespace ABDMS2000
