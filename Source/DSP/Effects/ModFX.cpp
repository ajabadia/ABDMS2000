#include "ModFX.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

void ModFX::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    chorusMaxSamples_ = static_cast<size_t>(sampleRate_ * 0.05); // 50ms buffer
    chorusBufferL_.assign(chorusMaxSamples_, 0.0f);
    chorusBufferR_.assign(chorusMaxSamples_, 0.0f);

    ensembleMaxSamples_ = static_cast<size_t>(sampleRate_ * 0.05);
    for (int i = 0; i < 3; ++i)
    {
        ensembleLinesL_[i].assign(ensembleMaxSamples_, 0.0f);
        ensembleLinesR_[i].assign(ensembleMaxSamples_, 0.0f);
    }

    setSpeed(speed_);
    reset();
}

void ModFX::reset() noexcept
{
    std::fill(chorusBufferL_.begin(), chorusBufferL_.end(), 0.0f);
    std::fill(chorusBufferR_.begin(), chorusBufferR_.end(), 0.0f);
    chorusWriteIndex_ = 0;

    for (int i = 0; i < 3; ++i)
    {
        std::fill(ensembleLinesL_[i].begin(), ensembleLinesL_[i].end(), 0.0f);
        std::fill(ensembleLinesR_[i].begin(), ensembleLinesR_[i].end(), 0.0f);
    }
    ensembleWriteIndex_ = 0;

    lfoPhase_ = 0.0;
    ensemblePhaseSlow_ = 0.0;
    ensemblePhaseFast_ = 0.0;

    for (auto& ap : phaserAPFL_) ap.clear();
    for (auto& ap : phaserAPFR_) ap.clear();
    phaserFeedbackL_ = phaserFeedbackR_ = 0.0f;
}

void ModFX::setSpeed(float speed0to1) noexcept
{
    speed_ = DSPUtils::clamp(speed0to1, 0.0f, 1.0f);
    // Speed range: 0.02 Hz to 15.0 Hz log scale
    float rateHz = 0.02f * std::pow(750.0f, speed_);
    lfoIncrement_ = rateHz / sampleRate_;
}

void ModFX::setDepth(float depth0to1) noexcept
{
    depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
}

void ModFX::setFeedback(float feedback0to127) noexcept
{
    rawFeedback_ = DSPUtils::clamp(feedback0to127, 0.0f, 127.0f);
}

float ModFX::readInterpolated(const std::vector<float>& buf, float readPos, size_t bufLen) const noexcept
{
    while (readPos < 0.0f) readPos += static_cast<float>(bufLen);
    size_t idx0 = static_cast<size_t>(readPos) % bufLen;
    size_t idx1 = (idx0 + 1) % bufLen;
    float frac = readPos - static_cast<float>(static_cast<size_t>(readPos));
    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
}

void ModFX::process(float& leftSample, float& rightSample) noexcept
{
    if (!enabled_ || chorusMaxSamples_ == 0) return;

    switch (type_)
    {
        case ModFXType::ChorusFlanger: processChorusFlanger(leftSample, rightSample); break;
        case ModFXType::Ensemble:      processEnsemble(leftSample, rightSample); break;
        case ModFXType::Phaser:        processPhaser(leftSample, rightSample); break;
    }
}

void ModFX::processChorusFlanger(float& left, float& right) noexcept
{
    // 1. Advance LFO
    float lfo = static_cast<float>(std::sin(DSPUtils::TWO_PI * lfoPhase_));
    lfoPhase_ += lfoIncrement_;
    if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;

    // 2. Base delay: transitions from 7.5ms (lush chorus) down to 1.8ms (jet flanger) as feedback increases
    float fbNorm = rawFeedback_ / 127.0f;
    float baseDelaySec = 0.0075f - (0.0057f * fbNorm);
    float excursionSec = (0.0035f - (0.0020f * fbNorm)) * depth_;

    float baseDelaySamples = baseDelaySec * static_cast<float>(sampleRate_);
    float modExcursionSamples = excursionSec * static_cast<float>(sampleRate_);

    float delayL = baseDelaySamples + (lfo * modExcursionSamples);
    float delayR = baseDelaySamples - (lfo * modExcursionSamples); // Out-of-phase for wide stereo

    float readPosL = static_cast<float>(chorusWriteIndex_) - delayL;
    float readPosR = static_cast<float>(chorusWriteIndex_) - delayR;

    float wetL = readInterpolated(chorusBufferL_, readPosL, chorusMaxSamples_);
    float wetR = readInterpolated(chorusBufferR_, readPosR, chorusMaxSamples_);

    // 3. Feedback logic: 0 is clean chorus, 127 is resonant flanger
    float fbGain = fbNorm * 0.94f;

    chorusBufferL_[chorusWriteIndex_] = left + (wetL * fbGain);
    chorusBufferR_[chorusWriteIndex_] = right + (wetR * fbGain);
    chorusWriteIndex_ = (chorusWriteIndex_ + 1) % chorusMaxSamples_;

    // 4. Equal-power wet mix for deep comb-filtering cancellation
    float wetGain = depth_ * 0.75f;
    float dryGain = std::max(0.35f, 1.0f - (depth_ * 0.40f));
    left  = (left * dryGain) + (wetL * wetGain);
    right = (right * dryGain) + (wetR * wetGain);
}

