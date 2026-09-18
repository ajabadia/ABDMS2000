#pragma once

#include <juce_gui_extra/juce_gui_extra.h>
#include <JUCE/JuceWebScopeComponent.h>
#include "../Core/SynthEngine.h"
#include <functional>

namespace ABDMS2000 {

/**
 * @class ScopeFloatingWindow
 * @brief Native floating analytical window embedding the ABDScope WebUI (Multi-Lane & Waterfall).
 *
 * Mirrors ABDAudioLab's ABDScope floating tool window. It hosts a
 * JuceWebScopeComponent (WebView2) fed from SynthEngine::getScopeCollector(),
 * so Standalone/VST/web-WASM all consume the same native C++ taps.
 */
class ScopeFloatingWindow : public juce::DocumentWindow
{
public:
    ScopeFloatingWindow(SynthEngine& engineRef, std::function<void()> onClose = nullptr)
        : DocumentWindow("ABDScope - ABDMS2000 Telemetry",
                         juce::Colour(0xff12141a),
                         DocumentWindow::allButtons),
          engine(engineRef),
          onCloseCallback(std::move(onClose))
    {
        setUsingNativeTitleBar(true);

        auto webScope = std::make_unique<abd::scope::JuceWebScopeComponent>(
            engineRef.getScopeCollector(),
            engineRef.getSampleRate(),
            30
        );
        webScope->setTheme("ms2000");
        setContentOwned(webScope.release(), true);

        setResizable(true, true);
        setResizeLimits(640, 400, 2560, 1440);
        centreWithSize(960, 580);
        setAlwaysOnTop(true);
    }

    ~ScopeFloatingWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);
        if (onCloseCallback)
            onCloseCallback();
    }

    /** Enable multi-lane: activate every registered tap when the window shows. */
    void onWindowShown()
    {
        for (size_t i = 0; i < engine.getScopeCollector().getTapCount(); ++i)
        {
            if (auto* tap = const_cast<abd::scope::ScopeTap*>(engine.getScopeCollector().getTap(i)))
                tap->setActive(true);
        }
        if (auto* ws = dynamic_cast<abd::scope::JuceWebScopeComponent*>(getContentComponent()))
        {
            ws->setSampleRate(engine.getSampleRate());
            ws->setTheme("ms2000");
        }
    }

    void syncSampleRate()
    {
        if (auto* ws = dynamic_cast<abd::scope::JuceWebScopeComponent*>(getContentComponent()))
            ws->setSampleRate(engine.getSampleRate());
    }

private:
    SynthEngine& engine;
    std::function<void()> onCloseCallback;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScopeFloatingWindow)
};

} // namespace ABDMS2000
