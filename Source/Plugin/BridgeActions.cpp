#include "BridgeActions.h"
#include "SoftwarePresetProtocol.h"
#include "../Core/AppLogger.h"
#include "../Core/SynthEngine.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../MIDI/SysExManager.h"
#include "../MIDI/MS2000ProgramData.h"
#include "../State/MS2000PatchBuilder.h"
#include "../DSP/Oscillators/DWGSTables.h"

namespace ABDMS2000 {




BridgeActions::BridgeActions(BridgeHost& host)
    : host_(host)
{
}

void BridgeActions::handleJsEvent(const juce::var& message)
{
    if (!message.isObject()) return;

    auto action = message.getProperty("action", "").toString();
    ABD_LOG(juce::String("[BRIDGE] handleJsEvent action: ") + action);

    // Anuncio del modelId del host (cppToWebui `hostModel`). Se resuelve antes del
    // despacho concreto para que `requestState` (que no tiene otro handler) y
    // `requestFullState` lo emitan siempre. Ver HostModelAnnouncement.h.
    announceHostModelForAction(action, [this](const juce::var& hostMessage) { emitJsMessage(hostMessage); });

    // Ficha del host (cppToWebui `hostInfo`): responde a `requestHostInfo`. Es lo
    // que permite al Bank Manager embebido saber si el binario que lo hospeda es
    // anterior al anuncio en vez de suponerlo. Ver HostModelAnnouncement.h.
    answerHostInfoForAction(action, [this](const juce::var& hostInfo) { emitJsMessage(hostInfo); });

    if (action == "setParam")
    {
        auto paramId = message.getProperty("paramId", "").toString();
        float rawValue = static_cast<float>(message.getProperty("value", 0.0));

        if (auto* param = host_.getAPVTS().getParameter(paramId))
        {
            float normValue = param->convertTo0to1(rawValue);
            param->setValueNotifyingHost(normValue);
        }
    }
    else if (action == "noteOn")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.8));
        ABD_LOG(juce::String("[BRIDGE] noteOn: ") + juce::String(note) + " vel: " + juce::String(vel));
        host_.getEngine().noteOn(1, note, vel);
    }
    else if (action == "noteOff")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.0));
        ABD_LOG(juce::String("[BRIDGE] noteOff: ") + juce::String(note));
        host_.getEngine().noteOff(1, note, vel);
    }
    else if (action == "pitchBend")
    {
        float val = static_cast<float>(message.getProperty("value", 0.0));
        host_.getEngine().setPitchBend(val);
    }
    else if (action == "modWheel")
    {
        float val = static_cast<float>(message.getProperty("value", 0.0));
        host_.getEngine().setModWheel(val);
    }
    else if (action == "setTestTone")
    {
        bool enabled = static_cast<bool>(message.getProperty("enabled", true));
        host_.getEngine().setTestToneEnabled(enabled);
    }
    else if (action == "setDiagnosticTone")
    {
        int point = static_cast<int>(message.getProperty("point", 0));
        float freq = static_cast<float>(message.getProperty("frequency", 440.0));
        float level = static_cast<float>(message.getProperty("level", 0.25));
        ABD_LOG(juce::String("[BRIDGE] setDiagnosticTone: point=") + juce::String(point) + " freq=" + juce::String(freq) + " level=" + juce::String(level));
        host_.getEngine().setDiagnosticTone(point, freq, level);
    }
    else if (action == "triggerDiagnosticNote")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.8));
        bool isNoteOn = static_cast<bool>(message.getProperty("isNoteOn", true));
        ABD_LOG(juce::String("[BRIDGE] triggerDiagnosticNote: note=") + juce::String(note) + " vel=" + juce::String(vel) + (isNoteOn ? " ON" : " OFF"));
        if (isNoteOn)
        {
            host_.getEngine().noteOn(1, note, vel);
        }
        else
        {
            host_.getEngine().noteOff(1, note, 0.0f, false);
        }
    }
    else if (action == "setDiagnosticBypass")
    {
        auto stage = message.getProperty("stage", "").toString();
        bool enabled = static_cast<bool>(message.getProperty("enabled", false));
        ABD_LOG(juce::String("[BRIDGE] setDiagnosticBypass: stage=") + stage + " enabled=" + (enabled ? "true" : "false"));
        host_.getEngine().setDiagnosticBypass(stage, enabled);
    }
    else if (action == "resetDiagnosticBypasses")
    {
        ABD_LOG("[BRIDGE] resetDiagnosticBypasses");
        host_.getEngine().resetAllDiagnosticBypasses();
    }
    else if (action == "allNotesOff")
    {
        ABD_LOG("[BRIDGE] allNotesOff");
        host_.getEngine().allNotesOff();
    }
    else if (action == "sendMidiCC")
    {
        int cc = static_cast<int>(message.getProperty("cc", 0));
        int val = static_cast<int>(message.getProperty("value", 0));
        host_.getMIDITelemetry().sendDirectCC(cc, val);
    }
    // ─── Puente MIDI de hardware (Bank Manager embebido) ────────────────────
    //
    // Sin esto dentro del plugin no había forma de traer un banco real: el Bank
    // Manager construía el SysEx y lo mandaba a un puerto que nadie atendía, así
    // que el fetch solo podía terminar en timeout. Mismo contrato de payload que
    // el core del Bank Manager standalone (bytes en base64 en "payload").
    else if (action == "hardware.listPorts")
    {
        sendEventToJs("hardware.ports", host_.getHardwareMidiTransport().listPorts());
    }
    else if (action == "hardware.selectPorts")
    {
        const auto outputId = message.getProperty("outputId", "").toString();
        const auto inputId = message.getProperty("inputId", "").toString();
        host_.getHardwareMidiTransport().selectPorts(outputId, inputId);
        ABD_LOG(juce::String("[HWMIDI] selected output=") + outputId + " input=" + inputId);
    }
    else if (action == "hardware.listen")
    {
        auto& transport = host_.getHardwareMidiTransport();
        const bool listening = transport.listen();

        juce::DynamicObject::Ptr ack = new juce::DynamicObject();
        ack->setProperty("listening", listening);
        ack->setProperty("device", transport.getLastDetail());
        sendEventToJs("hardware.listen.ack", juce::var(ack.get()));

        if (! listening)
            sendEventToJs("hardware.error", juce::var(transport.getLastDetail()));
    }
    else if (action == "hardware.send")
    {
        const auto payload = message.getProperty("payload", juce::var());

        if (! payload.isString())
        {
            sendEventToJs("hardware.error",
                          juce::var("hardware.send: payload must be a base64 string"));
        }
        else
        {
            juce::MemoryOutputStream memStream;
            juce::Base64::convertFromBase64(memStream, payload.toString());
            const juce::MemoryBlock block(memStream.getData(), memStream.getDataSize());

            if (block.isEmpty())
            {
                sendEventToJs("hardware.error",
                              juce::var("hardware.send: payload is empty or not valid base64"));
            }
            else
            {
                auto& transport = host_.getHardwareMidiTransport();
                if (transport.send(block))
                    sendEventToJs("hardware.sent", juce::var(static_cast<int>(block.getSize())));
                else
                    sendEventToJs("hardware.error", juce::var(transport.getLastDetail()));
            }
        }
    }
    else if (action == "setMidiChannel")
    {
        int ch = static_cast<int>(message.getProperty("channel", 1));
        host_.getMIDITelemetry().setMidiChannel(ch);
    }
    else if (action == "importSysexBase64")
    {
        auto b64 = message.getProperty("dataBase64", "").toString();
        juce::MemoryOutputStream mem;
        if (juce::Base64::convertFromBase64(mem, b64))
        {
            auto res = host_.getSysExManager().parseSysEx(
                static_cast<const uint8_t*>(mem.getData()),
                mem.getDataSize(),
                host_.getAPVTS()
            );

            juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
            resObj->setProperty("success", res.success);
            resObj->setProperty("programName", juce::String(res.programName));
            resObj->setProperty("programCount", res.programCount);
            resObj->setProperty("errorMessage", juce::String(res.errorMessage));

            // Acuse/volcado que toca devolver a quien mandó la trama, por si la UI
            // quiere reenviarlo al cable (`hardware.send`). El import de un archivo no
            // tiene con quién hablar, así que esto es informativo.
            if (!res.reply.empty())
            {
                juce::MemoryBlock replyBlock(res.reply.data(), res.reply.size());
                resObj->setProperty("replyBase64", replyBlock.toBase64Encoding());
                resObj->setProperty("replySize", static_cast<int>(res.reply.size()));
            }

            sendEventToJs("sysexImportResult", juce::var(resObj.get()));
        }
    }
    else if (action == "exportSysexProgram")
    {
        // Al equipo se le envía el programa **real** de 254 B, no el bloque nativo de
        // 128 B: partiendo de lo que vino del hardware, lo que el motor no modela
        // viaja intacto (MS2000HardwareProgram).
        auto bytes = host_.getSysExManager().createHardwareProgramDump(1, host_.getAPVTS(), "Exported");

        juce::MemoryBlock mb(bytes.data(), bytes.size());
        auto b64 = mb.toBase64Encoding();

        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
        resObj->setProperty("base64", b64);
        resObj->setProperty("size", static_cast<int>(bytes.size()));
        sendEventToJs("sysexProgramExported", juce::var(resObj.get()));
    }
    else if (action == "getWavetableCatalog")
    {
        const auto& catalog = DWGSTables::getCatalog();
        juce::Array<juce::var> catalogArray;
        for (const auto& entry : catalog)
        {
            juce::DynamicObject::Ptr item = new juce::DynamicObject();
            item->setProperty("slot", static_cast<int>(entry.slot));
            item->setProperty("name", juce::String(entry.name));
            item->setProperty("cat", static_cast<int>(entry.category));
            catalogArray.add(juce::var(item.get()));
        }
        juce::DynamicObject::Ptr res = new juce::DynamicObject();
        res->setProperty("catalog", catalogArray);
        sendEventToJs("wavetableCatalog", juce::var(res.get()));
    }
    else if (action == "getSysExHexDump")
    {
        // El inspector muestra lo que saldría hacia el MS2000 físico: la trama real.
        auto bytes = host_.getSysExManager().createHardwareProgramDump(1, host_.getAPVTS(), "HexInspect");

        std::string hexText = SysExManager::formatHexDump(bytes.data(), bytes.size());

        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
        resObj->setProperty("hexDump", juce::String(hexText));
        sendEventToJs("sysexHexDumpResult", juce::var(resObj.get()));
    }
    else if (action == "selectProgram")
    {
        int progIdx = static_cast<int>(message.getProperty("index", 0));
        host_.setCurrentProgram(progIdx);
        sendFullParamSync();
    }
    else if (action == "initPatch")
    {
        MS2000PatchBuilder::buildInitPatch(host_.getAPVTS());
        sendFullParamSync();
        sendEventToJs("patchInitialized", juce::var());
    }
    else if (action == "randomizePatch")
    {
        MS2000PatchBuilder::buildMusicalRandomPatch(host_.getAPVTS());
        sendFullParamSync();
        sendEventToJs("patchRandomized", juce::var());
    }
    else if (action == "getRawProgramData")
    {
        juce::String name = message.getProperty("name", "Active Patch").toString();
        MS2000ProgramData currentProg;
        currentProg.extractFromAPVTS(host_.getAPVTS(), name.toStdString());
        juce::MemoryBlock mb(currentProg.rawData.data(), currentProg.rawData.size());

        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
        resObj->setProperty("dataBase64", mb.toBase64Encoding());
        resObj->setProperty("name", juce::String(currentProg.getName()));
        resObj->setProperty("size", static_cast<int>(currentProg.rawData.size()));
        sendEventToJs("rawProgramDataResponse", juce::var(resObj.get()));
    }
    else if (action == "setRawProgramData")
    {
        auto b64 = message.getProperty("dataBase64", "").toString();
        juce::MemoryOutputStream mem;
        if (juce::Base64::convertFromBase64(mem, b64) && mem.getDataSize() >= MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
        {
            MS2000ProgramData prog;
            std::memcpy(prog.rawData.data(), mem.getData(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE);
            prog.applyToAPVTS(host_.getAPVTS());
            host_.changeProgramName(host_.getCurrentProgram(), juce::String(prog.getName()));
            sendFullParamSync();

            juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
            resObj->setProperty("success", true);
            resObj->setProperty("name", juce::String(prog.getName()));
            sendEventToJs("rawProgramDataApplied", juce::var(resObj.get()));
        }
    }
    else if (action == "getAllProgramsData")
    {
        auto allBytes = host_.getSysExManager().getAllPrograms();
        juce::MemoryBlock mb(allBytes.data(), allBytes.size());

        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
        resObj->setProperty("dataBase64", mb.toBase64Encoding());
        resObj->setProperty("totalPatches", 128);
        // Este volcado es la memoria **propia** del plugin (preset nativo de 128 B),
        // no el formato del MS2000 físico (254 B), que va por `createHardwareProgramDump`.
        resObj->setProperty("bytesPerPatch", static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE));
        sendEventToJs("allProgramsDataResponse", juce::var(resObj.get()));
    }
    // ─── Transporte de software (SoftwarePresetProtocol.h) ────────────────────
    // El ABD Bank Manager embebido lee y escribe los presets de este synth con los
    // mismos verbos que con un equipo físico (preset.read / bank.read / preset.write
    // / bank.write) más `preset.capture`. Ver
    // ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md §6.2/§6.4 y §7.
    //
    // Las acciones propias del plugin (`getRawProgramData`, `importSysexBase64`…)
    // siguen existiendo para la WebUI del sintetizador; lo que cambia es que el Bank
    // Manager ya no necesita inventarse una por synth.
    else if (action == SoftwarePresetProtocol::kReadPreset)
    {
        handlePresetRead(message);
    }
    else if (action == SoftwarePresetProtocol::kReadBank)
    {
        handleBankRead(message);
    }
    else if (action == SoftwarePresetProtocol::kWritePreset)
    {
        handlePresetWrite(message);
    }
    else if (action == SoftwarePresetProtocol::kWriteBank)
    {
        handleBankWrite(message);
    }
    else if (action == SoftwarePresetProtocol::kCapturePreset)
    {
        handlePresetCapture(message);
    }
    else if (action == "requestFullState")
    {
        sendFullParamSync();
    }
    else if (action == "toggleScope")
    {
        ABD_LOG("[BRIDGE] toggleScope");
        if (onToggleScope_)
            onToggleScope_();
    }
}

