#include "Equalizer.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

static const float kMS2000LowFreqs[4]  = { 160.0f, 250.0f, 400.0f, 600.0f };
static const float kMS2000HighFreqs[4] = { 4000.0f, 6000.0f, 8000.0f, 12000.0f };
static constexpr float kButterworthQ   = 0.70710678f;

void Equalizer::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    updateLowShelfCoeffs();
    updateHighShelfCoeffs();
    reset();
}

void Equalizer::reset() noexcept
{
    s1_LL_ = s2_LL_ = s1_LR_ = s2_LR_ = 0.0f;
    s1_HL_ = s2_HL_ = s1_HR_ = s2_HR_ = 0.0f;
}

void Equalizer::setLowFreqIndex(int index0to3) noexcept
{
    int clamped = std::max(0, std::min(3, index0to3));
    if (clamped != lowFreqIndex_)
    {
        lowFreqIndex_ = clamped;
        updateLowShelfCoeffs();
    }
}

void Equalizer::setLowGainDB(float gainDB) noexcept
{
    float clamped = DSPUtils::clamp(gainDB, -12.0f, 12.0f);
    if (std::abs(clamped - lowGainDB_) > 0.05f)
    {
        lowGainDB_ = clamped;
        updateLowShelfCoeffs();
    }
}

void Equalizer::setHighFreqIndex(int index0to3) noexcept
{
    int clamped = std::max(0, std::min(3, index0to3));
    if (clamped != highFreqIndex_)
    {
        highFreqIndex_ = clamped;
        updateHighShelfCoeffs();
    }
}

void Equalizer::setHighGainDB(float gainDB) noexcept
{
    float clamped = DSPUtils::clamp(gainDB, -12.0f, 12.0f);
    if (std::abs(clamped - highGainDB_) > 0.05f)
    {
        highGainDB_ = clamped;
        updateHighShelfCoeffs();
    }
}

void Equalizer::updateLowShelfCoeffs() noexcept
{
    float freqHz = kMS2000LowFreqs[lowFreqIndex_];
    float A = std::pow(10.0f, lowGainDB_ / 40.0f);
    float w0 = freqHz * DSPUtils::TWO_PI / static_cast<float>(sampleRate_);
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * kButterworthQ);

    float a0 = (A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha;
    b0_L_ = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha)) / a0;
    b1_L_ = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    b2_L_ = (A * ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha)) / a0;
    a1_L_ = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    a2_L_ = ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha) / a0;
}

void Equalizer::updateHighShelfCoeffs() noexcept
{
    float freqHz = kMS2000HighFreqs[highFreqIndex_];
    float A = std::pow(10.0f, highGainDB_ / 40.0f);
    float w0 = freqHz * DSPUtils::TWO_PI / static_cast<float>(sampleRate_);
    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 / (2.0f * kButterworthQ);

    float a0 = (A + 1.0f) - (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha;
    b0_H_ = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 + 2.0f * std::sqrt(A) * alpha)) / a0;
    b1_H_ = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cos_w0)) / a0;
    b2_H_ = (A * ((A + 1.0f) + (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha)) / a0;
    a1_H_ = (2.0f * ((A - 1.0f) - (A + 1.0f) * cos_w0)) / a0;
    a2_H_ = ((A + 1.0f) - (A - 1.0f) * cos_w0 - 2.0f * std::sqrt(A) * alpha) / a0;
}

void Equalizer::process(float& leftSample, float& rightSample) noexcept
{
    // Low shelf stage (Direct Form II Transposed)
    float yLL = b0_L_ * leftSample + s1_LL_;
    s1_LL_ = b1_L_ * leftSample - a1_L_ * yLL + s2_LL_;
    s2_LL_ = b2_L_ * leftSample - a2_L_ * yLL;

    float yLR = b0_L_ * rightSample + s1_LR_;
    s1_LR_ = b1_L_ * rightSample - a1_L_ * yLR + s2_LR_;
    s2_LR_ = b2_L_ * rightSample - a2_L_ * yLR;

    // High shelf stage
    float yHL = b0_H_ * yLL + s1_HL_;
    s1_HL_ = b1_H_ * yLL - a1_H_ * yHL + s2_HL_;
    s2_HL_ = b2_H_ * yLL - a2_H_ * yHL;

    float yHR = b0_H_ * yLR + s1_HR_;
    s1_HR_ = b1_H_ * yLR - a1_H_ * yHR + s2_HR_;
    s2_HR_ = b2_H_ * yLR - a2_H_ * yHR;

    leftSample = yHL;
    rightSample = yHR;
}

} // namespace ABDMS2000
