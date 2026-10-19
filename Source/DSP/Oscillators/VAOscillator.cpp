#include "VAOscillator.h"
#include <cmath>

namespace ABDMS2000
{

void VAOscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    setFrequency(frequency_);
}

void VAOscillator::reset(float initialPhase) noexcept
{
    phase_ = initialPhase;
    while (phase_ >= 1.0f)
        phase_ -= 1.0f;
    while (phase_ < 0.0f)
        phase_ += 1.0f;
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
    // On the Korg MS2000 hardware, OSC1 Control 1 (and Control 2 LFO1 mod) controls:
    // - Saw: Dual-slope / phase-distorted waveform modulation (0 = pure saw, 1 = double-saw/masked)
    // - Pulse: Duty cycle / Pulse Width from 50% (Square at 0.0) to 99% (narrow pulse at 1.0)
    // - Triangle: Wave-folding / harmonic shaping (0 = pure triangle, 1 = folded metallic reed)
    // - Sine (CROSS): Cross-modulation / FM depth from OSC2 (when OSC1 Wave = CROSS)
    control1_ = DSPUtils::clamp(ctrl1, 0.0f, 1.0f);
}

void VAOscillator::syncPhase(float masterPhaseDelta) noexcept
{
    // Hard sync: reset phase based on master oscillator's phase increment
    // masterPhaseDelta = phaseIncrement_ of master at sync moment
    // For saw/pulse: half-period sync (reset to ~0.5 * master phase increment)
    phase_ = masterPhaseDelta * 0.5f;
}

float VAOscillator::generateSaw() noexcept
{
    // Band-limited sawtooth via PolyBLEP
    float saw1 = (2.0f * phase_) - 1.0f;
    saw1 -= PolyBLEP::getResidual(phase_, phaseIncrement_);

    if (control1_ <= 0.0001f)
        return saw1;

    // DSP56362 P:$0A93-$0AC6: MS2000 Saw Waveform Modulation sums two phase-shifted
    // sawtooth ramps (0.5 * (saw1 + saw2)) with DC-offset compensation, morphing
    // from fundamental sawtooth (ctrl1 = 0) to octave-up double-saw (ctrl1 = 1).
    float shift = 0.5f * control1_;
    float phase2 = phase_ + shift;
    if (phase2 >= 1.0f)
        phase2 -= 1.0f;
    float saw2 = (2.0f * phase2) - 1.0f;
    saw2 -= PolyBLEP::getResidual(phase2, phaseIncrement_);

    float dualSaw = 0.5f * (saw1 + (control1_ * saw2 + (1.0f - control1_) * saw1));
    return DSPUtils::clamp(dualSaw, -1.0f, 1.0f);
}

float VAOscillator::generatePulse() noexcept
{
    // On the MS2000 (DSP56362 P:$0AC7-$0AF6), Control 1 = 0 produces a 50% square wave
    // (INIT Program default), and increasing Control 1 towards 127 narrows the pulse width.
    float pw = 0.50f + (control1_ * 0.49f);

    float raw = (phase_ < pw) ? 1.0f : -1.0f;

    // PolyBLEP at both edges
    raw += PolyBLEP::getResidual(phase_, phaseIncrement_);

    float phaseShifted = phase_ - pw;
    if (phaseShifted < 0.0f)
        phaseShifted += 1.0f;
    raw -= PolyBLEP::getResidual(phaseShifted, phaseIncrement_);

    return raw;
}

float VAOscillator::generateTriangle() noexcept
{
    // PolyBLEP-based band-limited triangle
    float raw = 2.0f * std::abs(2.0f * phase_ - 1.0f) - 1.0f;

    raw += PolyBLEP::getResidualIntegrated(phase_, phaseIncrement_);

    float phaseHalf = phase_ - 0.5f;
    if (phaseHalf < 0.0f)
        phaseHalf += 1.0f;
    raw -= PolyBLEP::getResidualIntegrated(phaseHalf, phaseIncrement_);

    if (control1_ <= 0.0001f)
        return raw;

    // DSP56362 P:$0AF7-$0B1D: MS2000 Triangle Wavefolder scales triangle by
    // (1/3 + 2 * (2/3) * ctrl1) / (1/3) = 1.0 + 4.0 * ctrl1, folding top peak
    // at +1.0 (neg ifpl) and bottom peak at -1.0 (neg ifmi).
    float drive = 1.0f + (control1_ * 4.0f);
    float folded = raw * drive;
    if (folded > 1.0f)
        folded = 2.0f - folded;
    if (folded < -1.0f)
        folded = -2.0f - folded;
    return DSPUtils::clamp(folded, -1.0f, 1.0f);
}

float VAOscillator::generateSine(float phaseModulation) noexcept
{
    // Sine / CROSS wave: pure sine when unmodulated; supports FM / Cross-Modulation
    // via Control 1 + Control 2 (and OSC2 Mod = CrossMod).
    float modPhase = phase_ + phaseModulation;
    return std::sin(DSPUtils::TWO_PI * modPhase);
}

float VAOscillator::getNextSample(float phaseModulation) noexcept
{
    float out = 0.0f;

    switch (waveform_)
    {
    case VAWaveform::Sawtooth:
        out = generateSaw();
        break;
    case VAWaveform::Pulse:
        out = generatePulse();
        break;
    case VAWaveform::Triangle:
        out = generateTriangle();
        break;
    case VAWaveform::Sine:
        out = generateSine(phaseModulation);
        break;
    default:
        out = generateSaw();
        break;
    }

    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f)
    {
        phase_ -= 1.0f;
    }

    return out;
}

}  // namespace ABDMS2000