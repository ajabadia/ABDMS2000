#pragma once

#include <vector>
#include "../Common/DSPUtils.h"

namespace ABDMS2000
{

enum class DelayType
{
    Stereo = 0,
    Cross,
    LeftRight
};

/**
 * @brief MS2KDelay - Motor de retardo Motorola DSP56362 del Korg MS2000.
 *
 * Características específicas del microcódigo:
 * - Tiempo máximo: 1400 ms (1.4 s).
 * - Tres topologías de ruteo:
 *     * Stereo: Líneas independientes en paralelo.
 *     * Cross: Ping-pong con realimentación invertida en el canal izquierdo (-R_fb en L, +L_fb en R).
 *     * LeftRight: Relación métrica asimétrica fija 0.75x en el canal derecho (timeR = timeL * 0.75).
 * - Filtro de amortiguación (Damping): Paso-bajos de 1-polo fijado a ~6.0 kHz en el lazo de retorno.
 * - Pitch Glide (Tape Drag): Suavizador lineal de 50 ms para simular el arrastre de cinta del DSP56362.
 * - Interpolación cúbica Hermite de 4 puntos para lectura libre de atenuación en altas frecuencias.
 * - 100% Real-Time Safe (memoria reservada únicamente en prepare()).
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
    }
    bool isEnabled() const noexcept
    {
        return enabled_;
    }

    void setType(DelayType type) noexcept
    {
        type_ = type;
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

    std::vector<float> delayBufferL_;
    std::vector<float> delayBufferR_;
    size_t writeIndex_ {0};
    size_t maxDelaySamples_ {61740};

    DSPUtils::LinearSmoother<float> smoothedDelaySamplesL_;
    DSPUtils::LinearSmoother<float> smoothedDelaySamplesR_;

    // Filtro 1-polo damping a 6.0 kHz
    float dampingStateL_ {0.0f};
    float dampingStateR_ {0.0f};

    float readHermite(const std::vector<float> &buf, float readPos) const noexcept;
};

}  // namespace ABDMS2000
