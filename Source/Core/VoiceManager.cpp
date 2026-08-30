#include "VoiceManager.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

void VoiceManager::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    for (size_t i = 0; i < voices_.size(); ++i)
    {
        voices_[i].prepare(sampleRate_);
    }
    reset();
}

void VoiceManager::reset() noexcept
{
    for (auto& v : voices_)
    {
        v.reset();
    }
    for (auto& s : slotStates_)
    {
        s = VoiceSlotState{};
    }
    heldNotesStack_.clear();
    lastMonoNote_ = -1;
    globalTimeCounter_ = 0;
}

void VoiceManager::setPortamentoTime(float portamentoTime0to127) noexcept
{
    portamentoTimeParam_ = DSPUtils::clamp(portamentoTime0to127, 0.0f, 127.0f);
    float norm = portamentoTimeParam_ / 127.0f;
    // MS2000 measured portamento range: 0.0s to 4.5s via power 2.5 curve
    glideTimeSec_ = (norm <= 0.001f) ? 0.0f : (std::pow(norm, 2.5f) * 4.5f);
}

void VoiceManager::setHoldButtonState(bool active) noexcept
{
    isHoldActive_ = active;
    if (!isHoldActive_)
    {
        // When HOLD is deactivated, immediately release all voices whose keys are physically up
        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
        {
            if (slotStates_[i].isHeldByLatch && !slotStates_[i].isKeyPressed)
            {
                releaseVoiceSlot(i);
            }
        }
    }
}

void VoiceManager::setMaxPolyphony(size_t numVoices) noexcept
{
    activeVoiceCapacity_ = std::max(size_t(1), std::min(MAX_EXPANDED_VOICES, numVoices));
}

bool VoiceManager::anyKeyPressed() const noexcept
{
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (slotStates_[i].isKeyPressed) return true;
    }
    return false;
}

void VoiceManager::releaseVoiceSlot(size_t index) noexcept
{
    if (index >= activeVoiceCapacity_) return;
    slotStates_[index].currentMidiNote = -1;
    slotStates_[index].isKeyPressed = false;
    slotStates_[index].isHeldByLatch = false;
    voices_[index].noteOff();
}

int VoiceManager::findVoiceToSteal(int incomingNote) noexcept
{
    // Priority 3: Repeated Note Protection (Re-triggering)
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (slotStates_[i].currentMidiNote == incomingNote)
        {
            return static_cast<int>(i);
        }
    }

    // Free voice search (completely idle)
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (!voices_[i].isActive() && slotStates_[i].currentMidiNote == -1)
        {
            return static_cast<int>(i);
        }
    }

    // Priority 4: Steal voice latched by HOLD whose key was physically released
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (slotStates_[i].isHeldByLatch && !slotStates_[i].isKeyPressed)
        {
            return static_cast<int>(i);
        }
    }

    // Priority 1: Release Phase Stealing (steals voice in release closest to silence)
    int bestReleaseVoice = -1;
    float lowestAmp = 9999.0f;
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (voices_[i].isInRelease() || !slotStates_[i].isKeyPressed)
        {
            float amp = voices_[i].getCurrentAmpLevel();
            if (amp < lowestAmp)
            {
                lowestAmp = amp;
                bestReleaseVoice = static_cast<int>(i);
            }
        }
    }
    if (bestReleaseVoice != -1)
    {
        return bestReleaseVoice;
    }

    // Priority 2: FIFO Sustain Stealing (steals oldest held note)
    int oldestVoice = 0;
    uint32_t oldestTime = 0xFFFFFFFF;
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (slotStates_[i].noteOnTime < oldestTime)
        {
            oldestTime = slotStates_[i].noteOnTime;
            oldestVoice = static_cast<int>(i);
        }
    }
    return oldestVoice;
}

