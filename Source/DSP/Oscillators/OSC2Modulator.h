#pragma once
#include "VAOscillator.h"

namespace ABDMS2000
{

enum class OSC2ModulationMode
{
    Off = 0,
    RingMod,               // Multiplicative ring modulation: OSC2 branch outputs OSC1 * OSC2
    Sync,                  // Hard sync: OSC2 phase resets at OSC1 cycle wrap
    CrossMod,              // Mode 3: Ring+Sync on MS2000 hardware (also supports FM/CrossMod)
    RingSync = CrossMod    // Alias matching hardware SysEx bits 4,5 = 3 (Ring+Sync)
};

/**
 * @brief Handles cross-oscillator interactions: Ring Modulation, Hard Sync, and Ring+Sync.
 */
class OSC2Modulator
{
  public:
    OSC2Modulator() = default;

    static float process(float osc1Sample, float osc2Sample, OSC2ModulationMode mode) noexcept;
    static void checkHardSync(VAOscillator &master, VAOscillator &slave, float prevMasterPhase) noexcept;
};

}  // namespace ABDMS2000
