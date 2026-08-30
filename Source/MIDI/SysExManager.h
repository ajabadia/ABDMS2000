#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "SysExCodec.h"
#include "MS2000ProgramData.h"
#include <vector>
#include <array>
#include <string>
#include <memory>

namespace ABDMS2000 {

enum class SysExMessageType {
    Unknown = 0,
    ProgramDump,        // 0x40: 1 Program Data Dump
    AllDataDump,        // 0x4C: All Data Dump (128 Programs)
    ParameterChange,    // 0x41: 1-Parameter Change
    ProgramDumpRequest, // 0x10
    AllDataDumpRequest  // 0x0E
};

struct SysExParseResult {
    bool success{ false };
    SysExMessageType messageType{ SysExMessageType::Unknown };
    int midiChannel{ 1 };
    int programCount{ 0 };
    std::string programName;
    std::string errorMessage;
};

class SysExManager {
public:
    static constexpr size_t BANK_SIZE = 128; // 128 Programs (Bank A: 1..64, Bank B: 65..128)

    SysExManager();
    ~SysExManager() = default;

    /**
     * Parses raw SysEx bytes (from .syx file or MIDI input).
     */
    SysExParseResult parseSysEx(const uint8_t* data, size_t size, juce::AudioProcessorValueTreeState& apvts);

    /**
     * Parses a Standard MIDI File (.mid) containing embedded SysEx dumps.
     */
    SysExParseResult parseMidiFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts);

    /**
     * Creates a 1-Program Data Dump SysEx message (0x40).
     */
    std::vector<uint8_t> createProgramDump(int channel, const MS2000ProgramData& program) const;

    /**
     * Creates an All-Data Dump SysEx message (0x4C) for all 128 bank programs.
     */
    std::vector<uint8_t> createAllDataDump(int channel) const;

    /**
     * Formats binary data as a 2-column Hex + ASCII Inspector dump string.
     */
    static std::string formatHexDump(const uint8_t* data, size_t size, size_t bytesPerLine = 16);

    // Bank Management
    const MS2000ProgramData& getProgram(size_t index) const noexcept { return bank_[index % BANK_SIZE]; }
    void setProgram(size_t index, const MS2000ProgramData& prog) noexcept { bank_[index % BANK_SIZE] = prog; }
    size_t getBankSize() const noexcept { return BANK_SIZE; }

    int getActiveProgramIndex() const noexcept { return activeProgramIndex_; }
    void setActiveProgramIndex(int idx) noexcept { activeProgramIndex_ = std::max(0, std::min(127, idx)); }

    void loadCurrentProgramIntoAPVTS(juce::AudioProcessorValueTreeState& apvts) const;
    void saveAPVTSIntoCurrentProgram(const juce::AudioProcessorValueTreeState& apvts, const std::string& name);

private:
    std::array<MS2000ProgramData, BANK_SIZE> bank_{};
    int activeProgramIndex_{ 0 };

    bool isKorgHeader(const uint8_t* data, size_t size, int& outChannel, uint8_t& outFunction) const noexcept;
};

} // namespace ABDMS2000
