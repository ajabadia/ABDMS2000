#include "ModSequencer.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

// ═══════════════════════════════════════════════════════════════
// MS2000 canonical Mod Seq Resolution → steps per beat
// Same table as Arpeggiator (shared MS2000 SysEx byte)
// ═══════════════════════════════════════════════════════════════
static constexpr float kModSeqResolutionStepsPerBeat[16] = {
    12.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f,
    0.75f, 0.5f, 0.3333f, 0.25f, 0.1667f, 0.125f, 0.0833f, 0.0625f
};

float ModSequencer::syncResolutionToStepsPerBeat(int idx) noexcept
{
    if (idx < 0 || idx > 15) return kModSeqResolutionStepsPerBeat[3];
    return kModSeqResolutionStepsPerBeat[idx];
}

void ModSequencer::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    // 10ms smooth slew rate
    slewMultiplier_ = std::exp(-5.0 / (0.010 * sampleRate_));
    static constexpr float kDefaultTrack1[16] = {
        0.15f, 0.40f, 0.75f, 0.30f, 0.90f, 0.55f, 0.20f, 0.85f,
        0.35f, 0.65f, 0.45f, 0.95f, 0.25f, 0.70f, 0.50f, 0.10f
    };
    static constexpr float kDefaultTrack2[16] = {
        0.50f, 0.50f, 0.75f, 0.50f, 0.62f, 0.50f, 0.83f, 0.50f,
        0.50f, 0.75f, 0.50f, 0.62f, 0.83f, 0.75f, 0.62f, 0.50f
    };
    static constexpr float kDefaultTrack3[16] = {
        0.20f, 0.80f, 0.30f, 0.70f, 0.10f, 0.90f, 0.40f, 0.60f,
        0.25f, 0.75f, 0.35f, 0.65f, 0.15f, 0.85f, 0.50f, 0.50f
    };

    tracks_[0].length = 16;
    tracks_[1].length = 16;
    tracks_[2].length = 16;
    for (size_t s = 0; s < NUM_STEPS; ++s)
    {
        tracks_[0].steps[s] = kDefaultTrack1[s];
        tracks_[1].steps[s] = kDefaultTrack2[s];
        tracks_[2].steps[s] = kDefaultTrack3[s];
    }
    setTempoBPM(bpm_);
    reset();
}

void ModSequencer::reset() noexcept
{
    stepSampleCounter_ = 0.0;
    for (size_t t = 0; t < NUM_TRACKS; ++t)
    {
        currentStep_[t] = 0;
        bounceDirection_[t] = true;
        smoothedOutput_[t] = tracks_[t].steps[0];
    }
}

void ModSequencer::setTempoBPM(float bpm) noexcept
{
    bpm_ = DSPUtils::clamp(bpm, 20.0f, 300.0f);
    double secondsPerBeat = 60.0 / bpm_;
    double secondsPerStep = secondsPerBeat / syncStepsPerBeat_;
    samplesPerStep_ = secondsPerStep * sampleRate_;
}

void ModSequencer::setSyncResolution(int idx) noexcept
{
    if (idx < 0 || idx > 15) idx = 3;
    syncStepsPerBeat_ = kModSeqResolutionStepsPerBeat[idx];
    setTempoBPM(bpm_); // re-evaluate samplesPerStep
}

void ModSequencer::triggerKeySync() noexcept
{
    reset();
}

void ModSequencer::advanceClock(int numSamples) noexcept
{
    if (!enabled_) return;

    stepSampleCounter_ += numSamples;
    while (stepSampleCounter_ >= samplesPerStep_)
    {
        stepSampleCounter_ -= samplesPerStep_;
        advanceStep();
    }

    // Apply slew limiter for Smooth mode:
    // Glides smoothly over 35ms in Smooth mode, or steps instantly in Step mode
    float glideTimeSec = 0.035f;
    float blockFactor = 1.0f - std::exp(-static_cast<float>(numSamples) / (glideTimeSec * static_cast<float>(sampleRate_)));
    blockFactor = std::max(0.01f, std::min(1.0f, blockFactor));

    for (size_t t = 0; t < NUM_TRACKS; ++t)
    {
        float target = tracks_[t].steps[currentStep_[t]];
        if (tracks_[t].motion == ModSeqMotion::Smooth)
        {
            smoothedOutput_[t] += (target - smoothedOutput_[t]) * blockFactor;
        }
        else
        {
            smoothedOutput_[t] = target;
        }
    }
}

void ModSequencer::advanceStep() noexcept
{
    for (size_t t = 0; t < NUM_TRACKS; ++t)
    {
        const int len = std::max(1, std::min(16, tracks_[t].length));

        switch (tracks_[t].mode)
        {
            case ModSeqMode::Forward:
                currentStep_[t] = (currentStep_[t] + 1) % len;
                break;

            case ModSeqMode::Backward:
                currentStep_[t] = (currentStep_[t] - 1 + len) % len;
                break;

            case ModSeqMode::Bounce:
                if (bounceDirection_[t])
                {
                    currentStep_[t]++;
                    if (currentStep_[t] >= len - 1)
                    {
                        currentStep_[t] = len - 1;
                        bounceDirection_[t] = false;
                    }
                }
                else
                {
                    currentStep_[t]--;
                    if (currentStep_[t] <= 0)
                    {
                        currentStep_[t] = 0;
                        bounceDirection_[t] = true;
                    }
                }
                break;

            case ModSeqMode::Random:
                DSPUtils::randomBipolar(rngState_); // Advance LCG
                currentStep_[t] = static_cast<int>(rngState_ % static_cast<uint32_t>(len));
                break;
        }
    }
}

float ModSequencer::getTrackOutput(size_t trackIndex) const noexcept
{
    if (!enabled_) return 0.0f;
    return smoothedOutput_[trackIndex % NUM_TRACKS];
}

} // namespace ABDMS2000
