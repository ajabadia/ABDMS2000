// ============================================================
// Voice.cpp - Implementación del motor DSP de voz individual del MS2000/microKORG
// ============================================================

#include "Voice.h"
#include <cmath>

namespace ABDMS2000
{

/**
 * @brief Prepara el Voice para un nuevo sample rate.
 *
 * Inicializa todos los osciladores, envolventes, LFOs y el generador de ruido.
 * @param sampleRate Nuevo sample rate (se asegura que sea > 1000 Hz).
 */
void Voice::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    osc1VA_.prepare(sampleRate_);
    osc1DWGS_.prepare(sampleRate_);
    osc1VoxWave_.prepare(sampleRate_);
    osc2_.prepare(sampleRate_);
    noiseGen_.reset();
    filter_.prepare(sampleRate_);
    eg1_.prepare(sampleRate_);
    eg2_.prepare(sampleRate_);
    glide_.prepare(sampleRate_);
    lfo1_.prepare(sampleRate_);
    lfo2_.prepare(sampleRate_);
    reset();
}

/**
 * @brief Restablece el Voice a su estado inicial.
 *
 * Apaga todos los osciladores, filtros, envolventes, LFOs y reinicia el generador de ruido.
 */
void Voice::reset() noexcept
{
    osc1VA_.reset();
    osc1DWGS_.reset();
    osc1VoxWave_.reset();
    osc2_.reset();
    filter_.reset();
    eg1_.reset();
    eg2_.reset();
    glide_.reset(static_cast<float>(currentMidiNote_));
    lfo1_.reset();
    lfo2_.reset();
    noteAge_ = 0;
    prevMasterPhase_ = 0.0f;
}

/**
 * @brief Inicia una nueva nota (noteOn MIDI).
 *
 * Configura la nota, velocidad, tiempo de glide y arranca las envolventes (a menos que sea legato).
 * @param midiNote Nota MIDI (0-127).
 * @param velocity Velocidad (0.0-1.0).
 * @param glideEnabled true si el tiempo de glide está activado.
 * @param isFirstTimbreNote true si esta es la primera nota del timbre actual.
 * @param isLegato true si la nota es parte de un legato (no se reinician envolventes).
 */
void Voice::noteOn(int midiNote, float velocity, bool glideEnabled, bool isFirstTimbreNote, bool isLegato) noexcept
{
    currentMidiNote_ = midiNote;
    velocity_ = velocity;
    noteAge_ = 0;

    glide_.setTargetNote(static_cast<float>(midiNote), glideEnabled);

    if (!isLegato)
    {
        osc1VA_.reset();
        osc1DWGS_.reset();
        osc1VoxWave_.reset();
        osc2_.reset();
        filter_.reset();

        eg1_.noteOn(velocity);
        eg2_.noteOn(velocity);
    }

    lfo1_.triggerKeySync(isFirstTimbreNote);
    lfo2_.triggerKeySync(isFirstTimbreNote);
}

/**
 * @brief Detiene una nota (noteOff MIDI).
 *
 * Solo detiene las envolventes (la liberación continúa hacia silence).
 */
void Voice::noteOff() noexcept
{
    eg1_.noteOff();
    eg2_.noteOff();
}

/**
 * @brief Detiene inmediatamente la voz (usar en stealage).
 */
void Voice::stopImmediately() noexcept
{
    eg1_.reset();
    eg2_.reset();
}

/**
 * @brief Indica si la voz está aún activa (en release o attack).
 * @return true si la envolvente principal aún está activa.
 */
bool Voice::isActive() const noexcept
{
    return !eg2_.isIdle();
}

/**
 * @brief Obtiene el nivel actual de la envolvente principal (VCA).
 * @return Nivel de la envolvente 2 (0.0-1.0).
 */
float Voice::getCurrentAmpLevel() const noexcept
{
    return eg2_.getCurrentLevel();
}

/**
 * @brief Aplica todos los parámetros estáticos del bloque actual.
 *
 * Este método se llama una vez por bloque de audio y establece todos los valores
 * no por muestra (tiempos de envolvente, tipos de onda, índices de wave, etc.).
 * @param params Parámetros completos del VoiceParameters.
 */
