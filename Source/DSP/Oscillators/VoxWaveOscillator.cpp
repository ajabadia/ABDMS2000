#include "VoxWaveOscillator.h"
#include <cmath>

namespace ABDMS2000 {

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
    for (auto& f : formants_) f.reset();
}

void VoxWaveOscillator::setFrequency(float freqHz) noexcept
{
    frequency_ = DSPUtils::clamp(freqHz, 10.0f, 8000.0f);
    phaseIncrement_ = frequency_ / static_cast<float>(sampleRate_);
}

void VoxWaveOscillator::setVowel(float vowel0to1) noexcept
{
    vowel_ = DSPUtils::clamp(vowel0to1, 0.0f, 1.0f);
    updateVowelCoefficients();
}

void VoxWaveOscillator::updateVowelCoefficients() noexcept
{
    // Morph between adjacent vowel pairs using linear interpolation of formant frequencies
    // 0.00-0.20 = A → E, 0.20-0.40 = E → I, 0.40-0.60 = I → O, 0.60-0.80 = O → U, 0.80-1.00 = U → A
    float morphed[5];

    const float* const vowels[5] = { VowelFormants::A, VowelFormants::E, VowelFormants::I,
                                     VowelFormants::O, VowelFormants::U };
    float scaled = vowel_ * 4.0f;
    int idx0 = static_cast<int>(scaled);
    if (idx0 < 0) idx0 = 0;
    if (idx0 > 3) idx0 = 3;

    int idx1 = idx0 + 1;
    float t = scaled - static_cast<float>(idx0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    const float* f0 = vowels[idx0];
    const float* f1 = vowels[idx1];

    for (int i = 0; i < 5; ++i)
    {
        morphed[i] = f0[i] + t * (f1[i] - f0[i]);
    }

    // Set bandpass filters with Q=6 for vocal resonance
    for (int i = 0; i < 5; ++i)
    {
        formants_[i].setFreq(morphed[i], 6.0f, sampleRate_);
    }
}

float VoxWaveOscillator::getNextSample() noexcept
{
    // Generate pulse train at the base frequency
    phase_ += phaseIncrement_;
    if (phase_ >= 1.0f) phase_ -= 1.0f;

    // Narrow pulse (10% duty cycle) drives the formant filters
    float pulse = (phase_ < 0.1f) ? 1.0f : -0.15f;

    // Pass pulse through 5 cascaded formant bandpass filters
    float out = pulse;
    for (auto& f : formants_)
    {
        out = f.process(out);
    }

    // Normalize and apply slight shaping for vocal character
    return out * 0.35f;
}

} // namespace ABDMS2000