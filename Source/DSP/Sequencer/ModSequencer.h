#pragma once
#include <array>
#include <cstdint>

namespace ABDMS2000 {

enum class ModSeqMotion {
    Step = 0,
    Smooth
};

/** Cómo arranca el reloj al pulsar una tecla (byte 53 bits 0,1 del bloque real). */
enum class ModSeqKeySync {
    Off = 0,
    Timbre,
    Voice
};

enum class ModSeqMode {
    Forward = 0,
    Backward,
    Bounce,
    Random
};

/**
 * Los **31 destinos reales** del Mod Sequence del MS2000 (`cbSeqKnob1..3` del panel
 * ReMS2000: `NONE … PATCH4 INT`), en su orden exacto. El índice del parámetro
 * `seq{N}Dest` es el propio valor de byte del equipo, así que el mapeo es directo.
 *
 * Los pasos tienen rango distinto según el destino (manual del MS2000): ±6 para
 * `StepLength`, ±24 para `Pitch`/`OSC2Semi` y ±63 para el resto.
 */
enum class ModSeqDest {
    None = 0,
    Pitch,
    StepLength,
    Portamento,
    OSC1Ctrl1,
    OSC1Ctrl2,
    OSC2Semi,
    OSC2Tune,
    OSC1Level,
    OSC2Level,
    NoiseLevel,
    Cutoff,
    Resonance,
    EG1Int,
    KbdTrk,
    AmpLevel,
    Panpot,
    EG1Attack,
    EG1Decay,
    EG1Sustain,
    EG1Release,
    EG2Attack,
    EG2Decay,
    EG2Sustain,
    EG2Release,
    LFO1Freq,
    LFO2Freq,
    Patch1Int,
    Patch2Int,
    Patch3Int,
    Patch4Int
};

/** Rango real de un paso para ese destino (±6 / ±24 / ±63). */
static constexpr float modSeqStepRange(ModSeqDest dest) noexcept
{
    switch (dest)
    {
        case ModSeqDest::StepLength: return 6.0f;
        case ModSeqDest::Pitch:
        case ModSeqDest::OSC2Semi:   return 24.0f;
        default:                     return 63.0f;
    }
}

struct ModSeqTrack {
    ModSeqDest destination{ ModSeqDest::None };
    ModSeqMotion motion{ ModSeqMotion::Step };
    ModSeqMode mode{ ModSeqMode::Forward };
    int length{ 16 }; // 1 to 16
    std::array<float, 16> steps{ 0.0f }; // Normalized 0..1
};

/**
 * @brief Mod sequencer de 6 pistas de 16 pasos.
 *
 * El MS2000 real tiene **tres** filas (A, B, C) **por timbre**, dentro del bloque de
 * 108 B de cada timbre (bytes 52..107). Aquí viven las seis en un solo secuenciador:
 *
 *   - pistas `0..2` → Timbre 1 (filas A, B, C)
 *   - pistas `3..5` → Timbre 2 (filas A, B, C)
 *
 * El reloj (tempo, resolución y avance) es común, como en el hardware: cada fila elige
 * su destino, su movimiento y sus 16 valores.
 */
class ModSequencer {
public:
    static constexpr size_t TRACKS_PER_TIMBRE = 3;
    static constexpr size_t NUM_TRACKS = TRACKS_PER_TIMBRE * 2; // 6 = 2 timbres × 3 filas
    static constexpr size_t NUM_STEPS = 16;

    /** Primera pista del Timbre 1 (0) y del Timbre 2 (3). */
    static constexpr size_t TIMBRE1_TRACK = 0;
    static constexpr size_t TIMBRE2_TRACK = TRACKS_PER_TIMBRE;

    ModSequencer() = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void setEnabled(bool enabled) noexcept { enabled_ = enabled; }
    bool isEnabled() const noexcept { return enabled_; }

    void setTempoBPM(float bpm) noexcept;

    // Tempo Sync resolution: index 0..15 (MS2000 SysEx canonical)
    // 0=1/48, 3=1/16, 7=1/4, 11=1/1, 15=4/1
    void setSyncResolution(int idx) noexcept;
    static float syncResolutionToStepsPerBeat(int idx) noexcept;

    ModSeqTrack& getTrack(size_t index) noexcept { return tracks_[index % NUM_TRACKS]; }
    const ModSeqTrack& getTrack(size_t index) const noexcept { return tracks_[index % NUM_TRACKS]; }

    /**
     * Reinicia las filas indicadas (por defecto, todas): el Key Sync del equipo es
     * por timbre, así que el motor lo pide con su rango de pistas.
     */
    void triggerKeySync(size_t firstTrack = 0, size_t count = NUM_TRACKS) noexcept;
    void advanceClock(int numSamples) noexcept;

    float getTrackOutput(size_t trackIndex) const noexcept;
    int getCurrentStepIndex(size_t trackIndex) const noexcept { return currentStep_[trackIndex % NUM_TRACKS]; }

private:
    double sampleRate_{ 44100.0 };
    bool enabled_{ false };
    float bpm_{ 120.0f };
    float syncStepsPerBeat_{ 16.0f }; // Default: 1/16 (16 steps per beat)

    double samplesPerStep_{ 5512.5 };
    double stepSampleCounter_{ 0.0 };

    std::array<ModSeqTrack, NUM_TRACKS> tracks_{};
    std::array<int, NUM_TRACKS> currentStep_{ 0, 0, 0, 0, 0, 0 };
    std::array<bool, NUM_TRACKS> bounceDirection_{ true, true, true, true, true, true }; // true = forward, false = backward

    // Slew limiters for Smooth mode
    std::array<float, NUM_TRACKS> smoothedOutput_{ 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    double slewMultiplier_{ 0.99 };

    uint32_t rngState_{ 0x12345678 };

    void advanceStep() noexcept;
};

} // namespace ABDMS2000
