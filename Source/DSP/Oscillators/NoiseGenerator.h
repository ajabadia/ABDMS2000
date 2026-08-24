#pragma once
#include <cstdint>

namespace ABDMS2000 {

/**
 * @brief Real-Time Safe White and Pink Noise Generator (LCG algorithm).
 */
class NoiseGenerator {
public:
    NoiseGenerator() = default;

    void reset(uint32_t seed = 0x12345678) noexcept;
    float getWhiteNoise() noexcept;
    float getPinkNoise() noexcept;

private:
    uint32_t state_{ 0x12345678 };

    // Pink noise filter state (Paul Kellet's filter)
    float b0_{ 0.0f };
    float b1_{ 0.0f };
    float b2_{ 0.0f };
    float b3_{ 0.0f };
    float b4_{ 0.0f };
    float b5_{ 0.0f };
    float b6_{ 0.0f };
};

} // namespace ABDMS2000
