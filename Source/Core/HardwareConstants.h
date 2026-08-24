#pragma once
#include <cstdint>

namespace ABDMS2000 {

namespace Constants {
    inline constexpr int kMaxPolyphony = 4;
    inline constexpr int kMaxPolyphonyAdvanced = 32;
    inline constexpr int kNumProgramsPerBank = 16;
    inline constexpr int kNumBanks = 8;
    inline constexpr int kTotalPrograms = 128;

    inline constexpr int kDwgWaveCount = 64;
    inline constexpr int kDwgTableSize = 2048;
    inline constexpr int kDwgTotalSamples = kDwgWaveCount * kDwgTableSize; // 131,072

    inline constexpr int kNumModSeqTracks = 3;
    inline constexpr int kNumModSeqSteps = 16;
    inline constexpr int kNumModSeqStepsAdvanced = 64;

    inline constexpr int kNumVocoderBands = 16;
    inline constexpr int kNumVirtualPatchSlots = 4;
    inline constexpr int kNumVirtualPatchSlotsAdvanced = 8;

    // SysEx Korg Constants
    inline constexpr uint8_t kKorgManufacturerId = 0x42;
    inline constexpr uint8_t kMS2000ModelId = 0x58;
    inline constexpr uint8_t kMicroKorgModelId = 0x5E;

    // Default Pitch Ranges
    inline constexpr float kMinFilterCutoffHz = 20.0f;
    inline constexpr float kMaxFilterCutoffHz = 20000.0f;
    inline constexpr float kMinLfoFreqHz = 0.01f;
    inline constexpr float kMaxLfoFreqHz = 20.0f;
}

} // namespace ABDMS2000
