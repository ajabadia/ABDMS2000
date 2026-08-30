#pragma once
#include <vector>
#include "../Common/DSPUtils.h"

namespace ABDMS2000 {

enum class DelayType {
    Stereo = 0,
    Cross,
    LeftRight
};

/**
 * @brief MS2000 / Motorola DSP56362 Delay FX Block.
 * Incorporates:
 * - Max delay: 1400ms (1.4s)
 * - Pitch Glitch / Analog Tape Drag using 50ms LinearSmoothedValue & fractional interpolation
 * - 6.0 kHz Damping One-Pole Low-Pass Filter in feedback path
 * - Stereo, Cross Delay (Inverted Left Feedback), and L/R Delay (0.75x right offset)
 */
class DelayFX {
public:
    DelayFX() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool isEnabled() const noexcept { return enabled_; }

    void setType(DelayType type) noexcept { type_ = type; }
    void setTimeSeconds(float timeSec) noexcept;
    void setDepth(float wetMix0to1) noexcept;
    void setFeedback(float feedbackGain0to1) noexcept;

    void process(float& leftSample, float& rightSample) noexcept;

private:
    double sampleRate_{ 44100.0 };
    bool enabled_{ false };
    DelayType type_{ DelayType::Stereo };

    float timeSeconds_{ 0.35f };
    float wetMix_{ 0.4f };
    float feedbackGain_{ 0.4f };

    std::vector<float> delayBufferL_;
    std::vector<float> delayBufferR_;
    size_t writeIndex_{ 0 };
    size_t maxDelaySamples_{ 61740 }; // 1.4s at 44.1kHz

    DSPUtils::LinearSmoother<float> smoothedDelaySamplesL_;
    DSPUtils::LinearSmoother<float> smoothedDelaySamplesR_;

    // 6.0 kHz 1-pole damping filter states
    float dampingStateL_{ 0.0f };
    float dampingStateR_{ 0.0f };

    float readInterpolated(const std::vector<float>& buf, float readPos) const noexcept;
};

} // namespace ABDMS2000
