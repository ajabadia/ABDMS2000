#include "MIDIMap.h"
#include <algorithm>
#include <cmath>
#include "../State/ParameterRegistry.gen.h"

namespace ABDMS2000
{

// ═══════════════════════════════════════════════════════════════
// Korg MS2000 canonical MIDI CC & NRPN mapping
// Source: Korg MS2000 MIDI Implementation Manual / midi.guide
// ═══════════════════════════════════════════════════════════════
static const std::vector<MIDICCInfo> kCanonicalMap = {
    // ── CC mappable parameters (Korg MS2000 hardware spec) ──

    // Pitch / Portamento / Pedals / Amp
    {ParamIDs::assignablePedal, 4, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::portamentoTime, 5, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::ampLevel, 7, -1, -1, 0.0f, 127.0f, 100.0f},
    {ParamIDs::ampPan, 10, -1, -1, -64.0f, 63.0f, 0.0f},
    {ParamIDs::assignableSwitch, 64, -1, -1, 0.0f, 1.0f, 0.0f},
    {ParamIDs::portamentoOn, 65, -1, -1, 0.0f, 1.0f, 0.0f},

    // Mod FX (HW: CC 12 = LFO Speed, CC 93 = Depth)
    {ParamIDs::modFxSpeed, 12, -1, -1, 0.0f, 127.0f, 40.0f},
    // Delay FX (HW: CC 13 = Time, CC 94 = Depth)
    {ParamIDs::delayTime, 13, -1, -1, 0.0f, 127.0f, 40.0f},

    // OSC1 & OSC2 Controls (HW: CC 14, 15, 18, 19)
    {ParamIDs::osc1Ctrl1, 14, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::osc1Ctrl2, 15, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::osc2Semitone, 18, -1, -1, -24.0f, 24.0f, 0.0f},
    {ParamIDs::osc2Tune, 19, -1, -1, -50.0f, 50.0f, 0.0f},

    // Mixer (HW: CC 20, 21, 22)
    {ParamIDs::mixOsc1Level, 20, -1, -1, 0.0f, 127.0f, 127.0f},
    {ParamIDs::mixOsc2Level, 21, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::mixNoiseLevel, 22, -1, -1, 0.0f, 127.0f, 0.0f},

    // EG1 (HW: CC 23, 24, 25, 26)
    {ParamIDs::eg1Attack, 23, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::eg1Decay, 24, -1, -1, 0.0f, 127.0f, 64.0f},
    {ParamIDs::eg1Sustain, 25, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::eg1Release, 26, -1, -1, 0.0f, 127.0f, 40.0f},

    // LFO1 Frequency (HW: CC 27)
    {ParamIDs::lfo1Freq, 27, -1, -1, 0.0f, 127.0f, 30.0f},

    // Virtual Patch 1..4 Intensity (HW: CC 28, 29, 30, 31)
    {ParamIDs::patch1Intensity, 28, -1, -1, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch2Intensity, 29, -1, -1, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch3Intensity, 30, -1, -1, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch4Intensity, 31, -1, -1, -63.0f, 63.0f, 0.0f},

    // EG2 & Filter (HW: CC 70..75)
    {ParamIDs::eg2Sustain, 70, -1, -1, 0.0f, 127.0f, 127.0f},
    {ParamIDs::filterResonance, 71, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::eg2Release, 72, -1, -1, 0.0f, 127.0f, 20.0f},
    {ParamIDs::eg2Attack, 73, -1, -1, 0.0f, 127.0f, 0.0f},
    {ParamIDs::filterCutoff, 74, -1, -1, 0.0f, 127.0f, 127.0f},
    {ParamIDs::eg2Decay, 75, -1, -1, 0.0f, 127.0f, 64.0f},

    // LFO2 Frequency (HW: CC 76)
    {ParamIDs::lfo2Freq, 76, -1, -1, 0.0f, 127.0f, 50.0f},

    // OSC Waveforms (HW: CC 77, 78)
    {ParamIDs::osc1Wave, 77, -1, -1, 0.0f, 7.0f, 0.0f},
    {ParamIDs::osc2Wave, 78, -1, -1, 0.0f, 2.0f, 0.0f},

    // Filter EG1 Intensity (HW: CC 79)
    {ParamIDs::filterEg1Int, 79, -1, -1, -63.0f, 63.0f, 0.0f},

    // OSC2 Modulation Type (HW: CC 82 — Off/Ring/Sync/Ring+Sync)
    {ParamIDs::osc2ModType, 82, -1, -1, 0.0f, 3.0f, 0.0f},

    // Filter Type (HW: CC 83)
    {ParamIDs::filterType, 83, -1, -1, 0.0f, 3.0f, 0.0f},

    // Filter Keyboard Track (HW: CC 85)
    {ParamIDs::filterKeyTrack, 85, -1, -1, -63.0f, 63.0f, 0.0f},

    // LFO1 Wave & LFO2 Wave (HW: CC 87, 88)
    {ParamIDs::lfo1Wave, 87, -1, -1, 0.0f, 3.0f, 0.0f},
    {ParamIDs::lfo2Wave, 88, -1, -1, 0.0f, 3.0f, 2.0f},

    // Mod Sequencer On/Off (HW: CC 89)
    {ParamIDs::modSeqOn, 89, -1, -1, 0.0f, 1.0f, 0.0f},

    // Amp Distortion On/Off (HW: CC 92)
    {ParamIDs::ampDistortion, 92, -1, -1, 0.0f, 1.0f, 0.0f},

    // Mod FX Depth (HW: CC 93) & Delay Depth (HW: CC 94)
    {ParamIDs::modFxDepth, 93, -1, -1, 0.0f, 127.0f, 64.0f},
    {ParamIDs::delayDepth, 94, -1, -1, 0.0f, 127.0f, 50.0f},

    // ── Real Korg MS2000 Hardware NRPNs (MSB 0: Arpeggiator, MSB 4: Patch & Mod Seq) ──
    {ParamIDs::arpOn, -1, 0, 2, 0.0f, 1.0f, 0.0f},
    {ParamIDs::arpRange, -1, 0, 3, 1.0f, 4.0f, 1.0f},
    {ParamIDs::arpLatch, -1, 0, 4, 0.0f, 1.0f, 0.0f},
    {ParamIDs::arpType, -1, 0, 7, 0.0f, 5.0f, 0.0f},
    {ParamIDs::arpLastStep, -1, 0, 9, 1.0f, 8.0f, 8.0f},
    {ParamIDs::arpGate, -1, 0, 10, 0.0f, 127.0f, 100.0f},

    // Virtual Patch 1..4 Source (MSB 4, LSB 0..3) & Destination (MSB 4, LSB 8..11)
    {ParamIDs::patch1Source, -1, 4, 0, 0.0f, 7.0f, 0.0f},
    {ParamIDs::patch2Source, -1, 4, 1, 0.0f, 7.0f, 1.0f},
    {ParamIDs::patch3Source, -1, 4, 2, 0.0f, 7.0f, 2.0f},
    {ParamIDs::patch4Source, -1, 4, 3, 0.0f, 7.0f, 3.0f},
    {ParamIDs::patch1Destination, -1, 4, 8, 0.0f, 7.0f, 4.0f},
    {ParamIDs::patch2Destination, -1, 4, 9, 0.0f, 7.0f, 4.0f},
    {ParamIDs::patch3Destination, -1, 4, 10, 0.0f, 7.0f, 0.0f},
    {ParamIDs::patch4Destination, -1, 4, 11, 0.0f, 7.0f, 7.0f},

    // Mod Sequencer Row 1 Steps 1..16 (MSB 4, LSB 16..31 / 0x10..0x1F)
    {ParamIDs::seq1Step1, -1, 4, 16, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step2, -1, 4, 17, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step3, -1, 4, 18, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step4, -1, 4, 19, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step5, -1, 4, 20, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step6, -1, 4, 21, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step7, -1, 4, 22, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step8, -1, 4, 23, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step9, -1, 4, 24, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step10, -1, 4, 25, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step11, -1, 4, 26, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step12, -1, 4, 27, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step13, -1, 4, 28, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step14, -1, 4, 29, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step15, -1, 4, 30, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq1Step16, -1, 4, 31, -63.0f, 63.0f, 0.0f},

    // Mod Sequencer Row 2 Steps 1..16 (MSB 4, LSB 32..47 / 0x20..0x2F)
    {ParamIDs::seq2Step1, -1, 4, 32, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step2, -1, 4, 33, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step3, -1, 4, 34, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step4, -1, 4, 35, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step5, -1, 4, 36, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step6, -1, 4, 37, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step7, -1, 4, 38, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step8, -1, 4, 39, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step9, -1, 4, 40, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step10, -1, 4, 41, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step11, -1, 4, 42, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step12, -1, 4, 43, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step13, -1, 4, 44, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step14, -1, 4, 45, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step15, -1, 4, 46, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq2Step16, -1, 4, 47, -63.0f, 63.0f, 0.0f},

    // Mod Sequencer Row 3 Steps 1..16 (MSB 4, LSB 48..63 / 0x30..0x3F)
    {ParamIDs::seq3Step1, -1, 4, 48, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step2, -1, 4, 49, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step3, -1, 4, 50, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step4, -1, 4, 51, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step5, -1, 4, 52, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step6, -1, 4, 53, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step7, -1, 4, 54, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step8, -1, 4, 55, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step9, -1, 4, 56, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step10, -1, 4, 57, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step11, -1, 4, 58, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step12, -1, 4, 59, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step13, -1, 4, 60, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step14, -1, 4, 61, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step15, -1, 4, 62, -63.0f, 63.0f, 0.0f},
    {ParamIDs::seq3Step16, -1, 4, 63, -63.0f, 63.0f, 0.0f},

    // ── Extended / Legacy Editor NRPNs (MSB 2) ──
    {ParamIDs::osc1DwgsWave, -1, 2, 0, 0.0f, 63.0f, 0.0f},
    {ParamIDs::osc2ModType, -1, 2, 1, 0.0f, 3.0f, 0.0f},
    {ParamIDs::voiceMode, -1, 2, 2, 0.0f, 2.0f, 1.0f},
    {ParamIDs::unisonDetune, -1, 2, 3, 0.0f, 99.0f, 10.0f},
    {ParamIDs::modFxType, -1, 2, 10, 0.0f, 2.0f, 0.0f},
    {ParamIDs::modFxFeedback, -1, 2, 11, 0.0f, 127.0f, 0.0f},
    {ParamIDs::delayType, -1, 2, 12, 0.0f, 2.0f, 0.0f},
    {ParamIDs::delayFeedback, -1, 2, 13, 0.0f, 127.0f, 40.0f},
    {ParamIDs::eqLowFreq, -1, 2, 20, 0.0f, 3.0f, 1.0f},
    {ParamIDs::eqLowGain, -1, 2, 21, -63.0f, 63.0f, 0.0f},
    {ParamIDs::eqHighFreq, -1, 2, 22, 0.0f, 3.0f, 2.0f},
    {ParamIDs::eqHighGain, -1, 2, 23, -63.0f, 63.0f, 0.0f},
    {ParamIDs::arpType, -1, 2, 30, 0.0f, 5.0f, 0.0f},
    {ParamIDs::arpRange, -1, 2, 31, 1.0f, 4.0f, 1.0f},
    {ParamIDs::arpGate, -1, 2, 32, 0.0f, 127.0f, 100.0f},
    {ParamIDs::arpLatch, -1, 2, 33, 0.0f, 1.0f, 0.0f},
    {ParamIDs::modFxOn, -1, 2, 40, 0.0f, 1.0f, 1.0f},
    {ParamIDs::delayOn, -1, 2, 41, 0.0f, 1.0f, 1.0f},
    {ParamIDs::patch1Source, -1, 2, 14, 0.0f, 7.0f, 0.0f},
    {ParamIDs::patch1Destination, -1, 2, 15, 0.0f, 7.0f, 4.0f},
    {ParamIDs::patch1Intensity, -1, 2, 16, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch2Source, -1, 2, 17, 0.0f, 7.0f, 1.0f},
    {ParamIDs::patch2Destination, -1, 2, 18, 0.0f, 7.0f, 4.0f},
    {ParamIDs::patch2Intensity, -1, 2, 19, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch3Source, -1, 2, 24, 0.0f, 7.0f, 2.0f},
    {ParamIDs::patch3Destination, -1, 2, 25, 0.0f, 7.0f, 0.0f},
    {ParamIDs::patch3Intensity, -1, 2, 26, -63.0f, 63.0f, 0.0f},
    {ParamIDs::patch4Source, -1, 2, 27, 0.0f, 7.0f, 3.0f},
    {ParamIDs::patch4Destination, -1, 2, 28, 0.0f, 7.0f, 7.0f},
    {ParamIDs::patch4Intensity, -1, 2, 29, -63.0f, 63.0f, 0.0f},
};

// ═══════════════════════════════════════════════════════════════
// Public API
// ═══════════════════════════════════════════════════════════════

const std::vector<MIDICCInfo> &MIDIMap::getAllMappings() noexcept
{
    return kCanonicalMap;
}

const MIDICCInfo *MIDIMap::findByCC(int ccNumber) noexcept
{
    for (const auto &item : kCanonicalMap)
        if (item.ccNumber == ccNumber)
            return &item;
    return nullptr;
}

const MIDICCInfo *MIDIMap::findByNRPN(int msb, int lsb) noexcept
{
    for (const auto &item : kCanonicalMap)
        if (item.nrpnMSB == msb && item.nrpnLSB == lsb)
            return &item;
    return nullptr;
}

const MIDICCInfo *MIDIMap::findByParamId(const std::string &paramId) noexcept
{
    for (const auto &item : kCanonicalMap)
        if (paramId == item.paramId)
            return &item;
    return nullptr;
}

float MIDIMap::midiValueToParamValue(const MIDICCInfo &info, int midi7BitValue) noexcept
{
    int clamped = std::max(0, std::min(127, midi7BitValue));
    if (info.ccNumber >= 0 && info.minValue == 0.0f && (info.maxValue == 3.0f || info.maxValue == 7.0f))
    {
        const int numChoices = static_cast<int>(info.maxValue) + 1;
        const int bucketSize = 128 / numChoices;
        return static_cast<float>(std::min(numChoices - 1, clamped / bucketSize));
    }
    float norm = static_cast<float>(clamped) / 127.0f;
    return info.minValue + (info.maxValue - info.minValue) * norm;
}

int MIDIMap::paramValueToMidiValue(const MIDICCInfo &info, float paramValue) noexcept
{
    float clamped = std::max(info.minValue, std::min(info.maxValue, paramValue));
    float range = info.maxValue - info.minValue;
    if (range <= 0.0001f)
        return static_cast<int>(std::round(clamped));
    float norm = (clamped - info.minValue) / range;
    return std::max(0, std::min(127, static_cast<int>(std::round(norm * 127.0f))));
}

}  // namespace ABDMS2000