void Voice::applyBlockParams(const VoiceParameters &params) noexcept
{
    cachedParams_ = params;

    // ── Static DSP settings: applied once per audio block ──

    glide_.setGlideTime(params.portamentoTime / 127.0f);

    // LFO1: Saw / Square / Triangle / S&H
    lfo1_.setBpm(params.bpm);
    lfo1_.setTempoSync(params.lfo1TempoSync, params.lfo1SyncNote);
    if (!params.lfo1TempoSync)
        lfo1_.setFrequencyHz(params.lfo1FreqHz);
    lfo1_.setWaveformLFO1(params.lfo1Wave);
    lfo1_.setKeySyncMode(static_cast<int>(params.lfo1KeySync));

    // LFO2: Saw / Square+ / Sine / S&H
    lfo2_.setBpm(params.bpm);
    lfo2_.setTempoSync(params.lfo2TempoSync, params.lfo2SyncNote);
    if (!params.lfo2TempoSync)
        lfo2_.setFrequencyHz(params.lfo2FreqHz);
    lfo2_.setWaveformLFO2(params.lfo2Wave);
    lfo2_.setKeySyncMode(static_cast<int>(params.lfo2KeySync));

    // Envelope ADSR times
    eg1_.setAttack(params.eg1Attack);
    eg1_.setDecay(params.eg1Decay);
    eg1_.setSustain(params.eg1Sustain);
    eg1_.setRelease(params.eg1Release);

    eg2_.setAttack(params.eg2Attack);
    eg2_.setDecay(params.eg2Decay);
    eg2_.setSustain(params.eg2Sustain);
    eg2_.setRelease(params.eg2Release);

    // Virtual Patch Matrix: 4 mod slot routings
    for (size_t s = 0; s < 4; ++s)
    {
        patchMatrix_.setSlot(
            s, params.patchSlots[s].source, params.patchSlots[s].destination, params.patchSlots[s].intensity);
    }

    // OSC 1: waveform type and DWGS index
    if (params.osc1Type <= OSC1Type::Sine)
        osc1VA_.setWaveform(static_cast<VAWaveform>(params.osc1Type));
    osc1DWGS_.setWaveIndex(params.osc1DwgsIndex);

    // OSC 2: waveform type
    osc2_.setWaveform(params.osc2Wave);

    // Filter: type and resonance (cutoff varies per-sample due to EG sweep)
    filter_.setType(params.filterType);
    filter_.setResonance(params.filterResonance);
}

/**
 * @brief Renderiza una muestra mono- o stereo- por muestra.
 *
 * Este es el método central del motor: calcula pitch, envolventes, LFOs,
 * realiza el ruteo de modulación, renderiza osciladores y aplica el procesamiento DSP completo.
 * @param leftOut Referencia al acumulador de salida izquierda (additivo).
 * @param rightOut Referencia al acumulador de salida derecha (additivo).
 * @param diagPoint Punto de inyección de tono diagnóstico (0-5) o 0 si no.
 * @param diagTone Valor del tono de diagnóstico a inyectar.
 */
