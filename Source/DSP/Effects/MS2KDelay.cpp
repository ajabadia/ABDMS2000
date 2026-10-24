#include "MS2KDelay.h"
#include <algorithm>
#include "../Common/DSPUtils.h"
#include "HardwareConstants.h"

namespace ABDMS2000
{

void MS2KDelay::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    engine_.prepare(sampleRate_);
    engine_.setEnabled(enabled_);
    engine_.setMode(static_cast<abd::dsp::DspMS2000Delay::Mode>(static_cast<int>(type_)));
    engine_.setTimeSeconds(timeSeconds_);
    engine_.setDepth(wetMix_);
    engine_.setFeedback(feedbackGain_);
}

void MS2KDelay::reset() noexcept
{
    engine_.reset();
}

void MS2KDelay::setTimeSeconds(float timeSec) noexcept
{
    timeSeconds_ = DSPUtils::clamp(timeSec, 0.001f, 1.4f);
    engine_.setTimeSeconds(timeSeconds_);
}

void MS2KDelay::setDepth(float wetMix0to1) noexcept
{
    wetMix_ = DSPUtils::clamp(wetMix0to1, 0.0f, 1.0f);
    engine_.setDepth(wetMix_);
}

void MS2KDelay::setFeedback(float feedbackGain0to1) noexcept
{
    feedbackGain_ = DSPUtils::clamp(feedbackGain0to1, 0.0f, Constants::kDelayMaxFeedback);
    engine_.setFeedback(feedbackGain_);
}

void MS2KDelay::process(float &leftSample, float &rightSample) noexcept
{
    if (!enabled_)
        return;

    engine_.processFrame(leftSample, rightSample);
}

}  // namespace ABDMS2000
