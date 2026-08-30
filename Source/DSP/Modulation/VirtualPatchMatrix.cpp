#include "VirtualPatchMatrix.h"
#include "../Common/DSPUtils.h"

namespace ABDMS2000 {

void VirtualPatchMatrix::setSlot(size_t slotIndex, PatchSource src, PatchDestination dst, float intensityBipolar) noexcept
{
    size_t idx = slotIndex % NUM_SLOTS;
    slots_[idx].source = src;
    slots_[idx].destination = dst;
    slots_[idx].intensity = DSPUtils::clamp(intensityBipolar, -1.0f, 1.0f);
}

float VirtualPatchMatrix::getSourceValue(PatchSource src, const PatchModulationSources& s) const noexcept
{
    switch (src)
    {
        case PatchSource::EG1:       return s.eg1;
        case PatchSource::EG2:       return s.eg2;
        case PatchSource::LFO1:      return s.lfo1;
        case PatchSource::LFO2:      return s.lfo2;
        case PatchSource::Velocity:  return s.velocity;
        case PatchSource::KbdTrack:  return s.kbdTrack;
        case PatchSource::PitchBend: return s.pitchBend;
        case PatchSource::ModWheel:  return s.modWheel;
        default:                     return 0.0f;
    }
}

PatchModulationOutputs VirtualPatchMatrix::evaluate(const PatchModulationSources& sources) const noexcept
{
    PatchModulationOutputs out{};

    for (const auto& slot : slots_)
    {
        if (std::abs(slot.intensity) < 0.0001f) continue;

        float srcVal = getSourceValue(slot.source, sources);
        float amount = srcVal * slot.intensity;

        switch (slot.destination)
        {
            case PatchDestination::Pitch:       out.pitchMod += amount; break;
            case PatchDestination::OSC2Pitch:   out.osc2PitchMod += amount; break;
            case PatchDestination::OSC1Ctrl1:   out.osc1Ctrl1Mod += amount; break;
            case PatchDestination::NoiseLevel:  out.noiseLevelMod += amount; break;
            case PatchDestination::Cutoff:      out.cutoffMod += amount; break;
            case PatchDestination::Amp:         out.ampMod += amount; break;
            case PatchDestination::Pan:         out.panMod += amount; break;
            case PatchDestination::LFO2Freq:    out.lfo2FreqMod += amount; break;
        }
    }

    return out;
}

} // namespace ABDMS2000
