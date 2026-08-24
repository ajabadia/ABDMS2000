#include "OSC2Modulator.h"

namespace ABDMS2000 {

float OSC2Modulator::process(float osc1Sample, float osc2Sample, OSC2ModulationMode mode) noexcept
{
    switch (mode)
    {
        case OSC2ModulationMode::RingMod:
        case OSC2ModulationMode::RingSync:
            // Multiplicative Ring Modulation
            return osc1Sample * osc2Sample;

        case OSC2ModulationMode::Sync:
        case OSC2ModulationMode::Off:
        default:
            return osc1Sample;
    }
}

void OSC2Modulator::checkHardSync(VAOscillator& master, VAOscillator& slave, float prevMasterPhase) noexcept
{
    float currentMasterPhase = master.getPhase();
    // Detect phase wrap-around (master completed a cycle)
    if (currentMasterPhase < prevMasterPhase)
    {
        slave.syncPhase(currentMasterPhase);
    }
}

} // namespace ABDMS2000
