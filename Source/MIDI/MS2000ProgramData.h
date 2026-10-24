#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../State/ParameterRegistry.gen.h"
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::SysExCodec;
#include <iomanip>
#include <sstream>

namespace ABDMS2000
{

/**
 * @brief Programa **propio** del plugin (el sistema `native` que declara el contrato
 *        del host, ver `Source/Plugin/HostModelId.gen.h`):
 *        bloque de 384 B con codificación dirigida por el `ParameterRegistry`.
 *
 * Byte Layout (384 bytes — v2):
 *   [0x00..0x0B]  Program Name (12 bytes, ASCII)
 *   [0x0C]        Voice Mode byte (packed: voiceMode | unisonDetune | portamentoTime | portamentoOn)
 *   [0x0D]        Reserved
 *   [0x0E..0x10A] Parámetros — byte dirigido por el `sysexOffset` del registro (1 byte por
 *                 parámetro marcado `sysex != false`, hoy 253). Cubre Timbre 1 **y Timbre 2**,
 *                 velocidades, escala/split y los pasos del mod sequence.
 *   [0x10B..0x17F] Reservado
 *
 * La v1 (128 B) solo cabía Timbre 1 + voz: los 30 parámetros marcados `sysex: false`
 * (vocoder entero, `modSeqOn/Type`, `unisonSpread`, `masterVolume`…) ya se perdían, y
 * Timbre 2 ni existía. La v2 es el mismo
 * layout al principio, así que **un bloque v1 se sigue leyendo** (`unpackFromSysexPayload`
 * rellena con ceros el resto). El bloque es opaco para el ABD Bank Manager: el tamaño
 * no viaja en el contrato, solo en la carga útil base64.
 *
 * Conversion between 8-bit rawData and APVTS is fully automatic:
 * it iterates ParameterRegistry::getAllParameters() and reads/writes bytes
 * at the sysexOffset position for each parameter that has a valid offset.
 *
 * Special-case signed parameters (bias = 64) are hardcoded in the registry
 * via their min/max ranges and handled by a small lookup set.
 */
struct MS2000ProgramData
{
    // v2: cabe todo lo que el motor modela (253 parámetros con byte propio).
    // Es múltiplo de 128 a propósito: el bloque v1 (128 B) sigue siendo un subconjunto.
    static constexpr size_t UNPACKED_PROGRAM_SIZE = 384;
    /** Bloque de la v1 (solo Timbre 1 + voz). Se sigue leyendo; se escribe en v2. */
    static constexpr size_t UNPACKED_PROGRAM_SIZE_V1 = 128;
    static constexpr size_t NAME_LENGTH = 12;
    static constexpr uint8_t NAME_END = 12;
    static constexpr uint8_t VOICE_BYTE = 0x0C;
    static constexpr uint8_t TIMBRE_START = 0x0E;

    std::array<uint8_t, UNPACKED_PROGRAM_SIZE> rawData {};

    MS2000ProgramData()
    {
        reset();
    }

    /** Resets to the canonical "Init Synth" state. */
    void reset()
    {
        rawData.fill(0);
        setName("Init Synth");

        // Voice defaults
        rawData[VOICE_BYTE] = 1;      // Poly (mode 1)
        rawData[VOICE_BYTE + 2] = 0;  // Portamento time 0

        // Populate default values from ParameterRegistry
        int nonSysexIdx = 0;
        for (const auto &meta : ParameterRegistry::getAllParameters())
        {
            if (isVoiceByteParam(meta.id))
                continue;
            const int offInt = (meta.sysexOffset >= 0) ? (static_cast<int>(TIMBRE_START) + meta.sysexOffset)
                                                       : (static_cast<int>(NON_SYSEX_START) + (nonSysexIdx++));
            if (offInt < 0 || static_cast<size_t>(offInt) >= UNPACKED_PROGRAM_SIZE)
                continue;
            const auto off = static_cast<size_t>(offInt);

            if (meta.type == ParamType::Boolean)
                rawData[off] = (meta.defaultValue > 0.5f) ? 64 : 0;
            else if (meta.type == ParamType::Continuous && meta.max <= 1.0f)
                rawData[off] =
                    static_cast<uint8_t>(std::clamp(static_cast<int>(meta.defaultValue * 100.0f + 0.5f), 0, 100));
            else if (meta.min < 0.0f)
                rawData[off] = static_cast<uint8_t>(static_cast<int>(meta.defaultValue) + 64);
            else
                rawData[off] = static_cast<uint8_t>(meta.defaultValue);
        }
    }

