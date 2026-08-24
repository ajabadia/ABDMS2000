#pragma once
#include "../DSP/Oscillators/VAOscillator.h"
#include "../DSP/Oscillators/NoiseGenerator.h"
#include "../DSP/Oscillators/OSC2Modulator.h"
#include "../DSP/Filters/MultiModeFilter.h"
#include "../DSP/Envelopes/ADSREnvelope.h"
#include "../DSP/Modulation/PortamentoGlide.h"

namespace ABDMS2000 {

struct VoiceParameters {
    // OSC 1
    VAWaveform osc1Wave{ VAWaveform::Sawtooth };
    float osc1Ctrl1{ 0.0f };
    float osc1Level{ 1.0f };

    // OSC 2
    VAWaveform osc2Wave{ VAWaveform::Sawtooth };
    float osc2Semitone{ 0.0f };
    float osc2Tune{ 0.0f };
    OSC2ModulationMode osc2ModMode{ OSC2ModulationMode::Off };
    float osc2Level{ 0.0f };

    // Noise
    float noiseLevel{ 0.0f };

    // Filter
    FilterType filterType{ FilterType::LPF24 };
    float filterCutoffNorm{ 1.0f }; // 0..1
    float filterResonance{ 0.0f };   // 0..1
    float eg1FilterIntensity{ 0.0f };// -1.0 to +1.0
    float filterKbdTrack{ 0.0f };    // -1.0 to +1.0

    // Envelopes
    float eg1Attack{ 0.01f };
    float eg1Decay{ 0.3f };
    float eg1Sustain{ 0.0f };
    float eg1Release{ 0.3f };

    float eg2Attack{ 0.01f };
    float eg2Decay{ 0.3f };
    float eg2Sustain{ 0.8f };
    float eg2Release{ 0.3f };

    // VCA / Amp
    float ampLevel{ 0.8f };
    float panpot{ 0.0f }; // -1.0 (L) to +1.0 (R)
    bool distortionOn{ false };
    float portamentoTime{ 0.0f };
};

/**
 * @brief Represents a single synth voice in the 4-voice architecture.
 */
class Voice {
public:
    Voice() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn(int midiNote, float velocity, bool glideEnabled) noexcept;
    void noteOff() noexcept;
    void stopImmediately() noexcept;

    bool isActive() const noexcept;
    int getCurrentNote() const noexcept { return currentMidiNote_; }
    uint32_t getNoteOnAge() const noexcept { return noteAge_; }
    float getCurrentAmpLevel() const noexcept;

    // Process a single stereo sample
    void renderNextSample(float& leftOut, float& rightOut, const VoiceParameters& params) noexcept;

private:
    double sampleRate_{ 44100.0 };
    int currentMidiNote_{ 60 };
    float velocity_{ 0.8f };
    uint32_t noteAge_{ 0 };
    float prevMasterPhase_{ 0.0f };

    VAOscillator osc1_;
    VAOscillator osc2_;
    NoiseGenerator noiseGen_;
    MultiModeFilter filter_;
    ADSREnvelope eg1_;
    ADSREnvelope eg2_;
    PortamentoGlide glide_;
};

} // namespace ABDMS2000