void VoiceManager::noteOn(int midiNote, float velocity) noexcept
{
    globalTimeCounter_++;

    if (assignMode_ == VoiceAssignMode::Mono)
    {
        bool isLegato = (lastMonoNote_ != -1 && anyKeyPressed());
        auto dup = std::find(heldNotesStack_.begin(), heldNotesStack_.end(), midiNote);
        if (dup != heldNotesStack_.end()) heldNotesStack_.erase(dup);
        heldNotesStack_.push_back(midiNote);

        slotStates_[0].isKeyPressed = true;
        slotStates_[0].isHeldByLatch = isHoldActive_;
        slotStates_[0].currentMidiNote = midiNote;
        slotStates_[0].noteOnTime = globalTimeCounter_;

        bool glideOn = (glideTimeSec_ > 0.001f);
        bool isFirstTimbre = (heldNotesStack_.size() <= 1);

        voices_[0].noteOn(midiNote, velocity, glideOn, isFirstTimbre, isLegato);
        lastMonoNote_ = midiNote;
    }
    else if (assignMode_ == VoiceAssignMode::Unison)
    {
        bool isLegato = (lastMonoNote_ != -1 && anyKeyPressed());
        auto dup = std::find(heldNotesStack_.begin(), heldNotesStack_.end(), midiNote);
        if (dup != heldNotesStack_.end()) heldNotesStack_.erase(dup);
        heldNotesStack_.push_back(midiNote);

        bool glideOn = (glideTimeSec_ > 0.001f);
        bool isFirstTimbre = (heldNotesStack_.size() <= 1);

        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
        {
            slotStates_[i].isKeyPressed = true;
            slotStates_[i].isHeldByLatch = isHoldActive_;
            slotStates_[i].currentMidiNote = midiNote;
            slotStates_[i].noteOnTime = globalTimeCounter_;
            voices_[i].noteOn(midiNote, velocity, glideOn, isFirstTimbre, isLegato);
        }
        lastMonoNote_ = midiNote;
    }
    else // VoiceAssignMode::Poly
    {
        int voiceIdx = findVoiceToSteal(midiNote);
        if (voiceIdx < 0 || static_cast<size_t>(voiceIdx) >= activeVoiceCapacity_)
            voiceIdx = 0;

        bool glideOn = (glideTimeSec_ > 0.001f);
        bool isFirstTimbre = (getActiveVoiceCount() == 0);

        slotStates_[voiceIdx].isKeyPressed = true;
        slotStates_[voiceIdx].isHeldByLatch = isHoldActive_;
        slotStates_[voiceIdx].currentMidiNote = midiNote;
        slotStates_[voiceIdx].noteOnTime = globalTimeCounter_;

        voices_[voiceIdx].noteOn(midiNote, velocity, glideOn, isFirstTimbre, false);
    }
}

void VoiceManager::noteOff(int midiNote) noexcept
{
    if (assignMode_ == VoiceAssignMode::Mono || assignMode_ == VoiceAssignMode::Unison)
    {
        heldNotesStack_.erase(std::remove(heldNotesStack_.begin(), heldNotesStack_.end(), midiNote), heldNotesStack_.end());

        if (slotStates_[0].currentMidiNote == midiNote)
        {
            slotStates_[0].isKeyPressed = false;
            if (isHoldActive_)
            {
                slotStates_[0].isHeldByLatch = true;
            }
            else
            {
                if (heldNotesStack_.empty())
                {
                    if (assignMode_ == VoiceAssignMode::Mono)
                    {
                        releaseVoiceSlot(0);
                    }
                    else
                    {
                        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
                            releaseVoiceSlot(i);
                    }
                    lastMonoNote_ = -1;
                }
                else
                {
                    // Fall back to previous held note in stack
                    int prevNote = heldNotesStack_.back();
                    slotStates_[0].isKeyPressed = true;
                    slotStates_[0].currentMidiNote = prevNote;
                    slotStates_[0].noteOnTime = ++globalTimeCounter_;

                    bool glideOn = (glideTimeSec_ > 0.001f);
                    if (assignMode_ == VoiceAssignMode::Mono)
                    {
                        voices_[0].noteOn(prevNote, 0.8f, glideOn, false, true);
                    }
                    else
                    {
                        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
                        {
                            slotStates_[i].isKeyPressed = true;
                            slotStates_[i].currentMidiNote = prevNote;
                            slotStates_[i].noteOnTime = globalTimeCounter_;
                            voices_[i].noteOn(prevNote, 0.8f, glideOn, false, true);
                        }
                    }
                    lastMonoNote_ = prevNote;
                }
            }
        }
    }
    else // VoiceAssignMode::Poly
    {
        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
        {
            if (slotStates_[i].currentMidiNote == midiNote)
            {
                slotStates_[i].isKeyPressed = false;
                if (isHoldActive_)
                {
                    slotStates_[i].isHeldByLatch = true;
                }
                else
                {
                    releaseVoiceSlot(i);
                }
            }
        }
    }
}

