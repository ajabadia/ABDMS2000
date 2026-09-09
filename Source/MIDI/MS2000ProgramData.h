#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <array>
#include "../State/ParameterRegistry.gen.h"
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::SysExCodec;
#include <sstream>
#include <iomanip>

namespace ABDMS2000 {

/**
 * @brief Korg MS2000 Program Data — 128-byte Sysex layout with registry-driven encoding.
 *
 * Byte Layout (128 bytes):
 *   [0x00..0x0B]  Program Name (12 bytes, ASCII)
 *   [0x0C]        Voice Mode byte (packed: voiceMode | unisonDetune | portamentoTime | portamentoOn)
 *   [0x0D]        Reserved
 *   [0x0E..0x4B]  Timbre parameters — byte positions driven by ParameterRegistry sysexOffset
 *   [0x4C..0x7F]  Reserved (future: all-data-dump extensions)
 *
 * Conversion between 8-bit rawData and APVTS is fully automatic:
 * it iterates ParameterRegistry::getAllParameters() and reads/writes bytes
 * at the sysexOffset position for each parameter that has a valid offset.
 *
 * Special-case signed parameters (bias = 64) are hardcoded in the registry
 * via their min/max ranges and handled by a small lookup set.
 */
struct MS2000ProgramData {
    static constexpr size_t UNPACKED_PROGRAM_SIZE = 128;
    static constexpr size_t NAME_LENGTH = 12;
    static constexpr uint8_t NAME_END = 12;
    static constexpr uint8_t VOICE_BYTE = 0x0C;
    static constexpr uint8_t TIMBRE_START = 0x0E;

    std::array<uint8_t, UNPACKED_PROGRAM_SIZE> rawData{};

    MS2000ProgramData() { reset(); }

    /** Resets to the canonical "Init Synth" state. */
    void reset()
    {
        rawData.fill(0);
        setName("Init Synth");

        // Voice defaults
        rawData[VOICE_BYTE] = 1; // Poly (mode 1)
        rawData[VOICE_BYTE + 2] = 0; // Portamento time 0

        // Populate default values from ParameterRegistry
        for (const auto& meta : ParameterRegistry::getAllParameters())
        {
            if (meta.sysexOffset < 0 || isVoiceByteParam(meta.id)) continue;
            const auto off = TIMBRE_START + static_cast<size_t>(meta.sysexOffset);
            if (off >= UNPACKED_PROGRAM_SIZE) continue;

            if (meta.type == ParamType::Boolean)
                rawData[off] = (meta.defaultValue > 0.5f) ? 64 : 0;
            else if (meta.min < 0.0f)
                rawData[off] = static_cast<uint8_t>(static_cast<int>(meta.defaultValue) + 64);
            else
                rawData[off] = static_cast<uint8_t>(meta.defaultValue);
        }
    }

    // ─── Name ────────────────────────────────────────────────────────────────

    void setName(const std::string& newName)
    {
        std::string padded = newName.substr(0, NAME_LENGTH);
        while (padded.length() < NAME_LENGTH) padded += ' ';
        for (size_t i = 0; i < NAME_LENGTH; ++i)
            rawData[i] = static_cast<uint8_t>(padded[i]);
    }

    std::string getName() const
    {
        std::string res;
        res.reserve(NAME_LENGTH);
        for (size_t i = 0; i < NAME_LENGTH; ++i)
        {
            char c = static_cast<char>(rawData[i]);
            res += (c >= 32 && c <= 126) ? c : ' ';
        }
        return res;
    }

    // ─── Byte-level access ───────────────────────────────────────────────────

    /** Read a raw byte at the given sysexOffset position. */
    uint8_t getByte(uint8_t offset) const { return rawData[offset]; }

    /** Write a raw byte at the given sysexOffset position. */
    void setByte(uint8_t offset, uint8_t value) { rawData[offset] = value; }

