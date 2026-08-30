#pragma once
#include "DWGSTables.h"

namespace ABDMS2000 {

/**
 * @brief Real-time Single-Cycle DWGS Wavetable Player with linear phase interpolation.
 */
class DWGSOscillator {
public:
    DWGSOscillator() = default;

    void prepare(double sampleRate) noexcept;
    void reset(float initialPhase = 0.0f) noexcept;

    void setFrequency(float frequencyHz) noexcept;
    void setWaveIndex(int index) noexcept;

    float getNextSample() noexcept;

private:
    double sampleRate_{ 44100.0 };
    float frequency_{ 440.0f };
    double phase_{ 0.0 };
    double phaseIncrement_{ 0.0 };
    int currentWaveIndex_{ 0 };

    const float* tableData_{ nullptr };
};

} // namespace ABDMS2000
