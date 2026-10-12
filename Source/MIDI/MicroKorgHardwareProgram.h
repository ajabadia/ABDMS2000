#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
#include "../DSP/Sequencer/ModSequencer.h"
#include "../State/ParameterRegistry.gen.h"
#include "MS2000HardwareProgram.h"
#include "MS2000ProgramData.h"
using abd::hw::SysExCodec;
#if ABD_HAS_JUCE
#include <juce_audio_processors/juce_audio_processors.h>
#endif

namespace ABDMS2000
{

/**
 * @brief Formato SysEx nativo y binario del Korg microKORG físico (254 bytes decodificados).
 *
 * Bifurcación especializada del programa de hardware que modela las características
 * exclusivas del microKORG frente al MS2000:
 *  - Byte 0x25: Arpeggiator Last Step (longitud rítmica de 1 a 8 pasos).
 *  - Byte 0x0C: Arpeggiator Step Mask (bitmask de 8 bits donde bit 0..7 representan
 *    el estado Play/Rest de los pasos 1 a 8).
 *  - Neutralización del Modulation Sequencer (el microKORG no dispone de secuenciador físico).
 *  - Identificador de modelo Korg: soporta 0x58 (compatibilidad estándar) y 0x71 (microKORG nativo).
 */
struct MicroKorgHardwareProgram
{
    static constexpr size_t PROGRAM_SIZE = 254;
    static constexpr size_t NAME_OFFSET = 0x00;
    static constexpr size_t NAME_LENGTH = 12;

    // Bytes exclusivos de arpegiador microKORG
    static constexpr size_t ARP_STEP_MASK = 0x0C;  // Bitmask 8 bits (Step 1..8 Play/Rest)
    static constexpr size_t ARP_LAST_STEP = 0x25;  // Longitud de 1 a 8 pasos

    // Sub-status de comandos SysEx Korg
    static constexpr uint8_t CMD_PROGRAM_DUMP = 0x40;
    static constexpr uint8_t CMD_ALL_DATA_DUMP = 0x4C;
    static constexpr uint8_t CMD_WRITE_COMPLETED = 0x23;
    static constexpr uint8_t CMD_WRITE_ERROR = 0x24;

    // Model IDs
    static constexpr uint8_t MODEL_ID_MICROKORG = 0x71;
    static constexpr uint8_t MODEL_ID_LEGACY = 0x58;

    std::array<uint8_t, PROGRAM_SIZE> raw {};

    MicroKorgHardwareProgram()
    {
        // Inicializar a partir del INIT patch de hardware base
        MS2000HardwareProgram base;
        raw = base.raw;

        // microKORG defaults: 8 pasos activos y Last Step = 8
        raw[ARP_LAST_STEP] = 8;
        raw[ARP_STEP_MASK] = 0xFF;  // Todos los 8 pasos en ON (Play)
    }

    explicit MicroKorgHardwareProgram(const MS2000HardwareProgram &base) : raw(base.raw)
    {
        raw[ARP_LAST_STEP] = 8;
        raw[ARP_STEP_MASK] = 0xFF;
    }

    std::string getName() const
    {
        std::string res;
        res.reserve(NAME_LENGTH);
        for (size_t i = 0; i < NAME_LENGTH; ++i)
        {
            const char c = static_cast<char>(raw[NAME_OFFSET + i]);
            res += (c >= 32 && c <= 126) ? c : ' ';
        }
        while (!res.empty() && res.back() == ' ')
            res.pop_back();
        return res;
    }

    void setName(const std::string &newName)
    {
        std::string padded = newName.substr(0, NAME_LENGTH);
        while (padded.length() < NAME_LENGTH)
            padded += ' ';
        for (size_t i = 0; i < NAME_LENGTH; ++i)
            raw[NAME_OFFSET + i] = static_cast<uint8_t>(padded[i]);
    }

    int getLastStep() const noexcept
    {
        const int val = static_cast<int>(raw[ARP_LAST_STEP]);
        return std::max(1, std::min(8, val == 0 ? 8 : val));
    }

    void setLastStep(int step) noexcept
    {
        raw[ARP_LAST_STEP] = static_cast<uint8_t>(std::max(1, std::min(8, step)));
    }

    bool getStepMask(int stepIndex1to8) const noexcept
    {
        if (stepIndex1to8 < 1 || stepIndex1to8 > 8)
            return true;
        const uint8_t mask = static_cast<uint8_t>(1u << (stepIndex1to8 - 1));
        return (raw[ARP_STEP_MASK] & mask) != 0;
    }

