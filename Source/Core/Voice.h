"#pragma once
#include "../DSP/Oscillators/VAOscillator.h"
#include "../DSP/Oscillators/DWGSOscillator.h"
#include "../DSP/Oscillators/VoxWaveOscillator.h"
#include "../DSP/Oscillators/NoiseGenerator.h"
#include "../DSP/Oscillators/OSC2Modulator.h"
#include "../DSP/Filters/MultiModeFilter.h"
#include "../DSP/Envelopes/ADSREnvelope.h"
#include "../DSP/Modulation/PortamentoGlide.h"
#include "../DSP/Modulation/LFO.h"
#include "../DSP/Modulation/VirtualPatchMatrix.h"

namespace ABDMS2000 {

/**
 * @brief Tipos de onda OSC1 del MS2000/microKORG.
 * Corresponden a los slots de hardware del sintetizador real.
 */
enum class OSC1Type {
    Saw = 0,
    Pulse,
    Triangle,
    Sine,
    VoxWave,   // Formant-based vocal wave (HW slot 4)
    DWGS,       // Digital Waveform Generator System (HW slot 5)
    Noise,      // White noise generator (HW slot 6)
    AudioIn     // External audio input for Vocoder (HW slot 7)
};

/**
 * @brief Modos de sincronización de LFO del MS2000.
 * Bits 4-5 del byte de control de LFO en el SysEx real.
 */
enum class KeySyncMode {
    Off = 0,    // LFO free-runs, never resets
    Timbre,     // Reset on first note of the timbre (not on legato in Mono mode)
    Voice       // Reset on every single note press per voice
};

/**
 * @brief Modulación del Mod Sequence del timbre.
 * 
 * El secuenciador es por timbre (3 filas A/B/C dentro del bloque de 108 B
 * del programa real), así que cada VoiceParameters lleva la suya.
 */
struct SeqModulation {
    float pitch{ 0.0f };
    float osc2Pitch{ 0.0f };   // semitonos (±24 a fondo de escala)
    float osc2Tune{ 0.0f };    // semitonos (±0.5 = ±50 cents)
    float osc1Ctrl1{ 0.0f };
    float noiseLevel{ 0.0f };
    float osc1Level{ 0.0f };
    float osc2Level{ 0.0f };
    float cutoff{ 0.0f };
    float amp{ 0.0f };
    float pan{ 0.0f };
    float lfo2Freq{ 0.0f };
};

/**
 * @brief Parámetros completos de un timbre del MS2000/microKORG.
 * 
 * Todos los valores están normalizados a rangos de 0.0-1.0 donde es posible,
 * coincidiendo con la representación interna del hardware (bytes 0-127).
 */
struct VoiceParameters {
    // OSC 1
    OSC1Type osc1Type{ OSC1Type::Saw };
    int osc1DwgsIndex{ 0 };
    float osc1Ctrl1{ 0.0f };
    float osc1Ctrl2{ 0.0f };
    float osc1Level{ 1.0f };

    // OSC 2
    VAWaveform osc2Wave{ VAWaveform::Sawtooth };
    float osc2Semitone{ 0.0f };
    float osc2Tune{ 0.0f };
    OSC2ModulationMode osc2ModMode{ OSC2ModulationMode::Off };
    float osc2Level{ 0.0f };

    // Noise
    float noiseLevel{ 0.0f };

    // Filter
    FilterType filterType{ FilterType::LPF24 };
    float filterCutoffNorm{ 1.0f }; // 0..1
    float filterResonance{ 0.0f };   // 0..1
    float eg1FilterIntensity{ 0.0f };// -1.0 to +1.0
    float filterKbdTrack{ 0.0f };    // -1.0 to +1.0
    float filterVeloSens{ 0.0f };    // -1.0 to +1.0 (byte 23 del bloque real, ±63)

    // Envelopes
    float eg1Attack{ 0.01f };
    float eg1Decay{ 0.3f };
    float eg1Sustain{ 0.0f };
    float eg1Release{ 0.3f };

    float eg2Attack{ 0.01f };
    float eg2Decay{ 0.3f };
    float eg2Sustain{ 0.8f };
    float eg2Release{ 0.3f };

    // LFOs: LFO1 uses LFOWaveform, LFO2 uses LFOWaveformLFO2
    LFOWaveform lfo1Wave{ LFOWaveform::Triangle };
    float lfo1FreqHz{ 5.0f };
    KeySyncMode lfo1KeySync{ KeySyncMode::Voice };
    bool lfo1TempoSync{ false };
    int lfo1SyncNote{ 4 };    // 0..14 = 1/1 .. 1/128

    LFOWaveformLFO2 lfo2Wave{ LFOWaveformLFO2::Sine };
    float lfo2FreqHz{ 5.0f };
    KeySyncMode lfo2KeySync{ KeySyncMode::Voice };
    bool lfo2TempoSync{ false };
    int lfo2SyncNote{ 4 };    // 0..14 = 1/1 .. 1/128

