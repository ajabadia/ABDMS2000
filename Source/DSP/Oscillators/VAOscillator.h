#pragma once
#include "../Common/DSPUtils.h"
#include "../Common/PolyBLEP.h"

namespace ABDMS2000
{

enum class VAWaveform
{
    Sawtooth = 0,
    Pulse,
    Triangle,
    Sine
};

/**
 * @brief PolyBLEP Band-Limited Virtual Analog Oscillator.
 * Emulates the Korg MS2000 / microKORG VA algorithms with wave shaping.
 */
class VAOscillator
{
  public:
    VAOscillator() = default;

    void prepare(double sampleRate) noexcept;
    void reset(float initialPhase = 0.0f) noexcept;

    void setFrequency(float frequencyHz) noexcept;
    void setWaveform(VAWaveform wave) noexcept;
    void setControl1(float ctrl1) noexcept;  // 0.0 to 1.0 (PWM, Wave Shaping, FM)
    void setIsOsc2(bool isOsc2) noexcept
    {
        isOsc2_ = isOsc2;
    }
    void setHardwareMode(bool enabled) noexcept
    {
        hardwareMode_ = enabled;
    }
    bool isHardwareMode() const noexcept
    {
        return hardwareMode_;
    }

    // Evalúa la ganancia de borde parabólico del DSP56362 (P:$0AA4..$0ABA / P:$0ADC..$0AEF)
    // a partir de la distancia a la discontinuidad (1 - |phi|) y del factor de pendiente Y:$0800..$08FF.
    static float computeHardwareParabolicEdgeGain(float distToEdge, float slopeFactor) noexcept;
    float getHardwareSlopeFactor() const noexcept
    {
        return slopeFactor_;
    }

    // Hard Sync trigger from master oscillator
    void syncPhase(float masterPhaseDelta) noexcept;

    // Direct phase access for ring mod and sync
    float getPhase() const noexcept
    {
        return phase_;
    }
    void setPhase(float newPhase) noexcept
    {
        phase_ = newPhase;
    }

    float getNextSample(float phaseModulation = 0.0f) noexcept;

  private:
    double sampleRate_ {44100.0};
    float frequency_ {440.0f};
    float phase_ {0.0f};
    float phaseIncrement_ {0.01f};
    float slopeFactor_ {1.0f};
    float highFreqRolloff_ {1.0f};
    bool isOsc2_ {false};
    bool hardwareMode_ {true};
    VAWaveform waveform_ {VAWaveform::Sawtooth};
    float control1_ {0.0f};  // Default 0.0 (50% square PW / unshaped saw & triangle)

    // Integrator state for triangle
    float triIntegrator_ {0.0f};

    float generateSaw() noexcept;
    float generatePulse() noexcept;
    float generateTriangle() noexcept;
    float generateSine(float phaseModulation = 0.0f) noexcept;
};

}  // namespace ABDMS2000
