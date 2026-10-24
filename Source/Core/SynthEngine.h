#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <ScopeDataCollector.h>
#include <array>
#include <atomic>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "AppLogger.h"
#include "AudioThreadSnapshot.h"
#include "DSP/Effects/DelayFX.h"
#include "DSP/Effects/Equalizer.h"
#include "DSP/Effects/ModFX.h"
#include "DSP/Sequencer/Arpeggiator.h"
#include "DSP/Sequencer/ModSequencer.h"
#include "DSP/Vocoder/Vocoder16Band.h"
#include "HardwareConstants.h"
#include "State/ParameterRegistry.gen.h"
#include "Voice.h"
#include "VoiceManager.h"

namespace ABDMS2000
{

/**
 * @brief Puntos de inyección de tono diagnóstico en el pipeline DSP.
 *
 * Cada punto permite inyectar una onda sinusoidal de prueba para depurar
 * etapas específicas del motor de audio, simulando el hardware de test del MS2000.
 */
enum class DiagnosticTonePoint
{
    Disabled = 0,
    PostMasterVolume = 1,  // Al final del processBlock (después del master volume)
    PreMasterVolume = 2,   // Antes de la atenuación del master volume
    PreEffects = 3,        // Después de Voices/Vocoder, antes de ModFX/DelayFX/EQ
    PreFilter = 4,         // Dentro de Voice: reemplaza mixedAudio antes del filtro
    OscillatorDirect = 5   // Dentro de Voice: reemplaza la salida de OSC1
};

/**
 * @brief Motor de sintetizador principal (abstracción de hardware MS2000/microKORG).
 *
 * La arquitectura de dos timbres (Single/Split/Layer) replica exactamente el
 * comportamiento del equipo físico: cada timbre tiene sus propios parámetros y su
 * propio juego de voces, con modo y reparto de voces globales del programa.
 *
 * @note Este motor es completamente thread-safe para el thread de audio y
 *       proporciona snapshoting a 60 FPS para ABDScope.
 */
class SynthEngine
{
  public:
    explicit SynthEngine(juce::AudioProcessorValueTreeState &apvts);
    ~SynthEngine() = default;

    // Inicialización del motor con buffer size de la DAW
    void prepare(double sampleRate, int samplesPerBlock);
    void reset();

  public:
    // Helpers publicos usados por la suite de C++.
    [[nodiscard]] ModSequencer &testGetModSequencer() const noexcept
    {
        return const_cast<SynthEngine &>(*this).modSeq_;
    }
    [[nodiscard]] const VoiceParameters &testGetVoiceParamsA() const noexcept
    {
        return voiceParamsA_;
    }
    [[nodiscard]] const VoiceParameters &testGetVoiceParamsB() const noexcept
    {
        return voiceParamsB_;
    }
    void testApplySeqModulation(bool second, VoiceParameters &out) const noexcept
    {
        const_cast<SynthEngine &>(*this).applySeqModulation(second, out);
    }

    // Procesa un bloque de audio completo (thread de audio)
    void processBlock(juce::AudioBuffer<float> &buffer,
                      juce::MidiBuffer &midiMessages,
                      juce::AudioPlayHead *playHead = nullptr);

    // Snapshot de telemetría para ABDScope (lock-free, POE)
    const AudioThreadSnapshot &getSnapshot() const noexcept
    {
        return currentSnapshot_;
    }

    // Eventos MIDI de alto nivel (thread de audio / tests síncronos)
    void noteOn(int midiChannel, int midiNoteNumber, float velocity);
    void noteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff = true);
    void allNotesOff();

    // Cola lock-free SPSC para eventos MIDI disparados desde el hilo de UI (BridgeActions)
    void enqueueNoteOn(int midiChannel, int midiNoteNumber, float velocity) noexcept;
    void enqueueNoteOff(int midiChannel, int midiNoteNumber, float velocity, bool allowTailOff = true) noexcept;
    void enqueueAllNotesOff() noexcept;

