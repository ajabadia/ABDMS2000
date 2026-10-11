#pragma once
#include <cstdint>
#include <vector>

namespace ABDMS2000
{

enum class ArpType
{
    Up = 0,
    Down,
    Alt1,
    Alt2,
    Random,
    Trigger
};

struct ArpNoteEvent
{
    int midiNote {60};
    float velocity {0.8f};
    bool isNoteOn {true};
};

/**
 * @brief Musical Arpeggiator with Tempo Sync, Octave Multi-Range, and Swing.
 */
class Arpeggiator
{
  public:
    Arpeggiator() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept
    {
        enabled_ = enabled;
    }
    bool isEnabled() const noexcept
    {
        return enabled_;
    }

    void setType(ArpType type) noexcept
    {
        type_ = type;
    }
    void setOctaveRange(int octaves1to4) noexcept;
    void setGateTime(float gate0to1) noexcept;
    void setTempoBPM(float bpm) noexcept;

    // Tempo Sync resolution: index 0..15 (MS2000 SysEx canonical)
    // 0=1/48, 3=1/16, 7=1/4, 11=1/1, 15=4/1
    void setSyncResolution(int idx) noexcept;
    static float syncResolutionToStepsPerBeat(int idx) noexcept;
    void setLatch(bool latch) noexcept;
    void setKeySync(bool keySync) noexcept
    {
        keySync_ = keySync;
    }

    // microKORG / Advanced 8-Step Rhythm Mask
    void setStepMask(int stepIdx, bool active) noexcept
    {
        if (stepIdx >= 0 && stepIdx < 8)
            stepMask_[stepIdx] = active;
    }
    bool getStepMask(int stepIdx) const noexcept
    {
        return (stepIdx >= 0 && stepIdx < 8) ? stepMask_[stepIdx] : true;
    }
    void setLastStep(int lastStep) noexcept
    {
        lastStep_ = (lastStep >= 1 && lastStep <= 8) ? lastStep : 8;
    }
    int getLastStep() const noexcept
    {
        return lastStep_;
    }
    void setStepMaskEnabled(bool enabled) noexcept
    {
        stepMaskEnabled_ = enabled;
    }
    bool isStepMaskEnabled() const noexcept
    {
        return stepMaskEnabled_;
    }

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;

    // Check if a note should be triggered or released at current sample step
    bool processStep(int numSamples, std::vector<ArpNoteEvent> &outEvents) noexcept;

  private:
    double sampleRate_ {44100.0};
    bool enabled_ {false};
    ArpType type_ {ArpType::Up};
    int octaveRange_ {1};
    float gateTime_ {0.8f};
    float bpm_ {120.0f};
    bool latch_ {false};
    bool keySync_ {true};
    int physicallyHeldCount_ {0};

    // microKORG 8-step rhythm mask state
    bool stepMask_[8] {true, true, true, true, true, true, true, true};
    int lastStep_ {8};
    int currentRhythmStep_ {0};
    bool stepMaskEnabled_ {true};

    float syncStepsPerBeat_ {16.0f};  // Default: 1/16 (16 steps per beat)
    double samplesPerStep_ {5512.5};
    double stepSampleCounter_ {0.0};
    bool currentNoteActive_ {false};
    int currentPlayingNote_ {-1};

    std::vector<int> heldNotes_;
    std::vector<int> generatedPattern_;
    size_t patternIndex_ {0};
    bool patternDirection_ {true};  // true = up, false = down

    uint32_t rngState_ {0x56781234};

    void rebuildPattern() noexcept;
};

}  // namespace ABDMS2000