    /** Convenient parameter setter by ID */
    void setParam(const char* id, float value)
    {
        if (std::strcmp(id, ParamIDs::voiceMode) == 0)
        {
            rawData[VOICE_BYTE] = (rawData[VOICE_BYTE] & ~0x03) | (static_cast<uint8_t>(value) & 0x03);
            return;
        }
        if (std::strcmp(id, ParamIDs::unisonDetune) == 0)
        {
            rawData[VOICE_BYTE] = (rawData[VOICE_BYTE] & ~(0x0F << 2)) | ((static_cast<uint8_t>(value) & 0x0F) << 2);
            return;
        }
        if (std::strcmp(id, ParamIDs::portamentoOn) == 0)
        {
            rawData[VOICE_BYTE] = (rawData[VOICE_BYTE] & ~(0x01 << 6)) | ((static_cast<uint8_t>(value > 0.5f ? 1 : 0) & 0x01) << 6);
            return;
        }
        if (std::strcmp(id, ParamIDs::portamentoTime) == 0)
        {
            rawData[VOICE_BYTE + 2] = static_cast<uint8_t>(value);
            return;
        }

        const auto* meta = ParameterRegistry::getParameter(id);
        if (!meta || meta->sysexOffset < 0) return;
        const auto off = TIMBRE_START + static_cast<size_t>(meta->sysexOffset);
        if (off >= UNPACKED_PROGRAM_SIZE) return;

        if (meta->type == ParamType::Boolean)
            rawData[off] = (value > 0.5f) ? 64 : 0;
        else if (meta->min < 0.0f)
            rawData[off] = static_cast<uint8_t>(static_cast<int>(value) + 64);
        else
            rawData[off] = static_cast<uint8_t>(value);
    }

    // ─── APVTS ↔ RawData (registry-driven) ───────────────────────────────────

    static bool isVoiceByteParam(const char* id) noexcept
    {
        return id == ParamIDs::voiceMode
            || id == ParamIDs::unisonDetune
            || id == ParamIDs::portamentoOn
            || id == ParamIDs::portamentoTime;
    }

#if ABD_HAS_JUCE
    void applyToAPVTS(juce::AudioProcessorValueTreeState& apvts) const
    {
        // 1. Unpack the voice byte
        const uint8_t voiceByte = rawData[VOICE_BYTE];
        auto setChoice = [&](const char* id, float rawVal) {
            if (auto* p = apvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(rawVal));
        };
        setChoice(ParamIDs::voiceMode,    static_cast<float>(voiceByte & 0x03));
        setChoice(ParamIDs::unisonDetune, static_cast<float>((voiceByte >> 2) & 0x0F));
        setChoice(ParamIDs::portamentoOn, static_cast<float>((voiceByte >> 6) & 0x01));

        if (auto* p = apvts.getParameter(ParamIDs::portamentoTime))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(rawData[VOICE_BYTE + 2])));

        // 2. Iterate registry for all other timbre parameters
        for (const auto& meta : ParameterRegistry::getAllParameters())
        {
            if (meta.sysexOffset < 0) continue;
            if (isVoiceByteParam(meta.id)) continue;
            const auto off = TIMBRE_START + static_cast<size_t>(meta.sysexOffset);
            if (off >= UNPACKED_PROGRAM_SIZE) continue;

            float value;
            if (meta.type == ParamType::Boolean)
            {
                value = (rawData[off] >= 64) ? 1.0f : 0.0f;
            }
            else if (meta.type == ParamType::Choice)
            {
                value = static_cast<float>(rawData[off]);
            }
            else if (meta.min < 0.0f)
            {
                // Signed parameters stored as unsigned with +64 bias
                value = static_cast<float>(static_cast<int>(rawData[off]) - 64);
            }
            else
            {
                value = static_cast<float>(rawData[off]);
            }

            if (auto* p = apvts.getParameter(meta.id))
                p->setValueNotifyingHost(p->convertTo0to1(value));
        }
    }

    void extractFromAPVTS(const juce::AudioProcessorValueTreeState& apvts,
                          const std::string& progName)
    {
        setName(progName);

        // 1. Pack voice parameters into voiceByte
        auto getChoice = [&](const char* id) -> uint8_t {
            if (auto* p = apvts.getRawParameterValue(id))
                return static_cast<uint8_t>(p->load());
            return 0;
        };
        rawData[VOICE_BYTE] = (getChoice(ParamIDs::voiceMode) & 0x03)
            | ((getChoice(ParamIDs::unisonDetune) & 0x0F) << 2)
            | ((getChoice(ParamIDs::portamentoOn) & 0x01) << 6);

        if (auto* p = apvts.getRawParameterValue(ParamIDs::portamentoTime))
            rawData[VOICE_BYTE + 2] = static_cast<uint8_t>(p->load());

        // 2. Iterate registry for all other timbre parameters
        for (const auto& meta : ParameterRegistry::getAllParameters())
        {
            if (meta.sysexOffset < 0) continue;
            if (isVoiceByteParam(meta.id)) continue;
            const auto off = TIMBRE_START + static_cast<size_t>(meta.sysexOffset);
            if (off >= UNPACKED_PROGRAM_SIZE) continue;

            float native = 0.0f;
            if (auto* p = apvts.getRawParameterValue(meta.id))
                native = p->load();

            if (meta.type == ParamType::Boolean)
            {
                rawData[off] = (native > 0.5f) ? 64 : 0;
            }
            else if (meta.type == ParamType::Choice)
            {
                rawData[off] = static_cast<uint8_t>(native);
            }
            else if (meta.min < 0.0f)
            {
                // Signed → unsigned with +64 bias
                rawData[off] = static_cast<uint8_t>(static_cast<int>(native) + 64);
            }
            else
            {
                rawData[off] = static_cast<uint8_t>(native);
            }
        }
    }
