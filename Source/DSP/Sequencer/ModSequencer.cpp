#include "ModSequencer.h"
#include <algorithm>
#include <cmath>
#include "../Common/DSPUtils.h"

namespace ABDMS2000
{

// ═══════════════════════════════════════════════════════════════
// MS2000 canonical Mod Seq Resolution → steps per beat (cbSeqResolution 0..15)
// 0=1/48, 1=1/32, 2=1/24, 3=1/16, 4=1/12, 5=3/32, 6=1/8, 7=1/6,
// 8=3/16, 9=1/4, 10=1/3, 11=3/8, 12=1/2, 13=2/3, 14=3/4, 15=1/1
// ═══════════════════════════════════════════════════════════════
static constexpr float kModSeqResolutionStepsPerBeat[16] = {
    12.0f,        // 0:  1/48
    8.0f,         // 1:  1/32
    6.0f,         // 2:  1/24
    4.0f,         // 3:  1/16
    3.0f,         // 4:  1/12
    8.0f / 3.0f,  // 5:  3/32
    2.0f,         // 6:  1/8
    1.5f,         // 7:  1/6
    4.0f / 3.0f,  // 8:  3/16
    1.0f,         // 9:  1/4
    0.75f,        // 10: 1/3
    2.0f / 3.0f,  // 11: 3/8
    0.5f,         // 12: 1/2
    0.375f,       // 13: 2/3
    1.0f / 3.0f,  // 14: 3/4
    0.25f         // 15: 1/1
};

float ModSequencer::syncResolutionToStepsPerBeat(int idx) noexcept
{
    if (idx < 0 || idx > 15)
        return kModSeqResolutionStepsPerBeat[3];
    return kModSeqResolutionStepsPerBeat[idx];
}

void ModSequencer::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    // 10ms smooth slew rate
    slewMultiplier_ = std::exp(-5.0 / (0.010 * sampleRate_));
    static constexpr float kDefaultTrack1[16] = {
        0.15f, 0.40f, 0.75f, 0.30f, 0.90f, 0.55f, 0.20f, 0.85f, 0.35f, 0.65f, 0.45f, 0.95f, 0.25f, 0.70f, 0.50f, 0.10f};
    static constexpr float kDefaultTrack2[16] = {
        0.50f, 0.50f, 0.75f, 0.50f, 0.62f, 0.50f, 0.83f, 0.50f, 0.50f, 0.75f, 0.50f, 0.62f, 0.83f, 0.75f, 0.62f, 0.50f};
    static constexpr float kDefaultTrack3[16] = {
        0.20f, 0.80f, 0.30f, 0.70f, 0.10f, 0.90f, 0.40f, 0.60f, 0.25f, 0.75f, 0.35f, 0.65f, 0.15f, 0.85f, 0.50f, 0.50f};

    // Las pistas del Timbre 2 arrancan con la misma fila que las del Timbre 1: el
    // programa INIT del equipo real tiene los dos bloques de timbre idénticos.
    for (size_t t = 0; t < NUM_TRACKS; ++t)
    {
        const float *defaults = (t % TRACKS_PER_TIMBRE == 0)   ? kDefaultTrack1
                                : (t % TRACKS_PER_TIMBRE == 1) ? kDefaultTrack2
                                                               : kDefaultTrack3;
        tracks_[t].length = static_cast<int>(NUM_STEPS);
        for (size_t s = 0; s < NUM_STEPS; ++s)
            tracks_[t].steps[s] = defaults[s];
    }
    setTempoBPM(bpm_);
    reset();
}

void ModSequencer::reset() noexcept
{
    stepSampleCounter_[0] = 0.0;
    stepSampleCounter_[1] = 0.0;
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
    for (size_t tim = 0; tim < 2; ++tim)
    {
        double secondsPerStep = secondsPerBeat / syncStepsPerBeat_[tim];
        samplesPerStep_[tim] = secondsPerStep * sampleRate_;
    }
}

void ModSequencer::setSyncResolution(int idx) noexcept
{
    if (idx < 0 || idx > 15)
        idx = 3;
    syncStepsPerBeat_[0] = kModSeqResolutionStepsPerBeat[idx];
    syncStepsPerBeat_[1] = kModSeqResolutionStepsPerBeat[idx];
    setTempoBPM(bpm_);  // re-evaluate samplesPerStep
}

void ModSequencer::setTimbreSyncResolution(bool second, int idx) noexcept
{
    if (idx < 0 || idx > 15)
        idx = 3;
    syncStepsPerBeat_[second ? 1 : 0] = kModSeqResolutionStepsPerBeat[idx];
    setTempoBPM(bpm_);
}

