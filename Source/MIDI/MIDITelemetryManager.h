#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "MIDIMap.h"
#include "../../ABDSharedCode/HardwareDrivers/NRPNParser.h"
using abd::hw::NRPNParser;
using abd::hw::NRPNMessage;
#include <array>
#include <atomic>
#include <vector>
#include <functional>

namespace ABDMS2000 {

struct MidiActivityEvent {
    int channel{ 1 };
    int ccNumber{ -1 };
    int value{ 0 };
    bool isIncoming{ true };
    const char* paramId{ nullptr };
};

/**
 * @brief Bidirectional MIDI Telemetry Manager.
 * Handles:
 * - Real-time incoming CC & NRPN decoding and APVTS synchronization.
 * - Outgoing CC & NRPN transmission on WebUI / automation parameter changes.
 * - Hardware feedback loop suppression (Anti-Echo Guard).
 * - Telemetry activity listener for WebUI LED & display monitors.
 */
class MIDITelemetryManager : public juce::AudioProcessorValueTreeState::Listener {
public:
    explicit MIDITelemetryManager(juce::AudioProcessorValueTreeState& apvts);
    ~MIDITelemetryManager() override;

    void setMidiChannel(int channel1to16) noexcept { midiChannel_ = std::max(1, std::min(16, channel1to16)); }
    int getMidiChannel() const noexcept { return midiChannel_; }

    void setEchoSuppressionEnabled(bool enabled) noexcept { echoSuppressionEnabled_ = enabled; }
    bool isEchoSuppressionEnabled() const noexcept { return echoSuppressionEnabled_; }

    // Activity callback for WebUI / Monitor
    void setActivityCallback(std::function<void(const MidiActivityEvent&)> cb) { activityCallback_ = std::move(cb); }

    // Audio thread methods
    void processIncomingMidi(juce::MidiBuffer& midiMessages) noexcept;
    void renderOutgoingMidi(juce::MidiBuffer& midiMessages) noexcept;

    // Direct trigger from UI/Bridge
    void sendDirectCC(int ccNumber, int value7Bit) noexcept;

    // APVTS Listener callback
    void parameterChanged(const juce::String& parameterID, float newValue) override;

private:
    juce::AudioProcessorValueTreeState& apvts_;
    int midiChannel_{ 1 };
    bool echoSuppressionEnabled_{ true };

    NRPNParser nrpnParser_;
    std::atomic<bool> isInternalMidiUpdate_{ false };

    // TTL-based echo suppression (300ms window)
    struct PendingTransaction {
        int ccNum{ -1 };
        int channel{ 1 };
        float value{ 0 };
        uint64_t timestamp{ 0 };
    };
    static constexpr size_t kMaxPendingTransactions = 32;
    std::array<PendingTransaction, kMaxPendingTransactions> pendingTransactions_;
    std::atomic<size_t> pendingWritePos_{ 0 };
    static constexpr uint64_t kEchoTTLMs = 300; // 300ms TTL

    // Thread-safe lock-free outgoing queue
    static constexpr size_t kMaxOutgoingEvents = 256;
    struct QueuedMidiEvent {
        int sampleOffset{ 0 };
        juce::MidiMessage message;
    };
    std::array<QueuedMidiEvent, kMaxOutgoingEvents> outgoingQueue_;
    std::atomic<size_t> queueWritePos_{ 0 };
    std::atomic<size_t> queueReadPos_{ 0 };

    std::function<void(const MidiActivityEvent&)> activityCallback_;

    void pushOutgoingMessage(const juce::MidiMessage& msg, int sampleOffset = 0) noexcept;

    bool isEcho(const juce::MidiMessage& msg) noexcept;
    void registerEcho(const juce::MidiMessage& msg) noexcept;
};

} // namespace ABDMS2000
