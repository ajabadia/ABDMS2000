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
    triIntegrator_ = 0.0f;
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
    control1_ = DSPUtils::clamp(ctrl1, 0.0f, 1.0f);
}

void VAOscillator::syncPhase(float /*masterPhaseDelta*/) noexcept
{
    // Hard Sync: Reset slave oscillator phase to 0
    phase_ = 0.0f;
}

float VAOscillator::generateSaw() noexcept
{
    // Naive saw ramp from -1.0 to +1.0
    float raw = (2.0f * phase_) - 1.0f;

    // Apply PolyBLEP antialiasing correction
    raw -= PolyBLEP::getResidual(phase_, phaseIncrement_);

    // MS2000 Wave Shaping: Control 1 tilts/morphs the saw profile
    if (control1_ > 0.01f)
    {
        float shape = (control1_ - 0.5f) * 1.5f;
        raw = raw + shape * (raw * raw - 1.0f) * 0.5f;
    }

    return raw;
}

float VAOscillator::generatePulse() noexcept
{
    // Pulse width: 0.01 to 0.99
    float pw = 0.01f + (control1_ * 0.98f);

    // Naive pulse
    float raw = (phase_ < pw) ? 1.0f : -1.0f;

    // PolyBLEP at phase = 0 (rising edge)
    raw += PolyBLEP::getResidual(phase_, phaseIncrement_);

    // PolyBLEP at phase = pw (falling edge)
    float phaseShifted = phase_ - pw;
    if (phaseShifted < 0.0f) phaseShifted += 1.0f;
    raw -= PolyBLEP::getResidual(phaseShifted, phaseIncrement_);

    return raw;
}

float VAOscillator::generateTriangle() noexcept
{
    // PolyBLEP-based integrated square for smooth antialiased triangle
    float raw = 2.0f * std::abs(2.0f * phase_ - 1.0f) - 1.0f;

    // Apply BLAMP smoothing at the two corners (0 and 0.5)
    raw += PolyBLEP::getResidualIntegrated(phase_, phaseIncrement_);

    float phaseHalf = phase_ - 0.5f;
    if (phaseHalf < 0.0f) phaseHalf += 1.0f;
    raw -= PolyBLEP::getResidualIntegrated(phaseHalf, phaseIncrement_);

    // Wave shaping
    if (control1_ > 0.01f)
    {
        float shape = (control1_ - 0.5f) * 2.0f;
        raw = raw + shape * (raw * raw * raw - raw) * 0.4f;
    }

    return raw;
}

float VAOscillator::generateSine() noexcept
{
    // Sine with optional self-cross-modulation
    float modPhase = phase_;
    if (control1_ > 0.01f)
    {
        // Simple phase modulation feedback for metallic/FM tones
        float fmAmount = control1_ * 0.25f;
        modPhase += fmAmount * std::sin(DSPUtils::TWO_PI * phase_);
    }

    return std::sin(DSPUtils::TWO_PI * modPhase);
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

    // Advance phase
    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
    }

    return out;
}

} // namespace ABDMS2000
