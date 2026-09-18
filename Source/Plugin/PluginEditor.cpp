#include "PluginEditor.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../Core/AppLogger.h"
#include "../Core/BuildVersion.h"

namespace ABDMS2000 {

namespace
{
// Removes the WebView2 user-data folder (e.g. ABDMS2000.exe.WebView2 next to the
// executable) whenever the embedded WebUI build changed, so the standalone never
// serves stale cached CSS/JS from a previous version. Only runs for the ABDMS2000
// standalone executable; a plugin hosted in a DAW must never touch the host's cache.
void purgeStaleWebView2CacheIfStandalone()
{
    juce::File exeFile = juce::File::getSpecialLocation(juce::File::currentExecutableFile);
    if (!exeFile.getFileName().startsWithIgnoreCase("ABDMS2000"))
        return;

    juce::File exeDir = exeFile.getParentDirectory();
    juce::File webView2Folder = exeDir.getChildFile(exeFile.getFileName() + ".WebView2");
    juce::File markerFile = exeDir.getChildFile(exeFile.getFileName() + ".WebView2.buildver");

    const juce::String currentBuild = juce::String(ABDMS2000::kBuildNumber)
                                    + "|" + juce::String(ABDMS2000::kBuildTimestamp);
    const juce::String lastBuild = markerFile.existsAsFile()
                                   ? markerFile.loadFileAsString().trim()
                                   : juce::String();

    if (currentBuild != lastBuild)
    {
        if (webView2Folder.exists())
        {
            ABD_LOG(juce::String("[EDITOR] Build changed (") + lastBuild + " -> " + currentBuild
                    + "): purging WebView2 cache at " + webView2Folder.getFullPathName());
            const bool ok = webView2Folder.deleteRecursively();
            ABD_LOG(juce::String("[EDITOR] WebView2 cache purge ") + (ok ? "OK" : "FAILED"));
        }
        markerFile.replaceWithText(currentBuild);
    }
    else
    {
        ABD_LOG(juce::String("[EDITOR] WebView2 cache up-to-date for build ") + currentBuild);
    }
}
} // namespace


ABDMS2000AudioProcessorEditor::ABDMS2000AudioProcessorEditor(ABDMS2000AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor_(p)
{
    ABD_LOG("[EDITOR] ABDMS2000AudioProcessorEditor constructor start.");
    setupWebBrowserBindings();

    if (webView_ != nullptr)
    {
        ABD_LOG("[EDITOR] Adding webView_ to editor component.");
        addAndMakeVisible(*webView_);
    }
    else
    {
        ABD_LOG("[EDITOR] ERROR: webView_ is null after setupWebBrowserBindings!");
    }

    ABD_LOG("[EDITOR] Creating BridgeActions.");
    bridge_ = std::make_unique<BridgeActions>(audioProcessor_);
    // El bridge no conoce la WebView: el Editor le inyecta el sumidero de eventos JS.
    bridge_->setJsMessageSink([this](const juce::var& message) { emitEventToWebView(message); });
    bridge_->setOnToggleScope([this]() { toggleScopeWindow(); });

    // Hardware MIDI del Bank Manager embebido: sin esto su puente MIDI no tenía
    // otro extremo y el fetch de un banco real solo podía morir por timeout.
    bindHardwareMidi();

    audioProcessor_.getMIDITelemetry().setActivityCallback([this](const MidiActivityEvent& ev) {
        juce::MessageManager::callAsync([this, ev]() {
            if (bridge_ != nullptr)
            {
                juce::DynamicObject::Ptr obj = new juce::DynamicObject();
                obj->setProperty(juce::Identifier("channel"), ev.channel);
                obj->setProperty(juce::Identifier("cc"), ev.ccNumber);
                obj->setProperty(juce::Identifier("value"), ev.value);
                obj->setProperty(juce::Identifier("isIncoming"), ev.isIncoming);
                if (ev.paramId != nullptr)
                    obj->setProperty(juce::Identifier("paramId"), juce::String(ev.paramId));

                bridge_->sendEventToJs("midiActivity", juce::var(obj.get()));
            }
        });
    });

    auto rootUrl = juce::WebBrowserComponent::getResourceProviderRoot();
    ABD_LOG(juce::String("[EDITOR] Navigating webView_ to URL: ") + rootUrl);
    webView_->goToURL(rootUrl);

    ABD_LOG("[EDITOR] Setting window size (1080, 680).");
    setSize(1080, 680);
    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1200);

    ABD_LOG("[EDITOR] Starting telemetry timer at 30Hz.");
    startTimerHz(30);
    ABD_LOG("[EDITOR] ABDMS2000AudioProcessorEditor constructor finished successfully.");
}

ABDMS2000AudioProcessorEditor::~ABDMS2000AudioProcessorEditor()
{
    ABD_LOG("[EDITOR] ABDMS2000AudioProcessorEditor destructor start.");
    audioProcessor_.getMIDITelemetry().setActivityCallback(nullptr);

    // El Editor es quien abrió los dispositivos: al irse, el transporte deja de
    // tener hardware (el Bank Manager sigue abierto en la WebUI, así que un
    // `hardware.send` posterior debe fallar con motivo, no en silencio).
    audioProcessor_.getHardwareMidiTransport().unbind();
    if (hardwareMidiInput_ != nullptr)
        hardwareMidiInput_->stop();
    hardwareMidiInput_.reset();
    hardwareMidiOutput_.reset();

    stopTimer();
    ABD_LOG("[EDITOR] ABDMS2000AudioProcessorEditor destructor finished.");
}

void ABDMS2000AudioProcessorEditor::bindHardwareMidi()
{
    auto& transport = audioProcessor_.getHardwareMidiTransport();

    transport.setPortListFunction([this]() { return listHardwareMidiPorts(); });

    transport.bind(
        [this](const juce::MemoryBlock& bytes) -> HardwareMidiTransport::Outcome
        {
            if (! openHardwareMidiOutput())
                return { false, "Selected MIDI output device is unavailable" };

            hardwareMidiOutput_->sendMessageNow(
                juce::MidiMessage(bytes.getData(), static_cast<int>(bytes.getSize())));

            ABD_LOG(juce::String("[HWMIDI] sent ") + juce::String(static_cast<int>(bytes.getSize()))
                    + " bytes to " + hardwareMidiOutput_->getName());
            return { true, hardwareMidiOutput_->getName() };
        },
        [this]() -> HardwareMidiTransport::Outcome
        {
            if (! openHardwareMidiInput())
                return { false, "Selected MIDI input device is unavailable" };

            return { true, hardwareMidiInput_->getName() };
        },
        [this](const juce::var& message)
        {
            // Los bytes llegan en el hilo de MIDI; el WebView es de mensajes.
            juce::MessageManager::callAsync([this, message]() { emitEventToWebView(message); });
        });
}

juce::var ABDMS2000AudioProcessorEditor::listHardwareMidiPorts() const
{
    juce::DynamicObject::Ptr result = new juce::DynamicObject();
    juce::Array<juce::var> outputs;
    juce::Array<juce::var> inputs;

    for (const auto& device : juce::MidiOutput::getAvailableDevices())
    {
        juce::DynamicObject::Ptr port = new juce::DynamicObject();
        port->setProperty("identifier", device.identifier);
        port->setProperty("name", device.name);
        outputs.add(juce::var(port.get()));
    }
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        juce::DynamicObject::Ptr port = new juce::DynamicObject();
        port->setProperty("identifier", device.identifier);
        port->setProperty("name", device.name);
        inputs.add(juce::var(port.get()));
    }

