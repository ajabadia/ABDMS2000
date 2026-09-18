#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::SysExCodec;
#include "MS2000ProgramData.h"
#include "MS2000HardwareProgram.h"
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
    AllDataDumpRequest, // 0x0E
    WriteCompleted,     // 0x23: el otro extremo guardó lo que le mandamos (ACK)
    WriteError          // 0x24: el otro extremo no pudo guardarlo (NACK)
};

struct SysExParseResult {
    bool success{ false };
    SysExMessageType messageType{ SysExMessageType::Unknown };
    int midiChannel{ 1 };
    int programCount{ 0 };
    std::string programName;
    std::string errorMessage;

    /**
     * Bytes que hay que devolver por el cable por haber recibido este mensaje
     * (vacío si no toca contestar nada):
     *  - el volcado que responde a una petición `0x10`/`0x0E`;
     *  - el acuse `0x23`/`0x24` de una escritura `0x40`/`0x4C` (guardada o no).
     * El transporte MIDI lo manda tal cual; al importar un archivo se ignora.
     */
    std::vector<uint8_t> reply;
};

class SysExManager {
public:
    static constexpr size_t BANK_SIZE = 128; // 128 Programs (Bank A: 1..64, Bank B: 65..128)

    SysExManager();
    ~SysExManager() = default;

    /**
     * Parses raw SysEx bytes (from .syx file or MIDI input).
     *
     * Peticiones reconocidas: `F0 7D 0A 10/0E F7` (ABDSynths) y
     * `F0 42 3n 58 10/0E F7` (Korg) → `ProgramDumpRequest` / `AllDataDumpRequest`
     * con `midiChannel` del canal pedido. La respuesta se construye con
     * `buildProgramDumpResponse` / `buildAllDataDumpResponse`.
     *
     * Escrituras (`0x40`/`0x4C`) de ambas familias: además de guardarlas, el
     * resultado lleva en `reply` el acuse que hay que devolver — `0x23` Write
     * Completed si se guardó, `0x24` Write Error si no. Los acuses **recibidos**
     * (`0x23`/`0x24`) se reconocen (`WriteCompleted` / `WriteError`) y no se contestan.
     */
    SysExParseResult parseSysEx(const uint8_t* data, size_t size, juce::AudioProcessorValueTreeState& apvts);

    /**
     * Respuesta a una `ProgramDumpRequest` **de ABDSynths** (`F0 7D 0A 10 F7`): el
     * preset **propio** del plugin activo, con la cabecera de la casa
     * (`F0 7D 0A 40 ... F7`, 444 B). El canal no participa (trama de ABDSynths);
     * se acepta por simetría con el resto de la API.
     */
    std::vector<uint8_t> buildProgramDumpResponse(int channel) const;

    /**
     * Respuesta a una `AllDataDumpRequest` **de ABDSynths** (`F0 7D 0A 0E F7`): la
     * memoria completa propia (128 presets nativos) con la cabecera de la casa
     * (`F0 7D 0A 4C ... F7`).
     */
    std::vector<uint8_t> buildAllDataDumpResponse(int channel) const;

    /**
     * Respuesta a una `AllDataDumpRequest` **de Korg** (`F0 42 3n 58 0E F7`): la memoria
     * completa con la cabecera del MS2000 (`F0 42 3n 58 4C ... F7`). Dos caminos:
     *
     *  1. Si llegó memoria del equipo (algún All Data Dump `0x4C`), sale **esa** — bytes
     *     intactos, incluido lo que el motor no modela — con el **programa activo refrescado
     *     desde el motor** para que el volcado refleje lo que se está oyendo.
     *  2. Si nunca llegó, el plugin hace de equipo con **su propia memoria**: convierte sus
     *     128 presets nativos a programas reales de 254 B (`MS2000HardwareProgram::
     *     fromNativeProgram`, conversión aproximada y declarada como tal — lo que el motor
     *     no modela sale del INIT Program del equipo). Ver §6.4 de
     *     `DOCS/ABDSynths_SysEx_Spec.md`; falta verificarlo contra hardware real.
     */
    std::vector<uint8_t> buildHardwareBankDumpResponse(int channel,
                                                      const juce::AudioProcessorValueTreeState& apvts);

    /**
     * Cuántos programas lleva la respuesta a un `0x0E` de Korg: los del banco real si
     * llegó alguno del equipo, o los 128 de la memoria propia convertidos a formato real.
     */
    size_t machineMemoryProgramCount() const noexcept
    {
        return hardwareBank_.empty() ? BANK_SIZE : hardwareBank_.size();
    }

