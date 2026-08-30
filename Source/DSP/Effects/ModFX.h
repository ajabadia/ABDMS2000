#pragma once
#include <vector>
#include <array>

namespace ABDMS2000 {

enum class ModFXType {
    ChorusFlanger = 0,
    Ensemble,
    Phaser
};

/**
 * @brief Modulation FX Block for MS2000.
 * Incorporates exact reverse-engineered specs:
 * - Chorus/Flanger: 8.5ms base offset, +/-4ms excursion, inverted feedback when > 100
 * - Ensemble: Solina 3-delay line matrix per channel with 120-degree 3-phase dual LFOs (0.5Hz / 6.0Hz) & asymmetric counter-phase output
 * - Phaser: 4-stage allpass with 800Hz..4.5kHz notch sweep
 */
class ModFX {
public:
    ModFX() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool isEnabled() const noexcept { return enabled_; }

    void setType(ModFXType type) noexcept { type_ = type; }
    void setSpeed(float speed0to1) noexcept;     // Maps 0.02Hz to 15.0Hz
    void setDepth(float depth0to1) noexcept;
    void setFeedback(float feedback0to127) noexcept;

    void process(float& leftSample, float& rightSample) noexcept;

private:
    double sampleRate_{ 44100.0 };
    bool enabled_{ false };
    ModFXType type_{ ModFXType::ChorusFlanger };

    float speed_{ 0.3f };
    float depth_{ 0.5f };
    float rawFeedback_{ 0.0f }; // 0..127

    // LFO phases
    double lfoPhase_{ 0.0 };
    double lfoIncrement_{ 0.0 };

    // Ensemble dual LFO phases
    double ensemblePhaseSlow_{ 0.0 };
    double ensemblePhaseFast_{ 0.0 };

    // Delay lines: 3 per channel for Ensemble, 1 per channel for Chorus/Flanger
    std::array<std::vector<float>, 3> ensembleLinesL_;
    std::array<std::vector<float>, 3> ensembleLinesR_;
    size_t ensembleWriteIndex_{ 0 };
    size_t ensembleMaxSamples_{ 4410 };

    std::vector<float> chorusBufferL_;
    std::vector<float> chorusBufferR_;
    size_t chorusWriteIndex_{ 0 };
    size_t chorusMaxSamples_{ 4410 };

    // 4-stage Phaser allpass filter states (Direct Form I)
    struct AllPassState {
        float x1{ 0.0f };
        float y1{ 0.0f };
        void clear() noexcept { x1 = y1 = 0.0f; }
    };
    std::array<AllPassState, 4> phaserAPFL_{};
    std::array<AllPassState, 4> phaserAPFR_{};
    float phaserFeedbackL_{ 0.0f };
    float phaserFeedbackR_{ 0.0f };

    float readInterpolated(const std::vector<float>& buf, float readPos, size_t bufLen) const noexcept;

    void processChorusFlanger(float& left, float& right) noexcept;
    void processEnsemble(float& left, float& right) noexcept;
    void processPhaser(float& left, float& right) noexcept;
};


} // namespace ABDMS2000