    // Ruedas de control (no bloqueantes)
    void setPitchBend(float bendMinus1to1) noexcept
    {
        pitchBendValue_.store(bendMinus1to1, std::memory_order_relaxed);
    }
    void setModWheel(float mod0to1) noexcept
    {
        modWheelValue_.store(mod0to1, std::memory_order_relaxed);
    }
    void setAftertouch(float at0to1) noexcept
    {
        aftertouchValue_.store(at0to1, std::memory_order_relaxed);
    }
    void setTempoBPM(float bpm) noexcept
    {
        if (bpm >= 20.0f && bpm <= 300.0f)
            tempoBpm_.store(bpm, std::memory_order_relaxed);
    }

    // Diagnóstico (solo en desarrollo)
    void setTestToneEnabled(bool enabled) noexcept;
    void setDiagnosticTone(int point, float freqHz = 440.0f, float level = 0.25f) noexcept;
    void setDiagnosticBypass(const juce::String &stage, bool enabled) noexcept;
    void resetAllDiagnosticBypasses() noexcept;
    [[nodiscard]] int getDiagnosticTonePoint() const noexcept;
    [[nodiscard]] const abd::scope::ScopeDataCollector &getScopeCollector() const noexcept;
    [[nodiscard]] double getSampleRate() const noexcept;

    [[nodiscard]] ModSequencer &getModSequencer() noexcept
    {
        return modSeq_;
    }

    [[nodiscard]] Arpeggiator &getArpeggiator() noexcept
    {
        return arpeggiator_;
    }
    [[nodiscard]] ModFX &getModFX() noexcept
    {
        return modFX_;
    }
    [[nodiscard]] DelayFX &getDelayFX() noexcept
    {
        return delayFX_;
    }
    [[nodiscard]] Equalizer &getMasterEQ() noexcept
    {
        return masterEQ_;
    }
    [[nodiscard]] Vocoder16Band &getVocoder() noexcept
    {
        return vocoder_;
    }

    // Sincronización completa de parámetros desde APVTS
    void updateParametersFromAPVTS() noexcept;
    void triggerArpNote(int note, float velocity, bool isNoteOn) noexcept;

    // Referencias públicas (evitar copia en el thread de audio)
    juce::AudioProcessorValueTreeState &apvts_;

    // Helpers pinchados por DSPCoreTests (timbre params + mod-seq apply).
    float paramValueById(const char *id, float defaultVal = 0.0f) const noexcept;
    float paramValueById(const juce::String &id, float defaultVal) const noexcept;
    float paramValue(bool second, const char *id, float defaultVal = 0.0f) const noexcept;

    /** Vuelca en `out` todos los parámetros del timbre (1 si `second` es false, 2 si no). */
    void readTimbreParams(bool second, VoiceParameters &out) noexcept;

    /** Lee las 3 filas del mod sequence del timbre en sus pistas del secuenciador. */
    void configureTimbreSeq(bool second, size_t firstTrack) noexcept;

    /** Aplica la salida de las 3 filas del timbre a sus parámetros de voz. */
    void applySeqModulation(bool second, VoiceParameters &out) noexcept;

  private:
    double sampleRate_ {44100.0};
    int samplesPerBlock_ {512};

    // Caché RT-safe de punteros atómicos del APVTS (cero alojamientos en processBlock)
    std::unordered_map<std::string_view, std::atomic<float> *> paramCache_;
    std::unordered_map<std::string_view, std::atomic<float> *> timbre2ParamCache_;

    struct SeqRowParamPtrs
    {
        std::atomic<float> *dest {nullptr};
        std::atomic<float> *motion {nullptr};
        std::array<std::atomic<float> *, ModSequencer::NUM_STEPS> steps {};
    };
    std::array<std::array<SeqRowParamPtrs, ModSequencer::TRACKS_PER_TIMBRE>, 2> seqParamPtrs_ {};

    // Cola SPSC lock-free de eventos MIDI desde la UI
    struct QueuedUiMidiEvent
    {
        enum class Type : uint8_t
        {
            NoteOn = 0,
            NoteOff = 1,
            AllNotesOff = 2
        };
        Type type {Type::NoteOn};
        int channel {1};
        int note {60};
        float velocity {0.0f};
        bool allowTailOff {true};
    };
    static constexpr size_t kUiMidiQueueSize = 256;
    std::array<QueuedUiMidiEvent, kUiMidiQueueSize> uiMidiQueue_ {};
    std::atomic<size_t> uiMidiWritePos_ {0};
    std::atomic<size_t> uiMidiReadPos_ {0};

