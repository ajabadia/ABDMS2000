#include "BridgeActions.h"

namespace ABDMS2000 {

BridgeActions::BridgeActions(ABDMS2000AudioProcessor& processor, juce::WebBrowserComponent& browser)
    : processor_(processor), browser_(browser)
{
}

void BridgeActions::handleJsEvent(const juce::var& message)
{
    if (!message.isObject()) return;

    auto action = message.getProperty("action", "").toString();

    if (action == "setParam")
    {
        auto paramId = message.getProperty("paramId", "").toString();
        float value = static_cast<float>(message.getProperty("value", 0.0));

        if (auto* param = processor_.getAPVTS().getParameter(paramId))
        {
            param->setValueNotifyingHost(value);
        }
    }
    else if (action == "noteOn")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.8));
        processor_.getEngine().noteOn(1, note, vel);
    }
    else if (action == "noteOff")
    {
        int note = static_cast<int>(message.getProperty("note", 60));
        float vel = static_cast<float>(message.getProperty("velocity", 0.0));
        processor_.getEngine().noteOff(1, note, vel);
    }
    else if (action == "allNotesOff")
    {
        processor_.getEngine().allNotesOff();
    }
}

void BridgeActions::sendEventToJs(const juce::String& eventType, const juce::var& payload)
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("type", eventType);
    obj->setProperty("data", payload);

    juce::var msg(obj.get());
    juce::String jsonString = juce::JSON::toString(msg);

    browser_.evaluateJavascript("if (window.__JUCE__ && window.__JUCE__.backend) { window.__JUCE__.backend.emitEvent(" + jsonString + "); }");
}

} // namespace ABDMS2000
