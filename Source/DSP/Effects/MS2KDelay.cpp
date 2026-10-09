#include "MS2KDelay.h"
#include <algorithm>
#include <cmath>
#include "../Common/DSPUtils.h"
#include "HardwareConstants.h"

namespace ABDMS2000
{

void MS2KDelay::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    // Buffer máximo con margen de seguridad para interpolación
    maxDelaySamples_ = static_cast<size_t>(sampleRate_ * Constants::kDelayBufferHeadroomRatio);
    delayBufferL_.assign(maxDelaySamples_, 0.0f);
    delayBufferR_.assign(maxDelaySamples_, 0.0f);

    // Suavizado de 50 ms para lograr el auténtico pitch glide del MS2000
    smoothedDelaySamplesL_.reset(sampleRate_, Constants::kDelayGlideTimeSec);
    smoothedDelaySamplesR_.reset(sampleRate_, Constants::kDelayGlideTimeSec);

    setTimeSeconds(timeSeconds_);
    reset();
}

void MS2KDelay::reset() noexcept
{
    std::fill(delayBufferL_.begin(), delayBufferL_.end(), 0.0f);
    std::fill(delayBufferR_.begin(), delayBufferR_.end(), 0.0f);
    writeIndex_ = 0;
    dampingStateL_ = 0.0f;
    dampingStateR_ = 0.0f;
}

void MS2KDelay::setTimeSeconds(float timeSec) noexcept
{
    // timeSec viene transformado según el schema JSON (domainMin=0.001s, domainMax=1.4s);
    // aseguramos únicamente el límite físico del buffer asignado.
    const float maxBufferSec = (sampleRate_ > 0.0) ? (static_cast<float>(maxDelaySamples_) / static_cast<float>(sampleRate_)) : 1.5f;
    timeSeconds_ = DSPUtils::clamp(timeSec, 0.0001f, maxBufferSec);
    float targetSamplesL = timeSeconds_ * static_cast<float>(sampleRate_);
    float targetSamplesR = targetSamplesL;

    if (type_ == DelayType::LeftRight)
    {
        // En modo L/R, el canal derecho tiene una relación fija de 0.75x
        targetSamplesR = targetSamplesL * Constants::kDelayLeftRightRatio;
    }

    smoothedDelaySamplesL_.setTargetValue(targetSamplesL);
    smoothedDelaySamplesR_.setTargetValue(targetSamplesR);
}

void MS2KDelay::setDepth(float wetMix0to1) noexcept
{
    wetMix_ = DSPUtils::clamp(wetMix0to1, 0.0f, 1.0f);
}

void MS2KDelay::setFeedback(float feedbackGain0to1) noexcept
{
    feedbackGain_ = DSPUtils::clamp(feedbackGain0to1, 0.0f, Constants::kDelayMaxFeedback);
}

float MS2KDelay::readHermite(const std::vector<float> &buf, float readPos) const noexcept
{
    const float fBufLen = static_cast<float>(maxDelaySamples_);
    while (readPos < 0.0f)
        readPos += fBufLen;

    const size_t i1 = static_cast<size_t>(readPos) % maxDelaySamples_;
    const size_t i0 = (i1 + maxDelaySamples_ - 1) % maxDelaySamples_;
    const size_t i2 = (i1 + 1) % maxDelaySamples_;
    const size_t i3 = (i1 + 2) % maxDelaySamples_;

    const float frac = readPos - static_cast<float>(static_cast<size_t>(readPos));

    const float y0 = buf[i0];
    const float y1 = buf[i1];
    const float y2 = buf[i2];
    const float y3 = buf[i3];

    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);

    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

void MS2KDelay::process(float &leftSample, float &rightSample) noexcept
{
    if (!enabled_ || maxDelaySamples_ == 0)
        return;

    const float currentDelayL = smoothedDelaySamplesL_.getNextValue();
    const float currentDelayR = smoothedDelaySamplesR_.getNextValue();

    const float readPosL = static_cast<float>(writeIndex_) - currentDelayL;
    const float readPosR = static_cast<float>(writeIndex_) - currentDelayR;

    const float delayedL = readHermite(delayBufferL_, readPosL);
    const float delayedR = readHermite(delayBufferR_, readPosR);

    // Filtro paso-bajos 1-polo de amortiguación a ~6.0 kHz en el Motorola DSP56362
    dampingStateL_ += Constants::kDelayDampingCoeff * (delayedL - dampingStateL_);
    dampingStateR_ += Constants::kDelayDampingCoeff * (delayedR - dampingStateR_);

    const float inL = leftSample;
    const float inR = rightSample;

    switch (type_)
    {
    case DelayType::Stereo:
        delayBufferL_[writeIndex_] = inL + (dampingStateL_ * feedbackGain_);
        delayBufferR_[writeIndex_] = inR + (dampingStateR_ * feedbackGain_);
        break;

    case DelayType::Cross:  // Ping-Pong con realimentación invertida en el canal izquierdo (-R en L, +L en R)
        delayBufferL_[writeIndex_] = inL - (dampingStateR_ * feedbackGain_);
        delayBufferR_[writeIndex_] = inR + (dampingStateL_ * feedbackGain_);
        break;

    case DelayType::LeftRight:
        delayBufferL_[writeIndex_] = inL + (dampingStateL_ * feedbackGain_);
        delayBufferR_[writeIndex_] = inR + (dampingStateR_ * feedbackGain_);
        break;
    }

    writeIndex_ = (writeIndex_ + 1) % maxDelaySamples_;

    leftSample = inL + (delayedL * wetMix_ * Constants::kDelayWetMixScale);
    rightSample = inR + (delayedR * wetMix_ * Constants::kDelayWetMixScale);
}

}  // namespace ABDMS2000
