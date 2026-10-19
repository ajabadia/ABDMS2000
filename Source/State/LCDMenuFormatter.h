#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include "../DSP/Oscillators/DWGSTables.h"

namespace ABDMS2000
{

struct LCDScreenText
{
    std::string line1;  // Exactly 16 characters
    std::string line2;  // Exactly 16 characters
};

class LCDMenuFormatter
{
  public:
    static std::string pad16(const std::string &str)
    {
        if (str.length() >= 16)
            return str.substr(0, 16);
        return str + std::string(16 - str.length(), ' ');
    }

    static LCDScreenText formatPlayMode(const std::string &bank, int progNum, const std::string &progName)
    {
        // Line 1: Prog: A11
        std::string l1 = "Prog: " + bank + std::to_string(progNum);
        // Line 2: [Init Synth   ]
        std::string l2 = "[" + progName + "]";
        return {pad16(l1), pad16(l2)};
    }

    static LCDScreenText formatParameter(const std::string &rawParamId, float value)
    {
        std::string paramId = rawParamId;
        if (paramId.size() > 2 && paramId[0] == 't' && paramId[1] == '2' && paramId[2] >= 'A' && paramId[2] <= 'Z')
        {
            paramId = static_cast<char>(paramId[2] - 'A' + 'a') + paramId.substr(3);
        }

        auto padVal3 = [](int v)
        {
            std::string s = std::to_string(v);
            while (s.length() < 3)
                s = " " + s;
            return s;
        };

        auto padSigned = [](int v)
        {
            std::string s = (v > 0 ? "+" : "") + std::to_string(v);
            while (s.length() < 3)
                s = " " + s;
            return s;
        };

        // Page headers and parameter text
        if (paramId == "filterCutoff")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("6.VCF           "), pad16("Cutoff     :" + padVal3(val))};
        }
        else if (paramId == "filterResonance")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("6.VCF           "), pad16("Resonance  :" + padVal3(val))};
        }
        else if (paramId == "filterType")
        {
            int t = static_cast<int>(value);
            const char *names[] = {"24LPF", "12LPF", "12BPF", "12HPF"};
            const char *typeName = (t >= 0 && t < 4) ? names[t] : "24LPF";
            return {pad16("6.VCF           "), pad16(std::string("Type       :") + typeName)};
        }
        else if (paramId == "filterEg1Int")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("6.VCF           "), pad16("EG1 Int    :" + padSigned(val))};
        }
        else if (paramId == "filterKeyTrack")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("6.VCF           "), pad16("KBD Track  :" + padSigned(val))};
        }
        else if (paramId == "filterVelo")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("6.VCF           "), pad16("Velo Sens  :" + padSigned(val))};
        }
        else if (paramId == "osc1Wave")
        {
            int w = static_cast<int>(value);
            const char *waves[] = {"SAW", "PULSE", "TRIANGLE", "CROSS", "VOX", "DWGS", "NOISE", "AUDIO IN"};
            const char *waveName = (w >= 0 && w < 8) ? waves[w] : "SAW";
            return {pad16("3.OSC 1         "), pad16(std::string("Wave      :") + waveName)};
        }
        else if (paramId == "osc1Ctrl1" || paramId == "osc1Control1")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("3.OSC 1         "), pad16("Control 1  :" + padVal3(val))};
        }
        else if (paramId == "osc1Ctrl2")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("3.OSC 1         "), pad16("Control 2  :" + padVal3(val))};
        }
        else if (paramId == "osc1DwgsWave")
        {
            int val = static_cast<int>(std::round(value));
            size_t idx = static_cast<size_t>(std::clamp(val, 0, static_cast<int>(DWGSTables::MAX_EXPANDED_TABLES - 1)));
            const char *wName = DWGSTables::getWaveName(idx);
            if (wName != nullptr && wName[0] != '\0')
            {
                return {pad16("3.OSC 1         "), pad16(std::string("DWGS:") + wName)};
            }
            std::string valStr = std::to_string(val);
            while (valStr.length() < 2)
                valStr = "0" + valStr;
            return {pad16("3.OSC 1         "), pad16("DWGS Wave  :DWGS" + valStr)};
        }
        else if (paramId == "osc2Wave")
        {
            int w = static_cast<int>(value);
            const char *waves[] = {"SAW", "SQUARE", "TRIANGLE"};
            const char *waveName = (w >= 0 && w < 3) ? waves[w] : "SAW";
            return {pad16("4.OSC 2         "), pad16(std::string("Wave       :") + waveName)};
        }
        else if (paramId == "osc2ModType" || paramId == "osc2ModMode")
        {
            int m = static_cast<int>(value);
            const char *modes[] = {"OFF", "RING", "SYNC", "RING+SYNC"};
            const char *modeName = (m >= 0 && m < 4) ? modes[m] : "OFF";
            return {pad16("4.OSC 2         "), pad16(std::string("Modulation :") + modeName)};
        }
        else if (paramId == "osc2Semitone")
        {
            int val = static_cast<int>(std::round(value));
            std::string sign = (val > 0) ? "+" : "";
            return {pad16("4.OSC 2         "), pad16("Semitone   :" + sign + std::to_string(val))};
        }
        else if (paramId == "osc2Tune")
        {
            int val = static_cast<int>(std::round(value));
            std::string sign = (val > 0) ? "+" : "";
            return {pad16("4.OSC 2         "), pad16("Tune       :" + sign + std::to_string(val))};
        }
        else if (paramId == "mixOsc1Level")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("5.MIXER         "), pad16("OSC1 Level :" + padVal3(val))};
        }
        else if (paramId == "mixOsc2Level")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("5.MIXER         "), pad16("OSC2 Level :" + padVal3(val))};
        }
        else if (paramId == "mixNoiseLevel")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("5.MIXER         "), pad16("Noise Level:" + padVal3(val))};
        }
        else if (paramId == "ampLevel")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("7.VCA/AMP       "), pad16("Level      :" + padVal3(val))};
        }
        else if (paramId == "ampPan")
        {
            int val = static_cast<int>(std::round(value));
            std::string panStr;
            if (val == 0 || val == 64)
                panStr = "CNT";
            else if (val < 0)
                panStr = "L" + std::to_string(-val);
            else
                panStr = "R" + std::to_string(val);
            return {pad16("7.VCA/AMP       "), pad16("Panpot     :" + panStr)};
        }
        else if (paramId == "ampDistortion")
        {
            return {pad16("7.VCA/AMP       "), pad16(std::string("Distortion :") + (value > 0.5f ? "ON" : "OFF"))};
        }
        else if (paramId == "ampKeyTrack")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("7.VCA/AMP       "), pad16("KBD Track  :" + padSigned(val))};
        }
        else if (paramId == "ampVelo")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("7.VCA/AMP       "), pad16("Velo Sens  :" + padSigned(val))};
        }
        else if (paramId == "eg1Attack")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("8.EG1 (VCF)     "), pad16("Attack     :" + std::to_string(val))};
        }
        else if (paramId == "eg1Decay")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("8.EG1 (VCF)     "), pad16("Decay      :" + std::to_string(val))};
        }
        else if (paramId == "eg1Sustain")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("8.EG1 (VCF)     "), pad16("Sustain    :" + std::to_string(val))};
        }
        else if (paramId == "eg1Release")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("8.EG1 (VCF)     "), pad16("Release    :" + std::to_string(val))};
        }
        else if (paramId == "eg2Attack")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("9.EG2 (AMP)     "), pad16("Attack     :" + std::to_string(val))};
        }
        else if (paramId == "eg2Decay")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("9.EG2 (AMP)     "), pad16("Decay      :" + std::to_string(val))};
        }
        else if (paramId == "eg2Sustain")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("9.EG2 (AMP)     "), pad16("Sustain    :" + std::to_string(val))};
        }
        else if (paramId == "eg2Release")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("9.EG2 (AMP)     "), pad16("Release    :" + std::to_string(val))};
        }
        else if (paramId == "lfo1Wave")
        {
            int w = static_cast<int>(value);
            const char *waves[] = {"SAW", "SQUARE", "TRI", "S&H"};
            const char *wn = (w >= 0 && w < 4) ? waves[w] : "TRI";
            return {pad16("10.LFO 1        "), pad16(std::string("Wave       :") + wn)};
        }
        else if (paramId == "lfo1Freq")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("10.LFO 1        "), pad16("Frequency  :" + padVal3(val))};
        }
        else if (paramId == "lfo2Wave")
        {
            int w = static_cast<int>(value);
            const char *waves[] = {"SAW", "SQU+", "SINE", "S&H"};
            const char *wn = (w >= 0 && w < 4) ? waves[w] : "SINE";
            return {pad16("11.LFO 2        "), pad16(std::string("Wave       :") + wn)};
        }
        else if (paramId == "lfo2Freq")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("11.LFO 2        "), pad16("Frequency  :" + padVal3(val))};
        }
        else if (paramId == "modFxType")
        {
            int t = static_cast<int>(value);
            const char *names[] = {"CHORUS/FLANG", "ENSEMBLE", "PHASER"};
            const char *n = (t >= 0 && t < 3) ? names[t] : "CHORUS/FLANG";
            return {pad16("12.MOD FX       "), pad16(std::string("Type       :") + n)};
        }
        else if (paramId == "modFxSpeed")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("12.MOD FX       "), pad16("LFO Speed  :" + padVal3(val))};
        }
        else if (paramId == "modFxDepth")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("12.MOD FX       "), pad16("Effect Depth:" + padVal3(val))};
        }
        else if (paramId == "delayType")
        {
            int t = static_cast<int>(value);
            const char *names[] = {"STEREO DELAY", "CROSS DELAY", "L/R DELAY"};
            const char *n = (t >= 0 && t < 3) ? names[t] : "STEREO DELAY";
            return {pad16("13.DELAY FX     "), pad16(std::string("Type       :") + n)};
        }
        else if (paramId == "delayTime")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("13.DELAY FX     "), pad16("Delay Time :" + padVal3(val))};
        }
        else if (paramId == "delayDepth")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("13.DELAY FX     "), pad16("Delay Depth:" + padVal3(val))};
        }
        else if (paramId == "delayFeedback")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("13.DELAY FX     "), pad16("Feedback   :" + padVal3(val))};
        }
        else if (paramId == "eqLowGain")
        {
            int db = static_cast<int>(std::round((value - 64.0f) * (12.0f / 63.0f)));
            return {pad16("14.EQUALIZER    "), pad16("Low Gain   :" + padSigned(db) + "dB")};
        }
        else if (paramId == "eqHighGain")
        {
            int db = static_cast<int>(std::round((value - 64.0f) * (12.0f / 63.0f)));
            return {pad16("14.EQUALIZER    "), pad16("High Gain  :" + padSigned(db) + "dB")};
        }
        else if (paramId == "arpOn")
        {
            return {pad16("15.ARPEGGIATOR  "), pad16(std::string("Arp Switch :") + (value > 0.5f ? "ON" : "OFF"))};
        }
        else if (paramId == "arpType")
        {
            int t = static_cast<int>(value);
            const char *names[] = {"UP", "DOWN", "ALT1", "ALT2", "RANDOM", "TRIGGER"};
            const char *n = (t >= 0 && t < 6) ? names[t] : "UP";
            return {pad16("15.ARPEGGIATOR  "), pad16(std::string("Type       :") + n)};
        }
        else if (paramId == "portamentoTime")
        {
            int val = static_cast<int>(std::round(value));
            return {pad16("2.PITCH         "), pad16("Portamento :" + std::to_string(val))};
        }
        else if (paramId == "scaleType")
        {
            int t = std::clamp(static_cast<int>(value), 0, 9);
            static const char *const kScales[10] = {"Equal Temp",
                                                    "Pure Major",
                                                    "Pure Minor",
                                                    "Arabic",
                                                    "Pythagorea",
                                                    "Werckmeist",
                                                    "Kirnberger",
                                                    "Slendro",
                                                    "Pelog",
                                                    "User Scale"};
            return {pad16("1.VOICE         "), pad16(std::string("Scale:") + kScales[t])};
        }
        else if (paramId == "scaleKey")
        {
            int k = std::clamp(static_cast<int>(value), 0, 11);
            static const char *const kKeys[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
            return {pad16("1.VOICE         "), pad16(std::string("Scale Key  :") + kKeys[k])};
        }
        else if (paramId == "seqRunMode")
        {
            return {pad16("16.MOD SEQ      "), pad16(std::string("Run Mode   :") + (value < 0.5f ? "1Shot" : "Loop"))};
        }
        else if (paramId == "seqLastStep")
        {
            int val = std::clamp(static_cast<int>(std::round(value)), 1, 16);
            return {pad16("16.MOD SEQ      "), pad16("Last Step  :" + padVal3(val))};
        }
        else if (paramId == "modSeqType")
        {
            int t = std::clamp(static_cast<int>(value), 0, 3);
            static const char *const kTypes[4] = {"FORWARD", "REVERSE", "ALT 1", "ALT 2"};
            return {pad16("16.MOD SEQ      "), pad16(std::string("Seq Type   :") + kTypes[t])};
        }
        else if (paramId == "seqKeySync")
        {
            int k = std::clamp(static_cast<int>(value), 0, 2);
            static const char *const kSync[3] = {"OFF", "TIMBRE", "VOICE"};
            return {pad16("16.MOD SEQ      "), pad16(std::string("Key Sync   :") + kSync[k])};
        }

        // Generic fallback
        std::string l1 = "EDIT PARAMETER  ";
        std::string l2 = paramId.substr(0, 10) + ":" + std::to_string(static_cast<int>(value));
        return {pad16(l1), pad16(l2)};
    }
};

}  // namespace ABDMS2000
