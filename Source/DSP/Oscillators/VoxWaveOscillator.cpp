#include "VoxWaveOscillator.h"
#include <cmath>

namespace ABDMS2000
{

void VoxWaveOscillator::FormantBPF::setFreq(float freqHz, float q, double sr) noexcept
{
    float w0 = freqHz * DSPUtils::TWO_PI / static_cast<float>(sr);
    float sinW0 = std::sin(w0);
    float cosW0 = std::cos(w0);
    float alpha = sinW0 / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = (sinW0 * 0.5f) / a0;
    b1 = 0.0f;
    b2 = (-sinW0 * 0.5f) / a0;
    a1 = (-2.0f * cosW0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float VoxWaveOscillator::FormantBPF::process(float in) noexcept
{
    float out = b0 * in + s1;
    s1 = b1 * in - a1 * out + s2;
    s2 = b2 * in - a2 * out;
    return out;
}

void VoxWaveOscillator::prepare(double sampleRate) noexcept
{
    sampleRate_ = sampleRate;
    setFrequency(frequency_);
    setVowel(vowel_);
    reset();
}

void VoxWaveOscillator::reset() noexcept
{
    phase_ = 0.0f;
    s1_ = 0.0f;
    s2_ = 0.0f;
    for (auto &f : formants_)
        f.reset();
}

void VoxWaveOscillator::setFrequency(float freqHz) noexcept
{
    frequency_ = DSPUtils::clamp(freqHz, 10.0f, 8000.0f);
    phaseIncrement_ = frequency_ / static_cast<float>(sampleRate_);
}

void VoxWaveOscillator::setVowel(float vowel0to1) noexcept
{
    vowel_ = DSPUtils::clamp(vowel0to1, 0.0f, 1.0f);
}

void VoxWaveOscillator::setFormantShift(float shift0to1) noexcept
{
    formantShift_ = DSPUtils::clamp(shift0to1, 0.0f, 1.0f);
}

void VoxWaveOscillator::updateVowelCoefficients() noexcept
{
    // Morph between adjacent vowel pairs using linear interpolation of formant frequencies
    float morphed[5];

    const float *const vowels[5] = {
        VowelFormants::A, VowelFormants::E, VowelFormants::I, VowelFormants::O, VowelFormants::U};
    float scaled = vowel_ * 4.0f;
    int idx0 = static_cast<int>(scaled);
    if (idx0 < 0)
        idx0 = 0;
    if (idx0 > 3)
        idx0 = 3;

    int idx1 = idx0 + 1;
    float t = scaled - static_cast<float>(idx0);
    if (t < 0.0f)
        t = 0.0f;
    if (t > 1.0f)
        t = 1.0f;

    const float *f0 = vowels[idx0];
    const float *f1 = vowels[idx1];

    for (int i = 0; i < 5; ++i)
    {
        morphed[i] = f0[i] + t * (f1[i] - f0[i]);
    }

    for (int i = 0; i < 5; ++i)
    {
        formants_[i].setFreq(morphed[i], 6.0f, sampleRate_);
    }
}

float VoxWaveOscillator::getNextSample() noexcept
{
    // DSP56362 P:$0A32 - $0A5D: Authentic MS2000 VoxWave Oscillator
    // 1. Bipolar oscillator phase phi in [-1.0, +1.0)
    float phi = (2.0f * phase_) - 1.0f;
    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f)
        phase_ -= 1.0f;

    // 2. Quadratic formant/window shaping from Control 1 + Control 2 offset (Y:$0450 constants: $3DDF2D = 0.48337, $312BE1 = 0.38415)
    constexpr float kC2 = 0.483373f; // $3DDF2D / $800000
    constexpr float kC1 = 0.384152f; // $312BE1 / $800000
    float effCtrl = DSPUtils::clamp(vowel_ + (formantShift_ * 0.35f), 0.0f, 1.0f);
    float ctrlShaped = 2.0f * (effCtrl * (kC1 + effCtrl * kC2)); // P:$0A41..$0A46

    // 3. Parabolic edge-rounded sawtooth window (P:$0A37..$0A4E):
    //    u = clamp((1 - |phi|) * slope, 0, 1), w = 1 - (1 - u)^2, exc = phi * w
    float distToEdge = 1.0f - std::abs(phi);
    float edgeSlope = DSPUtils::clamp(0.25f / std::max(phaseIncrement_, 0.0005f), 2.0f, 64.0f);
    float u = DSPUtils::clamp(distToEdge * edgeSlope, 0.0f, 1.0f);
    float oneMinusU = 1.0f - u;
    float window = 1.0f - (oneMinusU * oneMinusU);
    float excitation = phi * window;

    // 4. Pitch-tracked 2-pole resonant formant chamber (P:$0A50..$0A5D):
    //    Resonant frequency tracks oscillator pitch + Control 1 formant offset (1.5x to 6.5x f0).
    float formantRatio = 1.5f + ctrlShaped * 3.0f;
    float formantHz = DSPUtils::clamp(frequency_ * formantRatio, 150.0f, static_cast<float>(sampleRate_ * 0.42));
    float f = DSPUtils::clamp(2.0f * std::sin(DSPUtils::PI * formantHz / static_cast<float>(sampleRate_)), 0.01f, 0.92f);
    constexpr float qDamp = 0.22f; // High vocal resonance Q (~4.5)

    // 2-pole chamber state update with internal DSP56362 24-bit saturation [-1, +1]
    float hp = excitation * 0.35f - s2_ - qDamp * s1_;
    s1_ = DSPUtils::clamp(s1_ + f * hp, -1.0f, 1.0f);
    s2_ = DSPUtils::clamp(s2_ + f * s1_, -1.0f, 1.0f);

    // P:$0A5A..$0A5D: 4 * bp + lp output mix
    float out = DSPUtils::clamp((s1_ * 1.4f) + (s2_ * 0.6f), -1.0f, 1.0f);
    return out;
}

}  // namespace ABDMS2000