void VoiceManager::allNotesOff() noexcept
{
    heldNotesStack_.clear();
    lastMonoNote_ = -1;
    for (size_t i = 0; i < voices_.size(); ++i)
    {
        slotStates_[i].currentMidiNote = -1;
        slotStates_[i].isKeyPressed = false;
        slotStates_[i].isHeldByLatch = false;
        voices_[i].stopImmediately();
    }
}

void VoiceManager::applyBlockParams(const VoiceParameters& params) noexcept
{
    baseBlockParams_ = params;
    setPortamentoTime(params.portamentoTime);

    if (assignMode_ == VoiceAssignMode::Unison)
    {
        // Compute Unison Detune and Stereo Spread offsets per voice
        const size_t numUnison = std::min(activeVoiceCapacity_, size_t(4));
        const float detuneOffsets[4] = { -unisonDetune_, unisonDetune_ * 0.33f, -unisonDetune_ * 0.33f, unisonDetune_ };
        const float panOffsets[4]    = { -unisonSpread_, unisonSpread_ * 0.5f, -unisonSpread_ * 0.5f, unisonSpread_ };

        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
        {
            VoiceParameters p = baseBlockParams_;
            size_t uIdx = i % numUnison;
            p.voiceDetuneCents += detuneOffsets[uIdx];
            p.panpot = DSPUtils::clamp(p.panpot + panOffsets[uIdx], -1.0f, 1.0f);
            voices_[i].applyBlockParams(p);
        }
    }
    else
    {
        for (size_t i = 0; i < activeVoiceCapacity_; ++i)
        {
            voices_[i].applyBlockParams(baseBlockParams_);
        }
    }
}

void VoiceManager::process(float& leftOut, float& rightOut, int diagPoint, float diagTone) noexcept
{
    leftOut = 0.0f;
    rightOut = 0.0f;

    size_t activeCount = 0;
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (voices_[i].isActive())
        {
            float vl = 0.0f;
            float vr = 0.0f;
            voices_[i].renderNextSample(vl, vr, diagPoint, diagTone);
            leftOut += vl;
            rightOut += vr;
            activeCount++;
        }
    }

    // If diagnostic tone is requested at pre-filter or OSC1 (or VCA is bypassed) and no voice is triggered by noteOn,
    // route it through Voice 0 so the user can test the filter & VCA continuously!
    if (activeCount == 0)
    {
        if (diagPoint >= 4)
        {
            float vl = 0.0f;
            float vr = 0.0f;
            voices_[0].renderDiagnosticSample(vl, vr, diagPoint, diagTone);
            leftOut += vl;
            rightOut += vr;
        }
        else if (baseBlockParams_.diagBypassVCA)
        {
            float vl = 0.0f;
            float vr = 0.0f;
            voices_[0].renderNextSample(vl, vr, 0, 0.0f);
            leftOut += vl;
            rightOut += vr;
        }
    }

    // Normalization scale factor for unison / poly stack
    if (assignMode_ == VoiceAssignMode::Unison)
    {
        float scale = 1.0f / std::sqrt(static_cast<float>(std::min(activeVoiceCapacity_, size_t(4))));
        leftOut *= scale;
        rightOut *= scale;
    }
}

size_t VoiceManager::getActiveVoiceCount() const noexcept
{
    size_t count = 0;
    for (size_t i = 0; i < activeVoiceCapacity_; ++i)
    {
        if (voices_[i].isActive()) count++;
    }
    return count;
}

} // namespace ABDMS2000
