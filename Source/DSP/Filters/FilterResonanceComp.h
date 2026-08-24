#pragma once

namespace ABDMS2000 {

/**
 * @brief Handles analog resonance compensation and self-oscillation characteristics.
 */
class FilterResonanceComp {
public:
    static float computeGainCompensation(float resonance0to1) noexcept;
    static float computeEffectiveFeedback(float resonance0to1) noexcept;
};

} // namespace ABDMS2000
