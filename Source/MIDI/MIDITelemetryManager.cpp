#include "MIDITelemetryManager.h"
#include <algorithm>
#include <set>
#include "../State/ParameterRegistry.gen.h"
#include "MS2000HardwareProgram.h"

namespace ABDMS2000
{

MIDITelemetryManager::MIDITelemetryManager(juce::AudioProcessorValueTreeState &apvts) : apvts_(apvts)
{
    std::set<std::string> registered;
    for (const auto &item : MIDIMap::getAllMappings())
    {
        if (registered.insert(item.paramId).second)
            apvts_.addParameterListener(item.paramId, this);
    }
    for (const auto &meta : ParameterRegistry::getAllParameters())
    {
        if (registered.insert(meta.id).second)
            apvts_.addParameterListener(meta.id, this);
    }
}

MIDITelemetryManager::~MIDITelemetryManager()
{
    std::set<std::string> unregistered;
    for (const auto &item : MIDIMap::getAllMappings())
    {
        if (unregistered.insert(item.paramId).second)
            apvts_.removeParameterListener(item.paramId, this);
    }
    for (const auto &meta : ParameterRegistry::getAllParameters())
    {
        if (unregistered.insert(meta.id).second)
            apvts_.removeParameterListener(meta.id, this);
    }
}

void MIDITelemetryManager::notifyActivity(const MidiActivityEvent &ev) noexcept
{
    while (callbackLock_.test_and_set(std::memory_order_acquire))
    {
    }
    auto cb = activityCallback_;
    callbackLock_.clear(std::memory_order_release);
    if (cb)
        cb(ev);
}

void MIDITelemetryManager::pushOutgoingMessage(const juce::MidiMessage &msg, int sampleOffset) noexcept
{
    while (queueWriteLock_.test_and_set(std::memory_order_acquire))
    {
    }
    size_t currentWrite = queueWritePos_.load(std::memory_order_relaxed);
    size_t nextWrite = (currentWrite + 1) % kMaxOutgoingEvents;

    if (nextWrite != queueReadPos_.load(std::memory_order_acquire))
    {
        outgoingQueue_[currentWrite] = {sampleOffset, msg};
        queueWritePos_.store(nextWrite, std::memory_order_release);
    }
    queueWriteLock_.clear(std::memory_order_release);
}

void MIDITelemetryManager::processIncomingMidi(juce::MidiBuffer &midiMessages) noexcept
{
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isSysEx())
        {
            const uint8_t *sx = msg.getSysExData();
            const int sxLen = msg.getSysExDataSize();
            // Real-time Korg MS2000 Parameter Change (0x41): 42 3n 58 41 pLSB pMSB vLSB vMSB (8 bytes inside F0..F7)
            if (sx != nullptr && sxLen >= 8 && sx[0] == 0x42 && ((sx[1] & 0xF0) == 0x30)
                && (sx[2] == 0x58 || sx[2] == 0x71) && sx[3] == MS2000HardwareProgram::CMD_PARAMETER_CHANGE)
            {
                int paramId14 = 0;
                int value14 = 0;
                if (MS2000HardwareProgram::decodeParameterChangePayload(
                        sx + 4, static_cast<size_t>(sxLen - 4), paramId14, value14))
                {
                    isInternalMidiUpdate_.store(true, std::memory_order_release);
                    const std::string updatedId =
                        MS2000HardwareProgram::applyParameterChange41(paramId14, value14, apvts_);
                    isInternalMidiUpdate_.store(false, std::memory_order_release);
                    if (!updatedId.empty())
                    {
                        const int ch = (sx[1] & 0x0F) + 1;
                        const auto *meta = ParameterRegistry::getParameter(updatedId.c_str());
                        notifyActivity({ch, -1, value14, true, meta ? meta->id : nullptr});
                    }
                }
            }
            continue;
        }

        int ch = msg.getChannel();

        if (msg.isController())
        {
            int ccNum = msg.getControllerNumber();
            int ccVal = msg.getControllerValue();

            // Echo suppression with TTL (300ms window)
            if (echoSuppressionEnabled_ && isEcho(msg))
                continue;

            // 1. Check for NRPN sequence
            if (ccNum == 99 || ccNum == 98 || ccNum == 6 || ccNum == 38)
            {
                NRPNMessage nrpnMsg;
                if (nrpnParser_.processCC(ch, ccNum, ccVal, nrpnMsg))
                {
                    const auto *info = MIDIMap::findByNRPN(nrpnMsg.nrpnMSB, nrpnMsg.nrpnLSB);
                    if (info != nullptr)
                    {
                        float paramVal = MIDIMap::midiValueToParamValue(*info, nrpnMsg.dataMSB);
                        if (auto *param = apvts_.getParameter(info->paramId))
                        {
                            isInternalMidiUpdate_.store(true, std::memory_order_release);
                            param->setValueNotifyingHost(param->convertTo0to1(paramVal));
                            isInternalMidiUpdate_.store(false, std::memory_order_release);
                        }

                        notifyActivity({ch, -1, nrpnMsg.dataMSB, true, info->paramId});
                    }
                }
            }
            else
            {
                // 2. Standard CC message
                const auto *info = MIDIMap::findByCC(ccNum);
                if (info != nullptr)
                {
                    float paramVal = MIDIMap::midiValueToParamValue(*info, ccVal);
                    if (auto *param = apvts_.getParameter(info->paramId))
                    {
                        isInternalMidiUpdate_.store(true, std::memory_order_release);
                        param->setValueNotifyingHost(param->convertTo0to1(paramVal));
                        isInternalMidiUpdate_.store(false, std::memory_order_release);
                    }

                    notifyActivity({ch, ccNum, ccVal, true, info ? info->paramId : nullptr});
                }
            }
            registerEcho(msg);
        }
    }
}

