#include "PluginEditor.h"

namespace ABDMS2000 {

ABDMS2000AudioProcessorEditor::ABDMS2000AudioProcessorEditor(ABDMS2000AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor_(p)
{
    juce::WebBrowserComponent::Options options;
    options = options.withResourceProvider([this](const juce::String& url) {
        return resourceProvider_.getResource(url);
    });
    options = options.withNativeIntegrationEnabled();

    webView_ = std::make_unique<juce::WebBrowserComponent>(options);
    addAndMakeVisible(*webView_);

    bridge_ = std::make_unique<BridgeActions>(audioProcessor_, *webView_);

    webView_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize(1080, 680);
    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1200);

    startTimerHz(30); // 30 FPS timer for telemetry
}

ABDMS2000AudioProcessorEditor::~ABDMS2000AudioProcessorEditor()
{
    stopTimer();
}

void ABDMS2000AudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff12141a));
}

void ABDMS2000AudioProcessorEditor::resized()
{
    if (webView_)
    {
        webView_->setBounds(getLocalBounds());
    }
}

void ABDMS2000AudioProcessorEditor::timerCallback()
{
    // Snapshot telemetry can be pushed if needed
}

} // namespace ABDMS2000
