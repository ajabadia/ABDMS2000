#include "PluginEditor.h"

namespace ABDMS2000 {

ABDMS2000AudioProcessorEditor::ABDMS2000AudioProcessorEditor(ABDMS2000AudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor_(p)
{
    setupWebBrowserBindings();

    if (webView_ != nullptr)
        addAndMakeVisible(*webView_);

    bridge_ = std::make_unique<BridgeActions>(audioProcessor_, *webView_);

    webView_->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());

    setSize(1080, 680);
    setResizable(true, true);
    setResizeLimits(800, 500, 1920, 1200);

    startTimerHz(30);
}

ABDMS2000AudioProcessorEditor::~ABDMS2000AudioProcessorEditor()
{
    stopTimer();
}

void ABDMS2000AudioProcessorEditor::setupWebBrowserBindings()
{
    juce::WebBrowserComponent::Options options;
    options = options
        .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
        .withNativeIntegrationEnabled()
        .withResourceProvider(pluginResourceProvider);

    webView_ = std::make_unique<juce::WebBrowserComponent>(options);
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
}

} // namespace ABDMS2000
