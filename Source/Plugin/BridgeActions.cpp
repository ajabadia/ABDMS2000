#include "BridgeActions.h"
#include "PluginProcessor.h"
#include "../Core/AppLogger.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../MIDI/SysExManager.h"
#include "../MIDI/MS2000ProgramData.h"
#include "../State/MS2000PatchBuilder.h"
#include "../DSP/Oscillators/DWGSTables.h"

namespace ABDMS2000 {




BridgeActions::BridgeActions(ABDMS2000AudioProcessor& processor, juce::WebBrowserComponent& browser)
    : processor_(processor), browser_(browser)
{
}

void BridgeActions::handleJsEvent(const juce::var& message)
{
    if (!message.isObject()) return;

    auto action = message.getProperty("action", "").toString();
    ABD_LOG(juce::String("[BRIDGE] handleJsEvent action: ") + action);

    if (action == "setParam")
    {
        auto paramId = message.getProperty("paramId", "").toString();
        float rawValue = static_cast<float>(message.getProperty("value", 0.0));

        if (auto* param = processor_.getAPVTS().getParameter(paramId))
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
        processor_.getEngine().noteOn(1, note, vel);
    }
    else if (action == "noteOff")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.0));
        ABD_LOG(juce::String("[BRIDGE] noteOff: ") + juce::String(note));
        processor_.getEngine().noteOff(1, note, vel);
    }
    else if (action == "pitchBend")
    {
        float val = static_cast<float>(message.getProperty("value", 0.0));
        processor_.getEngine().setPitchBend(val);
    }
    else if (action == "modWheel")
    {
        float val = static_cast<float>(message.getProperty("value", 0.0));
        processor_.getEngine().setModWheel(val);
    }
    else if (action == "setTestTone")
    {
        bool enabled = static_cast<bool>(message.getProperty("enabled", true));
        processor_.getEngine().setTestToneEnabled(enabled);
    }
    else if (action == "setDiagnosticTone")
    {
        int point = static_cast<int>(message.getProperty("point", 0));
        float freq = static_cast<float>(message.getProperty("frequency", 440.0));
        float level = static_cast<float>(message.getProperty("level", 0.25));
        ABD_LOG(juce::String("[BRIDGE] setDiagnosticTone: point=") + juce::String(point) + " freq=" + juce::String(freq) + " level=" + juce::String(level));
        processor_.getEngine().setDiagnosticTone(point, freq, level);
    }
    else if (action == "triggerDiagnosticNote")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.8));
        bool isNoteOn = static_cast<bool>(message.getProperty("isNoteOn", true));
        ABD_LOG(juce::String("[BRIDGE] triggerDiagnosticNote: note=") + juce::String(note) + " vel=" + juce::String(vel) + (isNoteOn ? " ON" : " OFF"));
        if (isNoteOn)
        {
            processor_.getEngine().noteOn(1, note, vel);
        }
        else
        {
            processor_.getEngine().noteOff(1, note, 0.0f, false);
        }
    }
    else if (action == "setDiagnosticBypass")
    {
        auto stage = message.getProperty("stage", "").toString();
        bool enabled = static_cast<bool>(message.getProperty("enabled", false));
        ABD_LOG(juce::String("[BRIDGE] setDiagnosticBypass: stage=") + stage + " enabled=" + (enabled ? "true" : "false"));
        processor_.getEngine().setDiagnosticBypass(stage, enabled);
    }
    else if (action == "resetDiagnosticBypasses")
    {
        ABD_LOG("[BRIDGE] resetDiagnosticBypasses");
        processor_.getEngine().resetAllDiagnosticBypasses();
    }
    else if (action == "allNotesOff")
    {
        ABD_LOG("[BRIDGE] allNotesOff");
        processor_.getEngine().allNotesOff();
    }
    else if (action == "sendMidiCC")
    {
        int cc = static_cast<int>(message.getProperty("cc", 0));
        int val = static_cast<int>(message.getProperty("value", 0));
        processor_.getMIDITelemetry().sendDirectCC(cc, val);
    }
    else if (action == "setMidiChannel")
    {
        int ch = static_cast<int>(message.getProperty("channel", 1));
        processor_.getMIDITelemetry().setMidiChannel(ch);
    }
    else if (action == "importSysexBase64")
    {
        auto b64 = message.getProperty("dataBase64", "").toString();
        juce::MemoryOutputStream mem;
        if (juce::Base64::convertFromBase64(mem, b64))
        {
            auto res = processor_.getSysExManager().parseSysEx(
                static_cast<const uint8_t*>(mem.getData()),
                mem.getDataSize(),
                processor_.getAPVTS()
            );

            juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
            resObj->setProperty("success", res.success);
            resObj->setProperty("programName", juce::String(res.programName));
            resObj->setProperty("programCount", res.programCount);
            resObj->setProperty("errorMessage", juce::String(res.errorMessage));
            sendEventToJs("sysexImportResult", juce::var(resObj.get()));
        }
    }
    else if (action == "exportSysexProgram")
    {
        MS2000ProgramData currentProg;
        currentProg.extractFromAPVTS(processor_.getAPVTS(), "Exported");
        auto bytes = processor_.getSysExManager().createProgramDump(1, currentProg);

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
        MS2000ProgramData currentProg;
        currentProg.extractFromAPVTS(processor_.getAPVTS(), "HexInspect");
        auto bytes = processor_.getSysExManager().createProgramDump(1, currentProg);

        std::string hexText = SysExManager::formatHexDump(bytes.data(), bytes.size());

        juce::DynamicObject::Ptr resObj = new juce::DynamicObject();
        resObj->setProperty("hexDump", juce::String(hexText));
        sendEventToJs("sysexHexDumpResult", juce::var(resObj.get()));
    }
    else if (action == "selectProgram")
    {
        int progIdx = static_cast<int>(message.getProperty("index", 0));
        processor_.setCurrentProgram(progIdx);
        sendFullParamSync();
    }
    else if (action == "initPatch")
    {
        MS2000PatchBuilder::buildInitPatch(processor_.getAPVTS());
        sendFullParamSync();
        sendEventToJs("patchInitialized", juce::var());
    }
    else if (action == "randomizePatch")
    {
        MS2000PatchBuilder::buildMusicalRandomPatch(processor_.getAPVTS());
        sendFullParamSync();
        sendEventToJs("patchRandomized", juce::var());
    }
    else if (action == "requestFullState")
    {
        sendFullParamSync();
    }
}

void BridgeActions::sendFullParamSync()
{
    juce::DynamicObject::Ptr paramsObj = new juce::DynamicObject();
    for (const auto& meta : ParameterRegistry::getAllParameters())
    {
        if (auto* p = processor_.getAPVTS().getRawParameterValue(meta.id))
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

    juce::var msg(obj.get());
    juce::String jsonString = juce::JSON::toString(msg);

    browser_.evaluateJavascript("if (window.__JUCE__ && window.__JUCE__.backend) { window.__JUCE__.backend.emitEvent('event', " + jsonString + "); }");
}

} // namespace ABDMS2000

