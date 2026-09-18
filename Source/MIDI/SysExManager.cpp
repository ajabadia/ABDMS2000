#include "SysExManager.h"
#include "ABDSynthsSysEx.h"
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
    // 5 B = la trama mínima con significado: `F0 7D 0A [cmd] F7` (petición de la casa,
    // sin byte de canal). Las de Korg miden 6 B por el `3n` y isKorgHeader lo exige.
    if (data == nullptr || size < 5)
    {
        result.errorMessage = "Buffer too small for SysEx";
        return result;
    }

    if (data[size - 1] != 0xF7)
    {
        result.errorMessage = "Missing 0xF7 End Of Exclusive terminator";
        return result;
    }

    // Formato propio de ABDSynths (fabricante 0x7D, modelo 0x0A): presets nativos del
    // plugin. NO es un formato Korg y por eso se reconoce antes: una trama con la
    // cabecera de Korg lleva el programa real de 254 B, una de ABDSynths el bloque
    // nativo (v2 = 384 B; la v1 de 128 B se sigue aceptando al leer).
    if (ABDSynthsSysEx::isABDSynthsSysEx(data, size))
    {
        // Acuses de la casa (`F0 7D 0A 23/24 F7`): los manda el receptor de una
        // escritura para decir si la guardó. No se contestan — contestar un acuse
        // sería un ping-pong de acuses.
        if (ABDSynthsSysEx::isWriteAcknowledgement(data, size))
        {
            const bool ok = (data[3] == ABDSynthsSysEx::CMD_WRITE_COMPLETED);
            result.messageType = ok ? SysExMessageType::WriteCompleted : SysExMessageType::WriteError;
            result.success = ok;
            if (!ok)
                result.errorMessage = "The peer reported a write error (ABDSynths 0x24)";
            return result;
        }

        // Un comando de acuse con payload no es un acuse: se para antes de intentar
        // leerlo como preset.
        if (data[3] == ABDSynthsSysEx::CMD_WRITE_COMPLETED || data[3] == ABDSynthsSysEx::CMD_WRITE_ERROR)
        {
            result.errorMessage = "Malformed ABDSynths write acknowledgement (expected 5 bytes)";
            return result;
        }

        // Peticiones de la casa (`F0 7D 0A 10/0E F7`, sin payload): el plugin puede
        // actuar como "servidor de presets" respondiendo con sus tramas propias.
        if (data[3] == ABDSynthsSysEx::CMD_PROGRAM_REQUEST || data[3] == ABDSynthsSysEx::CMD_ALL_REQUEST)
        {
            result.messageType = (data[3] == ABDSynthsSysEx::CMD_ALL_REQUEST)
                ? SysExMessageType::AllDataDumpRequest : SysExMessageType::ProgramDumpRequest;
            result.success = true;
            result.programName = bank_[activeProgramIndex_].getName();
            result.reply = (data[3] == ABDSynthsSysEx::CMD_ALL_REQUEST)
                ? buildAllDataDumpResponse(1)
                : buildProgramDumpResponse(1);
            return result;
        }

        std::vector<uint8_t> nativePayload;
        if (!SysExCodec::unpack7to8(data + 4, size - 5, nativePayload) || nativePayload.empty())
        {
            result.errorMessage = "Failed to unpack the ABDSynths preset payload";
            result.reply = ABDSynthsSysEx::buildWriteError();
            return result;
        }

        // v2 (384 B) es divisible por 384; si no lo es pero sí por 128, es un volcado
        // anterior al Timbre 2 completo (v1). Se lee igual, con el resto a sus defaults.
        size_t blockSize = MS2000ProgramData::UNPACKED_PROGRAM_SIZE;
        size_t nativeCount = nativePayload.size() / blockSize;
        if (nativePayload.size() % blockSize != 0)
        {
            const size_t v1 = MS2000ProgramData::UNPACKED_PROGRAM_SIZE_V1;
            if (nativePayload.size() % v1 != 0)
            {
                result.errorMessage = "ABDSynths payload is not a whole number of native presets";
                result.reply = ABDSynthsSysEx::buildWriteError();
                return result;
            }
            blockSize = v1;
            nativeCount = nativePayload.size() / v1;
        }

        if (nativeCount == 0)
        {
            result.errorMessage = "ABDSynths payload shorter than one native preset";
            result.reply = ABDSynthsSysEx::buildWriteError();
            return result;
        }

        const size_t toLoad = std::min(nativeCount, BANK_SIZE);
        for (size_t p = 0; p < toLoad; ++p)
        {
            MS2000ProgramData prog;
            std::copy_n(nativePayload.data() + (p * blockSize), blockSize, prog.rawData.begin());
            bank_[p] = prog;
        }

        bank_[activeProgramIndex_].applyToAPVTS(apvts);

        result.messageType = (data[3] == ABDSynthsSysEx::CMD_ALL_DUMP)
            ? SysExMessageType::AllDataDump : SysExMessageType::ProgramDump;
        result.success = true;
        result.programCount = static_cast<int>(toLoad);
        result.programName = bank_[activeProgramIndex_].getName();
        // El bloque quedó guardado: acuse para quien lo mandó.
        result.reply = ABDSynthsSysEx::buildWriteCompleted();
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

    /** Acuse del equipo en el canal del emisor (`F0 42 3n 58 23/24 F7`). */
    const auto korgAck = [channel](bool ok)
    {
        return MS2000HardwareProgram::buildWriteAcknowledgement(channel, ok);
    };

    // Acuses **recibidos** del equipo (`F0 42 3n 58 23/24 F7`, 6 B, sin payload): el
    // MS2000 los manda tras guardar un volcado nuestro. Se reconocen para que quien lo
    // envió sepa si se guardó y NO se contestan (contestar un acuse es un ping-pong de
    // acuses). Un `0x23` con payload no es un acuse y cae al final como comando inválido.
    if ((func == MS2000HardwareProgram::CMD_WRITE_COMPLETED || func == MS2000HardwareProgram::CMD_WRITE_ERROR)
        && size == 6)
    {
        const bool ok = (func == MS2000HardwareProgram::CMD_WRITE_COMPLETED);
        result.messageType = ok ? SysExMessageType::WriteCompleted : SysExMessageType::WriteError;
        result.success = ok;
        if (!ok)
            result.errorMessage = "The MS2000 reported a write error (Korg 0x24)";
        return result;
    }

    // Peticiones del equipo (`F0 42 3n 58 10/0E F7`, 6 B, sin payload). Van antes de
    // cualquier manejo de payload: una trama de 6 B no tiene ni comando+payload que
    // aislar. La respuesta Korg sale por `createHardwareProgramDump` (programa real
    // de 254 B); la memoria propia del plugin no viaja con cabecera Korg nunca.
    if (func == 0x10 || func == 0x0E)
    {
        result.messageType = (func == 0x0E)
            ? SysExMessageType::AllDataDumpRequest : SysExMessageType::ProgramDumpRequest;
        result.success = true;
        if (func == 0x10 && hasHardwareProgram())
            result.programName = hardwareProgram_.getName();

        // Respuesta al equipo: el **programa real** de 254 B del canal pedido, nunca el
        // bloque nativo (que no cabría con cabecera Korg). Un `0x0E` (memoria completa)
        // se contesta con el banco real cargado (`F0 42 3n 58 4C ... F7`), si lo hay.
        if (func == 0x10)
        {
            result.reply = createHardwareProgramDump(channel, apvts, bank_[activeProgramIndex_].getName());
        }
        else
        {
            result.reply = buildHardwareBankDumpResponse(channel, apvts);
            result.programCount = static_cast<int>(machineMemoryProgramCount());
        }

        return result;
    }

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
            result.reply = korgAck(false);
            return result;
        }

        // 254 B = programa **real** del MS2000: se aplica al motor con el mapa real
        // (`MS2000HardwareProgram`) y se guardan sus bytes para devolvérselos al
        // equipo sin perder lo que el motor no modela. Cualquier otro tamaño es el
        // bloque nativo del plugin (v2 384 B, v1 128 B).
        if (unpacked.size() % MS2000HardwareProgram::PROGRAM_SIZE == 0)
        {
            MS2000HardwareProgram hw;
            if (!hw.unpackFromSysexPayload(payload, payloadLen))
            {
                result.errorMessage = "Failed to read the real MS2000 program (254 B)";
                result.reply = korgAck(false);
                return result;
            }

            hw.applyToAPVTS(apvts);

            hardwareProgram_ = hw;
            hardwareProgramValid_ = true;

            // La memoria propia del plugin queda espejada desde el motor.
            bank_[activeProgramIndex_].extractFromAPVTS(apvts, hw.getName());

            result.success = true;
            result.programCount = 1;
            result.programName = hw.getName();
            result.reply = korgAck(true);
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
        result.reply = korgAck(true);
        return result;
    }
    else if (func == 0x4C) // All-Data Dump (128 Programs)
    {
        result.messageType = SysExMessageType::AllDataDump;

        std::vector<uint8_t> unpacked;
        if (!SysExCodec::unpack7to8(payload, payloadLen, unpacked) || unpacked.empty())
        {
            result.errorMessage = "Failed to unpack 7-to-8 bit all-data dump payload";
            result.reply = korgAck(false);
            return result;
        }

        // Mismo criterio que en el volcado individual: múltiplos de 254 B son
        // programas reales del equipo; el resto es la memoria nativa del plugin.
        if (unpacked.size() >= MS2000HardwareProgram::PROGRAM_SIZE
            && unpacked.size() % MS2000HardwareProgram::PROGRAM_SIZE == 0)
        {
            const size_t numHardwarePrograms = unpacked.size() / MS2000HardwareProgram::PROGRAM_SIZE;

            hardwareBank_.clear();
            hardwareBank_.reserve(numHardwarePrograms);
            for (size_t p = 0; p < numHardwarePrograms; ++p)
            {
                MS2000HardwareProgram hw;
                std::copy_n(unpacked.data() + (p * MS2000HardwareProgram::PROGRAM_SIZE),
                            MS2000HardwareProgram::PROGRAM_SIZE, hw.raw.begin());
                hardwareBank_.push_back(hw);
            }

            // El motor solo puede sonar un programa a la vez: se aplica el activo.
            const int lastIndex = static_cast<int>(numHardwarePrograms) - 1;
            hardwareProgram_ = hardwareBank_[static_cast<size_t>(std::max(0, std::min(lastIndex, activeProgramIndex_)))];
            hardwareProgramValid_ = true;
            hardwareProgram_.applyToAPVTS(apvts);
            bank_[activeProgramIndex_].extractFromAPVTS(apvts, hardwareProgram_.getName());

            result.success = true;
            result.programCount = static_cast<int>(numHardwarePrograms);
            result.programName = hardwareProgram_.getName();
            result.reply = korgAck(true);
            return result;
        }

        // Bloque nativo: v2 (384 B) o, si el volcado es anterior, v1 (128 B).
        size_t blockSize = MS2000ProgramData::UNPACKED_PROGRAM_SIZE;
        if (unpacked.size() % blockSize != 0
            && unpacked.size() % MS2000ProgramData::UNPACKED_PROGRAM_SIZE_V1 == 0)
            blockSize = MS2000ProgramData::UNPACKED_PROGRAM_SIZE_V1;

        size_t numPrograms = unpacked.size() / blockSize;
        numPrograms = std::min(numPrograms, BANK_SIZE);

        for (size_t p = 0; p < numPrograms; ++p)
        {
            MS2000ProgramData prog;
            const uint8_t* pStart = unpacked.data() + (p * blockSize);
            std::copy_n(pStart, blockSize, prog.rawData.begin());
            bank_[p] = prog;
        }

        // Apply active program to APVTS
        bank_[activeProgramIndex_].applyToAPVTS(apvts);

        result.success = true;
        result.programCount = static_cast<int>(numPrograms);
        result.programName = bank_[activeProgramIndex_].getName();
        result.reply = korgAck(true);
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

/**
 * Trama del preset **propio** del plugin: fabricante ABDSynths, modelo 0x0A.
 *
 * Antes esto emitía una cabecera de Korg (`42 3n 58`) para un bloque nativo que **no**
 * es el programa del MS2000: el equipo real lo recibiría y lo leería mal. El formato
 * del hardware sale de `MS2000HardwareProgram` (254 B).
 *
 * `channel` ya no participa: una trama de ABDSynths no va dirigida a un canal MIDI.
 */
std::vector<uint8_t> SysExManager::createProgramDump(int /*channel*/, const MS2000ProgramData& program) const
{
    return ABDSynthsSysEx::buildProgramDump(program);
}

std::vector<uint8_t> SysExManager::createHardwareProgramDump(int channel,
                                                             const juce::AudioProcessorValueTreeState& apvts,
                                                             const std::string& name)
{
    if (!hardwareProgramValid_)
        hardwareProgram_ = MS2000HardwareProgram{}; // plantilla "INIT Program"

    hardwareProgram_.captureFromAPVTS(apvts, name);
    hardwareProgramValid_ = true;

    return hardwareProgram_.buildProgramDump(channel, 0x40);
}

/** Volcado completo de la memoria **propia** del plugin (presets nativos, `UNPACKED_PROGRAM_SIZE` cada uno). */
std::vector<uint8_t> SysExManager::createAllDataDump(int /*channel*/) const
{
    return ABDSynthsSysEx::buildBankDump(bank_);
}

// ─── Respuestas a peticiones (0x10 / 0x0E) ───────────────────────────────────

std::vector<uint8_t> SysExManager::buildProgramDumpResponse(int /*channel*/) const
{
    return ABDSynthsSysEx::buildProgramDump(bank_[static_cast<size_t>(activeProgramIndex_)]);
}

std::vector<uint8_t> SysExManager::buildAllDataDumpResponse(int /*channel*/) const
{
    return ABDSynthsSysEx::buildBankDump(bank_);
}

/**
 * La memoria completa en formato del **equipo**, para contestar un `0x0E` de Korg:
 *  - el banco real que haya llegado por MIDI (bytes del equipo intactos) con el programa
 *    activo refrescado desde el motor; o
 *  - si nunca llegó memoria del equipo, los 128 presets nativos convertidos a programas
 *    reales de 254 B (conversión aproximada, §6.4).
 */
std::vector<uint8_t> SysExManager::buildHardwareBankDumpResponse(int channel,
                                                                const juce::AudioProcessorValueTreeState& apvts)
{
    std::vector<MS2000HardwareProgram> programs;
    programs.reserve(BANK_SIZE);

    if (!hardwareBank_.empty())
    {
        programs = hardwareBank_;

        // El motor solo es autoridad sobre el programa activo: se refresca con lo que hay
        // en el APVTS (los mismos bytes que devuelve `createHardwareProgramDump`), sin tocar
        // las demás plazas, que se conservan tal como vinieron del equipo.
        const auto active = static_cast<size_t>(std::max(0, std::min(static_cast<int>(BANK_SIZE) - 1,
                                                                    activeProgramIndex_)));
        if (active < programs.size())
            programs[active].captureFromAPVTS(apvts, bank_[active].getName());

        return MS2000HardwareProgram::buildAllDataDump(channel, programs);
    }

    // Sin memoria de equipo: el plugin hace de equipo con la suya (§6.4). La memoria
    // nativa manda (el motor solo es autoridad sobre el preset activo), así que aquí no
    // se refresca nada: cada preset nativo se convierte tal cual está guardado.
    for (const auto& preset : bank_)
        programs.push_back(MS2000HardwareProgram::fromNativeProgram(preset));

    return MS2000HardwareProgram::buildAllDataDump(channel, programs);
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
