#include "Arpeggiator.h"
#include "../Common/DSPUtils.h"
#include <algorithm>

namespace ABDMS2000 {

// ═══════════════════════════════════════════════════════════════
// MS2000 canonical Arp/Seq Resolution → steps per beat
// SysEx: 0=1/48 .. 15=4/1
// ═══════════════════════════════════════════════════════════════
static constexpr float kResolutionStepsPerBeat[16] = {
    12.0f,  // 0:  1/48
    8.0f,   // 1:  1/32
    6.0f,   // 2:  1/24
    4.0f,   // 3:  1/16 (canonical 16th note arpeggio: 4 steps per beat)
    3.0f,   // 4:  1/12
    2.0f,   // 5:  1/8  (8th note arpeggio: 2 steps per beat)
    1.5f,   // 6:  1/6
    1.0f,   // 7:  1/4  (quarter note: 1 step per beat)
    0.75f,  // 8:  1/3
    0.5f,   // 9:  1/2
    0.3333f,// 10: 3/4
    0.25f,  // 11: 1/1  (whole note: 1 step every 4 beats)
    0.1667f,// 12: 3/2
    0.125f, // 13: 2/1
    0.0833f,// 14: 3/1
    0.0625f // 15: 4/1
};

float Arpeggiator::syncResolutionToStepsPerBeat(int idx) noexcept
{
    if (idx < 0 || idx > 15) return kResolutionStepsPerBeat[3];
    return kResolutionStepsPerBeat[idx];
}

void Arpeggiator::setLatch(bool latch) noexcept
{
    if (latch_ && !latch)
    {
        latch_ = false;
        physicallyHeldCount_ = 0;
        heldNotes_.clear();
        generatedPattern_.clear();
        reset();
        return;
    }
    latch_ = latch;
}

void Arpeggiator::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    setTempoBPM(bpm_);
    reset();
}

void Arpeggiator::reset() noexcept
{
    stepSampleCounter_ = 0.0;
    currentNoteActive_ = false;
    currentPlayingNote_ = -1;
    patternIndex_ = 0;
    patternDirection_ = true;
}

void Arpeggiator::setOctaveRange(int octaves1to4) noexcept
{
    octaveRange_ = std::max(1, std::min(4, octaves1to4));
    rebuildPattern();
}

void Arpeggiator::setGateTime(float gate0to1) noexcept
{
    gateTime_ = DSPUtils::clamp(gate0to1, 0.05f, 1.0f);
}

void Arpeggiator::setTempoBPM(float bpm) noexcept
{
    bpm_ = DSPUtils::clamp(bpm, 20.0f, 300.0f);
    double secondsPerBeat = 60.0 / bpm_;
    double secondsPerStep = secondsPerBeat / syncStepsPerBeat_;
    samplesPerStep_ = secondsPerStep * sampleRate_;
}

void Arpeggiator::setSyncResolution(int idx) noexcept
{
    if (idx < 0 || idx > 15) idx = 3;
    syncStepsPerBeat_ = kResolutionStepsPerBeat[idx];
    setTempoBPM(bpm_); // re-evaluate samplesPerStep
}

void Arpeggiator::noteOn(int midiNote, float /*velocity*/) noexcept
{
    physicallyHeldCount_++;

    if (latch_ && physicallyHeldCount_ == 1)
    {
        heldNotes_.clear();
        generatedPattern_.clear();
    }

    auto it = std::find(heldNotes_.begin(), heldNotes_.end(), midiNote);
    if (it == heldNotes_.end())
    {
        heldNotes_.push_back(midiNote);
        std::sort(heldNotes_.begin(), heldNotes_.end());
        rebuildPattern();

        if (heldNotes_.size() == 1)
        {
            reset();
            stepSampleCounter_ = samplesPerStep_;
        }
    }
}

void Arpeggiator::noteOff(int midiNote) noexcept
{
    if (physicallyHeldCount_ > 0) physicallyHeldCount_--;

    if (!latch_)
    {
        auto it = std::find(heldNotes_.begin(), heldNotes_.end(), midiNote);
        if (it != heldNotes_.end())
        {
            heldNotes_.erase(it);
            rebuildPattern();
        }
    }
}

void Arpeggiator::allNotesOff() noexcept
{
    physicallyHeldCount_ = 0;
    heldNotes_.clear();
    generatedPattern_.clear();
    reset();
}

void Arpeggiator::rebuildPattern() noexcept
{
    generatedPattern_.clear();
    if (heldNotes_.empty()) return;

    for (int oct = 0; oct < octaveRange_; ++oct)
    {
        for (int note : heldNotes_)
        {
            generatedPattern_.push_back(note + (oct * 12));
        }
    }

    if (type_ == ArpType::Down)
    {
        std::reverse(generatedPattern_.begin(), generatedPattern_.end());
    }
}

bool Arpeggiator::processStep(int numSamples, std::vector<ArpNoteEvent>& outEvents) noexcept
{
    if (!enabled_ || generatedPattern_.empty())
    {
        if (currentNoteActive_ && currentPlayingNote_ >= 0)
        {
            outEvents.push_back({ currentPlayingNote_, 0.0f, false });
            currentNoteActive_ = false;
            currentPlayingNote_ = -1;
        }
        return !outEvents.empty();
    }

    stepSampleCounter_ += numSamples;

    // Gate Off event
    double gateSamples = samplesPerStep_ * gateTime_;
    if (currentNoteActive_ && stepSampleCounter_ >= gateSamples)
    {
        outEvents.push_back({ currentPlayingNote_, 0.0f, false });
        currentNoteActive_ = false;
    }

    // Step trigger event
    if (stepSampleCounter_ >= samplesPerStep_)
    {
        stepSampleCounter_ -= samplesPerStep_;

        if (generatedPattern_.empty()) return !outEvents.empty();

        int noteToPlay = generatedPattern_[0];

        switch (type_)
        {
            case ArpType::Up:
            case ArpType::Down:
                noteToPlay = generatedPattern_[patternIndex_ % generatedPattern_.size()];
                patternIndex_ = (patternIndex_ + 1) % generatedPattern_.size();
                break;

            case ArpType::Alt1:
            case ArpType::Alt2:
                noteToPlay = generatedPattern_[patternIndex_];
                if (patternDirection_)
                {
                    patternIndex_++;
                    if (patternIndex_ >= generatedPattern_.size())
                    {
                        patternIndex_ = (type_ == ArpType::Alt1) ? (generatedPattern_.size() - 1) : (generatedPattern_.size() - 2);
                        patternDirection_ = false;
                    }
                }
                else
                {
                    if (patternIndex_ == 0)
                    {
                        patternIndex_ = (type_ == ArpType::Alt1) ? 0 : 1;
                        patternDirection_ = true;
                    }
                    else
                    {
                        patternIndex_--;
                    }
                }
                break;

            case ArpType::Random:
                DSPUtils::randomBipolar(rngState_); // Advance LCG
                noteToPlay = generatedPattern_[rngState_ % generatedPattern_.size()];
                break;

            case ArpType::Trigger:
                // Chord trigger mode
                for (int n : generatedPattern_)
                {
                    outEvents.push_back({ n, 0.85f, true });
                }
                currentNoteActive_ = true;
                currentPlayingNote_ = generatedPattern_[0];
                return true;
        }

        currentPlayingNote_ = noteToPlay;
        currentNoteActive_ = true;
        outEvents.push_back({ noteToPlay, 0.85f, true });
    }

    return !outEvents.empty();
}

} // namespace ABDMS2000