    // VCA / Amp
    float ampLevel{ 0.8f };
    float panpot{ 0.0f }; // -1.0 (L) to +1.0 (R)
    bool distortionOn{ false };
    float ampKeyTrack{ 0.0f };  // -1.0 to +1.0
    float ampVeloSens{ 0.0f };  // -1.0 to +1.0 (byte 28 del bloque real, ±63)

    // Mod Sequence de este timbre
    SeqModulation seq{};
    float portamentoTime{ 0.0f };
    float voiceDetuneCents{ 0.0f };

    // Virtual Patch Slots
    PatchSlot patchSlots[4]{};
    float modWheelValue{ 0.0f };
    float pitchBendValue{ 0.0f };

    // Diagnostic Tone Injection & Component Bypasses
    int diagnosticTonePoint{ 0 }; // 0: Disabled, 4: PreFilter, 5: OscillatorDirect
    float diagnosticToneValue{ 0.0f };
    bool diagBypassFilter{ false };
    bool diagBypassVCA{ false };
    bool diagBypassOscMixer{ false };
    bool diagBypassDistortion{ false };

    // Tempo (from DAW AudioPlayHead)
    float bpm{ 120.0f };
};

/**
 * @brief Voz sintetizadora individual en la arquitectura de 4 voces.
 * 
 * Cada voice instancia todos los bloques DSP (osciladores, filtro, envolventes,
 * LFOs, patch matrix) y procesa muestras stereo por sample.
 * 
 * @warning No es thread-safe: todas las llamadas a prepare/noteOn/noteOff/process
 *          deben provenir del thread de audio.
 */
class Voice {
public:
    Voice() = default;
    Voice(Voice&&) noexcept = default;
    Voice& operator=(Voice&&) noexcept = default;

    void prepare(double sampleRate) noexcept;
    void reset() noexcept;

    void noteOn(int midiNote, float velocity, bool glideEnabled, bool isFirstTimbreNote = false, bool isLegato = false) noexcept;
    void noteOff() noexcept;
    void stopImmediately() noexcept;

    [[nodiscard]] bool isActive() const noexcept;
    [[nodiscard]] bool isInRelease() const noexcept { return eg2_.getStage() == EnvelopeStage::Release; }
    [[nodiscard]] int getCurrentNote() const noexcept { return currentMidiNote_; }
    [[nodiscard]] uint32_t getNoteOnAge() const noexcept { return noteAge_; }
    [[nodiscard]] float getCurrentAmpLevel() const noexcept;

    struct DiagnosticStats {
        float basePitch{ 0.0f };
        float osc1Sig{ 0.0f };
        float mixedAudio{ 0.0f };
        float filtered{ 0.0f };
        float eg1Val{ 0.0f };
        float eg2Val{ 0.0f };
        float vcaOut{ 0.0f };
        float lGain{ 0.0f };
        float rGain{ 0.0f };
        float leftOutSample{ 0.0f };
        float rightOutSample{ 0.0f };
    };
    
    [[nodiscard]] const DiagnosticStats& getDiagnosticStats() const noexcept { return lastDiagStats_; }

    // Read-only access to the per-voice LFOs (used by the analytical scope taps).
    [[nodiscard]] const LFO& getLfo1() const noexcept { return lfo1_; }
    [[nodiscard]] const LFO& getLfo2() const noexcept { return lfo2_; }

    // Call once per audio block to apply all static/structure parameters.
    // Must be called before any renderNextSample() calls in the same block.
    void applyBlockParams(const VoiceParameters& params) noexcept;

    // Process a single stereo sample — only per-sample modulation values.
    // Assumes applyBlockParams() was already called this block.
    void renderNextSample(float& leftOut, float& rightOut, int diagPoint = 0, float diagTone = 0.0f) noexcept;
    void renderDiagnosticSample(float& leftOut, float& rightOut, int diagPoint, float diagTone) noexcept;

private:
    double sampleRate_{ 44100.0 };
    int currentMidiNote_{ 60 };
    float velocity_{ 0.8f };
    uint32_t noteAge_{ 0 };
    float prevMasterPhase_{ 0.0f };

    // Cached snapshot of the last applied block parameters
    VoiceParameters cachedParams_;

    VAOscillator osc1VA_;
    DWGSOscillator osc1DWGS_;
    VoxWaveOscillator osc1VoxWave_;
    VAOscillator osc2_;
    NoiseGenerator noiseGen_;
    MultiModeFilter filter_;
    ADSREnvelope eg1_;
    ADSREnvelope eg2_;
    PortamentoGlide glide_;
    LFO lfo1_;
    LFO lfo2_;
    VirtualPatchMatrix patchMatrix_;
};

} // namespace ABDMS2000"