    void pushUiMidiEvent(const QueuedUiMidiEvent &ev) noexcept;
    void drainUiMidiEvents() noexcept;

    // Arquitectura de dos timbres (Single / Split / Layer), como el equipo real:
    // cada timbre tiene sus **propios** parámetros (`t2…` para el Timbre 2) y su
    // propio juego de voces. El modo y el reparto de voces son globales del programa.
    VoiceParameters voiceParamsA_ {};
    VoiceParameters voiceParamsB_ {};
    VoiceManager voiceManagerA_;
    VoiceManager voiceManagerB_;
    int timbreMode_ {0};  // 0: Single, 1: Split, 2: Layer
    int splitKey_ {60};   // C4 por defecto (byte 0x12 real)
    int arpTarget_ {0};   // 0: Both, 1: Timbre 1, 2: Timbre 2 (byte 0x20 bits 4..5 real)
    bool hasHostBpm_ {false};

    ModSequencer modSeq_;
    Arpeggiator arpeggiator_;
    ModFX modFX_;
    DelayFX delayFX_;
    Equalizer masterEQ_;
    Vocoder16Band vocoder_;
    MultiModeFilter debugFilter_;

    std::vector<ArpNoteEvent> arpEventsBuffer_;

    AudioThreadSnapshot currentSnapshot_ {};
    std::atomic<float> masterVolume_ {0.8f};
    std::atomic<float> pitchBendValue_ {0.0f};
    std::atomic<float> modWheelValue_ {0.0f};
    std::atomic<float> aftertouchValue_ {0.0f};
    std::atomic<float> tempoBpm_ {120.0f};
    std::atomic<DiagnosticTonePoint> diagTonePoint_ {DiagnosticTonePoint::Disabled};
    std::atomic<float> testToneFrequency_ {440.0f};
    std::atomic<float> testToneLevel_ {0.25f};
    std::atomic<bool> testToneEnabled_ {false};
    std::atomic<bool> diagBypassFilter_ {false};
    std::atomic<bool> diagBypassVCA_ {false};
    std::atomic<bool> diagBypassOscMixer_ {false};
    std::atomic<bool> diagBypassDistortion_ {false};
    std::atomic<bool> diagBypassModFX_ {false};
    std::atomic<bool> diagBypassDelayFX_ {false};
    std::atomic<bool> diagBypassMasterEQ_ {false};
    double testTonePhase_ {0.0};
    juce::LinearSmoothedValue<float> smoothedMasterGain_ {0.8f};
    bool wasHostPlaying_ {false};
    double hostPpqPosition_ {0.0};
    int idleTailSamplesRemaining_ {0};

    // ── ABDScope taps (native C++ capture, block-flushed) ──
    abd::scope::ScopeDataCollector scopeCollector_;
    abd::scope::ScopeTap *tapMaster_ {nullptr};
    abd::scope::ScopeTap *tapPreFx_ {nullptr};
    abd::scope::ScopeTap *tapOscMix_ {nullptr};
    abd::scope::ScopeTap *tapPostFilter_ {nullptr};
    abd::scope::ScopeTap *tapPostVca_ {nullptr};
    abd::scope::ScopeTap *tapLfo1_ {nullptr};
    bool scopeTapsRegistered_ {false};

    // Per-block capture scratch buffers (sized to samplesPerBlock_ in prepare()).
    std::vector<float> scopeBufMasterL_, scopeBufMasterR_;
    std::vector<float> scopeBufPreFxL_, scopeBufPreFxR_;
    std::vector<float> scopeBufOscMix_;
    std::vector<float> scopeBufPostFilter_;
    std::vector<float> scopeBufPostVca_;
    std::vector<float> scopeBufLfo1_;

    void registerScopeTaps();
    void flushScopeTaps(int numSamples) noexcept;
    void dispatchMidiMessage(const juce::MidiMessage &msg);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SynthEngine)
};

}  // namespace ABDMS2000