void BridgeActions::sendHostModel()
{
    emitJsMessage(hostModelMessage());
}

void BridgeActions::emitJsMessage(const juce::var& message)
{
    if (jsEventSink != nullptr)
        jsEventSink(message);
}

// ─── Transporte de software ──────────────────────────────────────────────────
//
// El payload que cruza el bridge son los bytes del motor (128 B, MS2000ProgramData)
// en base64 estándar; el `system` que declara este host es `native` (decisión §7.1
// del modelo de transporte: el SysEx del MS2000 es un formato de 288 B con otro
// layout, así que no se finge que este synth lo hable por software).

bool BridgeActions::isValidProgramSlot(int slot)
{
    return slot >= 0 && slot < static_cast<int>(SysExManager::BANK_SIZE);
}

juce::MemoryBlock BridgeActions::readProgramBlob(int slot, juce::String& name) const
{
    const auto& program = host_.getSysExManager().getProgram(static_cast<size_t>(slot));
    name = juce::String(program.getName()).trim();
    return juce::MemoryBlock(program.rawData.data(), program.rawData.size());
}

bool BridgeActions::writeProgramBlob(int slot, const juce::MemoryBlock& blob, const juce::String& name,
                                     PresetWriteMode mode, juce::String& error, bool syncParams)
{
    if (blob.getSize() < MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
    {
        error = "payload must be at least "
              + juce::String(static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE)) + " bytes";
        return false;
    }

    MS2000ProgramData program;
    std::memcpy(program.rawData.data(), blob.getData(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE);
    if (name.isNotEmpty())
        program.setName(name.toStdString());

    bool touchedEngine = false;

    if (mode == PresetWriteMode::Audition)
    {
        // Audición: solo el motor (buffer de edición); la memoria del synth no se toca.
        program.applyToAPVTS(host_.getAPVTS());
        touchedEngine = true;
    }
    else
    {
        host_.getSysExManager().setProgram(static_cast<size_t>(slot), program);

        if (mode == PresetWriteMode::StoreAndLoad)
        {
            // `setCurrentProgram()` guarda el índice activo y carga el programa en el
            // motor: el patch recibido pasa a ser el que suena.
            host_.setCurrentProgram(slot);
            touchedEngine = true;
        }
    }

    if (touchedEngine && syncParams)
        sendFullParamSync();

    return true;
}

void BridgeActions::sendPresetError(const juce::var& message, const juce::String& code, const juce::String& reason)
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("code", code);
    data->setProperty("message", reason);

    ABD_LOG(juce::String("[BRIDGE] preset.error(") + code + "): " + reason);
    sendEventToJs(SoftwarePresetProtocol::kError, juce::var(data.get()));
}

