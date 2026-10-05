"#pragma once
#include "Voice.h"
#include <array>
#include <vector>

namespace ABDMS2000 {

/**
 * @brief Modos de asignación de voces compatibles con MS2000/microKORG.
 */
enum class VoiceAssignMode {
    Mono = 0,
    Poly,
    Unison
};

/**
 * @brief Modos de gatillo de envolventes.
 * 
 * - Single: legato, las envolventes no se re-disparan si se mantiene otra tecla
 * - Multi: las envolventes se re-disparan en cada nueva nota
 */
enum class TriggerMode {
    Single = 0, // Legato (envelopes do not retrigger if another key is held)
    Multi       // Retrigger envelopes on every note press
};

/**
 * @brief Estado de una ranura de voz individual.
 */
struct VoiceSlotState {
    int currentMidiNote{ -1 };
    uint32_t noteOnTime{ 0 };
    bool isKeyPressed{ false };
    bool isHeldByLatch{ false };
    double lastFrequencyHz{ 0.0 };
};

/**
 * @brief Administrador de voces de alto rendimiento.
 * 
 * Implementa la asignación de voces idéntica al hardware Korg MS2000 / microKORG:
 * - 4 voces de polifonía auténtica (ampliable a 32 en modo avanzado)
 * - Sistema inteligente de "voice stealing":
 *   1. Protección de re- triggering de notas repetidas
 *   2. Robo de voces marcadas con HOLD cuya tecla fue liberada
 *   3. Robo en fase de release (voz más cercana al silencio)
 *   4. Robo FIFO de notas sostenidas más antiguas
 * - Anti-click con rampa descendente rápida en voces robadas
 * - Portamento con glide (fingered/legato en Mono/Unison, per-voice en Poly)
 * - Soporte de HOLD / Latch
 */
class VoiceManager {
public:
    static constexpr size_t NUM_HW_VOICES = 4;
    static constexpr size_t MAX_EXPANDED_VOICES = 32;

    VoiceManager() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setAssignMode(VoiceAssignMode mode) noexcept
    {
        if (assignMode_ != mode)
        {
            allNotesOff();
            assignMode_ = mode;
        }
    }
    void setTriggerMode(TriggerMode mode) noexcept { triggerMode_ = mode; }
    void setUnisonDetune(float detuneCents) noexcept { unisonDetune_ = detuneCents; }
    void setUnisonSpread(float spread0to1) noexcept { unisonSpread_ = spread0to1; }
    void setPortamentoTime(float portamentoTime0to127) noexcept;
    void setHoldButtonState(bool active) noexcept;

    // Advanced mode polyphony configuration (4..32)
    void setMaxPolyphony(size_t numVoices) noexcept;
    [[nodiscard]] size_t getMaxPolyphony() const noexcept { return activeVoiceCapacity_; }

    void noteOn(int midiNote, float velocity) noexcept;
    void noteOff(int midiNote) noexcept;
    void allNotesOff() noexcept;

    // Call once per audio block to apply static parameters to all voices
    void applyBlockParams(const VoiceParameters& params) noexcept;

    // Process one stereo sample across all active voices
    void process(float& leftOut, float& rightOut, int diagPoint = 0, float diagTone = 0.0f) noexcept;

    [[nodiscard]] size_t getActiveVoiceCount() const noexcept;
    [[nodiscard]] const Voice& getVoice(size_t index) const noexcept { return voices_[index % MAX_EXPANDED_VOICES]; }

private:
    double sampleRate_{ 44100.0 };
    std::array<Voice, MAX_EXPANDED_VOICES> voices_;
    std::array<VoiceSlotState, MAX_EXPANDED_VOICES> slotStates_;

    size_t activeVoiceCapacity_{ NUM_HW_VOICES };
    VoiceAssignMode assignMode_{ VoiceAssignMode::Poly };
    TriggerMode triggerMode_{ TriggerMode::Multi };
    float unisonDetune_{ 10.0f }; // Cents
    float unisonSpread_{ 0.5f };  // Stereo width
    float portamentoTimeParam_{ 0.0f }; // 0..127
    float glideTimeSec_{ 0.0f };
    bool isHoldActive_{ false };

    uint32_t globalTimeCounter_{ 0 };

    // Per-block cached params for Unison precomputation
    VoiceParameters baseBlockParams_;

    // Key stack for Mono legato
    std::vector<int> heldNotesStack_;
    int lastMonoNote_{ -1 };

    int findVoiceToSteal(int incomingNote) noexcept;
    [[nodiscard]] bool anyKeyPressed() const noexcept;
    void releaseVoiceSlot(size_t index) noexcept;
};

} // namespace ABDMS2000"
