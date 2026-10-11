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
inline constexpr uint8_t kMicroKorgModelId = 0x71;    // microKORG model ID (0x71 native, 0x58 legacy)

// Modulation range constants
inline constexpr float kMinPitchBend = -8192.0f;  // Pitch bend range minimum
inline constexpr float kMaxPitchBend = 8191.0f;   // Pitch bend range maximum
inline constexpr float kMinModWheel = 0.0f;       // Mod wheel minimum
inline constexpr float kMaxModWheel = 1.0f;       // Mod wheel maximum

// Voice pitch & keyboard tracking constants
inline constexpr float kMidiMiddleC = 60.0f;                   // MIDI note number for Middle C (C4)
inline constexpr float kSemitonesPerOctave = 12.0f;            // Semitones per octave
inline constexpr float kCentsPerSemitone = 100.0f;             // Cents per semitone
inline constexpr float kPitchBendDefaultSemitones = 2.0f;      // Standard pitch bend range (±2 semitones)
inline constexpr float kVirtualPatchMaxPitchSemitones = 24.0f; // Virtual patch pitch span (±24 semitones = ±2 octaves)

// Filter & VCA modulation limits (MS2000 hardware specification & SysEx byte 23)
inline constexpr float kFilterModMaxOctaves = 5.0f;  // Filter EG/Velo/Patch intensity max span (±5 octaves)
inline constexpr float kLfoModMaxOctaves = 4.0f;     // LFO frequency cross-modulation max span (±4 octaves)
inline constexpr float kKbdTrackSpanSemitones = 64.0f; // Keyboard tracking span normalization factor (±64 semitones)
inline constexpr float kPanHalfWidth = 0.5f;         // Panning law half-width (0.5 for linear pan gain)

// Effects - Delay (Motorola DSP56362 microcode)
inline constexpr float kDelayBufferHeadroomRatio = 1.5f;   // Buffer headroom ratio
inline constexpr float kDelayGlideTimeSec = 0.05f;         // Tape drag / pitch glide filter time (50 ms)
inline constexpr float kDelayLeftRightRatio = 0.75f;       // Asymmetric ratio for LeftRight delay mode
inline constexpr float kDelayMaxFeedback = 0.95f;          // Maximum feedback gain before runaway
inline constexpr float kDelayDampingCoeff = 0.35f;         // 1-pole damping filter coefficient (~6.0 kHz)
inline constexpr float kDelayWetMixScale = 0.88f;          // Wet mix level scaling factor

// Effects - Vocoder 16-Band
inline constexpr float kVocoderSibilanceCutoffHz = 8000.0f;       // Sibilance detector Butterworth HPF cutoff (8.0 kHz)
inline constexpr float kVocoderBandQ = 6.5f;                      // Constant Q factor for 16 analysis/synthesis BPFs
inline constexpr float kVocoderSibilanceReleaseSec = 0.010f;      // Fast release for sibilance follower (10 ms)
inline constexpr float kVocoderGateSenseMinReleaseSec = 0.005f;   // Gate sense follower min release (5 ms)
inline constexpr float kVocoderGateSenseReleaseRangeSec = 0.195f; // Gate sense follower release range (0.005s .. 0.200s)

// Effects - ModFX / Chorus / Flanger
inline constexpr float kMidiMaxVal = 127.0f;               // Standard MIDI 7-bit max value
inline constexpr float kFlangerMaxFeedback = 0.94f;        // Maximum resonance feedback for jet flanger
inline constexpr float kTwoPi = 6.28318530717958647692f;   // Phase cycle 2*PI
}  // namespace Constants

}  // namespace ABDMS2000