void MIDITelemetryManager::renderOutgoingMidi(juce::MidiBuffer &midiMessages) noexcept
{
    size_t currentRead = queueReadPos_.load(std::memory_order_relaxed);
    size_t currentWrite = queueWritePos_.load(std::memory_order_acquire);

    while (currentRead != currentWrite)
    {
        const auto &item = outgoingQueue_[currentRead];
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

void MIDITelemetryManager::parameterChanged(const juce::String &parameterID, float newValue)
{
    // Anti-Echo Guard: Do not re-emit MIDI if change was triggered by incoming MIDI
    if (echoSuppressionEnabled_ && isInternalMidiUpdate_.load(std::memory_order_acquire))
    {
        return;
    }

    const std::string idStr = parameterID.toStdString();
    const auto *info = MIDIMap::findByParamId(idStr);
    if (info != nullptr)
    {
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

            notifyActivity({midiChannel_, info->ccNumber, midiVal, false, info->paramId});
            return;
        }
        else if (info->nrpnMSB >= 0 && info->nrpnLSB >= 0)
        {
            juce::MidiBuffer tempBuf;
            NRPNParser::appendNRPNToBuffer(tempBuf, midiChannel_, info->nrpnMSB, info->nrpnLSB, midiVal, false);
            for (const auto metadata : tempBuf)
            {
                pushOutgoingMessage(metadata.getMessage());
            }

            notifyActivity({midiChannel_, -1, midiVal, false, info->paramId});
            return;
        }
    }

    // Fallback to Korg MS2000 SysEx 0x41 Parameter Change for SysEx-only parameters (e.g. Timbre 2, Scale, Split, etc.)
    if (const auto *meta = ParameterRegistry::getParameter(idStr.c_str()))
    {
        const float rawValue = meta->min + newValue * (meta->max - meta->min);
        const auto sysex41 = MS2000HardwareProgram::buildParameterChangeForEngineParam(midiChannel_, idStr, rawValue);
        if (sysex41.size() >= 3)
        {
            // createSysExMessage expects payload without 0xF0 and 0xF7
            auto msg = juce::MidiMessage::createSysExMessage(sysex41.data() + 1, static_cast<int>(sysex41.size() - 2));
            pushOutgoingMessage(msg);
            notifyActivity({midiChannel_, -1, static_cast<int>(std::lround(rawValue)), false, meta->id});
        }
    }
}

bool MIDITelemetryManager::isEcho(const juce::MidiMessage &msg) noexcept
{
    if (!msg.isController())
        return false;

    uint64_t now = juce::Time::getMillisecondCounterHiRes() * 1000;
    int ccNum = msg.getControllerNumber();
    int ch = msg.getChannel();

    size_t writePos = pendingWritePos_.load(std::memory_order_relaxed);
    for (size_t i = 0; i < kMaxPendingTransactions; ++i)
    {
        size_t idx = (writePos + kMaxPendingTransactions - 1 - i) % kMaxPendingTransactions;
        const auto &t = pendingTransactions_[idx];
        if (t.timestamp == 0)
            break;

        if (now - t.timestamp > kEchoTTLMs * 1000)
            break;

        if (t.ccNum == ccNum && t.channel == ch)
            return true;
    }
    return false;
}

void MIDITelemetryManager::registerEcho(const juce::MidiMessage &msg) noexcept
{
    if (!msg.isController())
        return;

    PendingTransaction t;
    t.ccNum = msg.getControllerNumber();
    t.channel = msg.getChannel();
    t.value = static_cast<float>(msg.getControllerValue()) / 127.0f;
    t.timestamp = juce::Time::getMillisecondCounterHiRes() * 1000;

    size_t writePos = pendingWritePos_.load(std::memory_order_relaxed);
    pendingTransactions_[writePos] = t;
    pendingWritePos_.store((writePos + 1) % kMaxPendingTransactions, std::memory_order_release);
}

}  // namespace ABDMS2000
