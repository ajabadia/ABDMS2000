#include "MultiModeFilter.h"
#include <cmath>

namespace ABDMS2000
{

void MultiModeFilter::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    reset();
    updateCoefficients();
}

void MultiModeFilter::reset() noexcept
{
    s1_1_ = s1_2_ = 0.0f;
    s2_1_ = s2_2_ = 0.0f;
}

void MultiModeFilter::setType(FilterType type) noexcept
{
    type_ = type;
}

void MultiModeFilter::setCutoff(float cutoffHz) noexcept
{
    const float clamped = DSPUtils::clamp(cutoffHz, 5.0f, static_cast<float>(sampleRate_ * 0.49));
    if (std::abs(clamped - cutoffHz_) <= 1e-3f)
        return;
    cutoffHz_ = clamped;
    updateCoefficients();
}

void MultiModeFilter::setResonance(float resonance0to1) noexcept
{
    const float clamped = DSPUtils::clamp(resonance0to1, 0.0f, 1.0f);
    if (std::abs(clamped - resonance_) <= 1e-5f)
        return;
    resonance_ = clamped;
    updateCoefficients();
}

void MultiModeFilter::updateCoefficients() noexcept
{
    // Pre-warped analog frequency mapping (Bilinear transform)
    float wd = cutoffHz_ * DSPUtils::TWO_PI;
    float T = 1.0f / static_cast<float>(sampleRate_);
    float wa = (2.0f / T) * std::tan(wd * T * 0.5f);
    g_ = wa * T * 0.5f;

    // Damping / Feedback (Zavalishin TPT SVF: denominator is 1 + 2*R*g + g^2, so R = zeta = 1/(2Q))
    float feedback = FilterResonanceComp::computeEffectiveFeedback(resonance_);
    R_ = 1.0f - (feedback * DSPUtils::kResonanceFeedbackScale);
    if (R_ < DSPUtils::kResonanceMinFeedback)
        R_ = DSPUtils::kResonanceMinFeedback;

    h_ = 1.0f / (1.0f + 2.0f * R_ * g_ + g_ * g_);
}

float MultiModeFilter::process(float inputSample) noexcept
{
    // Apply subtle resonance headroom compensation
    float gainComp = FilterResonanceComp::computeGainCompensation(resonance_);
    float in = inputSample * gainComp;

    // Headroom-preserving input soft-clip (keeps +/-1.0 linear while bounding extreme mixer sums)
    in = DSPUtils::softClip(in * 0.25f) * 4.0f;

    // --- First 2-pole SVF stage (with DSP56362 internal resonator state saturation) ---
    float hp1 = (in - (2.0f * R_ + g_) * s1_1_ - s1_2_) * h_;
    float bp1 = g_ * hp1 + s1_1_;
    const float nextS1_1 = g_ * hp1 + bp1;
    // Preserve exact linear trapezoidal integration for HPF12/BPF12 so low-frequency zeros do not leak
    s1_1_ = (type_ == FilterType::HPF12 || type_ == FilterType::BPF12) ? nextS1_1
                                                                       : (DSPUtils::softClip(nextS1_1 * 0.25f) * 4.0f);
    float lp1 = g_ * bp1 + s1_2_;
    s1_2_ = g_ * bp1 + lp1;  // Integrator 2 state update

    switch (type_)
    {
    case FilterType::LPF12:
        return lp1;

    case FilterType::BPF12:
        return DSPUtils::softClip(bp1 * 0.25f) * 4.0f;

    case FilterType::HPF12:
        return DSPUtils::softClip(hp1 * 0.25f) * 4.0f;

    case FilterType::LPF24:
    default:
    {
        // Cascade into Second 2-pole SVF stage for -24dB/oct LPF
        float in2 = lp1;
        float hp2 = (in2 - (2.0f * R_ + g_) * s2_1_ - s2_2_) * h_;
        float bp2 = g_ * hp2 + s2_1_;
        s2_1_ = DSPUtils::softClip((g_ * hp2 + bp2) * 0.5f) * 2.0f;
        float lp2 = g_ * bp2 + s2_2_;
        s2_2_ = g_ * bp2 + lp2;

        return lp2;
    }
    }
}

}  // namespace ABDMS2000
