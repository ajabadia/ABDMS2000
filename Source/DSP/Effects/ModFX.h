#pragma once
#include "DspEffects/DspMS2000Ensemble.h"
#include "DspEffects/DspMS2000Phaser.h"
#include "MS2KChorusFlanger.h"

namespace ABDMS2000
{

enum class ModFXType
{
    ChorusFlanger = 0,
    Ensemble,
    Phaser
};

/**
 * @brief Modulation FX Block for MS2000 (DSP56362 P:$0F48–$10A7).
 *
 * Bloque coordinador de modulación fiel al microcódigo del DSP56362 y agnóstico al sample rate:
 * - Chorus/Flanger: MS2KChorusFlanger / abd::dsp::DspMS2000ChorusFlanger (P:$0F48–$0F95).
 * - Ensemble: abd::dsp::DspMS2000Ensemble (P:$0F96–$0FE8, 3 fases a 120° + shimmer 20% + matriz 0.8/0.4/0.2).
 * - Phaser: abd::dsp::DspMS2000Phaser<4> (P:$105D–$10A7, 4 etapas todo-paso con LFO cuadrático y polaridad L/R opuesta).
 */
class ModFX
{
  public:
    ModFX() = default;

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

    void setType(ModFXType type) noexcept
    {
        type_ = type;
    }
    void setSpeed(float speed0to1) noexcept;  // Maps 0.02Hz to 15.0Hz
    void setDepth(float depth0to1) noexcept;
    void setFeedback(float feedback0to127) noexcept;

    void process(float &leftSample, float &rightSample) noexcept;

  private:
    double sampleRate_ {44100.0};
    bool enabled_ {false};
    ModFXType type_ {ModFXType::ChorusFlanger};

    float speed_ {0.3f};
    float depth_ {0.5f};
    float rawFeedback_ {0.0f};  // 0..127

    MS2KChorusFlanger chorusFlanger_;
    abd::dsp::DspMS2000Ensemble ensemble_;
    abd::dsp::DspMS2000Phaser<4> phaser_;
};

}  // namespace ABDMS2000