    void setStepMask(int stepIndex1to8, bool active) noexcept
    {
        if (stepIndex1to8 < 1 || stepIndex1to8 > 8)
            return;
        const uint8_t mask = static_cast<uint8_t>(1u << (stepIndex1to8 - 1));
        if (active)
            raw[ARP_STEP_MASK] |= mask;
        else
            raw[ARP_STEP_MASK] &= static_cast<uint8_t>(~mask);
    }

    uint8_t getStepMaskByte() const noexcept
    {
        return raw[ARP_STEP_MASK];
    }

    void setStepMaskByte(uint8_t byteVal) noexcept
    {
        raw[ARP_STEP_MASK] = byteVal;
    }

    /** Desempaqueta 254 B de un payload 7→8 de microKORG. */
    bool unpackFromSysexPayload(const uint8_t *payload, size_t payloadLen)
    {
        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.empty())
            return false;
        if (unpacked.size() < PROGRAM_SIZE)
            return false;
        std::copy_n(unpacked.begin(), PROGRAM_SIZE, raw.begin());
        return true;
    }

    /** Empaqueta los 254 B al payload 7→8 de Korg. */
    bool packToSysexPayload(std::vector<uint8_t> &outPayload) const
    {
        return SysExCodec::pack8to7(raw.data(), PROGRAM_SIZE, outPayload);
    }

    /** Construye la trama SysEx completa para microKORG (F0 42 3n modelID 40 [payload] F7). */
    std::vector<uint8_t>
    buildProgramDump(int channel = 1, uint8_t modelId = MODEL_ID_MICROKORG, uint8_t function = CMD_PROGRAM_DUMP) const
    {
        std::vector<uint8_t> packed;
        if (!packToSysexPayload(packed))
            return {};

        std::vector<uint8_t> sysex;
        sysex.reserve(6 + packed.size());
        sysex.push_back(0xF0);
        sysex.push_back(0x42);  // Korg ID
        sysex.push_back(MS2000HardwareProgram::korgChannelByte(channel));
        sysex.push_back(modelId);   // 0x71 o 0x58
        sysex.push_back(function);  // 0x40
        sysex.insert(sysex.end(), packed.begin(), packed.end());
        sysex.push_back(0xF7);
        return sysex;
    }

#if ABD_HAS_JUCE
    /**
     * Vuelca los valores del volcado microKORG hacia el árbol de parámetros del motor (APVTS).
     */
    void applyToAPVTS(juce::AudioProcessorValueTreeState &apvts) const
    {
        // 1. Aplicar la parte compartida del motor MS2000/microKORG
        MS2000HardwareProgram base;
        base.raw = raw;
        base.applyToAPVTS(apvts);

        // 2. Aplicar los parámetros exclusivos de máscara y longitud rítmica del arpegiador
        auto setParam = [&apvts](const char *paramId, float val)
        {
            if (auto *p = apvts.getParameter(paramId))
                p->setValueNotifyingHost(p->convertTo0to1(val));
        };

        const int lastStep = getLastStep();
        setParam("arpLastStep", static_cast<float>(lastStep));

        for (int s = 1; s <= 8; ++s)
        {
            const std::string id = "arpStep" + std::to_string(s);
            setParam(id.c_str(), getStepMask(s) ? 1.0f : 0.0f);
        }

        // 3. Inhibir el mod sequencer si estuviera activo (paridad microKORG)
        setParam("modSeqOn", 0.0f);
    }

    /**
     * Extrae el estado actual del motor (APVTS) hacia la estructura microKORG.
     */
    void captureFromAPVTS(const juce::AudioProcessorValueTreeState &apvts, const std::string &name = {})
    {
        // 1. Extraer los parámetros comunes de síntesis
        MS2000HardwareProgram base;
        base.raw = raw;
        base.captureFromAPVTS(apvts, name);
        raw = base.raw;

        // 2. Extraer los parámetros exclusivos del arpegiador microKORG
        auto getParam = [&apvts](const char *paramId, float defaultVal) -> float
        {
            if (auto *p = apvts.getParameter(paramId))
                return p->convertFrom0to1(p->getValue());
            return defaultVal;
        };

        const int lastStep = static_cast<int>(std::round(getParam("arpLastStep", 8.0f)));
        setLastStep(lastStep);

        uint8_t maskByte = 0;
        for (int s = 1; s <= 8; ++s)
        {
            const std::string id = "arpStep" + std::to_string(s);
            const float val = getParam(id.c_str(), 1.0f);
            if (val > 0.5f)
                maskByte |= static_cast<uint8_t>(1u << (s - 1));
        }
        setStepMaskByte(maskByte);
    }

    void extractFromAPVTS(const juce::AudioProcessorValueTreeState &apvts, const std::string &name = {})
    {
        captureFromAPVTS(apvts, name);
    }
#endif
};

}  // namespace ABDMS2000
