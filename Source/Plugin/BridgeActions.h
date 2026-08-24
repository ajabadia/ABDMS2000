#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "PluginProcessor.h"

namespace ABDMS2000 {

class BridgeActions {
public:
    BridgeActions(ABDMS2000AudioProcessor& processor, juce::WebBrowserComponent& browser);
    ~BridgeActions() = default;

    void handleJsEvent(const juce::var& message);
    void sendEventToJs(const juce::String& eventType, const juce::var& payload);

private:
    ABDMS2000AudioProcessor& processor_;
    juce::WebBrowserComponent& browser_;
};

} // namespace ABDMS2000