void ModSequencer::triggerKeySync(size_t firstTrack, size_t count) noexcept
{
    // El Key Sync del equipo es por timbre: solo las filas de ese timbre vuelven al paso 0.
    if (firstTrack < TIMBRE2_TRACK)
        stepSampleCounter_[0] = 0.0;
    if (firstTrack + count > TIMBRE2_TRACK)
        stepSampleCounter_[1] = 0.0;
    const size_t end = std::min(NUM_TRACKS, firstTrack + count);
    for (size_t t = firstTrack; t < end; ++t)
    {
        currentStep_[t] = 0;
        bounceDirection_[t] = true;
        smoothedOutput_[t] = tracks_[t].steps[0];
    }
}

void ModSequencer::advanceClock(int numSamples) noexcept
{
    if (!enabled_)
        return;

    for (size_t tim = 0; tim < 2; ++tim)
    {
        stepSampleCounter_[tim] += numSamples;
        while (stepSampleCounter_[tim] >= samplesPerStep_[tim])
        {
            stepSampleCounter_[tim] -= samplesPerStep_[tim];
            advanceStep(tim * TRACKS_PER_TIMBRE, TRACKS_PER_TIMBRE);
        }
    }

    // Interpolate across the step duration in Smooth mode (matching H8S 0x18800 step-phase
    // interpolation), or hold discrete step values in Step mode.
    float glideTimeSec = 0.035f;
    float blockFactor =
        1.0f - std::exp(-static_cast<float>(numSamples) / (glideTimeSec * static_cast<float>(sampleRate_)));
    blockFactor = std::max(0.01f, std::min(1.0f, blockFactor));

    for (size_t t = 0; t < NUM_TRACKS; ++t)
    {
        const size_t tim = t / TRACKS_PER_TIMBRE;
        const int len = std::max(1, std::min(16, tracks_[t].length));
        const int cur = std::clamp(currentStep_[t], 0, len - 1);
        const float v0 = tracks_[t].steps[static_cast<size_t>(cur)];

        if (tracks_[t].motion == ModSeqMotion::Smooth)
        {
            if (tracks_[t].mode == ModSeqMode::Random)
            {
                smoothedOutput_[t] += (v0 - smoothedOutput_[t]) * blockFactor;
            }
            else
            {
                int next = cur;
                const bool isOneShot = tracks_[t].oneShot;
                if (tracks_[t].mode == ModSeqMode::Forward)
                {
                    next = (isOneShot && cur >= len - 1) ? (len - 1) : ((cur + 1) % len);
                }
                else if (tracks_[t].mode == ModSeqMode::Backward)
                {
                    next = (isOneShot && cur <= 0) ? 0 : ((cur - 1 + len) % len);
                }
                else if (tracks_[t].mode == ModSeqMode::Bounce)
                {
                    if (bounceDirection_[t])
                        next = (cur + 1 >= len) ? std::max(0, len - 1) : (cur + 1);
                    else
                        next = (isOneShot && cur <= 0) ? 0 : std::max(0, cur - 1);
                }
                const float v1 = tracks_[t].steps[static_cast<size_t>(next)];
                const float stepPhase =
                    (samplesPerStep_[tim] > 1.0)
                        ? static_cast<float>(std::clamp(stepSampleCounter_[tim] / samplesPerStep_[tim], 0.0, 1.0))
                        : 0.0f;
                // Smooth step-phase interpolation (H8S 0x18800 smooth mode)
                smoothedOutput_[t] = v0 + (v1 - v0) * stepPhase;
            }
        }
        else
        {
            smoothedOutput_[t] = v0;
        }
    }
}

void ModSequencer::advanceStep(size_t firstTrack, size_t count) noexcept
{
    const size_t end = std::min(NUM_TRACKS, firstTrack + count);
    for (size_t t = firstTrack; t < end; ++t)
    {
        const int len = std::max(1, std::min(16, tracks_[t].length));
        const bool isOneShot = tracks_[t].oneShot;

        switch (tracks_[t].mode)
        {
        case ModSeqMode::Forward:
            if (isOneShot && currentStep_[t] >= len - 1)
                currentStep_[t] = len - 1;
            else
                currentStep_[t] = (currentStep_[t] + 1) % len;
            break;

        case ModSeqMode::Backward:
            if (isOneShot && currentStep_[t] <= 0)
                currentStep_[t] = 0;
            else
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
                if (isOneShot && currentStep_[t] <= 0)
                {
                    currentStep_[t] = 0;
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
            }
            break;

        case ModSeqMode::Random:
            DSPUtils::randomBipolar(rngState_);  // Advance LCG
            currentStep_[t] = static_cast<int>(rngState_ % static_cast<uint32_t>(len));
            break;
        }
    }
}

float ModSequencer::getTrackOutput(size_t trackIndex) const noexcept
{
    if (!enabled_)
        return 0.0f;
    return smoothedOutput_[trackIndex % NUM_TRACKS];
}

}  // namespace ABDMS2000
