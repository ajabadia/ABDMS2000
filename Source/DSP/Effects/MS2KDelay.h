#pragma once

#include "../Common/DSPUtils.h"
#include "DspEffects/DspMS2000Delay.h"

namespace ABDMS2000
{

enum class DelayType
{
    Stereo = 0,
    Cross,
    LeftRight
};

/**
 * @brief MS2KDelay - Motor de retardo Motorola DSP56362 del Korg MS2000 (P:$0FE9–$105C).
 *
 * Envoltorio sobre `abd::dsp::DspMS2000Delay` (agnóstico al sample rate):
 * - Suavizado exponencial de 1 polo del tiempo de retardo (P:$0FED–$0FF4).
 * - Filtro paso-bajos de 1 polo en el lazo de realimentación (P:$100C–$1012).
 * - Tres topologías: Stereo, Cross (L<->R) y LeftRight (Ping-Pong L->R).
 */
class MS2KDelay
{
  public:
    MS2KDelay() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;
        engine_.setEnabled(enabled);
    }
    bool isEnabled() const noexcept
    {
        return enabled_;
    }

    void setType(DelayType type) noexcept
    {
        type_ = type;
        engine_.setMode(static_cast<abd::dsp::DspMS2000Delay::Mode>(static_cast<int>(type)));
    }
    void setTimeSeconds(float timeSec) noexcept;
    void setDepth(float wetMix0to1) noexcept;
    void setFeedback(float feedbackGain0to1) noexcept;

    void process(float &leftSample, float &rightSample) noexcept;

  private:
    double sampleRate_ {44100.0};
    bool enabled_ {false};
    DelayType type_ {DelayType::Stereo};

    float timeSeconds_ {0.35f};
    float wetMix_ {0.4f};
    float feedbackGain_ {0.4f};

    abd::dsp::DspMS2000Delay engine_;
};

}  // namespace ABDMS2000
