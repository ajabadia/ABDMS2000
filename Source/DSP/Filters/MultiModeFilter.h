#pragma once
#include "FilterResonanceComp.h"
#include "../Common/DSPUtils.h"

namespace ABDMS2000 {

enum class FilterType {
    LPF24 = 0, // 4-pole Lowpass
    LPF12,     // 2-pole Lowpass
    BPF12,     // 2-pole Bandpass
    HPF12      // 2-pole Highpass
};

/**
 * @brief Zero-Delay Feedback (ZDF/TPT) Multi-Mode Resonant Filter.
 * Accurately models the MS2000 / microKORG filter responses.
 */
class MultiModeFilter {
public:
    MultiModeFilter() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setType(FilterType type) noexcept;
    void setCutoff(float cutoffHz) noexcept;
    void setResonance(float resonance0to1) noexcept;

    float process(float inputSample) noexcept;

private:
    double sampleRate_{ 44100.0 };
    FilterType type_{ FilterType::LPF24 };
    float cutoffHz_{ 1000.0f };
    float resonance_{ 0.0f };

    // TPT SVF internal states for 2-pole stage 1
    float s1_1_{ 0.0f };
    float s1_2_{ 0.0f };

    // TPT SVF internal states for 2-pole stage 2 (for 4-pole 24dB LPF)
    float s2_1_{ 0.0f };
    float s2_2_{ 0.0f };

    // Cached filter coefficients
    float g_{ 0.1f };
    float R_{ 1.0f };
    float h_{ 1.0f };

    void updateCoefficients() noexcept;
};

} // namespace ABDMS2000
