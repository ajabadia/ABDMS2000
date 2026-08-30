#pragma once
#include <string>
#include <array>
#include <algorithm>
#include <cmath>

namespace ABDMS2000 {

struct LCDScreenText {
    std::string line1; // Exactly 16 characters
    std::string line2; // Exactly 16 characters
};

class LCDMenuFormatter {
public:
    static std::string pad16(const std::string& str) {
        if (str.length() >= 16) return str.substr(0, 16);
        return str + std::string(16 - str.length(), ' ');
    }

    static LCDScreenText formatPlayMode(const std::string& bank, int progNum, const std::string& progName) {
        // Line 1: Prog: A11       
        std::string l1 = "Prog: " + bank + std::to_string(progNum);
        // Line 2: [Init Synth   ]
        std::string l2 = "[" + progName + "]";
        return { pad16(l1), pad16(l2) };
    }

    static LCDScreenText formatParameter(const std::string& paramId, float value) {
        // Page headers and parameter text
        if (paramId == "filterCutoff") {
            int val = static_cast<int>(std::round(value));
            std::string valStr = std::to_string(val);
            while (valStr.length() < 3) valStr = " " + valStr;
            return { pad16("6.VCF           "), pad16("Cutoff     :" + valStr) };
        }
        else if (paramId == "filterResonance") {
            int val = static_cast<int>(std::round(value));
            std::string valStr = std::to_string(val);
            while (valStr.length() < 3) valStr = " " + valStr;
            return { pad16("6.VCF           "), pad16("Resonance  :" + valStr) };
        }
        else if (paramId == "filterType") {
            int t = static_cast<int>(value);
            const char* names[] = { "24LPF", "12LPF", "12BPF", "12HPF" };
            const char* typeName = (t >= 0 && t < 4) ? names[t] : "24LPF";
            return { pad16("6.VCF           "), pad16(std::string("Type       :") + typeName) };
        }
        else if (paramId == "osc1Wave") {
            int w = static_cast<int>(value);
            const char* waves[] = { "SAW", "PULSE", "TRIANGLE", "SINE", "VOX", "DWGS", "NOISE", "AUDIO IN" };
            const char* waveName = (w >= 0 && w < 8) ? waves[w] : "SAW";
            return { pad16("3.OSC 1         "), pad16(std::string("Wave       :") + waveName) };
        }
        else if (paramId == "osc1Control1") {
            int val = static_cast<int>(std::round(value));
            std::string valStr = std::to_string(val);
            while (valStr.length() < 3) valStr = " " + valStr;
            return { pad16("3.OSC 1         "), pad16("Control 1  :" + valStr) };
        }
        else if (paramId == "osc1DwgsWave") {
            int val = static_cast<int>(std::round(value));
            std::string valStr = std::to_string(val);
            while (valStr.length() < 2) valStr = "0" + valStr;
            return { pad16("3.OSC 1         "), pad16("DWGS Wave  :DWGS" + valStr) };
        }
        else if (paramId == "osc2Wave") {
            int w = static_cast<int>(value);
            const char* waves[] = { "SAW", "SQUARE", "TRIANGLE" };
            const char* waveName = (w >= 0 && w < 3) ? waves[w] : "SAW";
            return { pad16("4.OSC 2         "), pad16(std::string("Wave       :") + waveName) };
        }
        else if (paramId == "osc2ModMode") {
            int m = static_cast<int>(value);
            const char* modes[] = { "OFF", "RING", "SYNC", "RING+SYNC" };
            const char* modeName = (m >= 0 && m < 4) ? modes[m] : "OFF";
            return { pad16("4.OSC 2         "), pad16(std::string("Modulation :") + modeName) };
        }
        else if (paramId == "osc2Semitone") {
            int val = static_cast<int>(std::round(value));
            std::string sign = (val > 0) ? "+" : "";
            return { pad16("4.OSC 2         "), pad16("Semitone   :" + sign + std::to_string(val)) };
        }
        else if (paramId == "osc2Tune") {
            int val = static_cast<int>(std::round(value));
            std::string sign = (val > 0) ? "+" : "";
            return { pad16("4.OSC 2         "), pad16("Tune       :" + sign + std::to_string(val)) };
        }
        else if (paramId == "ampLevel") {
            int val = static_cast<int>(std::round(value));
            std::string valStr = std::to_string(val);
            while (valStr.length() < 3) valStr = " " + valStr;
            return { pad16("7.VCA/AMP       "), pad16("Level      :" + valStr) };
        }
        else if (paramId == "ampPan") {
            int val = static_cast<int>(std::round(value));
            std::string panStr = (val == 64) ? "CNT" : ((val < 64) ? "L" + std::to_string(64 - val) : "R" + std::to_string(val - 64));
            return { pad16("7.VCA/AMP       "), pad16("Panpot     :" + panStr) };
        }
        else if (paramId == "ampDistortion") {
            return { pad16("7.VCA/AMP       "), pad16(std::string("Distortion :") + (value > 0.5f ? "ON" : "OFF")) };
        }
        else if (paramId == "eg1Attack") {
            int val = static_cast<int>(std::round(value));
            return { pad16("8.EG1 (VCF)     "), pad16("Attack     :" + std::to_string(val)) };
        }
        else if (paramId == "eg1Decay") {
            int val = static_cast<int>(std::round(value));
            return { pad16("8.EG1 (VCF)     "), pad16("Decay      :" + std::to_string(val)) };
        }
        else if (paramId == "eg1Sustain") {
            int val = static_cast<int>(std::round(value));
            return { pad16("8.EG1 (VCF)     "), pad16("Sustain    :" + std::to_string(val)) };
        }
        else if (paramId == "eg1Release") {
            int val = static_cast<int>(std::round(value));
            return { pad16("8.EG1 (VCF)     "), pad16("Release    :" + std::to_string(val)) };
        }
        else if (paramId == "eg2Attack") {
            int val = static_cast<int>(std::round(value));
            return { pad16("9.EG2 (AMP)     "), pad16("Attack     :" + std::to_string(val)) };
        }
        else if (paramId == "eg2Decay") {
            int val = static_cast<int>(std::round(value));
            return { pad16("9.EG2 (AMP)     "), pad16("Decay      :" + std::to_string(val)) };
        }
        else if (paramId == "eg2Sustain") {
            int val = static_cast<int>(std::round(value));
            return { pad16("9.EG2 (AMP)     "), pad16("Sustain    :" + std::to_string(val)) };
        }
        else if (paramId == "eg2Release") {
            int val = static_cast<int>(std::round(value));
            return { pad16("9.EG2 (AMP)     "), pad16("Release    :" + std::to_string(val)) };
        }
        else if (paramId == "modFxType") {
            int t = static_cast<int>(value);
            const char* names[] = { "CHORUS/FLANG", "ENSEMBLE", "PHASER" };
            const char* n = (t >= 0 && t < 3) ? names[t] : "CHORUS/FLANG";
            return { pad16("12.MOD FX       "), pad16(std::string("Type       :") + n) };
        }
        else if (paramId == "delayType") {
            int t = static_cast<int>(value);
            const char* names[] = { "STEREO DELAY", "CROSS DELAY", "L/R DELAY" };
            const char* n = (t >= 0 && t < 3) ? names[t] : "STEREO DELAY";
            return { pad16("13.DELAY FX     "), pad16(std::string("Type       :") + n) };
        }
        else if (paramId == "arpOn") {
            return { pad16("15.ARPEGGIATOR  "), pad16(std::string("Arp Switch :") + (value > 0.5f ? "ON" : "OFF")) };
        }
        else if (paramId == "arpType") {
            int t = static_cast<int>(value);
            const char* names[] = { "UP", "DOWN", "ALT1", "ALT2", "RANDOM", "TRIGGER" };
            const char* n = (t >= 0 && t < 6) ? names[t] : "UP";
            return { pad16("15.ARPEGGIATOR  "), pad16(std::string("Type       :") + n) };
        }
        else if (paramId == "portamentoTime") {
            int val = static_cast<int>(std::round(value));
            return { pad16("2.PITCH         "), pad16("Portamento :" + std::to_string(val)) };
        }

        // Generic fallback
        std::string l1 = "EDIT PARAMETER  ";
        std::string l2 = paramId.substr(0, 10) + ":" + std::to_string(static_cast<int>(value));
        return { pad16(l1), pad16(l2) };
    }
};

} // namespace ABDMS2000
