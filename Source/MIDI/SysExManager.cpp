#include "SysExManager.h"
#include "../State/MS2000FactoryBank.h"
#include <iomanip>
#include <sstream>

namespace ABDMS2000 {

SysExManager::SysExManager()
{
    // Initialize bank with default program names
    for (size_t i = 0; i < BANK_SIZE; ++i)
    {
        char bankLetter = (i < 64) ? 'A' : 'B';
        int progNum = static_cast<int>((i % 64) + 1);
        std::string defaultName = "Init " + std::string(1, bankLetter) + std::to_string(progNum);
        bank_[i].setName(defaultName);
    }

    // Load compiled-in factory presets
    MS2000FactoryBank factory;
    for (int p = 0; p < factory.getNumPresets() && p < static_cast<int>(BANK_SIZE); ++p)
    {
        bank_[static_cast<size_t>(p)] = factory.getPresetProgramData(p);
    }
}


bool SysExManager::isKorgHeader(const uint8_t* data, size_t size, int& outChannel, uint8_t& outFunction) const noexcept
{
    if (data == nullptr || size < 6) return false;
    if (data[0] != 0xF0) return false;
    if (data[1] != 0x42) return false; // Korg
    if ((data[2] & 0xF0) != 0x30) return false; // Global channel format 30..3F
    
    // Model ID: 0x58 (MS2000 / MS2000R) or 0x71 (microKORG)
    if (data[3] != 0x58 && data[3] != 0x71) return false;

    outChannel = (data[2] & 0x0F) + 1;
    outFunction = data[4];
    return true;
}

SysExParseResult SysExManager::parseSysEx(const uint8_t* data, size_t size, juce::AudioProcessorValueTreeState& apvts)
{
    SysExParseResult result;
    if (data == nullptr || size < 7)
    {
        result.errorMessage = "Buffer too small for SysEx";
        return result;
    }

    if (data[size - 1] != 0xF7)
    {
        result.errorMessage = "Missing 0xF7 End Of Exclusive terminator";
        return result;
    }

    int channel = 1;
    uint8_t func = 0;
    if (!isKorgHeader(data, size, channel, func))
    {
        result.errorMessage = "Invalid Korg SysEx Header";
        return result;
    }

    result.midiChannel = channel;

    // Isolate payload between header (5 bytes) and 0xF7
    const uint8_t* payload = data + 5;
    size_t payloadLen = size - 6;

    if (func == 0x40) // 1-Program Data Dump
    {
        result.messageType = SysExMessageType::ProgramDump;

        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.empty())
        {
            result.errorMessage = "Failed to unpack 7-to-8 bit program dump payload";
            return result;
        }

        MS2000ProgramData prog;
        size_t copyLen = std::min(unpacked.size(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE);
        std::copy_n(unpacked.begin(), copyLen, prog.rawData.begin());

        bank_[activeProgramIndex_] = prog;
        prog.applyToAPVTS(apvts);

        result.success = true;
        result.programCount = 1;
        result.programName = prog.getName();
        return result;
    }
    else if (func == 0x4C) // All-Data Dump (128 Programs)
    {
        result.messageType = SysExMessageType::AllDataDump;

        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.size() < MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
        {
            result.errorMessage = "Failed to unpack 7-to-8 bit all-data dump payload";
            return result;
        }

        size_t numPrograms = unpacked.size() / MS2000ProgramData::UNPACKED_PROGRAM_SIZE;
        numPrograms = std::min(numPrograms, BANK_SIZE);

        for (size_t p = 0; p < numPrograms; ++p)
        {
            MS2000ProgramData prog;
            const uint8_t* pStart = unpacked.data() + (p * MS2000ProgramData::UNPACKED_PROGRAM_SIZE);
            std::copy_n(pStart, MS2000ProgramData::UNPACKED_PROGRAM_SIZE, prog.rawData.begin());
            bank_[p] = prog;
        }

        // Apply active program to APVTS
        bank_[activeProgramIndex_].applyToAPVTS(apvts);

        result.success = true;
        result.programCount = static_cast<int>(numPrograms);
        result.programName = bank_[activeProgramIndex_].getName();
        return result;
    }
    else if (func == 0x41) // Parameter Change
    {
        result.messageType = SysExMessageType::ParameterChange;
        result.success = true;
        return result;
    }

    result.errorMessage = "Unsupported SysEx Function Code";
    return result;
}

SysExParseResult SysExManager::parseMidiFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts)
{
    SysExParseResult result;
    if (!file.existsAsFile())
    {
        result.errorMessage = "File not found: " + file.getFullPathName().toStdString();
        return result;
    }

    juce::FileInputStream stream(file);
    if (!stream.openedOk())
    {
        result.errorMessage = "Failed to open file stream";
        return result;
    }

    juce::MidiFile midiFile;
    if (!midiFile.readFrom(stream))
    {
        result.errorMessage = "Failed to parse Standard MIDI File";
        return result;
    }

    int totalProgramsLoaded = 0;
    std::string lastLoadedName;

    for (int t = 0; t < midiFile.getNumTracks(); ++t)
    {
        const auto* track = midiFile.getTrack(t);
        if (track == nullptr) continue;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto& msg = track->getEventPointer(i)->message;
            if (msg.isSysEx())
            {
                auto subRes = parseSysEx(static_cast<const uint8_t*>(msg.getSysExData()),
                                         static_cast<size_t>(msg.getSysExDataSize()),
                                         apvts);
                if (subRes.success)
                {
                    totalProgramsLoaded += subRes.programCount;
                    lastLoadedName = subRes.programName;
                }
            }
        }
    }

    if (totalProgramsLoaded > 0)
    {
        result.success = true;
        result.programCount = totalProgramsLoaded;
        result.programName = lastLoadedName;
    }
    else
    {
        result.errorMessage = "No valid MS2000 SysEx messages found in MIDI File";
    }

    return result;
}