#endif

    // ─── Raw byte array ↔ Sysex block ────────────────────────────────────────

    /**
     * Unpacks a 7-bit encoded SysEx payload into this 128-byte rawData buffer.
     * @return true on success.
     */
    bool unpackFromSysexPayload(const uint8_t* payload, size_t payloadLen)
    {
        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.empty())
            return false;
        const size_t n = std::min(unpacked.size(), UNPACKED_PROGRAM_SIZE);
        std::copy_n(unpacked.begin(), n, rawData.begin());
        return true;
    }

    /**
     * Packs this 128-byte rawData buffer into a 7-bit SysEx payload.
     * @return true on success.
     */
    bool packToSysexPayload(std::vector<uint8_t>& outPayload) const
    {
        return SysExCodec::pack8to7(rawData.data(), UNPACKED_PROGRAM_SIZE, outPayload);
    }

    // ─── Validation ──────────────────────────────────────────────────────────

    /**
     * Validates a raw 128-byte buffer as a Korg MS2000 Program Data block.
     * @return empty string on success, error description on failure.
     */
    std::string validate() const
    {
        // Check name bytes are printable
        for (size_t i = 0; i < NAME_LENGTH; ++i)
        {
            if (rawData[i] > 127) return "Name byte " + std::to_string(i) + " out of range";
        }
        // Voice mode (byte 0) must be 0..2
        if (rawData[VOICE_BYTE] > 2)
            return "Voice mode byte out of range (expected 0..2, got " +
                   std::to_string(rawData[VOICE_BYTE]) + ")";
        return {};
    }

    /**
     * Returns the voice mode name from the raw voice byte.
     */
    const char* getVoiceModeName() const
    {
        switch (rawData[VOICE_BYTE] & 0x03)
        {
            case 0: return "Mono";
            case 1: return "Poly";
            case 2: return "Unison";
            default: return "Unknown";
        }
    }

    // ─── Hex Dump ────────────────────────────────────────────────────────────

    /**
     * Formats the 128-byte raw data as a 2-column hex + ASCII dump string.
     */
    std::string toHexDump(size_t bytesPerLine = 16) const
    {
        std::ostringstream oss;
        oss << "MS2000 Program: " << getName() << "\n";
        oss << "Voice Mode: " << getVoiceModeName() << "\n\n";

        size_t offset = 0;
        while (offset < UNPACKED_PROGRAM_SIZE)
        {
            oss << std::hex << std::setw(3) << std::setfill('0') << offset << ":  ";
            size_t lineBytes = std::min(bytesPerLine, UNPACKED_PROGRAM_SIZE - offset);
            for (size_t i = 0; i < bytesPerLine; ++i)
            {
                if (i < lineBytes)
                    oss << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(rawData[offset + i]) << " ";
                else oss << "   ";
                if (i == 7) oss << " ";
            }
            oss << " |";
            for (size_t i = 0; i < lineBytes; ++i)
            {
                char c = static_cast<char>(rawData[offset + i]);
                oss << ((c >= 32 && c <= 126) ? c : '.');
            }
            oss << "|\n";
            offset += lineBytes;
        }
        return oss.str();
    }
};

} // namespace ABDMS2000
