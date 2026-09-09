#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::SysExCodec;
#include "MS2000ProgramData.h"
#include "SysExManager.h"
#include <vector>
#include <array>
#include <cstring>

namespace ABDMS2000 {

/**
 * @brief Exporter for Korg MS2000 / microKORG System Exclusive (.syx) patch files.
 * Encodes 8-bit memory structures to 7-bit MIDI safe blocks.
 */
class MS2000SysExExporter 
{
public:
    MS2000SysExExporter() = default;
    ~MS2000SysExExporter() = default;

    /**
     * Builds a 1-program SysEx message (.syx) compatible with physical Korg MS2000 hardware.
     * @param apvts Reference to the AudioProcessorValueTreeState containing the current patch parameters.
     * @param programName 12-character ASCII program name to embed in the preset.
     * @param outSysExBuffer MemoryBlock where the resulting packed SysEx message will be written.
     * @param midiChannel Global MIDI channel (1 to 16, default 1).
     */
    static void exportSingleProgram(const juce::AudioProcessorValueTreeState& apvts, 
                                     const juce::String& programName, 
                                     juce::MemoryBlock& outSysExBuffer,
                                     int midiChannel = 1)
    {
        // 1. Extract data into the 128-byte raw program buffer
        MS2000ProgramData progData;
        progData.extractFromAPVTS(apvts, programName.toStdString());

        // 2. Empaquetar de 8-bits a 7-bits (ventanas de 7 bytes -> 1 MSB collector + 7 data bytes)
        std::vector<uint8_t> packed7Bit;
        SysExCodec::pack8to7(progData.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE, packed7Bit);

        // 3. Construir la cabecera oficial de Korg y volcar a MemoryBlock
        outSysExBuffer.clear();
        int ch = std::max(1, std::min(16, midiChannel));

        outSysExBuffer.appendByte(0xF0); // Exclusive Status
        outSysExBuffer.appendByte(0x42); // Korg ID
        outSysExBuffer.appendByte(static_cast<uint8_t>(0x30 | (ch - 1))); // Channel format 30..3F
        outSysExBuffer.appendByte(0x58); // Model ID: MS2000
        outSysExBuffer.appendByte(0x40); // 1-Program Data Dump (0x40 / 0x4C)

        outSysExBuffer.append(packed7Bit.data(), packed7Bit.size());
        outSysExBuffer.appendByte(0xF7); // End of Exclusive
    }

    /**
     * Exports a full bank of 128 programs to an All-Data Dump (.syx) message.
     */
    static void exportBank(const std::array<MS2000ProgramData, 128>& bank,
                           juce::MemoryBlock& outSysExBuffer,
                           int midiChannel = 1)
    {
        std::vector<uint8_t> allRaw;
        allRaw.reserve(128 * MS2000ProgramData::UNPACKED_PROGRAM_SIZE);

        for (const auto& prog : bank)
        {
            allRaw.insert(allRaw.end(), prog.rawData.begin(), prog.rawData.end());
        }

        std::vector<uint8_t> packed7Bit;
        SysExCodec::pack8to7(allRaw.data(), allRaw.size(), packed7Bit);

        outSysExBuffer.clear();
        int ch = std::max(1, std::min(16, midiChannel));

        outSysExBuffer.appendByte(0xF0);
        outSysExBuffer.appendByte(0x42);
        outSysExBuffer.appendByte(static_cast<uint8_t>(0x30 | (ch - 1)));
        outSysExBuffer.appendByte(0x58);
        outSysExBuffer.appendByte(0x4C); // All Data Dump

        outSysExBuffer.append(packed7Bit.data(), packed7Bit.size());
        outSysExBuffer.appendByte(0xF7);
    }
};

} // namespace ABDMS2000
