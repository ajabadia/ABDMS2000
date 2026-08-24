#pragma once
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

namespace EnvelopeCurves {

/**
 * @brief Maps normalized [0.0, 1.0] knob value to real-world seconds based on MS2000 measured curves.
 */
inline float getAttackTimeSeconds(float norm0to1) noexcept
{
    float norm = std::max(0.0f, std::min(1.0f, norm0to1));
    const float minTime = 0.001f; // 1 ms
    const float maxTime = 11.0f;  // 11 seconds
    float curved = std::pow(norm, 2.5f);
    return minTime + (maxTime - minTime) * curved;
}

inline float getDecayReleaseTimeSeconds(float norm0to1) noexcept
{
    float norm = std::max(0.0f, std::min(1.0f, norm0to1));
    const float minTime = 0.002f; // 2 ms
    const float maxTime = 20.0f;  // 20 seconds
    float curved = std::pow(norm, 2.5f);
    return minTime + (maxTime - minTime) * curved;
}

/**
 * @brief Computes exponential per-sample multiplier for capacitor decay discharge.
 */
inline double getDecayMultiplier(double timeSeconds, double sampleRate) noexcept
{
    if (timeSeconds <= 0.0001 || sampleRate <= 1000.0) return 0.0;
    // -4.605 corresponds to reaching ~1% (-40dB) at timeSeconds
    return std::exp(-4.60517 / (timeSeconds * sampleRate));
}

} // namespace EnvelopeCurves

} // namespace ABDMS2000
