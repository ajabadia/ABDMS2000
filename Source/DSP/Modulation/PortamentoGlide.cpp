#include "PortamentoGlide.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

void PortamentoGlide::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    updateMultiplier();
}

void PortamentoGlide::reset(float initialMidiNote) noexcept
{
    currentPitch_ = initialMidiNote;
    targetPitch_ = initialMidiNote;
}

void PortamentoGlide::setTargetNote(float targetMidiNote, bool glideEnabled) noexcept
{
    targetPitch_ = targetMidiNote;
    if (!glideEnabled || timeParam_ <= 0.001f)
    {
        currentPitch_ = targetMidiNote;
    }
}

void PortamentoGlide::setGlideTime(float timeParam0to1) noexcept
{
    timeParam_ = std::max(0.0f, std::min(1.0f, timeParam0to1));
    updateMultiplier();
}

void PortamentoGlide::updateMultiplier() noexcept
{
    if (timeParam_ <= 0.001f)
    {
        slewMultiplier_ = 0.0;
        return;
    }

    // Map 0..1 to 5ms..3.5 seconds
    float glideSec = 0.005f + (3.5f * std::pow(timeParam_, 2.0f));
    slewMultiplier_ = std::exp(-5.0 / (glideSec * sampleRate_));
}

float PortamentoGlide::getNextPitchSemitones() noexcept
{
    if (slewMultiplier_ > 0.0)
    {
        float diff = targetPitch_ - currentPitch_;
        if (std::abs(diff) > 0.001f)
        {
            currentPitch_ = targetPitch_ - (diff * static_cast<float>(slewMultiplier_));
        }
        else
        {
            currentPitch_ = targetPitch_;
        }
    }
    else
    {
        currentPitch_ = targetPitch_;
    }

    return currentPitch_;
}

} // namespace ABDMS2000
