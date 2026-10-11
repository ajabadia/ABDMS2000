#pragma once
#include "DspEffects/DspSolinaEnsemble.h"
#include "DspEffects/Phaser4.h"
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
 * @brief Modulation FX Block for MS2000.
 *
 * Bloque coordinador de modulación con cero memoria dinámica (100% Real-Time Safe):
 * - Chorus/Flanger: MS2KChorusFlanger (exclusivo MS2000 con transición continua y buffer estático).
 * - Ensemble: abd::dsp::DspSolinaEnsemble (modo DualLFO con LFO dual 0.5Hz/6.0Hz y matriz contrafase).
 * - Phaser: abd::dsp::Phaser4<4> (4 etapas allpass con barrido logarítmico determinista).
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
    abd::dsp::DspSolinaEnsemble ensemble_;
    abd::dsp::Phaser4<4> phaser_;
};

}  // namespace ABDMS2000

