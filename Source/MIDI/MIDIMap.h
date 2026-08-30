#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace ABDMS2000 {

struct MIDICCInfo {
    const char* paramId;
    int ccNumber;       // -1 if none
    int nrpnMSB;        // -1 if none
    int nrpnLSB;        // -1 if none
    float minValue;
    float maxValue;
    float defaultValue;
};

/**
 * @brief Canonical MIDI Control Change (CC) and NRPN mapping for Korg MS2000.
 */
class MIDIMap {
public:
    static const std::vector<MIDICCInfo>& getAllMappings() noexcept;
    static const MIDICCInfo* findByCC(int ccNumber) noexcept;
    static const MIDICCInfo* findByNRPN(int msb, int lsb) noexcept;
    static const MIDICCInfo* findByParamId(const std::string& paramId) noexcept;

    static float midiValueToParamValue(const MIDICCInfo& info, int midi7BitValue) noexcept;
    static int paramValueToMidiValue(const MIDICCInfo& info, float paramValue) noexcept;
};

} // namespace ABDMS2000