std::vector<uint8_t> SysExManager::createProgramDump(int channel, const MS2000ProgramData& program) const
{
    std::vector<uint8_t> packed;
    SysExCodec::pack8to7(program.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE, packed);

    std::vector<uint8_t> sysex;
    sysex.reserve(6 + packed.size());

    int ch = std::max(1, std::min(16, channel));
    sysex.push_back(0xF0);
    sysex.push_back(0x42);
    sysex.push_back(static_cast<uint8_t>(0x30 | (ch - 1)));
    sysex.push_back(0x58); // MS2000
    sysex.push_back(0x40); // 1-Program Dump

    sysex.insert(sysex.end(), packed.begin(), packed.end());
    sysex.push_back(0xF7);

    return sysex;
}

std::vector<uint8_t> SysExManager::createAllDataDump(int channel) const
{
    std::vector<uint8_t> allRaw;
    allRaw.reserve(BANK_SIZE * MS2000ProgramData::UNPACKED_PROGRAM_SIZE);

    for (const auto& prog : bank_)
    {
        allRaw.insert(allRaw.end(), prog.rawData.begin(), prog.rawData.end());
    }

    std::vector<uint8_t> packed;
    SysExCodec::pack8to7(allRaw.data(), allRaw.size(), packed);

    std::vector<uint8_t> sysex;
    sysex.reserve(6 + packed.size());

    int ch = std::max(1, std::min(16, channel));
    sysex.push_back(0xF0);
    sysex.push_back(0x42);
    sysex.push_back(static_cast<uint8_t>(0x30 | (ch - 1)));
    sysex.push_back(0x58); // MS2000
    sysex.push_back(0x4C); // All Data Dump

    sysex.insert(sysex.end(), packed.begin(), packed.end());
    sysex.push_back(0xF7);

    return sysex;
}

std::string SysExManager::formatHexDump(const uint8_t* data, size_t size, size_t bytesPerLine)
{
    if (data == nullptr || size == 0) return "Empty Buffer\n";

    std::ostringstream oss;
    size_t offset = 0;

    while (offset < size)
    {
        // 1. Offset
        oss << std::hex << std::setw(4) << std::setfill('0') << offset << ":  ";

        // 2. Hex values
        size_t lineBytes = std::min(bytesPerLine, size - offset);
        for (size_t i = 0; i < bytesPerLine; ++i)
        {
            if (i < lineBytes)
            {
                oss << std::hex << std::setw(2) << std::setfill('0')
                    << static_cast<int>(data[offset + i]) << " ";
            }
            else
            {
                oss << "   ";
            }
            if (i == 7) oss << " ";
        }

        oss << " |";

        // 3. ASCII representation
        for (size_t i = 0; i < lineBytes; ++i)
        {
            char c = static_cast<char>(data[offset + i]);
            if (c >= 32 && c <= 126) oss << c;
            else oss << '.';
        }

        oss << "|\n";
        offset += lineBytes;
    }

    return oss.str();
}

void SysExManager::loadCurrentProgramIntoAPVTS(juce::AudioProcessorValueTreeState& apvts) const
{
    bank_[activeProgramIndex_].applyToAPVTS(apvts);
}

void SysExManager::saveAPVTSIntoCurrentProgram(const juce::AudioProcessorValueTreeState& apvts, const std::string& name)
{
    bank_[activeProgramIndex_].extractFromAPVTS(apvts, name);
}

} // namespace ABDMS2000
