#include "PluginEditor.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../Core/AppLogger.h"

namespace ABDMS2000 {

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
    bridge_ = std::make_unique<BridgeActions>(audioProcessor_, *webView_);

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
    stopTimer();
    ABD_LOG("[EDITOR] ABDMS2000AudioProcessorEditor destructor finished.");
}


void ABDMS2000AudioProcessorEditor::setupWebBrowserBindings()
{
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

