#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include "../Common/DSPUtils.h"
#include "DspCore/DspCore.h"
#include "HardwareConstants.h"

namespace ABDMS2000
{

/**
 * @brief MS2KChorusFlanger - Efecto híbrido continuo Chorus/Flanger del Korg MS2000 (DSP56362).
 *
 * Características:
 * - Transición continua gobernada por Feedback: 0 es un Chorus cálido (7.5ms base, +/-3.5ms excursión),
 *   y a 127 se convierte en un Flanger resonante tipo "jet" (1.8ms base, +/-1.5ms excursión, feedback 0.94).
 * - LFO en contrafase para apertura estéreo amplia (L = +lfo, R = -lfo).
 * - Buffer estático circular en array de 4096 muestras (100% Real-Time Safe, cero heap).
 * - Interpolación cúbica Hermite de 4 puntos (sin amortiguación de frecuencias altas ni escalonamiento).
 * - Determinismo de fase con abd::dsp::sin.
 */
class MS2KChorusFlanger
{
  public:
    static constexpr size_t kBufferSize = 4096;
    static constexpr size_t kBufferMask = kBufferSize - 1;

    MS2KChorusFlanger() noexcept
    {
        reset();
    }

    void prepare(double sampleRate) noexcept
    {
        sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
        updateLFOIncrement();
        reset();
    }

    void reset() noexcept
    {
        bufferL_.fill(0.0f);
        bufferR_.fill(0.0f);
        writeIndex_ = 0;
        lfoPhase_ = 0.0f;
    }

    void setSpeed(float speed0to1) noexcept
    {
        speed_ = DSPUtils::clamp(speed0to1, 0.0f, 1.0f);
        updateLFOIncrement();
    }

    void setDepth(float depth0to1) noexcept
    {
        depth_ = DSPUtils::clamp(depth0to1, 0.0f, 1.0f);
    }

    void setFeedback(float feedback0to127) noexcept
    {
        rawFeedback_ = DSPUtils::clamp(feedback0to127, 0.0f, Constants::kMidiMaxVal);
    }

    inline void processSample(float &left, float &right) noexcept
    {
        // 1. Avance del LFO determinista
        const float lfo = abd::dsp::sin(lfoPhase_);
        lfoPhase_ += lfoInc_;
        if (lfoPhase_ >= Constants::kTwoPi)
            lfoPhase_ -= Constants::kTwoPi;

        // 2. Retardo base y excursión gobernados por el lazo de feedback híbrido
        const float fbNorm = rawFeedback_ / Constants::kMidiMaxVal;
        const float baseDelaySec = DSPUtils::kChorusBaseDelaySec - (DSPUtils::kFlangerDelayRangeSec * fbNorm);
        const float excursionSec =
            (DSPUtils::kChorusExcursionSec - (DSPUtils::kFlangerExcursionRangeSec * fbNorm)) * depth_;

        const float baseDelaySamples = baseDelaySec * static_cast<float>(sampleRate_);
        const float modExcursionSamples = excursionSec * static_cast<float>(sampleRate_);

        const float delayL = baseDelaySamples + (lfo * modExcursionSamples);
        const float delayR = baseDelaySamples - (lfo * modExcursionSamples);  // Desfase 180° estéreo

        // 3. Lectura con interpolación cúbica Hermite
        const float wetL = readHermite(bufferL_, delayL);
        const float wetR = readHermite(bufferR_, delayR);

        // 4. Realimentación en la línea de retardo (hasta 0.94 en Flanger extremo)
        const float fbGain = fbNorm * Constants::kFlangerMaxFeedback;
        bufferL_[writeIndex_] = left + (wetL * fbGain);
        bufferR_[writeIndex_] = right + (wetR * fbGain);
        writeIndex_ = (writeIndex_ + 1) & kBufferMask;

        // 5. Mezcla Wet/Dry dinámica
        const float wetGain = depth_ * 0.75f;
        const float dryGain = std::max(0.35f, 1.0f - (depth_ * 0.40f));
        left = (left * dryGain) + (wetL * wetGain);
        right = (right * dryGain) + (wetR * wetGain);
    }

  private:
    void updateLFOIncrement() noexcept
    {
        // Rango de velocidad MS2000: 0.02 Hz a 15.0 Hz en escala logarítmica
        const float rateHz = 0.02f * abd::dsp::pow(DSPUtils::kSpeedLogScaleMax, speed_);
        lfoInc_ = static_cast<float>(6.2831853f * rateHz / sampleRate_);
    }

    static inline float hermite(float frac, float y0, float y1, float y2, float y3) noexcept
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    inline float readHermite(const std::array<float, kBufferSize> &buf, float delaySamples) const noexcept
    {
        float readPos = static_cast<float>(writeIndex_) - delaySamples;
        if (readPos < 0.0f)
            readPos += static_cast<float>(kBufferSize);

        const int i0 = static_cast<int>(readPos) & kBufferMask;
        const float frac = readPos - static_cast<float>(static_cast<int>(readPos));
        const int i1 = (i0 + 1) & kBufferMask;
        const int i2 = (i0 + 2) & kBufferMask;
        const int i3 = (i0 + 3) & kBufferMask;

        return hermite(frac, buf[i0], buf[i1], buf[i2], buf[i3]);
    }

    double sampleRate_ {44100.0};
    float speed_ {0.3f};
    float depth_ {0.5f};
    float rawFeedback_ {0.0f};

    float lfoPhase_ {0.0f};
    float lfoInc_ {0.0f};

    std::array<float, kBufferSize> bufferL_ {};
    std::array<float, kBufferSize> bufferR_ {};
    size_t writeIndex_ {0};
};

}  // namespace ABDMS2000
