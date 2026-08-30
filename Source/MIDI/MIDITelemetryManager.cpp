#include "MIDITelemetryManager.h"
#include <algorithm>

namespace ABDMS2000 {

MIDITelemetryManager::MIDITelemetryManager(juce::AudioProcessorValueTreeState& apvts)
    : apvts_(apvts)
{
    // Register as listener for all mapped parameters
    const auto& mappings = MIDIMap::getAllMappings();
    for (const auto& item : mappings)
    {
        apvts_.addParameterListener(item.paramId, this);
    }
}

MIDITelemetryManager::~MIDITelemetryManager()
{
    const auto& mappings = MIDIMap::getAllMappings();
    for (const auto& item : mappings)
    {
        apvts_.removeParameterListener(item.paramId, this);
    }
}

void MIDITelemetryManager::pushOutgoingMessage(const juce::MidiMessage& msg, int sampleOffset) noexcept
{
    size_t currentWrite = queueWritePos_.load(std::memory_order_relaxed);
    size_t nextWrite = (currentWrite + 1) % kMaxOutgoingEvents;

    if (nextWrite != queueReadPos_.load(std::memory_order_acquire))
    {
        outgoingQueue_[currentWrite] = { sampleOffset, msg };
        queueWritePos_.store(nextWrite, std::memory_order_release);
    }
}

void MIDITelemetryManager::processIncomingMidi(juce::MidiBuffer& midiMessages) noexcept
{
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        int ch = msg.getChannel();

        if (msg.isController())
        {
            int ccNum = msg.getControllerNumber();
            int ccVal = msg.getControllerValue();

            // 1. Check for NRPN sequence
            if (ccNum == 99 || ccNum == 98 || ccNum == 6 || ccNum == 38)
            {
                NRPNMessage nrpnMsg;
                if (nrpnParser_.processCC(ch, ccNum, ccVal, nrpnMsg))
                {
                    const auto* info = MIDIMap::findByNRPN(nrpnMsg.nrpnMSB, nrpnMsg.nrpnLSB);
                    if (info != nullptr)
                    {
                        float paramVal = MIDIMap::midiValueToParamValue(*info, nrpnMsg.dataMSB);
                        if (auto* param = apvts_.getParameter(info->paramId))
                        {
                            isInternalMidiUpdate_.store(true, std::memory_order_release);
                            param->setValueNotifyingHost(param->convertTo0to1(paramVal));
                            isInternalMidiUpdate_.store(false, std::memory_order_release);
                        }

                        if (activityCallback_)
                        {
                            activityCallback_({ ch, -1, nrpnMsg.dataMSB, true, info->paramId });
                        }
                    }
                }
            }
            else
            {
                // 2. Standard CC message
                const auto* info = MIDIMap::findByCC(ccNum);
                if (info != nullptr)
                {
                    float paramVal = MIDIMap::midiValueToParamValue(*info, ccVal);
                    if (auto* param = apvts_.getParameter(info->paramId))
                    {
                        isInternalMidiUpdate_.store(true, std::memory_order_release);
                        param->setValueNotifyingHost(param->convertTo0to1(paramVal));
                        isInternalMidiUpdate_.store(false, std::memory_order_release);
                    }

                    if (activityCallback_)
                    {
                        activityCallback_({ ch, ccNum, ccVal, true, info->paramId });
                    }
                }
            }
        }
    }
}

void MIDITelemetryManager::renderOutgoingMidi(juce::MidiBuffer& midiMessages) noexcept
{
    size_t currentRead = queueReadPos_.load(std::memory_order_relaxed);
    size_t currentWrite = queueWritePos_.load(std::memory_order_acquire);

    while (currentRead != currentWrite)
    {
        const auto& item = outgoingQueue_[currentRead];
        midiMessages.addEvent(item.message, item.sampleOffset);
        currentRead = (currentRead + 1) % kMaxOutgoingEvents;
    }

    queueReadPos_.store(currentRead, std::memory_order_release);
}

void MIDITelemetryManager::sendDirectCC(int ccNumber, int value7Bit) noexcept
{
    auto msg = juce::MidiMessage::controllerEvent(midiChannel_, ccNumber, value7Bit & 0x7F);
    pushOutgoingMessage(msg);
}

void MIDITelemetryManager::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Anti-Echo Guard: Do not re-emit MIDI if change was triggered by incoming MIDI
    if (echoSuppressionEnabled_ && isInternalMidiUpdate_.load(std::memory_order_acquire))
    {
        return;
    }

    const auto* info = MIDIMap::findByParamId(parameterID.toStdString());
    if (info == nullptr) return;

    // APVTS Listener delivers normalized [0,1] values.
    // Denormalize to raw [min..max] range before converting to MIDI.
    float rawMin = info->minValue;
    float rawMax = info->maxValue;
    float rawValue = rawMin + newValue * (rawMax - rawMin);

    int midiVal = MIDIMap::paramValueToMidiValue(*info, rawValue);

    if (info->ccNumber >= 0)
    {
        auto msg = juce::MidiMessage::controllerEvent(midiChannel_, info->ccNumber, midiVal);
        pushOutgoingMessage(msg);

        if (activityCallback_)
        {
            activityCallback_({ midiChannel_, info->ccNumber, midiVal, false, info->paramId });
        }
    }
    else if (info->nrpnMSB >= 0 && info->nrpnLSB >= 0)
    {
        juce::MidiBuffer tempBuf;
        NRPNParser::appendNRPNToBuffer(tempBuf, midiChannel_, info->nrpnMSB, info->nrpnLSB,
                                        midiVal, false);
        for (const auto metadata : tempBuf)
        {
            pushOutgoingMessage(metadata.getMessage());
        }

        if (activityCallback_)
        {
            activityCallback_({ midiChannel_, -1, midiVal, false, info->paramId });
        }
    }
}

} // namespace ABDMS2000
