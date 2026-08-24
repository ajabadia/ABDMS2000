#pragma once
#include "VAOscillator.h"

namespace ABDMS2000 {

enum class OSC2ModulationMode {
    Off = 0,
    RingMod,
    Sync,
    RingSync
};

/**
 * @brief Handles cross-oscillator interactions: Ring Modulation, Hard Sync, and Ring+Sync.
 */
class OSC2Modulator {
public:
    OSC2Modulator() = default;

    static float process(float osc1Sample, float osc2Sample, OSC2ModulationMode mode) noexcept;
    static void checkHardSync(VAOscillator& master, VAOscillator& slave, float prevMasterPhase) noexcept;
};

} // namespace ABDMS2000
