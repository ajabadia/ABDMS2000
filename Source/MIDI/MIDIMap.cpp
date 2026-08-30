#include "MIDIMap.h"
#include "../State/ParameterRegistry.gen.h"
#include <algorithm>
#include <cmath>

namespace ABDMS2000 {

// ═══════════════════════════════════════════════════════════════
// Korg MS2000 canonical MIDI CC & NRPN mapping
// Source: Korg MS2000 MIDI Implementation Manual / midi.guide
// ═══════════════════════════════════════════════════════════════
static const std::vector<MIDICCInfo> kCanonicalMap = {
    // ── CC mappable parameters (hardware spec) ──
    // Each CC appears EXACTLY ONCE matching the Korg MS2000 spec.

    // Pitch / Portamento / Pedals
    { ParamIDs::assignablePedal,   4,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::portamentoTime,    5,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::masterVolume,      7,  -1, -1,   0.0f,   1.0f,    0.8f },
    { ParamIDs::ampPan,           10,  -1, -1, -64.0f,  63.0f,    0.0f },
    { ParamIDs::assignableSwitch, 64,  -1, -1,   0.0f,   1.0f,    0.0f },
    { ParamIDs::portamentoOn,     65,  -1, -1,   0.0f,   1.0f,    0.0f },


    // Mod FX  (HW: CC 12 = LFO Speed, CC 93 = Depth)
    { ParamIDs::modFxSpeed,       12,  -1, -1,   0.0f, 127.0f,   40.0f },
    // Delay FX (HW: CC 13 = Time, CC 94 = Depth)
    { ParamIDs::delayTime,        13,  -1, -1,   0.0f, 127.0f,   40.0f },

    // OSC1 & OSC2 Controls
    { ParamIDs::osc1Ctrl1,        14,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::osc1Ctrl2,        15,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::osc2Semitone,     16,  -1, -1, -24.0f,  24.0f,    0.0f },
    { ParamIDs::osc2Tune,         17,  -1, -1, -50.0f,  50.0f,    0.0f },

    // Mixer
    { ParamIDs::mixOsc1Level,     20,  -1, -1,   0.0f, 127.0f,  127.0f },
    { ParamIDs::mixOsc2Level,     21,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::mixNoiseLevel,    22,  -1, -1,   0.0f, 127.0f,    0.0f },

    // EG1
    { ParamIDs::eg1Attack,        23,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::eg1Decay,         24,  -1, -1,   0.0f, 127.0f,   64.0f },
    { ParamIDs::eg1Sustain,       25,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::eg1Release,       26,  -1, -1,   0.0f, 127.0f,   40.0f },

    // EG2
    { ParamIDs::eg2Sustain,       27,  -1, -1,   0.0f, 127.0f,  127.0f },

    // Filter

    { ParamIDs::filterResonance,  71,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::eg2Release,       72,  -1, -1,   0.0f, 127.0f,   20.0f },
    { ParamIDs::eg2Attack,        73,  -1, -1,   0.0f, 127.0f,    0.0f },
    { ParamIDs::filterCutoff,     74,  -1, -1,   0.0f, 127.0f,  127.0f },
    { ParamIDs::eg2Decay,         75,  -1, -1,   0.0f, 127.0f,   64.0f },

    // LFO1 Frequency
    { ParamIDs::lfo1Freq,         76,  -1, -1,   0.0f, 127.0f,   30.0f },

    // OSC Waveforms
    { ParamIDs::osc1Wave,         77,  -1, -1,   0.0f,   7.0f,    0.0f },
    { ParamIDs::osc2Wave,         78,  -1, -1,   0.0f,   2.0f,    0.0f },

    // Filter
    { ParamIDs::filterEg1Int,     79,  -1, -1, -63.0f,  63.0f,    0.0f },
    { ParamIDs::filterKeyTrack,   80,  -1, -1, -63.0f,  63.0f,    0.0f },
    { ParamIDs::ampDistortion,    81,  -1, -1,   0.0f,   1.0f,    0.0f },
    { ParamIDs::ampKeyTrack,      82,  -1, -1, -63.0f,  63.0f,    0.0f },

    // Filter Type (HW: CC 83 — but also used for Vocoder Formant in Vocoder mode)
    { ParamIDs::filterType,       83,  -1, -1,   0.0f,   3.0f,    0.0f },

    // Portamento Time (HW: CC 84 — alternate to CC 5)
    { ParamIDs::portamentoTime,   84,  -1, -1,   0.0f, 127.0f,    0.0f },

    // LFO1 Key Sync (HW: CC 85)
    { ParamIDs::lfo1KeySync,      85,  -1, -1,   0.0f,   2.0f,    0.0f },

    // LFO1 Wave (HW: CC 86)
    { ParamIDs::lfo1Wave,         86,  -1, -1,   0.0f,   3.0f,    0.0f },

    // LFO2 Frequency (HW: CC 87)
    { ParamIDs::lfo2Freq,         87,  -1, -1,   0.0f, 127.0f,   50.0f },

    // LFO2 Wave (HW: CC 88)
    { ParamIDs::lfo2Wave,         88,  -1, -1,   0.0f,   3.0f,    2.0f },

    // Arpeggiator On/Off (HW: CC 89)
    { ParamIDs::arpOn,            89,  -1, -1,   0.0f,   1.0f,    0.0f },

    // LFO2 Key Sync (HW: CC 90)
    { ParamIDs::lfo2KeySync,      90,  -1, -1,   0.0f,   2.0f,    0.0f },

    // Mod FX Depth (HW: CC 93)
    { ParamIDs::modFxDepth,       93,  -1, -1,   0.0f, 127.0f,   64.0f },

    // Delay Depth (HW: CC 94)
    { ParamIDs::delayDepth,       94,  -1, -1,   0.0f, 127.0f,   50.0f },

    // ── NRPN-only parameters ──
    // These parameters have no direct CC assignment in the hardware.
    // They are accessed via SysEx Parameter Change or NRPN.
    // The NRPN numbers below match common MS2000 editor conventions.

    { ParamIDs::osc1DwgsWave,    -1,   2,  0,   1.0f, 512.0f,    1.0f },
    { ParamIDs::osc2ModType,     -1,   2,  1,   0.0f,   3.0f,    0.0f },
    { ParamIDs::voiceMode,       -1,   2,  2,   0.0f,   2.0f,    1.0f },
    { ParamIDs::unisonDetune,    -1,   2,  3,   0.0f,  99.0f,   10.0f },
    { ParamIDs::modFxType,       -1,   2, 10,   0.0f,   2.0f,    0.0f },
    { ParamIDs::modFxFeedback,   -1,   2, 11,   0.0f, 127.0f,    0.0f },
    { ParamIDs::delayType,       -1,   2, 12,   0.0f,   2.0f,    0.0f },
    { ParamIDs::delayFeedback,   -1,   2, 13,   0.0f, 127.0f,   40.0f },
    { ParamIDs::eqLowFreq,       -1,   2, 20,   0.0f,   3.0f,    1.0f },
    { ParamIDs::eqLowGain,       -1,   2, 21,   0.0f, 127.0f,   64.0f },
    { ParamIDs::eqHighFreq,      -1,   2, 22,   0.0f,   3.0f,    2.0f },
    { ParamIDs::eqHighGain,      -1,   2, 23,   0.0f, 127.0f,   64.0f },
    { ParamIDs::arpType,         -1,   2, 30,   0.0f,   5.0f,    0.0f },
    { ParamIDs::arpRange,        -1,   2, 31,   1.0f,   4.0f,    1.0f },
    { ParamIDs::arpGate,         -1,   2, 32,   0.0f, 127.0f,  100.0f },
    { ParamIDs::arpLatch,        -1,   2, 33,   0.0f,   1.0f,    0.0f },
    { ParamIDs::modFxOn,         -1,   2, 40,   0.0f,   1.0f,    1.0f },
    { ParamIDs::delayOn,         -1,   2, 41,   0.0f,   1.0f,    1.0f },

    // Virtual Patch 1-4
    { ParamIDs::patch1Source,      -1,   2, 14,   0.0f,   7.0f,    0.0f },
    { ParamIDs::patch1Destination, -1,   2, 15,   0.0f,   7.0f,    4.0f },
    { ParamIDs::patch1Intensity,   -1,   2, 16, -63.0f,  63.0f,    0.0f },
    { ParamIDs::patch2Source,      -1,   2, 17,   0.0f,   7.0f,    1.0f },
    { ParamIDs::patch2Destination, -1,   2, 18,   0.0f,   7.0f,    4.0f },
    { ParamIDs::patch2Intensity,   -1,   2, 19, -63.0f,  63.0f,    0.0f },
    { ParamIDs::patch3Source,      -1,   2, 20,   0.0f,   7.0f,    2.0f },
    { ParamIDs::patch3Destination, -1,   2, 21,   0.0f,   7.0f,    0.0f },
    { ParamIDs::patch3Intensity,   -1,   2, 22, -63.0f,  63.0f,    0.0f },
    { ParamIDs::patch4Source,      -1,   2, 23,   0.0f,   7.0f,    3.0f },
    { ParamIDs::patch4Destination, -1,   2, 24,   0.0f,   7.0f,    7.0f },
    { ParamIDs::patch4Intensity,   -1,   2, 25, -63.0f,  63.0f,    0.0f },
};


// ═══════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════

const std::vector<MIDICCInfo>& MIDIMap::getAllMappings() noexcept
{
    return kCanonicalMap;
}

const MIDICCInfo* MIDIMap::findByCC(int ccNumber) noexcept
{
    for (const auto& item : kCanonicalMap)
        if (item.ccNumber == ccNumber) return &item;
    return nullptr;
}

const MIDICCInfo* MIDIMap::findByNRPN(int msb, int lsb) noexcept
{
    for (const auto& item : kCanonicalMap)
        if (item.nrpnMSB == msb && item.nrpnLSB == lsb) return &item;
    return nullptr;
}

const MIDICCInfo* MIDIMap::findByParamId(const std::string& paramId) noexcept
{
    for (const auto& item : kCanonicalMap)
        if (paramId == item.paramId) return &item;
    return nullptr;
}

float MIDIMap::midiValueToParamValue(const MIDICCInfo& info, int midi7BitValue) noexcept
{
    int clamped = std::max(0, std::min(127, midi7BitValue));
    float norm = static_cast<float>(clamped) / 127.0f;
    return info.minValue + (info.maxValue - info.minValue) * norm;
}

int MIDIMap::paramValueToMidiValue(const MIDICCInfo& info, float paramValue) noexcept
{
    float clamped = std::max(info.minValue, std::min(info.maxValue, paramValue));
    float range = info.maxValue - info.minValue;
    if (range <= 0.0001f) return static_cast<int>(std::round(clamped));
    float norm = (clamped - info.minValue) / range;
    return std::max(0, std::min(127, static_cast<int>(std::round(norm * 127.0f))));
}

} // namespace ABDMS2000