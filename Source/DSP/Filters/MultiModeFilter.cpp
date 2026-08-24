#include "MultiModeFilter.h"
#include <cmath>

namespace ABDMS2000 {

void MultiModeFilter::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
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
    cutoffHz_ = DSPUtils::clamp(cutoffHz, 20.0f, static_cast<float>(sampleRate_ * 0.49));
    updateCoefficients();
}

void MultiModeFilter::setResonance(float resonance0to1) noexcept
{
    resonance_ = DSPUtils::clamp(resonance0to1, 0.0f, 1.0f);
    updateCoefficients();
}

void MultiModeFilter::updateCoefficients() noexcept
{
    // Pre-warped analog frequency mapping (Bilinear transform)
    float wd = cutoffHz_ * DSPUtils::TWO_PI;
    float T = 1.0f / static_cast<float>(sampleRate_);
    float wa = (2.0f / T) * std::tan(wd * T * 0.5f);
    g_ = wa * T * 0.5f;

    // Damping / Feedback
    float feedback = FilterResonanceComp::computeEffectiveFeedback(resonance_);
    // R = 2 - 2 * resonance damping
    R_ = 2.0f * (1.0f - (feedback * 0.245f));
    if (R_ < 0.01f) R_ = 0.01f;

    h_ = 1.0f / (1.0f + 2.0f * R_ * g_ + g_ * g_);
}

float MultiModeFilter::process(float inputSample) noexcept
{
    // Apply bass compensation
    float gainComp = FilterResonanceComp::computeGainCompensation(resonance_);
    float in = inputSample * gainComp;

    // Anti-denormal / soft-clip protection in feedback loop
    in = DSPUtils::softClip(in);

    // --- First 2-pole SVF stage ---
    float hp1 = (in - (2.0f * R_ + g_) * s1_1_ - s1_2_) * h_;
    float bp1 = g_ * hp1 + s1_1_;
    s1_1_ = g_ * hp1 + bp1; // Integrator 1 state update
    float lp1 = g_ * bp1 + s1_2_;
    s1_2_ = g_ * bp1 + lp1; // Integrator 2 state update

    switch (type_)
    {
        case FilterType::LPF12:
            return lp1;

        case FilterType::BPF12:
            return bp1;

        case FilterType::HPF12:
            return hp1;

        case FilterType::LPF24:
        default:
        {
            // Cascade into Second 2-pole SVF stage for -24dB/oct LPF
            float in2 = lp1;
            float hp2 = (in2 - (2.0f * R_ + g_) * s2_1_ - s2_2_) * h_;
            float bp2 = g_ * hp2 + s2_1_;
            s2_1_ = g_ * hp2 + bp2;
            float lp2 = g_ * bp2 + s2_2_;
            s2_2_ = g_ * bp2 + lp2;

            return lp2;
        }
    }
}

} // namespace ABDMS2000