    // La respuesta a una `ProgramDumpRequest` **de Korg** (`F0 42 3n 58 10 F7`) es el
    // programa real de 254 B del canal pedido: `createHardwareProgramDump(channel, apvts)`
    // (`F0 42 3n 58 40 ... F7`, 297 B), con lo que el motor no modela viajando intacto.

    /**
     * Parses a Standard MIDI File (.mid) containing embedded SysEx dumps.
     */
    SysExParseResult parseMidiFile(const juce::File& file, juce::AudioProcessorValueTreeState& apvts);

    /**
     * Trama SysEx del preset **propio** del plugin: fabricante ABDSynths (0x7D),
     * modelo 0x0A, payload de `MS2000ProgramData::UNPACKED_PROGRAM_SIZE` (384 B).
     * NO es una trama de MS2000 (para eso está `createHardwareProgramDump`).
     * `channel` no se usa.
     */
    std::vector<uint8_t> createProgramDump(int channel, const MS2000ProgramData& program) const;

    /**
     * Volcado completo de la memoria propia del plugin (128 presets nativos) con la
     * cabecera de ABDSynths. `channel` no se usa.
     */
    std::vector<uint8_t> createAllDataDump(int channel) const;

    /**
     * Formats binary data as a 2-column Hex + ASCII Inspector dump string.
     */
    static std::string formatHexDump(const uint8_t* data, size_t size, size_t bytesPerLine = 16);

    // Bank Management
    const MS2000ProgramData& getProgram(size_t index) const noexcept { return bank_[index % BANK_SIZE]; }
    void setProgram(size_t index, const MS2000ProgramData& prog) noexcept { bank_[index % BANK_SIZE] = prog; }
    const std::array<MS2000ProgramData, BANK_SIZE>& getAllPrograms() const noexcept { return bank_; }
    void setAllPrograms(const std::array<MS2000ProgramData, BANK_SIZE>& bank) noexcept { bank_ = bank; }
    size_t getBankSize() const noexcept { return BANK_SIZE; }

    int getActiveProgramIndex() const noexcept { return activeProgramIndex_; }
    void setActiveProgramIndex(int idx) noexcept { activeProgramIndex_ = std::max(0, std::min(127, idx)); }

    void loadCurrentProgramIntoAPVTS(juce::AudioProcessorValueTreeState& apvts) const;
    void saveAPVTSIntoCurrentProgram(const juce::AudioProcessorValueTreeState& apvts, const std::string& name);

    // ─── Formato del equipo físico (254 B reales) ────────────────────────────
    //
    // El plugin tiene dos formatos y no se mezclan:
    //  - `MS2000ProgramData` (384 B, v2): el preset **propio** del plugin, el sistema
    //    `native` que lee y escribe el ABD Bank Manager embebido. Cabe Timbre 1,
    //    Timbre 2, velocidades, escala/split y los pasos del mod sequence.
    //  - `MS2000HardwareProgram` (254 B): el programa **real** de un MS2000, el que
    //    entra por MIDI desde el equipo y el que se le devuelve.
    //
    // Una trama `0x40` de 254 B está escrita en el formato del hardware y se aplica
    // al motor por el mapa real (`applyToAPVTS`); una de 384 B (o 128 B, la v1) es un
    // preset nativo del plugin. El tamaño los distingue.

    bool hasHardwareProgram() const noexcept { return hardwareProgramValid_; }
    const MS2000HardwareProgram& getHardwareProgram() const noexcept { return hardwareProgram_; }
    void setHardwareProgram(const MS2000HardwareProgram& program) noexcept
    {
        hardwareProgram_ = program;
        hardwareProgramValid_ = true;
    }

    /** Programas de hardware de un All Data Dump (vacío si no ha llegado ninguno). */
    const std::vector<MS2000HardwareProgram>& getHardwareBank() const noexcept { return hardwareBank_; }

    /**
     * Construye la trama **real** del MS2000 para enviarla al equipo: parte de los
     * bytes que vinieron del hardware (si vinieron) y vuelca encima lo que el motor
     * modela, así lo demás viaja intacto.
     */
    std::vector<uint8_t> createHardwareProgramDump(int channel,
                                                   const juce::AudioProcessorValueTreeState& apvts,
                                                   const std::string& name = {});

private:
    std::array<MS2000ProgramData, BANK_SIZE> bank_{};
    int activeProgramIndex_{ 0 };

    MS2000HardwareProgram hardwareProgram_{};
    bool hardwareProgramValid_{ false };
    std::vector<MS2000HardwareProgram> hardwareBank_;

    bool isKorgHeader(const uint8_t* data, size_t size, int& outChannel, uint8_t& outFunction) const noexcept;
};

} // namespace ABDMS2000