    result->setProperty("outputs", outputs);
    result->setProperty("inputs", inputs);
    return juce::var(result.get());
}

bool ABDMS2000AudioProcessorEditor::isHardwareOutputAvailable(const juce::String& identifier) const
{
    for (const auto& device : juce::MidiOutput::getAvailableDevices())
        if (device.identifier == identifier)
            return true;
    return false;
}

bool ABDMS2000AudioProcessorEditor::isHardwareInputAvailable(const juce::String& identifier) const
{
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (device.identifier == identifier)
            return true;
    return false;
}

void ABDMS2000AudioProcessorEditor::refreshHardwareMidiAvailability()
{
    auto& transport = audioProcessor_.getHardwareMidiTransport();
    bool changed = false;

    if (hardwareMidiOutput_ != nullptr && ! isHardwareOutputAvailable(openedHardwareOutputId_))
    {
        hardwareMidiOutput_.reset();
        openedHardwareOutputId_.clear();
        changed = true;
        if (bridge_ != nullptr)
            bridge_->sendEventToJs("hardware.error", juce::var("Selected MIDI output device disappeared"));
    }

    if (hardwareMidiInput_ != nullptr && ! isHardwareInputAvailable(openedHardwareInputId_))
    {
        hardwareMidiInput_->stop();
        hardwareMidiInput_.reset();
        openedHardwareInputId_.clear();
        transport.markInputUnavailable("Selected MIDI input device disappeared");
        changed = true;
        if (bridge_ != nullptr)
            bridge_->sendEventToJs("hardware.error", juce::var("Selected MIDI input device disappeared"));
    }

    if (changed && bridge_ != nullptr)
        bridge_->sendEventToJs("hardware.ports", listHardwareMidiPorts());
}

