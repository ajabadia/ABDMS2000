#pragma once
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

/**
 * @brief High-precision Envelope Follower with configurable release time (Gate Sense).
 */
class EnvelopeFollower {
public:
    EnvelopeFollower() = default;

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
        envelope_ = 0.0f;
        setReleaseTime(0.05f); // 50ms default
    }

    void reset() noexcept
    {
        envelope_ = 0.0f;
    }

    void setReleaseTime(float timeInSeconds) noexcept
    {
        float t = std::max(0.002f, std::min(0.5f, timeInSeconds));
        releaseCoef_ = std::exp(-1.0f / (t * static_cast<float>(sampleRate_)));
    }

    float process(float sample) noexcept
    {
        float absSample = std::abs(sample);
        // Instant attack, exponential decay release
        if (absSample > envelope_)
        {
            envelope_ = absSample;
        }
        else
        {
            envelope_ = absSample + releaseCoef_ * (envelope_ - absSample);
        }
        return envelope_;
    }

    float getCurrentLevel() const noexcept { return envelope_; }

private:
    double sampleRate_{ 44100.0 };
    float envelope_{ 0.0f };
    float releaseCoef_{ 0.999f };
};

} // namespace ABDMS2000
