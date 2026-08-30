#include "DelayFX.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

void DelayFX::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    // 1400ms max buffer size with headroom
    maxDelaySamples_ = static_cast<size_t>(sampleRate_ * 1.5);
    delayBufferL_.assign(maxDelaySamples_, 0.0f);
    delayBufferR_.assign(maxDelaySamples_, 0.0f);

    // 50ms smoothing time to achieve authentic MS2000 tape pitch glide
    smoothedDelaySamplesL_.reset(sampleRate_, 0.05);
    smoothedDelaySamplesR_.reset(sampleRate_, 0.05);

    setTimeSeconds(timeSeconds_);
    reset();
}

void DelayFX::reset() noexcept
{
    std::fill(delayBufferL_.begin(), delayBufferL_.end(), 0.0f);
    std::fill(delayBufferR_.begin(), delayBufferR_.end(), 0.0f);
    writeIndex_ = 0;
    dampingStateL_ = 0.0f;
    dampingStateR_ = 0.0f;
}

void DelayFX::setTimeSeconds(float timeSec) noexcept
{
    timeSeconds_ = DSPUtils::clamp(timeSec, 0.001f, 1.400f); // 1400ms max limit
    float targetSamplesL = timeSeconds_ * static_cast<float>(sampleRate_);
    float targetSamplesR = targetSamplesL;

    if (type_ == DelayType::LeftRight)
    {
        targetSamplesR = targetSamplesL * 0.75f;
    }

    smoothedDelaySamplesL_.setTargetValue(targetSamplesL);
    smoothedDelaySamplesR_.setTargetValue(targetSamplesR);
}

void DelayFX::setDepth(float wetMix0to1) noexcept
{
    wetMix_ = DSPUtils::clamp(wetMix0to1, 0.0f, 1.0f);
}

void DelayFX::setFeedback(float feedbackGain0to1) noexcept
{
    feedbackGain_ = DSPUtils::clamp(feedbackGain0to1, 0.0f, 0.95f);
}

float DelayFX::readInterpolated(const std::vector<float>& buf, float readPos) const noexcept
{
    while (readPos < 0.0f) readPos += static_cast<float>(maxDelaySamples_);

    size_t idx0 = static_cast<size_t>(readPos) % maxDelaySamples_;
    size_t idx1 = (idx0 + 1) % maxDelaySamples_;
    float frac = readPos - static_cast<float>(static_cast<size_t>(readPos));

    return buf[idx0] + frac * (buf[idx1] - buf[idx0]);
}

void DelayFX::process(float& leftSample, float& rightSample) noexcept
{
    if (!enabled_ || maxDelaySamples_ == 0) return;

    float currentDelayL = smoothedDelaySamplesL_.getNextValue();
    float currentDelayR = smoothedDelaySamplesR_.getNextValue();

    float readPosL = static_cast<float>(writeIndex_) - currentDelayL;
    float readPosR = static_cast<float>(writeIndex_) - currentDelayR;

    float delayedL = readInterpolated(delayBufferL_, readPosL);
    float delayedR = readInterpolated(delayBufferR_, readPosR);

    // 1-pole Low-Pass Damping Filter in feedback path (~6.0 kHz cutoff on Motorola DSP56362)
    dampingStateL_ += 0.35f * (delayedL - dampingStateL_);
    dampingStateR_ += 0.35f * (delayedR - dampingStateR_);

    float inL = leftSample;
    float inR = rightSample;

    switch (type_)
    {
        case DelayType::Stereo:
            delayBufferL_[writeIndex_] = inL + (dampingStateL_ * feedbackGain_);
            delayBufferR_[writeIndex_] = inR + (dampingStateR_ * feedbackGain_);
            break;

        case DelayType::Cross: // Ping-Pong with Inverted Feedback on Left
            delayBufferL_[writeIndex_] = inL - (dampingStateR_ * feedbackGain_);
            delayBufferR_[writeIndex_] = inR + (dampingStateL_ * feedbackGain_);
            break;

        case DelayType::LeftRight:
            delayBufferL_[writeIndex_] = inL + (dampingStateL_ * feedbackGain_);
            delayBufferR_[writeIndex_] = inR + (dampingStateR_ * feedbackGain_);
            break;
    }

    writeIndex_ = (writeIndex_ + 1) % maxDelaySamples_;

    leftSample  = inL + (delayedL * wetMix_ * 0.88f);
    rightSample = inR + (delayedR * wetMix_ * 0.88f);
}

} // namespace ABDMS2000
