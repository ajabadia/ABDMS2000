#pragma once
#include <cstdint>

namespace ABDMS2000
{

/**
 * @brief Hardware constants for ABDMS2000 and microKORG synthesizers.
 * These constants represent the actual hardware parameters and limits
 * as documented in the Korg MS2000 and microKORG manuals.
 */
namespace Constants
{

// Polyphony
inline constexpr int kMaxPolyphony = 4;           // Standard MS2000 polyphony
inline constexpr int kMaxPolyphonyAdvanced = 32;  // Advanced mode polyphony
inline constexpr int kNumProgramsPerBank = 16;    // Standard banks
inline constexpr int kNumBanks = 8;               // Total banks
inline constexpr int kTotalPrograms = 128;        // Total programs (16 × 8)

// DWG (Digital Waveform Generator) system
inline constexpr int kDwgWaveCount = 512;                               // Number of waveforms
inline constexpr int kDwgTableSize = 2048;                              // Samples per waveform
inline constexpr int kDwgTotalSamples = kDwgWaveCount * kDwgTableSize;  // 1,048,576 (~4MB)

// Modulation sequencer
inline constexpr int kNumModSeqTracks = 3;          // Tracks per timbre
inline constexpr int kNumModSeqSteps = 16;          // Steps per track (standard)
inline constexpr int kNumModSeqStepsAdvanced = 64;  // Extended steps

// Vocoder and patch system
inline constexpr int kNumVocoderBands = 16;              // Vocoder bands
inline constexpr int kNumVirtualPatchSlots = 4;          // Virtual patch slots
inline constexpr int kNumVirtualPatchSlotsAdvanced = 8;  // Extended slots

// Korg SysEx constants
inline constexpr uint8_t kKorgManufacturerId = 0x42;  // Korg manufacturer ID
inline constexpr uint8_t kMS2000ModelId = 0x58;       // MS2000 model ID
inline constexpr uint8_t kMicroKorgModelId = 0x5E;    // microKORG model ID

// Pitch ranges
inline constexpr float kMinFilterCutoffHz = 20.0f;     // Minimum filter cutoff
inline constexpr float kMaxFilterCutoffHz = 20000.0f;  // Maximum filter cutoff
inline constexpr float kMinLfoFreqHz = 0.01f;          // Minimum LFO frequency
inline constexpr float kMaxLfoFreqHz = 20.0f;          // Maximum LFO frequency

// Modulation range constants
inline constexpr float kMinPitchBend = -8192.0f;  // Pitch bend range minimum
inline constexpr float kMaxPitchBend = 8191.0f;   // Pitch bend range maximum
inline constexpr float kMinModWheel = 0.0f;       // Mod wheel minimum
inline constexpr float kMaxModWheel = 1.0f;       // Mod wheel maximum
}  // namespace Constants

}  // namespace ABDMS2000
