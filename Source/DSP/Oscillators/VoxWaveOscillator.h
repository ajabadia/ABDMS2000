#pragma once
#include "../Common/DSPUtils.h"

namespace ABDMS2000 {

/**
 * @brief MS2000 VoxWave Oscillator — formant-based vocal wave synthesis.
 *
 * The Korg MS2000 VoxWave simulates human vocal sounds by passing a pulse
 * train through 5 resonant bandpass filters tuned to vowel formant frequencies.
 * Control 1 morphs between vowel shapes (A-E-I-O-U).
 */
class VoxWaveOscillator {
public:
    VoxWaveOscillator() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    // Set base pitch frequency (from MIDI note)
    void setFrequency(float freqHz) noexcept;

    // Vowel morph: 0.0 = A, 0.25 = E, 0.5 = I, 0.75 = O, 1.0 = U
    void setVowel(float vowel0to1) noexcept;

    float getNextSample() noexcept;

private:
    double sampleRate_{ 44100.0 };
    float frequency_{ 440.0f };
    float phase_{ 0.0f };
    float phaseIncrement_{ 0.01f };
    float vowel_{ 0.0f };

    // Five formant bandpass filters (Direct Form II Transposed)
    struct FormantBPF {
        float b0{ 0.0f }, b1{ 0.0f }, b2{ 0.0f }, a1{ 0.0f }, a2{ 0.0f };
        float s1{ 0.0f }, s2{ 0.0f };

        void setFreq(float freqHz, float q, double sr) noexcept;
        void reset() noexcept { s1 = s2 = 0.0f; }
        float process(float in) noexcept;
    };

    FormantBPF formants_[5]{};

    void updateVowelCoefficients() noexcept;
};

/**
 * @brief Formant frequency data for the 5 classic vowel sounds.
 * Frequencies in Hz: F1, F2, F3, F4, F5 for each vowel.
 * Data from MS2000 reverse-engineering measurements.
 */
struct VowelFormants {
    static constexpr float A[5] = { 730.0f, 1090.0f, 2440.0f, 3300.0f, 3900.0f };
    static constexpr float E[5] = { 390.0f, 1950.0f, 2550.0f, 3000.0f, 3500.0f };
    static constexpr float I[5] = { 270.0f, 2200.0f, 2900.0f, 3300.0f, 3700.0f };
    static constexpr float O[5] = { 450.0f, 750.0f, 2450.0f, 3100.0f, 3800.0f };
    static constexpr float U[5] = { 300.0f, 850.0f, 2250.0f, 2800.0f, 3300.0f };
};

} // namespace ABDMS2000