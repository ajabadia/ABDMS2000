#pragma once
#include "Voice.h"
#include <array>
#include <vector>

namespace ABDMS2000 {

enum class VoiceAssignMode {
    Mono = 0,
    Poly,
    Unison
};

enum class TriggerMode {
    Single = 0, // Legato (envelopes do not retrigger if another key is held)
    Multi       // Retrigger envelopes on every note press
};

/**
 * @brief 4-Voice Polyphonic and Unison Voice Manager with intelligent Voice Stealing.
 */
class VoiceManager {
public:
    static constexpr size_t NUM_VOICES = 4;

    VoiceManager() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setAssignMode(VoiceAssignMode mode) noexcept { assignMode_ = mode; }
    void setTriggerMode(TriggerMode mode) noexcept { triggerMode_ = mode; }
    void setUnisonDetune(float detuneCents) noexcept { unisonDetune_ = detuneCents; }
    void setUnisonSpread(float spread0to1) noexcept { unisonSpread_ = spread0to1; }

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;

    void process(float& leftOut, float& rightOut, const VoiceParameters& params) noexcept;

    size_t getActiveVoiceCount() const noexcept;

private:
    double sampleRate_{ 44100.0 };
    std::array<Voice, NUM_VOICES> voices_;

    VoiceAssignMode assignMode_{ VoiceAssignMode::Poly };
    TriggerMode triggerMode_{ TriggerMode::Multi };
    float unisonDetune_{ 10.0f }; // Cents
    float unisonSpread_{ 0.5f };  // Stereo width

    // Key stack for Mono legato
    std::vector<int> heldNotesStack_;
    int lastPlayedNote_{ -1 };

    int findFreeOrOldestVoice() noexcept;
};

} // namespace ABDMS2000