void BridgeActions::handlePresetRead(const juce::var& message)
{
    if (! SoftwarePresetProtocol::isSupportedSystem(message.getProperty("system", "").toString()))
    {
        sendPresetError(message, SoftwarePresetProtocol::kUnsupportedAction,
                        juce::String("This synth exposes the '") + SoftwarePresetProtocol::kPresetSystem
                            + "' preset system");
        return;
    }

    const int slot = static_cast<int>(message.getProperty("slot", 0));
    if (! isValidProgramSlot(slot))
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidSlot,
                        "slot " + juce::String(slot) + " is outside the synth memory (0..127)");
        return;
    }

    juce::String name;
    const auto blob = readProgramBlob(slot, name);

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("slot", slot);
    data->setProperty("name", name);
    data->setProperty("payload", SoftwarePresetProtocol::encodePayload(blob.getData(), blob.getSize()));

    sendEventToJs(SoftwarePresetProtocol::kPresetData, juce::var(data.get()));
}

void BridgeActions::handleBankRead(const juce::var& message)
{
    if (! SoftwarePresetProtocol::isSupportedSystem(message.getProperty("system", "").toString()))
    {
        sendPresetError(message, SoftwarePresetProtocol::kUnsupportedAction,
                        juce::String("This synth exposes the '") + SoftwarePresetProtocol::kPresetSystem
                            + "' preset system");
        return;
    }

    juce::Array<juce::var> slots;
    const int count = static_cast<int>(host_.getSysExManager().getBankSize());
    for (int slot = 0; slot < count; ++slot)
    {
        juce::String name;
        const auto blob = readProgramBlob(slot, name);

        juce::DynamicObject::Ptr item = new juce::DynamicObject();
        item->setProperty("slot", slot);
        item->setProperty("name", name);
        item->setProperty("payload", SoftwarePresetProtocol::encodePayload(blob.getData(), blob.getSize()));
        slots.add(juce::var(item.get()));
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("slots", slots);

    sendEventToJs(SoftwarePresetProtocol::kBankData, juce::var(data.get()));
}

void BridgeActions::handlePresetWrite(const juce::var& message)
{
    if (! SoftwarePresetProtocol::isSupportedSystem(message.getProperty("system", "").toString()))
    {
        sendPresetError(message, SoftwarePresetProtocol::kUnsupportedAction,
                        juce::String("This synth exposes the '") + SoftwarePresetProtocol::kPresetSystem
                            + "' preset system");
        return;
    }

    const int slot = static_cast<int>(message.getProperty("slot", -1));
    if (! isValidProgramSlot(slot))
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidSlot,
                        "slot " + juce::String(slot) + " is outside the synth memory (0..127)");
        return;
    }

    juce::MemoryBlock blob;
    if (! SoftwarePresetProtocol::decodePayload(message.getProperty("payload", "").toString(), blob))
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidPayload,
                        "preset.write arrived without a decodable payload");
        return;
    }

    const bool audition = static_cast<bool>(message.getProperty("audition", false));
    juce::String error;
    const auto mode = audition ? PresetWriteMode::Audition : PresetWriteMode::StoreAndLoad;
    if (! writeProgramBlob(slot, blob, message.getProperty("name", "").toString(), mode, error))
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidPayload, error);
        return;
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("slot", slot);
    data->setProperty("name", juce::String(host_.getSysExManager().getProgram(static_cast<size_t>(slot)).getName()).trim());
    data->setProperty("audition", audition);

    sendEventToJs(SoftwarePresetProtocol::kPresetWritten, juce::var(data.get()));
}

