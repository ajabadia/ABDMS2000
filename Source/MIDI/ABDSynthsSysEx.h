#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
#include "MS2000ProgramData.h"
using abd::hw::SysExCodec;

namespace ABDMS2000 {

/**
 * @brief SysEx **propio de ABDSynths** para los presets del plugin (bloque nativo).
 *
 * Spec canónica del formato: `DOCS/ABDSynths_SysEx_Spec.md`.
 *
 * Por qué existe: el bloque de programa del plugin (`UNPACKED_PROGRAM_SIZE` = 384 B,
 * layout denso del motor, hoy v2) **no**
 * es el programa del MS2000 físico (254 B, ver `MS2000HardwareProgram.h`). Emitirlo con
 * la cabecera de Korg (`42 3n 58`) sería disfrazar un formato de otro: un MS2000 real lo
 * recibiría y lo interpretaría mal, y la biblioteca del Bank Manager lo abriría con el
 * tamaño y los offsets equivocados. Así que el formato propio se identifica como lo que
 * es: **fabricante ABDSynths, modelo propio**.
 *
 * Identificadores (los mismos que declara el contrato del host en ABDBankManager, que se
 * genera en `Source/Plugin/HostModelId.gen.h`; el id no se repite aquí a propósito):
 *   - Fabricante `0x7D`: reservado por la especificación MIDI 1.0 para uso
 *     **no comercial / educativo** (escuelas, investigación, proyectos abiertos), sin
 *     registro ni cuota. No es válido para un producto publicado: si ABDSynths se
 *     comercializa habría que registrar un ID de 3 bytes (p. ej. `00 20 xx`).
 *   - Modelo `0x0A`: primer modelo de la casa (ABD MS2000). Los
 *     siguientes van detrás.
 *
 * Trama: `F0 7D 0A [comando] [payload 7→8] F7`
 *   - `0x40` Program Data Dump  (un preset nativo de 384 B → payload de 439 B)
 *   - `0x4C` All Data Dump      (N presets nativos)
 *   - `0x10` Program Dump Request, `0x0E` All Data Dump Request
 *   - `0x23` Write Completed (ACK), `0x24` Write Error (NACK): acuse de 5 B que el
 *     receptor devuelve tras **guardar** (o no) un `0x40`/`0x4C`. Llega **después**
 *     del dump, nunca dentro, así que no rompe a un emisor que no lo espere.
 *
 * El empaquetado 7→8 es el de siempre (`abd::hw::SysExCodec`), con el último grupo
 * parcial sin rellenar.
 */
struct ABDSynthsSysEx
{
    static constexpr uint8_t MANUFACTURER_ID = 0x7D;
    static constexpr uint8_t MODEL_BYTE      = 0x0A;

    static constexpr uint8_t CMD_PROGRAM_DUMP     = 0x40;
    static constexpr uint8_t CMD_ALL_DUMP         = 0x4C;
    static constexpr uint8_t CMD_PROGRAM_REQUEST  = 0x10;
    static constexpr uint8_t CMD_ALL_REQUEST      = 0x0E;

    // Acuse de escritura (los mismos códigos que usa el MS2000 real, §2 de
    // `MS2000_SysEx_Spec.md`): `0x23` Write Completed, `0x24` Write Error.
    static constexpr uint8_t CMD_WRITE_COMPLETED  = 0x23;
    static constexpr uint8_t CMD_WRITE_ERROR      = 0x24;

    /** Longitud del payload 7→8 de un bloque nativo (384 B → 439 B). */
    static size_t packedProgramPayloadSize()
    {
        const size_t n = MS2000ProgramData::UNPACKED_PROGRAM_SIZE;
        return (n / 7) * 8 + ((n % 7 == 0) ? 0 : (1 + (n % 7)));
    }

    /** Longitud de la trama de un preset nativo: `F0 7D 0A [cmd] [payload] F7`. */
    static size_t programFrameSize() { return 4 + packedProgramPayloadSize() + 1; }

    /** ¿Es un acuse de escritura de la casa (`F0 7D 0A 23/24 F7`, 5 B)? */
    static bool isWriteAcknowledgement(const uint8_t* data, size_t size) noexcept
    {
        return isABDSynthsSysEx(data, size) && size == 5
            && (data[3] == CMD_WRITE_COMPLETED || data[3] == CMD_WRITE_ERROR);
    }

    /** ¿Es una trama de ABDSynths (y de este modelo)? Acepta las peticiones de 5 B
     *  (`F0 7D 0A [cmd] F7`) además de las tramas con payload. */
    static bool isABDSynthsSysEx(const uint8_t* data, size_t size) noexcept
    {
        return data != nullptr && size >= 5
            && data[0] == 0xF0
            && data[1] == MANUFACTURER_ID
            && data[2] == MODEL_BYTE
            && data[size - 1] == 0xF7;
    }

    /** `F0 7D 0A 40 [bloque nativo empaquetado] F7` desde un preset nativo. */
    static std::vector<uint8_t> buildProgramDump(const MS2000ProgramData& program)
    {
        std::vector<uint8_t> packed;
        if (!SysExCodec::pack8to7(program.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE, packed))
            return {};

        std::vector<uint8_t> sysex;
        sysex.reserve(5 + packed.size());
        sysex.push_back(0xF0);
        sysex.push_back(MANUFACTURER_ID);
        sysex.push_back(MODEL_BYTE);
        sysex.push_back(CMD_PROGRAM_DUMP);
        sysex.insert(sysex.end(), packed.begin(), packed.end());
        sysex.push_back(0xF7);
        return sysex;
    }

    /** `F0 7D 0A 4C [N bloques nativos empaquetados] F7` desde una memoria completa del plugin. */
    template <typename Container>
    static std::vector<uint8_t> buildBankDump(const Container& programs)
    {
        std::vector<uint8_t> allRaw;
        allRaw.reserve(programs.size() * MS2000ProgramData::UNPACKED_PROGRAM_SIZE);
        for (const auto& program : programs)
            allRaw.insert(allRaw.end(), program.rawData.begin(), program.rawData.end());

        std::vector<uint8_t> packed;
        if (allRaw.empty() || !SysExCodec::pack8to7(allRaw.data(), allRaw.size(), packed))
            return {};

        std::vector<uint8_t> sysex;
        sysex.reserve(5 + packed.size());
        sysex.push_back(0xF0);
        sysex.push_back(MANUFACTURER_ID);
        sysex.push_back(MODEL_BYTE);
        sysex.push_back(CMD_ALL_DUMP);
        sysex.insert(sysex.end(), packed.begin(), packed.end());
        sysex.push_back(0xF7);
        return sysex;
    }

    /** `F0 7D 0A [cmd] F7` */
    static std::vector<uint8_t> buildRequest(uint8_t command) { return { 0xF0, MANUFACTURER_ID, MODEL_BYTE, command, 0xF7 }; }

    /** `F0 7D 0A 23 F7` — el bloque llegó y se guardó. */
    static std::vector<uint8_t> buildWriteCompleted() { return buildRequest(CMD_WRITE_COMPLETED); }

    /** `F0 7D 0A 24 F7` — el bloque llegó pero no se pudo guardar. */
    static std::vector<uint8_t> buildWriteError() { return buildRequest(CMD_WRITE_ERROR); }
};

} // namespace ABDMS2000