bool ABDMS2000AudioProcessorEditor::openHardwareMidiOutput()
{
    if (hardwareMidiOutput_ != nullptr)
    {
        const auto selectedId = audioProcessor_.getHardwareMidiTransport().getSelectedOutputId();
        if (selectedId == openedHardwareOutputId_ && isHardwareOutputAvailable(selectedId))
            return true;
        hardwareMidiOutput_.reset();
        openedHardwareOutputId_.clear();
    }

    const auto identifier = audioProcessor_.getHardwareMidiTransport().getSelectedOutputId();
    if (identifier.isEmpty() || ! isHardwareOutputAvailable(identifier))
        return false;

    for (const auto& candidate : juce::MidiOutput::getAvailableDevices())
    {
        if (candidate.identifier == identifier)
        {
            hardwareMidiOutput_ = juce::MidiOutput::openDevice(candidate.identifier);
            if (hardwareMidiOutput_ == nullptr)
            {
                openedHardwareOutputId_.clear();
                return false;
            }
            openedHardwareOutputId_ = candidate.identifier;
            return true;
        }
    }
    return false;
}

bool ABDMS2000AudioProcessorEditor::openHardwareMidiInput()
{
    if (hardwareMidiInput_ != nullptr)
    {
        const auto selectedId = audioProcessor_.getHardwareMidiTransport().getSelectedInputId();
        if (selectedId == openedHardwareInputId_ && isHardwareInputAvailable(selectedId))
            return true;
        hardwareMidiInput_->stop();
        hardwareMidiInput_.reset();
        openedHardwareInputId_.clear();
    }

    const auto identifier = audioProcessor_.getHardwareMidiTransport().getSelectedInputId();
    if (identifier.isEmpty() || ! isHardwareInputAvailable(identifier))
        return false;

    for (const auto& candidate : juce::MidiInput::getAvailableDevices())
    {
        if (candidate.identifier == identifier)
        {
            hardwareMidiInput_ = juce::MidiInput::openDevice(candidate.identifier, this);
            if (hardwareMidiInput_ == nullptr)
                return false;
            hardwareMidiInput_->start();
            openedHardwareInputId_ = candidate.identifier;
            return true;
        }
    }
    return false;
}


void ABDMS2000AudioProcessorEditor::handleIncomingMidiMessage(juce::MidiInput* /*source*/,
                                                             const juce::MidiMessage& message)
{
    if (! message.isSysEx())
        return;

    const auto* raw = message.getSysExData();
    const auto size = static_cast<int>(message.getSysExDataSize());
    if (raw == nullptr || size <= 0)
        return;

    // El transporte filtra: hasta que el Bank Manager no manda `hardware.listen`
    // no hay nadie suscrito al otro lado.
    audioProcessor_.getHardwareMidiTransport().deliverIncoming(
        juce::MemoryBlock(raw, static_cast<std::size_t>(size)));
}