void BridgeActions::handleBankWrite(const juce::var& message)
{
    if (! SoftwarePresetProtocol::isSupportedSystem(message.getProperty("system", "").toString()))
    {
        sendPresetError(message, SoftwarePresetProtocol::kUnsupportedAction,
                        juce::String("This synth exposes the '") + SoftwarePresetProtocol::kPresetSystem
                            + "' preset system");
        return;
    }

    const auto slots = message.getProperty("slots", juce::var());
    if (! slots.isArray())
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidPayload,
                        "bank.write needs a 'slots' array");
        return;
    }

    int written = 0;
    juce::StringArray failures;
    for (const auto& item : *slots.getArray())
    {
        const int slot = static_cast<int>(item.getProperty("slot", -1));
        if (! isValidProgramSlot(slot))
        {
            failures.add("slot " + juce::String(slot) + " out of range");
            continue;
        }

        juce::MemoryBlock blob;
        if (! SoftwarePresetProtocol::decodePayload(item.getProperty("payload", "").toString(), blob))
        {
            failures.add("slot " + juce::String(slot) + " without payload");
            continue;
        }

        juce::String error;
        // Un lote escribe memoria, no lo que suena: no se toca el patch activo ni el
        // motor, y la WebUI se sincroniza una sola vez (no 128 veces).
        if (! writeProgramBlob(slot, blob, item.getProperty("name", "").toString(),
                              PresetWriteMode::StoreOnly, error, false))
        {
            failures.add("slot " + juce::String(slot) + ": " + error);
            continue;
        }

        ++written;
    }

    if (written == 0 && ! failures.isEmpty())
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidPayload, failures.joinIntoString("; "));
        return;
    }

    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("count", written);
    if (! failures.isEmpty())
        data->setProperty("warnings", failures.joinIntoString("; "));

    sendEventToJs(SoftwarePresetProtocol::kBankWritten, juce::var(data.get()));
}

