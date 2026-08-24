#include "NoiseGenerator.h"

namespace ABDMS2000 {

void NoiseGenerator::reset(uint32_t seed) noexcept
{
    state_ = (seed != 0) ? seed : 0x12345678;
    b0_ = b1_ = b2_ = b3_ = b4_ = b5_ = b6_ = 0.0f;
}

float NoiseGenerator::getWhiteNoise() noexcept
{
    // 32-bit Linear Congruential Generator (Numerical Recipes)
    state_ = state_ * 1664525u + 1013904223u;
    // Map to [-1.0, 1.0]
    return (static_cast<float>(state_) * (2.0f / 4294967295.0f)) - 1.0f;
}

float NoiseGenerator::getPinkNoise() noexcept
{
    float white = getWhiteNoise();

    b0_ = 0.99886f * b0_ + white * 0.0555179f;
    b1_ = 0.99332f * b1_ + white * 0.0750759f;
    b2_ = 0.96900f * b2_ + white * 0.1538520f;
    b3_ = 0.86650f * b3_ + white * 0.3104856f;
    b4_ = 0.55000f * b4_ + white * 0.5329522f;
    b5_ = -0.7616f * b5_ - white * 0.0168980f;

    float pink = b0_ + b1_ + b2_ + b3_ + b4_ + b5_ + b6_ + white * 0.5362f;
    b6_ = white * 0.115926f;

    return pink * 0.11f; // Normalize roughly to [-1.0, 1.0]
}

} // namespace ABDMS2000
