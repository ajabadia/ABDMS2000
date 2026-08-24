#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <optional>

namespace ABDMS2000 {

class WebUIResourceProvider {
public:
    WebUIResourceProvider();
    ~WebUIResourceProvider() = default;

    std::optional<juce::WebBrowserComponent::Resource> getResource(const juce::String& url);

private:
    juce::File devRootDirectory_;
    bool isDevMode_{true};
};

} // namespace ABDMS2000
