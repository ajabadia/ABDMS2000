#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_devices/juce_audio_devices.h>
#include "PluginProcessor.h"
#include "PluginEditor_ResourceProvider.h"
#include "BridgeActions.h"
#include "ScopeFloatingWindow.h"
#include <memory>

namespace ABDMS2000 {

class ABDMS2000AudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer,
                                      private juce::MidiInputCallback {
public:
    explicit ABDMS2000AudioProcessorEditor(ABDMS2000AudioProcessor&);
    ~ABDMS2000AudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

    void showScopeWindow();
    void toggleScopeWindow();

private:
    void setupWebBrowserBindings();

    /** Sumidero de eventos JS del bridge: emite `event` en el WebView. */
    void emitEventToWebView(const juce::var& message);

    /**
     * Da al puente MIDI del Bank Manager su otro extremo: los dispositivos MIDI
     * del sistema. Se abren perezosamente al escuchar/enviar y siempre por el
     * identificador seleccionado; nunca se elige silenciosamente el primero.
     */
    void bindHardwareMidi();
    juce::var listHardwareMidiPorts() const;
    bool isHardwareOutputAvailable(const juce::String& identifier) const;
    bool isHardwareInputAvailable(const juce::String& identifier) const;
    void refreshHardwareMidiAvailability();
    bool openHardwareMidiOutput();
    bool openHardwareMidiInput();

    /** SysEx del dispositivo → Bank Manager embebido (`hardware.receive`). */
    void handleIncomingMidiMessage(juce::MidiInput* source, const juce::MidiMessage& message) override;

    ABDMS2000AudioProcessor& audioProcessor_;
    std::unique_ptr<juce::WebBrowserComponent> webView_;
    std::unique_ptr<BridgeActions> bridge_;
    std::unique_ptr<ScopeFloatingWindow> scopeWindow_;
    std::unique_ptr<juce::MidiOutput> hardwareMidiOutput_;
    std::unique_ptr<juce::MidiInput> hardwareMidiInput_;
    juce::String openedHardwareOutputId_;
    juce::String openedHardwareInputId_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ABDMS2000AudioProcessorEditor)
};

} // namespace ABDMS2000