void Voice::renderNextSample(float &leftOut, float &rightOut, int diagPoint, float diagTone) noexcept
{
    const VoiceParameters &p = cachedParams_;
    if (!isActive() && !p.diagBypassVCA)
        return;

    noteAge_++;

    // 1. Portamento glide → per-sample base pitch
    float basePitch = glide_.getNextPitchSemitones();

    // 2. Step LFO1 and Envelopes
    float lfo1Val = lfo1_.getNextSample();
    float eg1Val = eg1_.getNextSample();
    float eg2Val = eg2_.getNextSample();
    // El VCA y el filtro usan el nivel **crudo** del EG (sin su ganancia por velocidad):
    // el MS2000 tiene sus propios controles de sensibilidad (AMP VELO / FILTER VELO) y
    // con ellos a 0 la velocidad no cambia el sonido. Los EG como fuente de patch siguen
    // entregando su valor escalado, como hasta ahora.
    const float eg1Raw = eg1_.getCurrentLevel();
    const float eg2Raw = eg2_.getCurrentLevel();

    // 3. Evaluate LFO2 with potential Virtual Patch frequency cross-modulation
    PatchModulationSources prelimSources;
    prelimSources.eg1 = eg1Val;
    prelimSources.eg2 = eg2Val;
    prelimSources.lfo1 = lfo1Val;
    prelimSources.lfo2 = lfo2_.getCurrentValue();
    prelimSources.velocity = velocity_;
    prelimSources.kbdTrack = (basePitch - 60.0f) / 64.0f;
    prelimSources.pitchBend = p.pitchBendValue;
    prelimSources.modWheel = p.modWheelValue;

    PatchModulationOutputs prelimMod = patchMatrix_.evaluate(prelimSources);
    float lfo2Freq = p.lfo2FreqHz * std::pow(2.0f, (prelimMod.lfo2FreqMod + p.seq.lfo2Freq) * 4.0f);
    lfo2_.setFrequencyHz(lfo2Freq);
    float lfo2Val = lfo2_.getNextSample();

    // 4. Full modulation matrix evaluation
    PatchModulationSources sources = prelimSources;
    sources.lfo2 = lfo2Val;
    PatchModulationOutputs mod = patchMatrix_.evaluate(sources);

    // 5. Portamento + LFO + Virtual Patch Pitch Modulation + Mod Seq + Voice Detune
    float voiceDetuneSemitones = p.voiceDetuneCents / 100.0f;
    const float seqPitchSemis = p.seq.pitch * 24.0f;     // "PITCH" del seq: ±24 st
    const float seqOsc2Semis = p.seq.osc2Pitch * 24.0f;  // "OSC2 SEMI" del seq: ±24 st
    float osc1PitchMod = (p.pitchBendValue * 2.0f) + (mod.pitchMod * 24.0f) + voiceDetuneSemitones + seqPitchSemis;
    float osc1FinalPitch = basePitch + osc1PitchMod;
    float osc1Freq = DSPUtils::midiNoteToFrequency(osc1FinalPitch);

    float osc2PitchMod = (p.pitchBendValue * 2.0f) + (mod.pitchMod * 24.0f) + voiceDetuneSemitones + seqPitchSemis
                         + seqOsc2Semis + p.osc2Semitone + ((p.osc2Tune + (p.seq.osc2Tune * 100.0f)) / 100.0f);
    float osc2FinalPitch = basePitch + osc2PitchMod;
    float osc2Freq = DSPUtils::midiNoteToFrequency(osc2FinalPitch);

    // 6. Set oscillator frequencies
    osc1VA_.setFrequency(osc1Freq);
    osc1DWGS_.setFrequency(osc1Freq);
    osc1VoxWave_.setFrequency(osc1Freq);
    osc2_.setFrequency(osc2Freq);

    // 7. Render OSC 1
    float osc1Sig = 0.0f;
    switch (p.osc1Type)
    {
    case OSC1Type::Saw:
    case OSC1Type::Pulse:
    case OSC1Type::Triangle:
    case OSC1Type::Sine:
        osc1VA_.setWaveform(static_cast<VAWaveform>(p.osc1Type));
        osc1VA_.setControl1(DSPUtils::clamp(p.osc1Ctrl1 + mod.osc1Ctrl1Mod + p.seq.osc1Ctrl1, 0.0f, 1.0f));
        osc1Sig = osc1VA_.getNextSample();
        break;

    case OSC1Type::DWGS:
        osc1DWGS_.setWaveIndex(p.osc1DwgsIndex);
        osc1Sig = osc1DWGS_.getNextSample();
        break;

    case OSC1Type::VoxWave:
        osc1VoxWave_.setVowel(DSPUtils::clamp(p.osc1Ctrl1 + mod.osc1Ctrl1Mod + p.seq.osc1Ctrl1, 0.0f, 1.0f));
        osc1Sig = osc1VoxWave_.getNextSample();
        break;

    case OSC1Type::Noise:
        osc1Sig = noiseGen_.getWhiteNoise();
        break;

    case OSC1Type::AudioIn:
    default:
        osc1Sig = 0.0f;
        break;
    }

    // 8. Render OSC 2 with modulation mode
    float osc2Sig = 0.0f;
    if (p.osc2Level > 0.001f || p.osc2ModMode != OSC2ModulationMode::Off)
    {
        if (p.osc2ModMode == OSC2ModulationMode::Sync)
        {
            if (osc1VA_.getPhase() < prevMasterPhase_)
                osc2_.reset();
        }
        osc2Sig = osc2_.getNextSample();
    }
    prevMasterPhase_ = osc1VA_.getPhase();

    if (diagPoint == 5)  // Diagnostic Point 5: Override OSC1
    {
        osc1Sig = diagTone;
    }

    // 9. Noise
    float noiseLevel = DSPUtils::clamp(p.noiseLevel + mod.noiseLevelMod + p.seq.noiseLevel, 0.0f, 1.0f);
    float noiseSig = (noiseLevel > 0.001f) ? noiseGen_.getWhiteNoise() : 0.0f;

    // 10. Mixer stage — sum OSC1, OSC2, Noise
    float osc1Level = p.diagBypassOscMixer ? 1.0f : DSPUtils::clamp(p.osc1Level + p.seq.osc1Level, 0.0f, 1.0f);
    float osc1Mixed = osc1Sig * osc1Level;
    float osc2Level = DSPUtils::clamp(p.osc2Level + p.seq.osc2Level, 0.0f, 1.0f);
    float osc2Mixed = osc2Sig * osc2Level;
    float mixedAudio = osc1Mixed + osc2Mixed + (noiseSig * noiseLevel);

    if (diagPoint == 4)  // Diagnostic Point 4: PreFilter (Bypasses OSC & Mixer)
    {
        mixedAudio = diagTone;
    }

    // 11. Filter stage
    float filtered = mixedAudio;
    if (!p.diagBypassFilter)
    {
        float baseHz = DSPUtils::convertSysExToCutoffHz(p.filterCutoffNorm);
        float egOctaves = eg1Raw * (p.eg1FilterIntensity * 5.0f);
        float kbdOctaves = ((basePitch - 60.0f) / 12.0f) * p.filterKbdTrack;
        float patchOctaves = (mod.cutoffMod + p.seq.cutoff) * 5.0f;
        // FILTER VELO del byte 23 (±63 → ±5 octavas a fondo): la velocidad abre el
        // filtro con intensidad positiva y lo cierra con negativa. A 0, sin efecto.
        float veloOctaves = p.filterVeloSens * velocity_ * 5.0f;
        float cutoffHz = baseHz * std::pow(2.0f, egOctaves + kbdOctaves + patchOctaves + veloOctaves);

        filter