void ABDMS2000AudioProcessorEditor::emitEventToWebView(const juce::var& message)
{
    if (webView_ == nullptr)
        return;

    // Direccion nativo -> JS de JUCE 8: `emitEventIfBrowserIsVisible(eventId, object)`,
    // que es lo que dispara los `window.__JUCE__.backend.addEventListener(eventId, ...)`
    // del WebUI.
    //
    // OJO: `evaluateJavascript("window.__JUCE__.backend.emitEvent('event', ...)")`
    // NO sirve — el `emitEvent` del backend JS va en la direccion contraria
    // (JS -> nativo, `withEventListener`), asi que el mensaje se descartaba y todo
    // el canal cppToWebui (hostModel, syncAllParams, state, programData...) moria
    // en silencio: el Bank Manager embebido nunca recibia el modelId del host.
    webView_->emitEventIfBrowserIsVisible("event", message);
}

void ABDMS2000AudioProcessorEditor::showScopeWindow()
{
    if (scopeWindow_ == nullptr)
    {
        scopeWindow_ = std::make_unique<ScopeFloatingWindow>(
            audioProcessor_.getEngine(),
            [this]() { if (scopeWindow_) scopeWindow_->setVisible(false); }
        );
        scopeWindow_->addToDesktop();
    }
    scopeWindow_->syncSampleRate();
    scopeWindow_->onWindowShown();
    scopeWindow_->setVisible(true);
    scopeWindow_->toFront(true);
}

void ABDMS2000AudioProcessorEditor::toggleScopeWindow()
{
    if (scopeWindow_ != nullptr && scopeWindow_->isVisible())
        scopeWindow_->setVisible(false);
    else
        showScopeWindow();
}


void ABDMS2000AudioProcessorEditor::setupWebBrowserBindings()
{
    purgeStaleWebView2CacheIfStandalone();
    ABD_LOG("[EDITOR] setupWebBrowserBindings: Configuring WebBrowserComponent options...");
    auto options = juce::WebBrowserComponent::Options{}
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled(true)
        .withResourceProvider(pluginResourceProvider)
        .withEventListener("nativeEvent", [this](const juce::var& msg) {
            ABD_LOG(juce::String("[BRIDGE] nativeEvent received from JS: ") + juce::JSON::toString(msg));
            if (bridge_ != nullptr)
                bridge_->handleJsEvent(msg);
        });

    ABD_LOG("[EDITOR] setupWebBrowserBindings: Instantiating WebBrowserComponent (WebView2)...");
    webView_ = std::make_unique<juce::WebBrowserComponent>(options);
    ABD_LOG("[EDITOR] setupWebBrowserBindings: WebBrowserComponent created.");
}

void ABDMS2000AudioProcessorEditor::paint(juce::Graphics& g)
{
    static bool firstPaint = true;
    if (firstPaint) {
        ABD_LOG("[EDITOR] paint() called for the first time.");
        firstPaint = false;
    }
    g.fillAll(juce::Colour(0xff12141a));
}

void ABDMS2000AudioProcessorEditor::resized()
{
    static bool firstResize = true;
    if (firstResize) {
        ABD_LOG(juce::String("[EDITOR] resized() called. Bounds: ") + getLocalBounds().toString());
        firstResize = false;
    }
    if (webView_)
    {
        webView_->setBounds(getLocalBounds());
    }
}

void ABDMS2000AudioProcessorEditor::timerCallback()
{
    refreshHardwareMidiAvailability();

    if (bridge_ != nullptr)
    {
        const auto& snap = audioProcessor_.getEngine().getSnapshot();
        juce::DynamicObject::Ptr obj = new juce::DynamicObject();
        obj->setProperty("activeVoices", static_cast<int>(snap.activeVoiceCount));
        obj->setProperty("vuLeft", static_cast<double>(snap.vuLeft));
        obj->setProperty("vuRight", static_cast<double>(snap.vuRight));
        bridge_->sendEventToJs("telemetryUpdate", juce::var(obj.get()));
    }
}

} // namespace ABDMS2000

