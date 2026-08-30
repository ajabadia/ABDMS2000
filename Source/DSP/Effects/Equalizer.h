#pragma once
#include <cstddef>

namespace ABDMS2000 {

/**
 * @brief 2-Band Cascaded Shelving Equalizer based on reverse-engineered Korg MS2000 DSP specs.
 * Uses 2nd order Butterworth Shelves (Q = 0.7071) with exact hardware frequency lookup tables:
 * - Low Freq:  160Hz, 250Hz (default), 400Hz, 600Hz
 * - High Freq: 4.0kHz, 6.0kHz, 8.0kHz (default), 12.0kHz
 * - Gain Range: -12.0dB to +12.0dB (0..127 with 64 = 0dB)
 */
class Equalizer {
public:
    Equalizer() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setLowFreqIndex(int index0to3) noexcept;
    void setLowGainDB(float gainDB) noexcept;
    void setHighFreqIndex(int index0to3) noexcept;
    void setHighGainDB(float gainDB) noexcept;

    void process(float& leftSample, float& rightSample) noexcept;

private:
    double sampleRate_{ 44100.0 };

    int lowFreqIndex_{ 1 };   // 250 Hz default
    float lowGainDB_{ 0.0f }; // 0 dB
    int highFreqIndex_{ 2 };  // 8.0 kHz default
    float highGainDB_{ 0.0f };// 0 dB

    // Direct Form II Transposed Low Shelf Biquad coefficients & states
    float b0_L_{ 1.0f }, b1_L_{ 0.0f }, b2_L_{ 0.0f }, a1_L_{ 0.0f }, a2_L_{ 0.0f };
    float s1_LL_{ 0.0f }, s2_LL_{ 0.0f }, s1_LR_{ 0.0f }, s2_LR_{ 0.0f };

    // High Shelf Biquad coefficients & states
    float b0_H_{ 1.0f }, b1_H_{ 0.0f }, b2_H_{ 0.0f }, a1_H_{ 0.0f }, a2_H_{ 0.0f };
    float s1_HL_{ 0.0f }, s2_HL_{ 0.0f }, s1_HR_{ 0.0f }, s2_HR_{ 0.0f };

    void updateLowShelfCoeffs() noexcept;
    void updateHighShelfCoeffs() noexcept;
};

} // namespace ABDMS2000
