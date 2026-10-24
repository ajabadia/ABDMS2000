#include "FilterResonanceComp.h"
#include <algorithm>
#include <cmath>

namespace ABDMS2000
{

float FilterResonanceComp::computeGainCompensation(float resonance0to1) noexcept
{
    float res = std::max(0.0f, std::min(1.0f, resonance0to1));
    // Gentle headroom trim as resonance increases without starving SVF passband low-end
    return 1.0f - (res * 0.18f);
}

float FilterResonanceComp::computeEffectiveFeedback(float resonance0to1) noexcept
{
    float res = std::max(0.0f, std::min(1.0f, resonance0to1));
    // Hardware enters pure self-oscillation above ~105 (0.82)
    if (res > 0.82f)
    {
        float excess = (res - 0.82f) / 0.18f;
        return 3.92f + (excess * 0.25f);  // Resonance > 4.0 causes pure self-oscillation
    }
    return res * 3.90f;
}

}  // namespace ABDMS2000
