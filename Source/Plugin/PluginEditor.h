#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"
#include "PluginEditor_ResourceProvider.h"
#include "BridgeActions.h"
#include <memory>

namespace ABDMS2000 {

class ABDMS2000AudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer {
public:
    explicit ABDMS2000AudioProcessorEditor(ABDMS2000AudioProcessor&);
    ~ABDMS2000AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    ABDMS2000AudioProcessor& audioProcessor_;
    WebUIResourceProvider resourceProvider_;
    std::unique_ptr<juce::WebBrowserComponent> webView_;
    std::unique_ptr<BridgeActions> bridge_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ABDMS2000AudioProcessorEditor)
};

} // namespace ABDMS2000
