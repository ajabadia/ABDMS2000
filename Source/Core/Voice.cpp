// ============================================================
// Voice.cpp - Implementación del motor DSP de voz individual del MS2000/microKORG
// Canonical schema-driven VCA & HardwareConstants integration
// ============================================================

#include "Voice.h"
#include <cmath>
#include "Conversion.gen.h"
#include "HardwareConstants.h"
#include "MS2000FirmwareTables.h"

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
    osc1VA_.setIsOsc2(false);
    osc2_.setIsOsc2(true);
    osc1VA_.prepare(sampleRate_);
    osc1DWGS_.prepare(sampleRate_);
    osc1VoxWave_.prepare(sampleRate_);
    osc2_.prepare(sampleRate_);
    noiseGen_.reset();
    filter_.prepare(sampleRate_);
    eg1_.setHardwareMS2000Mode(true);
    eg2_.setHardwareMS2000Mode(true);
    eg1_.prepare(sampleRate_);
    eg2_.prepare(sampleRate_);
    glide_.prepare(sampleRate_);
    lfo1_.setHardwareMS2000Mode(true);
    lfo2_.setHardwareMS2000Mode(true);
    lfo1_.prepare(sampleRate_);
    lfo2_.prepare(sampleRate_);
    voiceDistortion_.prepare(sampleRate_);
    cutoffSmoothCoeff_ = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate_ * 0.002));
    gateSmoothCoeff_ = 1.0f - std::exp(-1.0f / static_cast<float>(sampleRate_ * 0.0015));
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
    noiseGen_.reset(noiseSeed_);
    filter_.reset();
    eg1_.reset();
    eg2_.reset();
    glide_.reset(static_cast<float>(currentMidiNote_));
    lfo1_.reset();
    lfo2_.reset();
    voiceDistortion_.reset();
    noteAge_ = 0;
    prevMasterPhase_ = 0.0f;
    cutoffSmoothInitialized_ = false;
    noteHeld_ = false;
    gateEnv_ = 0.0f;
    postFilterX1_ = 0.0f;
    postFilterY1_ = 0.0f;
    osc1NoiseLp_ = 0.0f;
    osc1NoiseBp_ = 0.0f;
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
    const bool wasIdle = !isActive();
    currentMidiNote_ = midiNote;
    velocity_ = velocity;
    noteAge_ = 0;
    noteHeld_ = true;

    glide_.setTargetNote(static_cast<float>(midiNote), glideEnabled);

    if (!isLegato)
    {
        if (wasIdle || cachedParams_.osc2ModMode == OSC2ModulationMode::Sync
            || cachedParams_.osc2ModMode == OSC2ModulationMode::CrossMod)
        {
            osc1VA_.reset();
            osc1DWGS_.reset();
            osc1VoxWave_.reset();
            osc2_.reset();
        }
        if (wasIdle)
        {
            filter_.reset();
            postFilterX1_ = 0.0f;
            postFilterY1_ = 0.0f;
        }

        eg1_.noteOn(1.0f);
        eg2_.noteOn(1.0f);
        cutoffSmoothInitialized_ = false;
    }

    // Apply KeySync per the hardware mode (byte 38/41 bits 4-5):
    //   Off = 0: never reset
    //   Timbre = 1: reset on first note of the timbre
    //   Voice = 2: reset on every note press per voice
    // Read mode from cached params (set by applyBlockParams)
    const auto lfo1Mode = cachedParams_.lfo1KeySync;
    const auto lfo2Mode = cachedParams_.lfo2KeySync;

    if (lfo1Mode != KeySyncMode::Off)
    {
        // KeySync is enabled - reset LFO phase appropriately
        if (lfo1Mode == KeySyncMode::Timbre)
        {
            // Reset only on first note of the timbre
            lfo1_.triggerKeySync(isFirstTimbreNote);
        }
        else
        {
            // Voice mode: reset on every note press
            lfo1_.triggerKeySync(isFirstTimbreNote);
        }
    }
    // else: lfo1Mode == Off, never reset

    if (lfo2Mode != KeySyncMode::Off)
    {
        // KeySync is enabled - reset LFO phase appropriately
        if (lfo2Mode == KeySyncMode::Timbre)
        {
            // Reset only on first note of the timbre
            lfo2_.triggerKeySync(isFirstTimbreNote);
        }
        else
        {
            // Voice mode: reset on every note press
            lfo2_.triggerKeySync(isFirstTimbreNote);
        }
    }
    // else: lfo2Mode == Off, never reset
}