void BridgeActions::handlePresetCapture(const juce::var& message)
{
    if (! SoftwarePresetProtocol::isSupportedSystem(message.getProperty("system", "").toString()))
    {
        sendPresetError(message, SoftwarePresetProtocol::kUnsupportedAction,
                        juce::String("This synth exposes the '") + SoftwarePresetProtocol::kPresetSystem
                            + "' preset system");
        return;
    }

    const auto slotValue = message.getProperty("slot", juce::var());
    const bool hasSlot = ! slotValue.isVoid();
    const int slot = hasSlot ? static_cast<int>(slotValue) : -1;
    if (hasSlot && ! isValidProgramSlot(slot))
    {
        sendPresetError(message, SoftwarePresetProtocol::kInvalidSlot,
                        "slot " + juce::String(slot) + " is outside the synth memory (0..127)");
        return;
    }

    // El preset activo: el nombre se toma del programa activo para no devolver una
    // etiqueta genérica al Bank Manager.
    const int activeIndex = host_.getSysExManager().getActiveProgramIndex();
    const juce::String activeName =
        juce::String(host_.getSysExManager().getProgram(static_cast<size_t>(activeIndex)).getName()).trim();

    MS2000ProgramData program;
    program.extractFromAPVTS(host_.getAPVTS(), activeName.isEmpty() ? "Captured" : activeName.toStdString());

    // Con `slot`, además se guarda en la memoria del synth virtual; sin él, la
    // captura solo devuelve el preset activo (guardarlo es cosa del Bank Manager).
    if (hasSlot)
    {
        host_.getSysExManager().setProgram(static_cast<size_t>(slot), program);
        host_.changeProgramName(slot, juce::String(program.getName()).trim());
    }

    juce::MemoryBlock blob(program.rawData.data(), program.rawData.size());
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    const auto requestId = message.getProperty("requestId", juce::var());
    if (! requestId.isVoid())
        data->setProperty("requestId", requestId);
    data->setProperty("slot", hasSlot ? slot : activeIndex);
    data->setProperty("name", juce::String(program.getName()).trim());
    data->setProperty("payload", SoftwarePresetProtocol::encodePayload(blob.getData(), blob.getSize()));

    sendEventToJs(SoftwarePresetProtocol::kPresetCaptured, juce::var(data.get()));
}

void BridgeActions::sendFullParamSync()
{
    juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
    for (const auto& meta : ParameterRegistry::getAllParameters())
    {
        if (auto* p = host_.getAPVTS().getRawParameterValue(meta.id))
        {
            paramsObj->setProperty(juce::Identifier(meta.id), static_cast<double>(p->load()));
        }
    }
    sendEventToJs("syncAllParams", juce::var(paramsObj.get()));
}

void BridgeActions::sendEventToJs(const juce::String& eventType, const juce::var& payload)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", eventType);
    obj->setProperty("data", payload);

    emitJsMessage(juce::var(obj.get()));
}

} // namespace ABDMS2000