    static constexpr size_t NON_SYSEX_START = 0x10B;
    static constexpr size_t TEMPO_MSB_OFFSET = 0x170;
    static constexpr size_t TEMPO_LSB_OFFSET = 0x171;
    static constexpr size_t ARP_TARGET_OFFSET = 0x172;
    static constexpr size_t ARP_KEYSYNC_OFFSET = 0x173;

    int getTempoBPM() const noexcept
    {
        const int bpm =
            (static_cast<int>(rawData[TEMPO_MSB_OFFSET]) << 8) | static_cast<int>(rawData[TEMPO_LSB_OFFSET]);
        return (bpm >= 20 && bpm <= 300) ? bpm : 120;
    }

    void setTempoBPM(int bpm) noexcept
    {
        const int clamped = std::clamp(bpm, 20, 300);
        rawData[TEMPO_MSB_OFFSET] = static_cast<uint8_t>((clamped >> 8) & 0xFF);
        rawData[TEMPO_LSB_OFFSET] = static_cast<uint8_t>(clamped & 0xFF);
    }

    uint8_t getArpTarget() const noexcept
    {
        return rawData[ARP_TARGET_OFFSET] <= 2 ? rawData[ARP_TARGET_OFFSET] : 0;
    }

    void setArpTarget(uint8_t target) noexcept
    {
        rawData[ARP_TARGET_OFFSET] = std::min<uint8_t>(target, 2);
    }

    bool getArpKeySync() const noexcept
    {
        return rawData[ARP_KEYSYNC_OFFSET] != 0;
    }

    void setArpKeySync(bool sync) noexcept
    {
        rawData[ARP_KEYSYNC_OFFSET] = sync ? 1 : 0;
    }

    static int getStorageOffsetById(const char *id) noexcept
    {
        if (id == nullptr)
            return -1;
        int nonSysexIdx = 0;
        for (const auto &meta : ParameterRegistry::getAllParameters())
        {
            if (isVoiceByteParam(meta.id))
                continue;
            const int offInt = (meta.sysexOffset >= 0) ? (static_cast<int>(TIMBRE_START) + meta.sysexOffset)
                                                       : (static_cast<int>(NON_SYSEX_START) + (nonSysexIdx++));
            if (std::strcmp(meta.id, id) == 0)
            {
                return (offInt >= 0 && static_cast<size_t>(offInt) < UNPACKED_PROGRAM_SIZE) ? offInt : -1;
            }
        }
        return -1;
    }

    // ─── Name ────────────────────────────────────────────────────────────────

    void setName(const std::string &newName)
    {
        std::string padded = newName.substr(0, NAME_LENGTH);
        while (padded.length() < NAME_LENGTH)
            padded += ' ';
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
        // El nombre viaja en 12 B rellenas con espacios, pero no se devuelve el relleno:
        // el Bank Manager lo muestra tal cual (igual que `MS2000HardwareProgram::getName`).
        while (!res.empty() && res.back() == ' ')
            res.pop_back();
        return res;
    }

    // ─── Byte-level access ───────────────────────────────────────────────────

    /** Read a raw byte at the given sysexOffset position. */
    uint8_t getByte(uint8_t offset) const
    {
        return rawData[offset];
    }

    /** Write a raw byte at the given sysexOffset position. */
    void setByte(uint8_t offset, uint8_t value)
    {
        rawData[offset] = value;
    }

