#pragma once
#include "../Common/PolyBLEP.h"
#include "../Common/DSPUtils.h"

namespace ABDMS2000 {

enum class VAWaveform {
    Sawtooth = 0,
    Pulse,
    Triangle,
    Sine
};

/**
 * @brief PolyBLEP Band-Limited Virtual Analog Oscillator.
 * Emulates the Korg MS2000 / microKORG VA algorithms with wave shaping.
 */
class VAOscillator {
public:
    VAOscillator() = default;

    void prepare(double sampleRate) noexcept;
    void reset(float initialPhase = 0.0f) noexcept;

    void setFrequency(float frequencyHz) noexcept;
    void setWaveform(VAWaveform wave) noexcept;
    void setControl1(float ctrl1) noexcept; // 0.0 to 1.0 (PWM, Wave Shaping, FM)

    // Hard Sync trigger from master oscillator
    void syncPhase(float masterPhaseDelta) noexcept;

    // Direct phase access for ring mod and sync
    float getPhase() const noexcept { return phase_; }
    void setPhase(float newPhase) noexcept { phase_ = newPhase; }

    float getNextSample() noexcept;

private:
    double sampleRate_{ 44100.0 };
    float frequency_{ 440.0f };
    float phase_{ 0.0f };
    float phaseIncrement_{ 0.01f };
    VAWaveform waveform_{ VAWaveform::Sawtooth };
    float control1_{ 0.5f }; // Default 50% PWM or neutral shaping

    // Integrator state for triangle
    float triIntegrator_{ 0.0f };

    float generateSaw() noexcept;
    float generatePulse() noexcept;
    float generateTriangle() noexcept;
    float generateSine() noexcept;
};

} // namespace ABDMS2000
