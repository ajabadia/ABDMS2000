#pragma once
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace ABDMS2000 {

namespace DSPUtils {

constexpr float PI = 3.14159265358979323846f;
constexpr float TWO_PI = 6.28318530717958647692f;
constexpr float HALF_PI = 1.57079632679489661923f;

inline float clamp(float value, float minVal, float maxVal) noexcept
{
    return std::max(minVal, std::min(maxVal, value));
}

inline float midiNoteToFrequency(float midiNote) noexcept
{
    return 440.0f * std::pow(2.0f, (midiNote - 69.0f) / 12.0f);
}

inline float semitonesAndCentsToRatio(float semitones, float cents) noexcept
{
    return std::pow(2.0f, (semitones + (cents / 100.0f)) / 12.0f);
}

inline float convertSysExToCutoffHz(float normalized0to1) noexcept
{
    const float minHz = 20.0f;
    const float maxHz = 20000.0f;
    return minHz * std::pow(maxHz / minHz, clamp(normalized0to1, 0.0f, 1.0f));
}

inline float decibelsToLinear(float dB) noexcept
{
    return std::pow(10.0f, dB * 0.05f);
}

inline float linearToDecibels(float linear) noexcept
{
    return (linear > 0.00001f) ? (20.0f * std::log10(linear)) : -100.0f;
}

// Analog saturation soft clipper (MS2000 style hyperbolic tangent)
inline float softClip(float x) noexcept
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    return std::tanh(x);
}

// Asymmetric distortion for MS2000 Amp Distortion
inline float ampDistortion(float x, float drive) noexcept
{
    float in = x * (1.0f + drive * 3.0f);
    // Asymmetric clipping curve
    if (in > 0.0f)
        return std::tanh(in);
    else
        return std::tanh(in * 1.2f) * 0.833f;
}

} // namespace DSPUtils

} // namespace ABDMS2000
