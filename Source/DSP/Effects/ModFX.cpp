#include "ModFX.h"
#include <algorithm>
#include "../Common/DSPUtils.h"

namespace ABDMS2000
{

void ModFX::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);

    chorusFlanger_.prepare(sampleRate_);
    ensemble_.prepare(sampleRate_);

    phaser_.prepare(sampleRate_);
    phaser_.setSweepRange(200.0f, 5500.0f);  // El barrido del MS2000

    setSpeed(speed_);
    setDepth(depth_);
    setFeedback(rawFeedback_);

    reset();
}

void ModFX::reset() noexcept
{
    chorusFlanger_.reset();
    ensemble_.reset();
    phaser_.reset();
}

void ModFX::setSpeed(float speed0to1) noexcept
{
    speed_ = DSPUtils::clamp(speed0to1, 0.0f, 1.0f);

    chorusFlanger_.setSpeed(speed_);
    ensemble_.setSpeed(speed_);
    phaser_.setRateHz(0.02f * abd::dsp::pow(DSPUtils::kSpeedLogScaleMax, speed_));
}

void ModFX::setDepth(float depth0to1) noexcept
{
    depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);

    chorusFlanger_.setDepth(depth_);
    ensemble_.setDepth(depth_);
    phaser_.setDepth(depth_);
}

void ModFX::setFeedback(float feedback0to127) noexcept
{
    rawFeedback_ = DSPUtils::clamp(feedback0to127, 0.0f, 127.0f);

    chorusFlanger_.setFeedback(rawFeedback_);
    // En el DSP56362 (X:$0266 = $3C0000 = 0.46875), el Phaser tiene realimentación resonante base
    // incluso cuando el preset del MS2000 no expone un byte dedicado de ModFX Feedback.
    const float phaserFb = (rawFeedback_ > 0.5f) ? ((rawFeedback_ / 127.0f) * 0.90f) : 0.46875f;
    phaser_.setFeedback(phaserFb);
}

void ModFX::process(float &leftSample, float &rightSample) noexcept
{
    if (!enabled_ || depth_ <= 0.0001f)
        return;

    switch (type_)
    {
    case ModFXType::ChorusFlanger:
        chorusFlanger_.processSample(leftSample, rightSample);
        break;
    case ModFXType::Ensemble:
        ensemble_.processFrame(leftSample, rightSample);
        break;
    case ModFXType::Phaser:
        phaser_.processFrame(leftSample, rightSample);
        break;
    }
}

}  // namespace ABDMS2000
