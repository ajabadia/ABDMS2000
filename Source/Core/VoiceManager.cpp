#include "VoiceManager.h"
#include <algorithm>

namespace ABDMS2000 {

void VoiceManager::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    for (auto& voice : voices_)
    {
        voice.prepare(sampleRate_);
    }
    reset();
}

void VoiceManager::reset() noexcept
{
    for (auto& voice : voices_)
    {
        voice.reset();
    }
    heldNotesStack_.clear();
    lastPlayedNote_ = -1;
}

int VoiceManager::findFreeOrOldestVoice() noexcept
{
    // 1. Look for an idle voice
    for (size_t i = 0; i < NUM_VOICES; ++i)
    {
        if (!voices_[i].isActive())
        {
            return static_cast<int>(i);
        }
    }

    // 2. Voice Stealing: Steal the voice with lowest amp envelope level or oldest note
    int bestCandidate = 0;
    float lowestAmp = 999.0f;
    uint32_t oldestAge = 0;

    for (size_t i = 0; i < NUM_VOICES; ++i)
    {
        float amp = voices_[i].getCurrentAmpLevel();
        uint32_t age = voices_[i].getNoteOnAge();

        if (amp < lowestAmp)
        {
            lowestAmp = amp;
            bestCandidate = static_cast<int>(i);
        }
        else if (std::abs(amp - lowestAmp) < 0.01f && age > oldestAge)
        {
            oldestAge = age;
            bestCandidate = static_cast<int>(i);
        }
    }

    return bestCandidate;
}

void VoiceManager::noteOn(int midiNote, float velocity) noexcept
{
    heldNotesStack_.push_back(midiNote);

    switch (assignMode_)
    {
        case VoiceAssignMode::Poly:
        {
            int voiceIdx = findFreeOrOldestVoice();
            voices_[voiceIdx].noteOn(midiNote, velocity, false);
            break;
        }

        case VoiceAssignMode::Mono:
        {
            bool isLegato = (heldNotesStack_.size() > 1);
            bool shouldRetrigger = (triggerMode_ == TriggerMode::Multi) || !isLegato;

            if (shouldRetrigger || !voices_[0].isActive())
            {
                voices_[0].noteOn(midiNote, velocity, isLegato);
            }
            else
            {
                // In Single Trigger mode, update pitch without retriggering envelope
                voices_[0].noteOn(midiNote, velocity, true);
            }
            break;
        }

        case VoiceAssignMode::Unison:
        {
            // All 4 voices play the same note with detune & spread
            for (size_t i = 0; i < NUM_VOICES; ++i)
            {
                voices_[i].noteOn(midiNote, velocity, false);
            }
            break;
        }
    }

    lastPlayedNote_ = midiNote;
}

void VoiceManager::noteOff(int midiNote) noexcept
{
    // Remove from held notes stack
    auto it = std::find(heldNotesStack_.begin(), heldNotesStack_.end(), midiNote);
    if (it != heldNotesStack_.end())
    {
        heldNotesStack_.erase(it);
    }

    switch (assignMode_)
    {
        case VoiceAssignMode::Poly:
        {
            for (auto& voice : voices_)
            {
                if (voice.getCurrentNote() == midiNote && voice.isActive())
                {
                    voice.noteOff();
                }
            }
            break;
        }

        case VoiceAssignMode::Mono:
        {
            if (heldNotesStack_.empty())
            {
                voices_[0].noteOff();
                lastPlayedNote_ = -1;
            }
            else
            {
                // Fall back to previous held key
                int previousNote = heldNotesStack_.back();
                bool shouldRetrigger = (triggerMode_ == TriggerMode::Multi);
                voices_[0].noteOn(previousNote, 0.8f, !shouldRetrigger);
                lastPlayedNote_ = previousNote;
            }
            break;
        }

        case VoiceAssignMode::Unison:
        {
            for (auto& voice : voices_)
            {
                if (voice.getCurrentNote() == midiNote)
                {
                    voice.noteOff();
                }
            }
            break;
        }
    }
}

void VoiceManager::allNotesOff() noexcept
{
    for (auto& voice : voices_)
    {
        voice.stopImmediately();
    }
    heldNotesStack_.clear();
    lastPlayedNote_ = -1;
}

void VoiceManager::process(float& leftOut, float& rightOut, const VoiceParameters& baseParams) noexcept
{
    if (assignMode_ == VoiceAssignMode::Unison)
    {
        // Calculate detune offsets for the 4 unison voices: -1.5, -0.5, +0.5, +1.5
        const float detuneFactors[4] = { -1.5f, -0.5f, 0.5f, 1.5f };
        const float panOffsets[4] = { -1.0f, -0.33f, 0.33f, 1.0f };

        for (size_t i = 0; i < NUM_VOICES; ++i)
        {
            if (voices_[i].isActive())
            {
                VoiceParameters unisonParams = baseParams;
                unisonParams.osc2Tune += (detuneFactors[i] * unisonDetune_);
                unisonParams.panpot = DSPUtils::clamp(baseParams.panpot + (panOffsets[i] * unisonSpread_), -1.0f, 1.0f);
                // Lower individual voice volume slightly in Unison to avoid clipping
                unisonParams.ampLevel *= 0.55f;

                voices_[i].renderNextSample(leftOut, rightOut, unisonParams);
            }
        }
    }
    else
    {
        for (auto& voice : voices_)
        {
            if (voice.isActive())
            {
                voice.renderNextSample(leftOut, rightOut, baseParams);
            }
        }
    }
}

size_t VoiceManager::getActiveVoiceCount() const noexcept
{
    size_t count = 0;
    for (const auto& voice : voices_)
    {
        if (voice.isActive()) count++;
    }
    return count;
}

} // namespace ABDMS2000