    /** Convenient parameter setter by ID */
    void setParam(const char *id, float value)
    {
        if (std::strcmp(id, ParamIDs::voiceMode) == 0)
        {
            rawData[VOICE_BYTE] = (rawData[VOICE_BYTE] & ~0x03) | (static_cast<uint8_t>(value) & 0x03);
            return;
        }
        if (std::strcmp(id, ParamIDs::unisonDetune) == 0)
        {
            const uint8_t u = static_cast<uint8_t>(std::clamp(static_cast<int>(value), 0, 99));
            rawData[VOICE_BYTE] = (rawData[VOICE_BYTE] & ~(0x0F << 2)) | ((u & 0x0F) << 2);
            rawData[TIMBRE_START + 1] = u;
            return;
        }
        if (std::strcmp(id, ParamIDs::portamentoOn) == 0)
        {
            rawData[VOICE_BYTE] =
                (rawData[VOICE_BYTE] & ~(0x01 << 6)) | ((static_cast<uint8_t>(value > 0.5f ? 1 : 0) & 0x01) << 6);
            return;
        }
        if (std::strcmp(id, ParamIDs::portamentoTime) == 0)
        {
            rawData[VOICE_BYTE + 2] = static_cast<uint8_t>(value);
            return;
        }

        const auto *meta = ParameterRegistry::getParameter(id);
        if (!meta)
            return;
        const int offInt = getStorageOffsetById(id);
        if (offInt < 0 || static_cast<size_t>(offInt) >= UNPACKED_PROGRAM_SIZE)
            return;
        const auto off = static_cast<size_t>(offInt);

        if (meta->type == ParamType::Boolean)
            rawData[off] = (value > 0.5f) ? 64 : 0;
        else if (meta->type == ParamType::Continuous && meta->max <= 1.0f)
            rawData[off] = static_cast<uint8_t>(std::clamp(static_cast<int>(value * 100.0f + 0.5f), 0, 100));
        else if (meta->min < 0.0f)
            rawData[off] = static_cast<uint8_t>(static_cast<int>(value) + 64);
        else
            rawData[off] = static_cast<uint8_t>(value);
    }

    // ─── APVTS / Engine ↔ RawData (registry-driven) ──────────────────────────

    static bool isVoiceByteParam(const char *id) noexcept
    {
        if (id == nullptr)
            return false;
        return std::strcmp(id, ParamIDs::voiceMode) == 0 || std::strcmp(id, ParamIDs::unisonDetune) == 0
               || std::strcmp(id, ParamIDs::portamentoOn) == 0 || std::strcmp(id, ParamIDs::portamentoTime) == 0;
    }

    template <typename Fn> void forEachParameterValue(Fn &&callback) const
    {
        const uint8_t voiceByte = rawData[VOICE_BYTE];
        const uint8_t fullDetune = rawData[TIMBRE_START + 1];
        callback(ParamIDs::voiceMode, static_cast<float>(voiceByte & 0x03));
        callback(ParamIDs::unisonDetune, static_cast<float>(fullDetune != 0 ? fullDetune : ((voiceByte >> 2) & 0x0F)));
        callback(ParamIDs::portamentoOn, static_cast<float>((voiceByte >> 6) & 0x01));
        callback(ParamIDs::portamentoTime, static_cast<float>(rawData[VOICE_BYTE + 2]));

        int nonSysexIdx = 0;
        for (const auto &meta : ParameterRegistry::getAllParameters())
        {
            if (isVoiceByteParam(meta.id))
                continue;
            const int offInt = (meta.sysexOffset >= 0) ? (static_cast<int>(TIMBRE_START) + meta.sysexOffset)
                                                       : (static_cast<int>(NON_SYSEX_START) + (nonSysexIdx++));
            if (offInt < 0 || static_cast<size_t>(offInt) >= UNPACKED_PROGRAM_SIZE)
                continue;
            const auto off = static_cast<size_t>(offInt);

            float value;
            if (meta.type == ParamType::Boolean)
            {
                value = (rawData[off] >= 64) ? 1.0f : 0.0f;
            }
            else if (meta.type == ParamType::Continuous && meta.max <= 1.0f)
            {
                if (std::strcmp(meta.id, ParamIDs::masterVolume) == 0 && rawData[off] == 0)
                    value = meta.defaultValue;
                else
                    value = static_cast<float>(std::min<uint8_t>(rawData[off], 100)) / 100.0f;
            }
            else if (meta.type == ParamType::Choice)
            {
                value = static_cast<float>(rawData[off]);
            }
            else if (meta.min < 0.0f)
            {
                value = static_cast<float>(static_cast<int>(rawData[off]) - 64);
            }
            else
            {
                value = static_cast<float>(rawData[off]);
            }
            callback(meta.id, value);
        }
    }

#if ABD_HAS_JUCE
    void applyToAPVTS(juce::AudioProcessorValueTreeState &apvts) const
    {
        forEachParameterValue(
            [&apvts](const char *id, float value)
            {
                // Preserve global host settings across individual program changes
                if (std::strcmp(id, ParamIDs::masterVolume) == 0 || std::strcmp(id, ParamIDs::synthMode) == 0)
                    return;
                if (auto *p = apvts.getParameter(id))
                    p->setValueNotifyingHost(p->convertTo0to1(value));
            });

        const int bpm =
            (static_cast<int>(rawData[TEMPO_MSB_OFFSET]) << 8) | static_cast<int>(rawData[TEMPO_LSB_OFFSET]);
        if (bpm >= 20 && bpm <= 300)
            apvts.state.setProperty("presetTempoBPM", bpm, nullptr);
        apvts.state.setProperty("arpTarget", static_cast<int>(getArpTarget()), nullptr);
        apvts.state.setProperty("arpKeySync", static_cast<int>(getArpKeySync() ? 1 : 0), nullptr);
    }

