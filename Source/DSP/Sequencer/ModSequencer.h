#pragma once
#include <array>
#include <cstdint>

namespace ABDMS2000 {

enum class ModSeqMotion {
    Step = 0,
    Smooth
};

enum class ModSeqMode {
    Forward = 0,
    Backward,
    Bounce,
    Random
};

enum class ModSeqDest {
    None = 0,
    Pitch,
    OSC2Pitch,
    OSC1Ctrl1,
    Cutoff,
    Amp,
    Pan,
    LFO2Freq
};

struct ModSeqTrack {
    ModSeqDest destination{ ModSeqDest::None };
    ModSeqMotion motion{ ModSeqMotion::Step };
    ModSeqMode mode{ ModSeqMode::Forward };
    int length{ 16 }; // 1 to 16
    std::array<float, 16> steps{ 0.0f }; // Normalized 0..1
};

/**
 * @brief 3-Track 16-Step Modulation Sequencer (Tracks A, B, C).
 */
class ModSequencer {
public:
    static constexpr size_t NUM_TRACKS = 3;
    static constexpr size_t NUM_STEPS = 16;

    ModSequencer() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool isEnabled() const noexcept { return enabled_; }

    void setTempoBPM(float bpm) noexcept;

    // Tempo Sync resolution: index 0..15 (MS2000 SysEx canonical)
    // 0=1/48, 3=1/16, 7=1/4, 11=1/1, 15=4/1
    void setSyncResolution(int idx) noexcept;
    static float syncResolutionToStepsPerBeat(int idx) noexcept;

    ModSeqTrack& getTrack(size_t index) noexcept { return tracks_[index % NUM_TRACKS]; }
    const ModSeqTrack& getTrack(size_t index) const noexcept { return tracks_[index % NUM_TRACKS]; }

    void triggerKeySync() noexcept;
    void advanceClock(int numSamples) noexcept;

    float getTrackOutput(size_t trackIndex) const noexcept;
    int getCurrentStepIndex(size_t trackIndex) const noexcept { return currentStep_[trackIndex % NUM_TRACKS]; }

private:
    double sampleRate_{ 44100.0 };
    bool enabled_{ false };
    float bpm_{ 120.0f };
    float syncStepsPerBeat_{ 16.0f }; // Default: 1/16 (16 steps per beat)

    double samplesPerStep_{ 5512.5 };
    double stepSampleCounter_{ 0.0 };

    std::array<ModSeqTrack, NUM_TRACKS> tracks_{};
    std::array<int, NUM_TRACKS> currentStep_{ 0, 0, 0 };
    std::array<bool, NUM_TRACKS> bounceDirection_{ true, true, true }; // true = forward, false = backward

    // Slew limiters for Smooth mode
    std::array<float, NUM_TRACKS> smoothedOutput_{ 0.0f, 0.0f, 0.0f };
    double slewMultiplier_{ 0.99 };

    uint32_t rngState_{ 0x12345678 };

    void advanceStep() noexcept;
};

} // namespace ABDMS2000
