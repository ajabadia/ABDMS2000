#include "VAOscillator.h"
#include <cmath>

namespace ABDMS2000 {

void VAOscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    setFrequency(frequency_);
}

void VAOscillator::reset(float initialPhase) noexcept
{
    phase_ = initialPhase;
    while (phase_ >= 1.0f) phase_ -= 1.0f;
    while (phase_ < 0.0f)  phase_ += 1.0f;
}

void VAOscillator::setFrequency(float frequencyHz) noexcept
{
    frequency_ = DSPUtils::clamp(frequencyHz, 0.1f, static_cast<float>(sampleRate_ * 0.499));
    phaseIncrement_ = static_cast<float>(frequency_ / sampleRate_);
}

void VAOscillator::setWaveform(VAWaveform wave) noexcept
{
    waveform_ = wave;
}

void VAOscillator::setControl1(float ctrl1) noexcept
{
    // In the real MS2000, Control 1 is only used for:
    // - PWM on Pulse wave
    // - DWGS index on DWGS
    // Other waveforms ignore this value
    control1_ = DSPUtils::clamp(ctrl1, 0.0f, 1.0f);
}

void VAOscillator::syncPhase(float /*masterPhaseDelta*/) noexcept
{
    phase_ = 0.0f;
}

float VAOscillator::generateSaw() noexcept
{
    // Band-limited sawtooth via PolyBLEP
    float raw = (2.0f * phase_) - 1.0f;
    raw -= PolyBLEP::getResidual(phase_, phaseIncrement_);
    return raw;
}

float VAOscillator::generatePulse() noexcept
{
    // PWM: pulse width from 0.01 to 0.99 controlled by control1_
    float pw = 0.01f + (control1_ * 0.98f);

    float raw = (phase_ < pw) ? 1.0f : -1.0f;

    // PolyBLEP at both edges
    raw += PolyBLEP::getResidual(phase_, phaseIncrement_);

    float phaseShifted = phase_ - pw;
    if (phaseShifted < 0.0f) phaseShifted += 1.0f;
    raw -= PolyBLEP::getResidual(phaseShifted, phaseIncrement_);

    return raw;
}

float VAOscillator::generateTriangle() noexcept
{
    // PolyBLEP-based band-limited triangle
    float raw = 2.0f * std::abs(2.0f * phase_ - 1.0f) - 1.0f;

    raw += PolyBLEP::getResidualIntegrated(phase_, phaseIncrement_);

    float phaseHalf = phase_ - 0.5f;
    if (phaseHalf < 0.0f) phaseHalf += 1.0f;
    raw -= PolyBLEP::getResidualIntegrated(phaseHalf, phaseIncrement_);

    return raw;
}

float VAOscillator::generateSine() noexcept
{
    // Pure sine — no FM, no wave shaping (MS2000 HW behavior)
    return std::sin(DSPUtils::TWO_PI * phase_);
}

float VAOscillator::getNextSample() noexcept
{
    float out = 0.0f;

    switch (waveform_)
    {
        case VAWaveform::Sawtooth: out = generateSaw(); break;
        case VAWaveform::Pulse:    out = generatePulse(); break;
        case VAWaveform::Triangle: out = generateTriangle(); break;
        case VAWaveform::Sine:     out = generateSine(); break;
        default:                   out = generateSaw(); break;
    }

    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
    }

    return out;
}

} // namespace ABDMS2000