    void extractFromAPVTS(const juce::AudioProcessorValueTreeState &apvts, const std::string &progName)
    {
        setName(progName);
        if (apvts.state.hasProperty("presetTempoBPM"))
            setTempoBPM(static_cast<int>(apvts.state.getProperty("presetTempoBPM", 120)));
        if (apvts.state.hasProperty("arpTarget"))
            setArpTarget(static_cast<uint8_t>(static_cast<int>(apvts.state.getProperty("arpTarget", 0))));
        if (apvts.state.hasProperty("arpKeySync"))
            setArpKeySync(static_cast<int>(apvts.state.getProperty("arpKeySync", 0)) != 0);

        // 1. Pack voice parameters into voiceByte
        auto getChoice = [&](const char *id) -> uint8_t
        {
            if (auto *p = apvts.getRawParameterValue(id))
                return static_cast<uint8_t>(p->load());
            return 0;
        };
        const uint8_t fullDetune = static_cast<uint8_t>(std::clamp<int>(getChoice(ParamIDs::unisonDetune), 0, 99));
        rawData[VOICE_BYTE] = (getChoice(ParamIDs::voiceMode) & 0x03) | ((fullDetune & 0x0F) << 2)
                              | ((getChoice(ParamIDs::portamentoOn) & 0x01) << 6);
        rawData[TIMBRE_START + 1] = fullDetune;

        if (auto *p = apvts.getRawParameterValue(ParamIDs::portamentoTime))
            rawData[VOICE_BYTE + 2] = static_cast<uint8_t>(p->load());

        // 2. Iterate registry for all other timbre parameters
        int nonSysexIdx = 0;
        for (const auto &meta : ParameterRegistry::getAllParameters())
        {
            if (isVoiceByteParam(meta.id))
                continue;
            const int offInt = (meta.sysexOffset >= 0) ? (static_cast<int>(TIMBRE_START) + meta.sysexOffset)
                                                       : (static_cast<int>(NON_SYSEX_START) + (nonSysexIdx++));
            if (offInt < 0 || static_cast<size_t>(offInt) >= UNPACKED_PROGRAM_SIZE)
                continue;
            const auto off = static_cast<size_t>(offInt);

            float native = 0.0f;
            if (auto *p = apvts.getRawParameterValue(meta.id))
                native = p->load();

            if (meta.type == ParamType::Boolean)
            {
                rawData[off] = (native > 0.5f) ? 64 : 0;
            }
            else if (meta.type == ParamType::Continuous && meta.max <= 1.0f)
            {
                rawData[off] = static_cast<uint8_t>(std::clamp(static_cast<int>(native * 100.0f + 0.5f), 0, 100));
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
    bool unpackFromSysexPayload(const uint8_t *payload, size_t payloadLen)
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
    bool packToSysexPayload(std::vector<uint8_t> &outPayload) const
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
            if (rawData[i] > 127)
                return "Name byte " + std::to_string(i) + " out of range";
        }
        // Voice mode (byte 0) must be 0..2
        if (rawData[VOICE_BYTE] > 2)
            return "Voice mode byte out of range (expected 0..2, got " + std::to_string(rawData[VOICE_BYTE]) + ")";
        return {};
    }

    /**
     * Returns the voice mode name from the raw voice byte.
     */
    const char *getVoiceModeName() const
    {
        switch (rawData[VOICE_BYTE] & 0x03)
        {
        case 0:
            return "Mono";
        case 1:
            return "Poly";
        case 2:
            return "Unison";
        default:
            return "Unknown";
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
                    oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(rawData[offset + i])
                        << " ";
                else
                    oss << "   ";
                if (i == 7)
                    oss << " ";
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

}  // namespace ABDMS2000