void ModFX::processEnsemble(float& left, float& right) noexcept
{
    // 1. Advance slow (0.5Hz) & fast (6.0Hz) LFOs
    float slowFreq = 0.5f;
    float fastFreq = 6.0f;

    ensemblePhaseSlow_ += (DSPUtils::TWO_PI * slowFreq) / sampleRate_;
    ensemblePhaseFast_ += (DSPUtils::TWO_PI * fastFreq) / sampleRate_;

    if (ensemblePhaseSlow_ >= DSPUtils::TWO_PI) ensemblePhaseSlow_ -= DSPUtils::TWO_PI;
    if (ensemblePhaseFast_ >= DSPUtils::TWO_PI) ensemblePhaseFast_ -= DSPUtils::TWO_PI;

    // 2. 120-degree phase separation constants
    constexpr float kPhase120 = 2.0943951f;
    constexpr float kPhase240 = 4.1887902f;

    float pS = static_cast<float>(ensemblePhaseSlow_);
    float pF = static_cast<float>(ensemblePhaseFast_);

    float mod1 = std::sin(pS) * 0.8f + std::sin(pF) * 0.2f;
    float mod2 = std::sin(pS + kPhase120) * 0.8f + std::sin(pF + kPhase120) * 0.2f;
    float mod3 = std::sin(pS + kPhase240) * 0.8f + std::sin(pF + kPhase240) * 0.2f;

    float baseDelaySamples = static_cast<float>(0.015 * sampleRate_); // 15ms base
    float modDepthSamples  = static_cast<float>(0.004 * sampleRate_) * depth_; // 4ms excursion

    float d1 = baseDelaySamples + (mod1 * modDepthSamples);
    float d2 = baseDelaySamples + (mod2 * modDepthSamples);
    float d3 = baseDelaySamples + (mod3 * modDepthSamples);

    // 3. Write audio to delay lines
    for (int i = 0; i < 3; ++i)
    {
        ensembleLinesL_[i][ensembleWriteIndex_] = left;
        ensembleLinesR_[i][ensembleWriteIndex_] = right;
    }

    // 4. Read 3 delay lines per channel
    float r1 = static_cast<float>(ensembleWriteIndex_) - d1;
    float r2 = static_cast<float>(ensembleWriteIndex_) - d2;
    float r3 = static_cast<float>(ensembleWriteIndex_) - d3;

    float sL1 = readInterpolated(ensembleLinesL_[0], r1, ensembleMaxSamples_);
    float sL2 = readInterpolated(ensembleLinesL_[1], r2, ensembleMaxSamples_);
    float sL3 = readInterpolated(ensembleLinesL_[2], r3, ensembleMaxSamples_);

    float sR1 = readInterpolated(ensembleLinesR_[0], r1, ensembleMaxSamples_);
    float sR2 = readInterpolated(ensembleLinesR_[1], r2, ensembleMaxSamples_);
    float sR3 = readInterpolated(ensembleLinesR_[2], r3, ensembleMaxSamples_);

    // 5. Asymmetric counter-phase stereo matrix (cancels static center, extreme spatial stereo)
    left  = 0.5f * left  + 0.5f * (sL1 + sR2 - sL3);
    right = 0.5f * right + 0.5f * (sR1 + sL2 - sR3);

    ensembleWriteIndex_ = (ensembleWriteIndex_ + 1) % ensembleMaxSamples_;
}

void ModFX::processPhaser(float& left, float& right) noexcept
{
    // 1. Quadrature LFOs: Left channel (0 deg) and Right channel (+90 deg / PI_2)
    float lfoL = static_cast<float>(std::sin(DSPUtils::TWO_PI * lfoPhase_));
    float lfoR = static_cast<float>(std::sin(DSPUtils::TWO_PI * lfoPhase_ + (DSPUtils::PI * 0.5f)));

    lfoPhase_ += lfoIncrement_;
    if (lfoPhase_ >= 1.0) lfoPhase_ -= 1.0;

    // 2. Logarithmic notch sweep from 200 Hz to 5.5 kHz
    const float minHz = 200.0f;
    const float maxHz = 5500.0f;
    float cutoffHzL = minHz * std::pow(maxHz / minHz, (lfoL * 0.5f + 0.5f) * depth_);
    float cutoffHzR = minHz * std::pow(maxHz / minHz, (lfoR * 0.5f + 0.5f) * depth_);

    // 3. Feedback loop
    float fbGain = (rawFeedback_ / 127.0f) * 0.90f; // Max 90%
    float xL = left + (phaserFeedbackL_ * fbGain);
    float xR = right + (phaserFeedbackR_ * fbGain);

    // 4. 4-Stage 1st-order Allpass Filters in cascade
    auto updateAPF = [this](AllPassState& state, float in, float cutoffHz) noexcept -> float {
        float tanVal = std::tan(DSPUtils::PI * cutoffHz / static_cast<float>(sampleRate_));
        float alpha = (tanVal - 1.0f) / (tanVal + 1.0f);
        float out = alpha * in + state.x1 - alpha * state.y1;
        state.x1 = in;
        state.y1 = out;
        return out;
    };

    for (int i = 0; i < 4; ++i)
    {
        xL = updateAPF(phaserAPFL_[i], xL, cutoffHzL);
        xR = updateAPF(phaserAPFR_[i], xR, cutoffHzR);
    }

    phaserFeedbackL_ = xL;
    phaserFeedbackR_ = xR;

    // 5. 50% Wet blend for perfect notch cancellation
    left  = (left * 0.5f) + (xL * 0.5f);
    right = (right * 0.5f) + (xR * 0.5f);
}

} // namespace ABDMS2000
