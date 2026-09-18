#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::SysExCodec;
#include "MS2000ProgramData.h"
#include "ABDSynthsSysEx.h"
#include "SysExManager.h"
#include <vector>
#include <array>
#include <cstring>

namespace ABDMS2000 {

/**
 * @brief Exportador de presets **propios** del plugin a `.syx`.
 *
 * Emite tramas de **ABDSynths** (fabricante `0x7D`, modelo `0x0A`), no tramas de Korg:
 * el bloque que exporta es el preset nativo del motor (`UNPACKED_PROGRAM_SIZE`, hoy
 * 384 B), que no es el programa de 254 B de un MS2000 real. Para volcar un parche *al* MS2000 está
 * `SysExManager::createHardwareProgramDump` / `MS2000HardwareProgram`. Con la cabecera
 * de Korg un equipo real recibiría estos datos y los interpretaría mal.
 *
 * (El nombre del archivo/ clase se conserva por compatibilidad con lo que ya lo
 * incluía; el formato que emite es el de la casa.)
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
        // 1. Extract data into the raw program buffer (UNPACKED_PROGRAM_SIZE)
        MS2000ProgramData progData;
        progData.extractFromAPVTS(apvts, programName.toStdString());

        // 2. Trama propia de ABDSynths (fabricante 0x7D, modelo 0x0A)
        const auto sysex = ABDSynthsSysEx::buildProgramDump(progData);

        outSysExBuffer.reset(); // `MemoryBlock` no tiene clear(): este header no compilaba
        outSysExBuffer.append(sysex.data(), sysex.size());
        juce::ignoreUnused(midiChannel);
    }

    /** Exporta la memoria completa (128 presets nativos con byte dirigido) como volcado de ABDSynths. */
    static void exportBank(const std::array<MS2000ProgramData, 128>& bank,
                           juce::MemoryBlock& outSysExBuffer,
                           int midiChannel = 1)
    {
        const auto sysex = ABDSynthsSysEx::buildBankDump(bank);

        outSysExBuffer.reset();
        outSysExBuffer.append(sysex.data(), sysex.size());
        juce::ignoreUnused(midiChannel);
    }
};

} // namespace ABDMS2000
