#pragma once

#include <algorithm>
#include "../Common/DSPUtils.h"
#include "DspEffects/DspMS2000ChorusFlanger.h"
#include "HardwareConstants.h"

namespace ABDMS2000
{

/**
 * @brief MS2KChorusFlanger - Envoltorio sobre abd::dsp::DspMS2000ChorusFlanger (DSP56362 P:$0F48–$0F95).
 */
class MS2KChorusFlanger
{
  public:
    MS2KChorusFlanger() noexcept = default;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        engine_.prepare(sampleRate_);
        setSpeed(speed_);
        setDepth(depth_);
        setFeedback(rawFeedback_);
    }

    void reset() noexcept
    {
        engine_.reset();
    }

    void setSpeed(float speed0to1) noexcept
    {
        speed_ = DSPUtils::clamp(speed0to1, 0.0f, 1.0f);
        const float rateHz = 0.02f * abd::dsp::pow(DSPUtils::kSpeedLogScaleMax, speed_);
        engine_.setSpeedHz(rateHz);
    }

    void setDepth(float depth0to1) noexcept
    {
        depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
        engine_.setDepth(depth_);
    }

    void setFeedback(float feedback0to127) noexcept
    {
        rawFeedback_ = DSPUtils::clamp(feedback0to127, 0.0f, Constants::kMidiMaxVal);
        engine_.setFeedback((rawFeedback_ / Constants::kMidiMaxVal) * 0.90f);
    }

    inline void processSample(float &left, float &right) noexcept
    {
        engine_.processFrame(left, right);
    }

  private:
    double sampleRate_ {44100.0};
    float speed_ {0.3f};
    float depth_ {0.5f};
    float rawFeedback_ {0.0f};

    abd::dsp::DspMS2000ChorusFlanger engine_;
};

}  // namespace ABDMS2000