/**
 * @brief Detiene una nota (noteOff MIDI).
 *
 * Solo detiene las envolventes (la liberación continúa hacia silence).
 */
void Voice::noteOff() noexcept
{
    noteHeld_ = false;
    eg1_.noteOff();
    eg2_.noteOff();
}

/**
 * @brief Detiene inmediatamente la voz (usar en stealage).
 */
void Voice::stopImmediately() noexcept
{
    noteHeld_ = false;
    gateEnv_ = 0.0f;
    eg1_.reset();
    eg2_.reset();
}

/**
 * @brief Indica si la voz está aún activa (en release o attack).
 * @return true si la envolvente principal aún está activa.
 */
bool Voice::isActive() const noexcept
{
    return !eg2_.isIdle() || (cachedParams_.ampGate && (noteHeld_ || gateEnv_ > 0.0001f));
}

/**
 * @brief Obtiene el nivel actual de la envolvente principal (VCA).
 * @return Nivel de la envolvente 2 (0.0-1.0).
 */
float Voice::getCurrentAmpLevel() const noexcept
{
    return cachedParams_.ampGate ? gateEnv_ : eg2_.getCurrentLevel();
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

    glide_.setGlideTime(conversion::normalizedToDomain("portamentoTime", params.portamentoTime / 127.0f));

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
void Voice::renderNextSample(
    float &leftOut, float &rightOut, int diagPoint, float diagTone, float audioInSample) noexcept
{
    const VoiceParameters &p = cachedParams_;
    if (!isActive() && !p.diagBypassVCA)
        return;

    noteAge_++;

    // 1. Portamento glide → per-sample base pitch
    float basePitch = glide_.getNextPitchSemitones();

    // 2. Step LFO1 and Envelopes
    float lfo1Val = lfo1_.getNextSample();
    (void) eg1_.getNextSample();
    (void) eg2_.getNextSample();
    // El VCA y el filtro usan el nivel **crudo** del EG (sin su ganancia por velocidad):
    // el MS2000 tiene sus propios controles de sensibilidad (AMP VELO / FILTER VELO) y
    // con ellos a 0 la velocidad no cambia el sonido. Los EG como fuente de patch siguen
    // entregando su valor escalado, como hasta ahora.
    const float eg1Raw = eg1_.getCurrentLevel();
    const float eg2Raw = eg2_.getCurrentLevel();

    // 3. Evaluate LFO2 with potential Virtual Patch frequency cross-modulation
    PatchModulationSources prelimSources;
    prelimSources.eg1 = eg1Raw;
    prelimSources.eg2 = eg2Raw;
    prelimSources.lfo1 = lfo1Val;
    prelimSources.lfo2 = lfo2_.getCurrentValue();
    prelimSources.velocity = velocity_;
    prelimSources.kbdTrack = (basePitch - Constants::kMidiMiddleC) / Constants::kKbdTrackSpanSemitones;
    prelimSources.pitchBend = p.pitchBendValue;
    prelimSources.modWheel = p.modWheelValue;

    PatchModulationOutputs prelimMod = patchMatrix_.evaluate(prelimSources);
    float lfo2Freq =
        p.lfo2FreqHz * std::pow(2.0f, (prelimMod.lfo2FreqMod + p.seq.lfo2Freq) * Constants::kLfoModMaxOctaves);
    lfo2_.setFrequencyHz(lfo2Freq);
    float lfo2Val = lfo2_.getNextSample();

    // 4. Full modulation matrix evaluation
    PatchModulationSources sources = prelimSources;
    sources.lfo2 = lfo2Val;
    PatchModulationOutputs mod = patchMatrix_.evaluate(sources);

    // 5. Portamento + Timbre Transpose/Tune + Microtonal Scale (ROM 0x13914) + Virtual Patch Pitch + Mod Seq + Voice
    // Detune
    const float scaleSemitones =
        FirmwareTables::getHardwareScaleDetuneSemitones(currentMidiNote_, p.scaleType, p.scaleKey);
    const float timbreTuningSemitones = p.pitchTranspose + (p.pitchTune / Constants::kCentsPerSemitone);
    float voiceDetuneSemitones =
        (p.voiceDetuneCents / Constants::kCentsPerSemitone) + scaleSemitones + timbreTuningSemitones;
    const float seqPitchSemis = p.seq.pitch * Constants::kVirtualPatchMaxPitchSemitones;  // "PITCH" del seq: ±24 st
    const float seqOsc2Semis =
        p.seq.osc2Pitch * Constants::kVirtualPatchMaxPitchSemitones;  // "OSC2 SEMI" del seq: ±24 st
    const float bendSemitones = p.pitchBendValue * p.pitchBendRange;
    const float vibratoSemitones = lfo1Val * p.modWheelValue * p.vibratoInt * 12.0f;
    float osc1PitchMod = bendSemitones + vibratoSemitones + (mod.pitchMod * Constants::kVirtualPatchMaxPitchSemitones)
                         + voiceDetuneSemitones + seqPitchSemis;
    float osc1FinalPitch = basePitch + osc1PitchMod;
    float osc1Freq = DSPUtils::midiNoteToFrequency(osc1FinalPitch);

    float osc2PitchMod =
        bendSemitones + vibratoSemitones
        + ((mod.pitchMod + mod.osc2PitchMod) * Constants::kVirtualPatchMaxPitchSemitones) + voiceDetuneSemitones
        + seqPitchSemis + seqOsc2Semis + p.osc2Semitone
        + ((p.osc2Tune + (p.seq.osc2Tune * Constants::kCentsPerSemitone)) / Constants::kCentsPerSemitone);
    float osc2FinalPitch = basePitch + osc2PitchMod;
    float osc2Freq = DSPUtils::midiNoteToFrequency(osc2FinalPitch);

    // 6. Set oscillator frequencies
    osc1VA_.setFrequency(osc1Freq);
    osc1DWGS_.setFrequency(osc1Freq);
    osc1VoxWave_.setFrequency(osc1Freq);
    osc2_.setFrequency(osc2Freq);

    // 7. Render OSC 2 first when needed for CROSS wave FM or OSC2 modulation/mixing
    //    Check Hard Sync against OSC1's phase wrap from the previous sample.
    const bool needOsc2 = (p.osc2Level > 0.001f) || (std::abs(p.seq.osc2Level) > 0.001f)
                          || (p.osc2ModMode != OSC2ModulationMode::Off) || (p.osc1Type == OSC1Type::Sine);
    float osc2Raw = 0.0f;
    if (needOsc2)
    {
        if (p.osc2ModMode == OSC2ModulationMode::Sync || p.osc2ModMode == OSC2ModulationMode::CrossMod)
        {
            OSC2Modulator::checkHardSync(osc1VA_, osc2_, prevMasterPhase_);
        }
        osc2Raw = osc2_.getNextSample();
    }
    prevMasterPhase_ = osc1VA_.getPhase();

    // 8. Compute effective OSC1 Control 1 (with LFO1 modulation from Control 2 on Saw/Pulse/Tri)
    const float ctrl2Eff = DSPUtils::clamp(p.osc1Ctrl2 + p.seq.osc1Ctrl2, 0.0f, 1.0f);
    const float ctrl1Base = p.osc1Ctrl1 + mod.osc1Ctrl1Mod + p.seq.osc1Ctrl1;

    // 9. Render OSC 1
    float osc1Sig = 0.0f;
    switch (p.osc1Type)
    {
    case OSC1Type::Saw:
    case OSC1Type::Pulse:
    case OSC1Type::Triangle:
    {
        osc1VA_.setWaveform(static_cast<VAWaveform>(p.osc1Type));
        // On the MS2000, Control 2 sets the LFO1 modulation depth applied to Control 1
        float shapedCtrl1 = DSPUtils::clamp(ctrl1Base + (lfo1Val * ctrl2Eff * 0.5f), 0.0f, 1.0f);
        osc1VA_.setControl1(shapedCtrl1);
        osc1Sig = osc1VA_.getNextSample();
        break;
    }

    case OSC1Type::Sine:
    {
        // Hardware Wave 3 = CROSS (Sine carrier phase-modulated by OSC2, depth = Ctrl1 + LFO1*Ctrl2)
        osc1VA_.setWaveform(VAWaveform::Sine);
        float fmDepth = DSPUtils::clamp(ctrl1Base + (std::max(0.0f, lfo1Val) * ctrl2Eff), 0.0f, 1.0f);
        osc1VA_.setControl1(fmDepth);
        float phaseMod = osc2Raw * fmDepth * 1.5f;
        osc1Sig = osc1VA_.getNextSample(phaseMod);
        break;
    }

    case OSC1Type::DWGS:
        osc1DWGS_.setWaveIndex(p.osc1DwgsIndex);
        osc1Sig = osc1DWGS_.getNextSample();
        break;

    case OSC1Type::VoxWave:
    {
        float shapedVowel = DSPUtils::clamp(ctrl1Base + (lfo1Val * ctrl2Eff * 0.5f), 0.0f, 1.0f);
        osc1VoxWave_.setVowel(shapedVowel);
        osc1VoxWave_.setFormantShift(ctrl2Eff * 0.25f);
        osc1Sig = osc1VoxWave_.getNextSample();
        break;
    }

    case OSC1Type::Noise:
    {
        // Hardware Wave 6 (P:$0A5E–$0A92): White noise passed through a dedicated 2-pole LP SVF
        // where Control 1 (offset 46 + Virtual Patch / ModSeq) sets Cutoff and Control 2 (offset 47) sets Resonance.
        const float noiseWhite = noiseGen_.getWhiteNoise();
        const float noiseCutoffNorm = DSPUtils::clamp(ctrl1Base, 0.0f, 1.0f);
        const auto noiseRes = FirmwareTables::getHardwareFilterResonanceCoeffs(ctrl2Eff, false);
        const float noiseFc = FirmwareTables::getHardwareFilterCutoffHz(noiseCutoffNorm, 0.0f, noiseRes.w3);
        const float clampedFc =
            DSPUtils::clamp(noiseFc, 10.0f, static_cast<float>(sampleRate_) * 0.485f);
        const float g = std::tan(DSPUtils::PI * clampedFc / static_cast<float>(sampleRate_));
        const float k = 2.0f * noiseRes.dampingR;
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float a3 = g * a2;
        const float xIn = noiseWhite * noiseRes.w1;
        const float v3 = xIn - osc1NoiseLp_;
        const float v1 = a1 * osc1NoiseBp_ + a2 * v3;
        const float v2 = osc1NoiseLp_ + a2 * osc1NoiseBp_ + a3 * v3;
        osc1NoiseBp_ = DSPUtils::clamp(2.0f * v1 - osc1NoiseBp_, -4.0f, 4.0f);
        osc1NoiseLp_ = DSPUtils::clamp(2.0f * v2 - osc1NoiseLp_, -4.0f, 4.0f);
        // P:$0A8F–$0A91: mpy y0,x0,a (w0) + asl #$03,a,a (x8); since w1*w0 = 0.25 at res=0, * 4.0f gives unity passband
        osc1Sig = DSPUtils::clamp(v2 * noiseRes.w0 * 4.0f, -1.5f, 1.5f);
        break;
    }

    case OSC1Type::AudioIn:
        osc1Sig = audioInSample;
        break;
    default:
        osc1Sig = 0.0f;
        break;
    }

    // Apply OSC2 Modulation Mode (Off, RingMod, Sync, Ring+Sync) to the OSC2 mixer signal
    float osc2Sig = OSC2Modulator::process(osc1Sig, osc2Raw, p.osc2ModMode);

    if (diagPoint == 5)  // Diagnostic Point 5: Override OSC1
    {
        osc1Sig = diagTone;
    }

    // 10. Noise
    float noiseLevel = DSPUtils::clamp(p.noiseLevel + mod.noiseLevelMod + p.seq.noiseLevel, 0.0f, 1.0f);
    float noiseSig = (noiseLevel > 0.001f) ? noiseGen_.getWhiteNoise() : 0.0f;

    // 11. Mixer stage — sum OSC1, OSC2 (or RingMod), Noise
    float osc1Level = p.diagBypassOscMixer ? 1.0f : DSPUtils::clamp(p.osc1Level + p.seq.osc1Level, 0.0f, 1.0f);
    float osc1Mixed = osc1Sig * osc1Level;
    float osc2Level = DSPUtils::clamp(p.osc2Level + p.seq.osc2Level, 0.0f, 1.0f);
    float osc2Mixed = osc2Sig * osc2Level;
    float mixedAudio = osc1Mixed + osc2Mixed + (noiseSig * noiseLevel);

    if (diagPoint == 4)  // Diagnostic Point 4: PreFilter (Bypasses OSC & Mixer)
    {
        mixedAudio = diagTone;
    }

    // 12. Filter stage (ROM 0x26154 256-entry VCF table with sub-20Hz closing & 12-idx/octave tracking)
    float filtered = mixedAudio;
    if (!p.diagBypassFilter)
    {
        if (!cutoffSmoothInitialized_)
        {
            smoothedCutoffNorm_ = p.filterCutoffNorm;
            cutoffSmoothInitialized_ = true;
        }
        else
        {
            smoothedCutoffNorm_ += cutoffSmoothCoeff_ * (p.filterCutoffNorm - smoothedCutoffNorm_);
        }

        constexpr float kRomFullCutoffSpanOctaves = 127.0f / 12.0f;  // 0..127 knob span in ROM 0x26154 (10.5833 oct)
        // H8S 0x0166B4..0x0166C4: Filter EG1 Intensity lee la tabla lineal 0x2BD1C (`shar.l #1` en 0x166BE
        // se cancela con `<< 1` en 0x11576 -> X:$04AF en [-1.0, +1.0] Q23). En DSP P:$074B..$0751,
        // `mac y0,x0,a #$40,x0` + `add x0,a` + clamp `[0, $7FFFFF]` recorta la suma de modulación a [-8.0, +8.0] octavas.
        constexpr float kDspFullPitchSpaceOctaves = 16.0f;
        const float eg1Coeff = FirmwareTables::getHardwareFilterEg1IntensityCoeff(p.eg1FilterIntensity);
        // FILTER VELO (byte 23): en el firmware (H8S 0x0163D0 -> 0x013466) la velocidad escala la
        // amplitud del EG1 que llega al filtro (descriptores X:$0049..$004C), no el cutoff estático.
        float eg1VeloFactor = std::max(0.0f, 1.0f + p.filterVeloSens * velocity_);
        float egOctaves = eg1Raw * eg1VeloFactor * (eg1Coeff * kDspFullPitchSpaceOctaves);
        float kbdOctaves =
            ((basePitch - Constants::kMidiMiddleC) / Constants::kSemitonesPerOctave) * (p.filterKbdTrack * 2.0f);
        float patchOctaves = (mod.cutoffMod + p.seq.cutoff) * kRomFullCutoffSpanOctaves;
        float dynModOctaves = egOctaves + patchOctaves;
        const auto resRom = FirmwareTables::getHardwareFilterResonanceCoeffs(p.filterResonance, p.filterType == FilterType::LPF24);
        float cutoffHz = FirmwareTables::getHardwareFilterCutoffHz(smoothedCutoffNorm_, dynModOctaves, resRom.w3, kbdOctaves);

        filter_.setCutoff(cutoffHz);
        filtered = filter_.process(mixedAudio);
    }

    // 13. Envelope 2 (VCA) - apply amplitude envelope, velocity, key track, mod & distortion
    float vcaVelocity = 1.0f;
    if (p.ampVeloSens > 0.0f)
        vcaVelocity = 1.0f - p.ampVeloSens * (1.0f - velocity_);
    else if (p.ampVeloSens < 0.0f)
        vcaVelocity = 1.0f + p.ampVeloSens * velocity_;

    // AMP KEY TRACK (byte 29, ±63): escala la ganancia según la distancia a C4 (nota 60)
    float kbdAmpFactor = 1.0f;
    if (std::abs(p.ampKeyTrack) > 0.0001f)
    {
        float kbdNorm = (basePitch - Constants::kMidiMiddleC) / Constants::kKbdTrackSpanSemitones;
        kbdAmpFactor = DSPUtils::clamp(1.0f + (p.ampKeyTrack * kbdNorm), 0.0f, 2.0f);
    }

    // H8S 0x013562: AMP LEVEL + Mod Seq se suman y recortan a [0,1]; después se MULTIPLICAN por
    // el factor del Virtual Patch (1 + ampMod, recortado a >= 0) y el producto se recorta a 1.0.
    // Así un LFO cuadrado negativo con Int +63 (o un paso de seq a -63) cierra el VCA del todo.
    const float ampBase = DSPUtils::clamp(p.ampLevel + p.seq.amp, 0.0f, 1.0f);
    const float ampPatchFactor = std::max(0.0f, 1.0f + mod.ampMod);
    float effectiveAmpLevel = DSPUtils::clamp(ampBase * ampPatchFactor, 0.0f, 1.0f);
    const float gateTarget = noteHeld_ ? 1.0f : 0.0f;
    gateEnv_ += gateSmoothCoeff_ * (gateTarget - gateEnv_);
    if (!noteHeld_ && gateEnv_ < 0.0001f)
        gateEnv_ = 0.0f;
    float envGain = p.diagBypassVCA ? 1.0f : (p.ampGate ? gateEnv_ : eg2Raw);
    float vcaOut = filtered * envGain * effectiveAmpLevel * vcaVelocity * kbdAmpFactor;

    if (p.distortionOn && !p.diagBypassDistortion)
    {
        // En DSP56362 P:$083D..$086B, la etapa de distorsión por voz aplica pre-énfasis alto (`asl #$08,a,b`
        // en P:$083D) para saturar la señal (`>15% THD`), pero su techo de salida en X:$052A (`$1F5601..$300000`)
        // queda acotado antes de entrar a la suma de voces P:$0891..$08AF.
        //
        // El drive interno (`gain = 1 + 0.85*6 = 6.1x`) seguido del makeup (`1/(0.55+0.45*drive) = 1.072x`)
        // deja una ganancia de pequeña señal de 6.1 * 1.072 = 6.55x; con el techo de 0.5 quedaba 3.27x
        // (+10.3 dB) sobre una señal ya escalada por AMP LEVEL. Medido con --compare-emu: A10
        // (LPF12, RES 97, AMP_LEVEL 45, Distortion ON) salía +12.0 dB sobre el oráculo con peak -2.9 dB
        // frente a -12.3 dB, y sin embargo cumplía Cal 6.1 (THD > 15%). Es decir: la etapa saturaba bien
        // pero amplificaba de más. Compensando a ganancia unidad de pequeña señal (0.5 / 3.27 = 0.153) el
        // THD se conserva y A10 pasa a +1.59 dB / peak -13.3 dB. Presets con Distortion OFF no cambian.
        vcaOut = voiceDistortion_.processSample(vcaOut, 0.85f) * 0.153f;
    }

    // 13b. Filtro de salida de voz de 1.er orden (DSP56362 P:$086D..$0890, X:$0524..$0528 con asl #$03,a,a):
    //   y[n] = b0 * x[n] + b1 * x[n-1] + a1 * y[n-1]
    // En 24LPF/12LPF mantiene ganancia unidad exacta en DC (1.0000) y aporta realce de agudos post-VCA;
    // en 12BPF/12HPF es bypass plano de ganancia unidad (b0 = 8 * $100000 = 1.0, b1 = 0, a1 = 0).
    // P3.8 — Alcance real de este shelf (medido, no supuesto). Del desensamblado se deduce que los
    // tres coeficientes son FIJOS por modo (P:$086F..$0873 los lee del descriptor y P:$087F/$0880 +
    // P:$088D/$088E aplican `asl #$03` al acumulador completo, incluida la realimentación): un realce
    // fijo no depende del cutoff.
    // Se probó a desactivar esta etapa (A/B con --compare-emu sobre el corpus de 8 presets) con la
    // hipótesis de que duplicara un realce que nuestro TPT ya no necesita. El resultado la descarta:
    // B01 Lazy Pitch sigue en 77,5 % (banda 4..10 kHz clavada en 91,3 % frente a 62,8 % del oráculo),
    // A10 sigue en 82,3 % y el centroide apenas se mueve; sólo bajan algunos picos (B05 -0,2 -> -2,1 dB).
    // Como la etapa SÍ existe en el microcódigo y su retirada no mejora la paridad, se mantiene activa.
    constexpr bool kApplyHardwareVoiceOutputFilter = true;
    if (kApplyHardwareVoiceOutputFilter && !p.diagBypassFilter && !p.diagBypassVCA)
    {
        const auto outFlt =
            FirmwareTables::getHardwareVoiceOutputFilterCoeffs(static_cast<int>(p.filterType), sampleRate_);
        const float postY = outFlt.b0 * vcaOut + outFlt.b1 * postFilterX1_ + outFlt.a1 * postFilterY1_;
        postFilterX1_ = vcaOut;
        postFilterY1_ = postY;
        vcaOut = postY;
    }

    // 14. Pan and stereo positioning (with Virtual Patch & Mod Seq Panpot modulation)
    // En H8S 0x1686C + DSP56362 P:$0891..$08AF, el bus estéreo por voz aplica la ley de panorámica
    // lineal centrada en 0.5 (`X:$0530 = $3FFFFF` en pan=64 -> -6 dB por canal) con el headroom de
    // 4 voces del acumulador de bus (`P:$08AB`). El factor `kHardwareBusHeadroom = 0.5f` alinea el
    // techo de salida por voz con el del hardware real sin alterar la ganancia de entrada a `voiceDistortion_`.
    constexpr float kHardwareBusHeadroom = 0.5f;
    float effectivePan = DSPUtils::clamp(p.panpot + mod.panMod + p.seq.pan, -1.0f, 1.0f);
    float panGainL = (1.0f - (effectivePan * Constants::kPanHalfWidth)) * kHardwareBusHeadroom;
    float panGainR = (1.0f + (effectivePan * Constants::kPanHalfWidth)) * kHardwareBusHeadroom;
    float outL = vcaOut * panGainL;
    float outR = vcaOut * panGainR;

    lastDiagStats_.basePitch = basePitch;
    lastDiagStats_.osc1Sig = osc1Sig;
    lastDiagStats_.mixedAudio = mixedAudio;
    lastDiagStats_.filtered = filtered;
    lastDiagStats_.eg1Val = eg1Raw;
    lastDiagStats_.eg2Val = eg2Raw;
    lastDiagStats_.vcaOut = vcaOut;
    lastDiagStats_.lGain = panGainL;
    lastDiagStats_.rGain = panGainR;
    lastDiagStats_.leftOutSample = outL;
    lastDiagStats_.rightOutSample = outR;

    leftOut = outL;
    rightOut = outR;
}

void Voice::renderDiagnosticSample(float &leftOut, float &rightOut, int /*diagPoint*/, float diagTone) noexcept
{
    // Diagnostic sample override - bypasses all normal DSP processing
    leftOut = diagTone;
    rightOut = diagTone;
}

}  // namespace ABDMS2000
