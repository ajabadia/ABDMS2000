#pragma once
#include <array>

namespace ABDMS2000 {

enum class PatchSource {
    EG1 = 0,
    EG2,
    LFO1,
    LFO2,
    Velocity,
    KbdTrack,
    PitchBend,
    ModWheel
};

enum class PatchDestination {
    Pitch = 0,
    OSC2Pitch,
    OSC1Ctrl1,
    NoiseLevel,
    Cutoff,
    Amp,
    Pan,
    LFO2Freq
};

struct PatchSlot {
    PatchSource source{ PatchSource::EG1 };
    PatchDestination destination{ PatchDestination::Cutoff };
    float intensity{ 0.0f }; // -1.0 to +1.0 (Mapped from -63..+63)
};

struct PatchModulationSources {
    float eg1{ 0.0f };
    float eg2{ 0.0f };
    float lfo1{ 0.0f };
    float lfo2{ 0.0f };
    float velocity{ 0.0f };
    float kbdTrack{ 0.0f };
    float pitchBend{ 0.0f };
    float modWheel{ 0.0f };
};

struct PatchModulationOutputs {
    float pitchMod{ 0.0f };
    float osc2PitchMod{ 0.0f };
    float osc1Ctrl1Mod{ 0.0f };
    float noiseLevelMod{ 0.0f };
    float cutoffMod{ 0.0f };
    float ampMod{ 0.0f };
    float panMod{ 0.0f };
    float lfo2FreqMod{ 0.0f };
};

/**
 * @brief 4-Slot Virtual Patch Modulation Matrix.
 */
class VirtualPatchMatrix {
public:
    static constexpr size_t NUM_SLOTS = 4;

    VirtualPatchMatrix() = default;

    void setSlot(size_t slotIndex, PatchSource src, PatchDestination dst, float intensityBipolar) noexcept;
    PatchSlot& getSlot(size_t slotIndex) noexcept { return slots_[slotIndex % NUM_SLOTS]; }
    const PatchSlot& getSlot(size_t slotIndex) const noexcept { return slots_[slotIndex % NUM_SLOTS]; }

    PatchModulationOutputs evaluate(const PatchModulationSources& sources) const noexcept;

private:
    std::array<PatchSlot, NUM_SLOTS> slots_{};

    float getSourceValue(PatchSource src, const PatchModulationSources& s) const noexcept;
};

} // namespace ABDMS2000
