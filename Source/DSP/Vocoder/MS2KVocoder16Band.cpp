#include "MS2KVocoder16Band.h"
#include <algorithm>
#include <cmath>
#include "../Common/DSPUtils.h"
#include "HardwareConstants.h"

namespace ABDMS2000
{

// Frecuencias centrales medidas de las 16 bandas del vocoder del Korg MS2000
static const float kMS2000VocoderFreqs[16] = {125.0f,
                                              180.0f,
                                              250.0f,
                                              340.0f,
                                              460.0f,
                                              620.0f,
                                              820.0f,
                                              1050.0f,
                                              1350.0f,
                                              1700.0f,
                                              2150.0f,
                                              2700.0f,
                                              3350.0f,
                                              4100.0f,
                                              4900.0f,
                                              5700.0f};

void BiquadBPF::setBandpass(float centerFreq, float q, double sampleRate) noexcept
{
    float w0 = centerFreq * DSPUtils::TWO_PI / static_cast<float>(sampleRate);
    float sin_w0 = std::sin(w0);
    float cos_w0 = std::cos(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = alpha / a0;
    b1 = 0.0f;
    b2 = -alpha / a0;
    a1 = (-2.0f * cos_w0) / a0;
    a2 = (1.0f - alpha) / a0;
}

float BiquadBPF::process(float in) noexcept
{
    float out = b0 * in + s1;
    s1 = b1 * in - a1 * out + s2;
    s2 = b2 * in - a2 * out;
    return out;
}

void MS2KVocoder16Band::prepare(double sampleRate) noexcept
{
    sampleRate_ = DSPUtils::validateSampleRate(sampleRate);
    whiteNoiseGen_.reset(0xABCDEF12);

    // Perfil formántico vocal clásico ("AH-EE") para Formant Hold inmediato
    static const float kDefaultFrozenFormants[16] = {
        0.15f, 0.25f, 0.45f, 0.85f, 0.65f, 0.25f, 0.15f, 0.30f, 0.75f, 0.90f, 0.55f, 0.35f, 0.25f, 0.20f, 0.15f, 0.10f};

    for (size_t i = 0; i < NUM_BANDS; ++i)
    {
        followers_[i].prepare(sampleRate_);
        bandLevels_[i] = 1.0f;
        bandPans_[i] = (i % 2 == 0) ? 0.325f : 0.675f;  // Spread estéreo alternado MS2000
        frozenEnvs_[i] = kDefaultFrozenFormants[i];
    }

    intFormant1_.setBandpass(730.0f, 5.5f, sampleRate_);
    intFormant2_.setBandpass(1090.0f, 6.5f, sampleRate_);
    intFormant3_.setBandpass(2440.0f, 7.5f, sampleRate_);

    updateFilterFrequencies();
    updateHPFFilter();
    sibilanceFollower_.prepare(sampleRate_);
    sibilanceFollower_.setReleaseTime(Constants::kVocoderSibilanceReleaseSec);
    reset();
}

void MS2KVocoder16Band::reset() noexcept
{
    for (auto &f : analysisFilters_)
        f.reset();
    for (auto &f : analysisFilters2_)
        f.reset();
    for (auto &f : synthesisFilters_)
        f.reset();
    for (auto &f : synthesisFilters2_)
        f.reset();
    for (auto &f : followers_)
        f.reset();
    intFormant1_.reset();
    intFormant2_.reset();
    intFormant3_.reset();
    sibilanceFollower_.reset();
    hpfS1_ = hpfS2_ = 0.0f;
    extActivityEnv_ = 0.0f;
}

void MS2KVocoder16Band::setFormantShift(int shiftValue) noexcept
{
    int clamped = std::max(-2, std::min(2, shiftValue));
    if (formantShift_ != clamped)
    {
        formantShift_ = clamped;
        updateFilterFrequencies();
    }
}

void MS2KVocoder16Band::setCutoffOffset(float offsetMinus1to1) noexcept
{
    float clamped = DSPUtils::clamp(offsetMinus1to1, -1.0f, 1.0f);
    if (std::abs(cutoffOffset_ - clamped) > 0.001f)
    {
        cutoffOffset_ = clamped;
        updateSynthesisFrequencies(cutoffOffset_ * 1.5f + modIntensity_ * modSourceValue_ * 1.5f);
    }
}

void MS2KVocoder16Band::setResonance(float res0to1) noexcept
{
    float clamped = DSPUtils::clamp(res0to1, 0.0f, 1.0f);
    if (std::abs(resonance_ - clamped) > 0.001f)
    {
        resonance_ = clamped;
        updateSynthesisFrequencies(cutoffOffset_ * 1.5f + modIntensity_ * modSourceValue_ * 1.5f);
    }
}

void MS2KVocoder16Band::setModIntensity(float modIntMinus1to1) noexcept
{
    modIntensity_ = DSPUtils::clamp(modIntMinus1to1, -1.0f, 1.0f);
}

void MS2KVocoder16Band::setEfSense(float sense0to1) noexcept
{
    efSense_ = DSPUtils::clamp(sense0to1, 0.0f, 1.0f);
    // E.F. Sense: 0 = articulación rápida (8ms), 126 = suave (180ms), 127 (>=0.995) = Hold
    float releaseSec = 0.008f + efSense_ * 0.172f;
    for (auto &f : followers_)
    {
        f.setReleaseTime(releaseSec);
    }
}

void MS2KVocoder16Band::setFormantHold(bool hold) noexcept
{
    if (hold && !formantHold_)
    {
        float sum = 0.0f;
        for (size_t i = 0; i < NUM_BANDS; ++i)
            sum += frozenEnvs_[i];
        if (sum < 0.01f)
        {
            static const float kDefaultFrozenFormants[16] = {0.15f,
                                                             0.25f,
                                                             0.45f,
                                                             0.85f,
                                                             0.65f,
                                                             0.25f,
                                                             0.15f,
                                                             0.30f,
                                                             0.75f,
                                                             0.90f,
                                                             0.55f,
                                                             0.35f,
                                                             0.25f,
                                                             0.20f,
                                                             0.15f,
                                                             0.10f};
            for (size_t i = 0; i < NUM_BANDS; ++i)
                frozenEnvs_[i] = kDefaultFrozenFormants[i];
        }
    }
    formantHold_ = hold;
}

void MS2KVocoder16Band::setGateSense(float gateSense0to1) noexcept
{
    gateSense_ = DSPUtils::clamp(gateSense0to1, 0.0f, 1.0f);
    float releaseSec =
        Constants::kVocoderGateSenseMinReleaseSec + (Constants::kVocoderGateSenseReleaseRangeSec * gateSense_);
    for (auto &f : followers_)
    {
        f.setReleaseTime(releaseSec);
    }
}

void MS2KVocoder16Band::setHPFLevel(float level0to1) noexcept
{
    hpfLevel_ = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void MS2KVocoder16Band::setHPFThreshold(float thresh0to1) noexcept
{
    hpfThreshold_ = DSPUtils::clamp(thresh0to1, 0.001f, 1.0f);
}

void MS2KVocoder16Band::setDirectLevel(float level0to1) noexcept
{
    directLevel_ = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void MS2KVocoder16Band::setBandLevel(size_t bandIndex, float level0to1) noexcept
{
    bandLevels_[bandIndex % NUM_BANDS] = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void MS2KVocoder16Band::setBandPan(size_t bandIndex, float pan0to1) noexcept
{
    bandPans_[bandIndex % NUM_BANDS] = DSPUtils::clamp(pan0to1, 0.0f, 1.0f);
}

void MS2KVocoder16Band::updateFilterFrequencies() noexcept
{
    const float analysisQ = Constants::kVocoderBandQ;

    for (size_t i = 0; i < NUM_BANDS; ++i)
    {
        // 1. Filtros de análisis del modulador (4º orden = 2 biquads en cascada)
        analysisFilters_[i].setBandpass(kMS2000VocoderFreqs[i], analysisQ, sampleRate_);
        analysisFilters2_[i].setBandpass(kMS2000VocoderFreqs[i], analysisQ, sampleRate_);
    }

    updateSynthesisFrequencies(cutoffOffset_ * 1.5f + modIntensity_ * modSourceValue_ * 1.5f);
}

void MS2KVocoder16Band::updateSynthesisFrequencies(float octaveOffset) noexcept
{
    lastAppliedOctaveShift_ = octaveOffset;
    const float freqMult = std::pow(2.0f, DSPUtils::clamp(octaveOffset, -2.5f, 2.5f));
    const float nyquistLimit = 0.45f * static_cast<float>(sampleRate_);
    // Resonance (0..1) escala el Q de síntesis entre 3.5 (cálido/ancho) y 14.0 (pico formántico agudo)
    const float synthQ = 3.5f + resonance_ * 10.5f;

    for (size_t i = 0; i < NUM_BANDS; ++i)
    {
        int synthIdx = std::max(0, std::min(15, static_cast<int>(i) + formantShift_));
        float shiftedFreq = DSPUtils::clamp(kMS2000VocoderFreqs[synthIdx] * freqMult, 60.0f, nyquistLimit);
        synthesisFilters_[i].setBandpass(shiftedFreq, synthQ, sampleRate_);
        synthesisFilters2_[i].setBandpass(shiftedFreq, synthQ, sampleRate_);
    }
}

void MS2KVocoder16Band::updateHPFFilter() noexcept
{
    // Filtro Butterworth paso-altos a 8.0 kHz para el detector de sibilantes (clamped bajo Nyquist)
    const float safeCutoffHz = std::min(Constants::kVocoderSibilanceCutoffHz, 0.45f * static_cast<float>(sampleRate_));
    float w0 = safeCutoffHz * DSPUtils::TWO_PI / static_cast<float>(sampleRate_);

    float cos_w0 = std::cos(w0);
    float sin_w0 = std::sin(w0);
    float alpha = sin_w0 * 0.7071f;

    float a0 = 1.0f + alpha;
    hpfB0_ = ((1.0f + cos_w0) * 0.5f) / a0;
    hpfB1_ = (-(1.0f + cos_w0)) / a0;
    hpfB2_ = ((1.0f + cos_w0) * 0.5f) / a0;
    hpfA1_ = (-2.0f * cos_w0) / a0;
    hpfA2_ = (1.0f - alpha) / a0;
}

float MS2KVocoder16Band::generateInternalFormantModulator() noexcept
{
    // Generador interno de voz sintética formántica (A-E-I-O-U + consonantes)
    // Se activa automáticamente cuando el usuario toca el Vocoder sin micrófono externo conectado.
    const float invSr = 1.0f / static_cast<float>(sampleRate_);

    // Ciclo de vocales a ~1.6 Hz
    intVowelPhase_ += 1.6f * invSr;
    if (intVowelPhase_ >= 1.0f)
        intVowelPhase_ -= 1.0f;

    // Actualizar formantes F1, F2, F3 cada 64 muestras según la fase vocal
    if ((coeffUpdateCounter_ & 63) == 0)
    {
        const float angle = intVowelPhase_ * DSPUtils::TWO_PI;
        const float f1 = 520.0f + 280.0f * std::sin(angle);
        const float f2 = 1480.0f + 720.0f * std::cos(angle * 2.0f);
        const float f3 = 2550.0f + 350.0f * std::sin(angle * 3.0f);
        intFormant1_.setBandpass(f1, 5.5f, sampleRate_);
        intFormant2_.setBandpass(f2, 6.5f, sampleRate_);
        intFormant3_.setBandpass(f3, 7.5f, sampleRate_);
    }

    // Tren de pulsos glotales (~115 Hz con ligero vibrato natural)
    const float glottalFreq = 115.0f + 4.0f * std::sin(intVowelPhase_ * DSPUtils::TWO_PI * 3.0f);
    intGlottalPhase_ += glottalFreq * invSr;
    if (intGlottalPhase_ >= 1.0f)
        intGlottalPhase_ -= 1.0f;

    // Onda glotal rica en armónicos (diente de sierra + pulso estrecho)
    const float saw = 2.0f * intGlottalPhase_ - 1.0f;
    const float pulse = (intGlottalPhase_ < 0.22f) ? 1.0f : -0.28f;
    const float glottal = 0.55f * saw + 0.45f * pulse;

    // Ráfagas de consonantes / sibilancia en los bordes de sílaba
    const float syllablePos = std::fmod(intVowelPhase_ * 4.0f, 1.0f);
    const float sibilanceBurst = (syllablePos < 0.08f) ? (whiteNoiseGen_.getWhiteNoise() * 0.45f) : 0.0f;

    const float voiced = intFormant1_.process(glottal) * 0.9f + intFormant2_.process(glottal) * 0.75f
                         + intFormant3_.process(glottal) * 0.45f;

    return (voiced + sibilanceBurst) * 0.65f;
}

void MS2KVocoder16Band::process(float modSample, float carrierSample, float &outLeft, float &outRight) noexcept
{
    if (!enabled_)
        return;

    // 0. Detectar si existe señal de modulador externo (micrófono / sidechain DAW)
    const float absMod = std::abs(modSample);
    if (absMod > 1.0e-4f)
        extActivityEnv_ = 1.0f;
    else
        extActivityEnv_ *= 0.9995f;

    // Si no hay modulador externo pero sí hay portadora (notas del sintetizador),
    // usar el modulador vocal formántico interno para que el Vocoder suene de inmediato.
    float effectiveMod = modSample;
    if (extActivityEnv_ < 0.01f && std::abs(carrierSample) > 1.0e-4f)
    {
        effectiveMod = generateInternalFormantModulator();
    }

    // Actualizar desplazamiento continuo de Fc (Cutoff Offset + Modulación) cada 32 muestras
    ++coeffUpdateCounter_;
    if ((coeffUpdateCounter_ & 31) == 0)
    {
        const float targetShift = cutoffOffset_ * 1.5f + modIntensity_ * modSourceValue_ * 1.5f;
        if (std::abs(targetShift - lastAppliedOctaveShift_) > 0.002f)
        {
            updateSynthesisFrequencies(targetShift);
        }
    }

    // 1. Detección de sibilantes (Paso-altos 8kHz sobre el modulador)
    float modHighFreqs = hpfB0_ * effectiveMod + hpfS1_;
    hpfS1_ = hpfB1_ * effectiveMod - hpfA1_ * modHighFreqs + hpfS2_;
    hpfS2_ = hpfB2_ * effectiveMod - hpfA2_ * modHighFreqs;

    float sibilanceEnvelope = sibilanceFollower_.process(modHighFreqs);
    float sibilanceSignal = 0.0f;

    // Comprobar umbral de consonante
    if (sibilanceEnvelope > hpfThreshold_)
    {
        if (hpfGate_)
        {
            // MODO ENABLE: Inyectar ruido blanco modulado por la envolvente de la consonante
            float whiteNoise = whiteNoiseGen_.getWhiteNoise();
            sibilanceSignal = whiteNoise * sibilanceEnvelope * hpfLevel_;
        }
        else
        {
            // MODO DISABLE: Pasar directamente los agudos del micro
            sibilanceSignal = modHighFreqs * hpfLevel_;
        }
    }

    float leftAcc = 0.0f;
    float rightAcc = 0.0f;
    const bool holdActive = isFormantHold();
    const float gateFloor = hpfThreshold_ * 0.015f;

    // 2. Procesamiento de las bandas (cascada de 4º orden + ganancia de compensación)
    const float bandGainMult = is8BandMode_ ? 8.0f : 4.0f;
#define PROCESS_VOCODER_BAND(idx)                                                                                      \
    do                                                                                                                 \
    {                                                                                                                  \
        float vEnv = 0.0f;                                                                                             \
        if (holdActive)                                                                                                \
        {                                                                                                              \
            vEnv = frozenEnvs_[idx];                                                                                   \
        }                                                                                                              \
        else                                                                                                           \
        {                                                                                                              \
            float fMod = analysisFilters2_[idx].process(analysisFilters_[idx].process(effectiveMod));                  \
            vEnv = followers_[idx].process(fMod);                                                                      \
            if (vEnv < gateFloor)                                                                                      \
                vEnv = 0.0f;                                                                                           \
            if (vEnv > 0.001f)                                                                                         \
                frozenEnvs_[idx] = vEnv;                                                                               \
        }                                                                                                              \
        float fCar = synthesisFilters2_[idx].process(synthesisFilters_[idx].process(carrierSample));                   \
        float bAudio = fCar * vEnv * bandLevels_[idx] * bandGainMult;                                                  \
        float pNorm = bandPans_[idx];                                                                                  \
        leftAcc += bAudio * (1.0f - pNorm);                                                                            \
        rightAcc += bAudio * pNorm;                                                                                    \
    } while (0)

    if (is8BandMode_)
    {
        // En perfil microKORG: 8 bandas activas (canales alternos 0, 2, 4, 6, 8, 10, 12, 14)
        PROCESS_VOCODER_BAND(0);
        PROCESS_VOCODER_BAND(2);
        PROCESS_VOCODER_BAND(4);
        PROCESS_VOCODER_BAND(6);
        PROCESS_VOCODER_BAND(8);
        PROCESS_VOCODER_BAND(10);
        PROCESS_VOCODER_BAND(12);
        PROCESS_VOCODER_BAND(14);
    }
    else
    {
        // En perfil MS2000 / Advanced: 16 bandas completas
        PROCESS_VOCODER_BAND(0);
        PROCESS_VOCODER_BAND(1);
        PROCESS_VOCODER_BAND(2);
        PROCESS_VOCODER_BAND(3);
        PROCESS_VOCODER_BAND(4);
        PROCESS_VOCODER_BAND(5);
        PROCESS_VOCODER_BAND(6);
        PROCESS_VOCODER_BAND(7);
        PROCESS_VOCODER_BAND(8);
        PROCESS_VOCODER_BAND(9);
        PROCESS_VOCODER_BAND(10);
        PROCESS_VOCODER_BAND(11);
        PROCESS_VOCODER_BAND(12);
        PROCESS_VOCODER_BAND(13);
        PROCESS_VOCODER_BAND(14);
        PROCESS_VOCODER_BAND(15);
    }

#undef PROCESS_VOCODER_BAND

    // 3. Mezcla de modulador directo y bus de sibilantes centrado
    leftAcc += (modSample * directLevel_) + sibilanceSignal;
    rightAcc += (modSample * directLevel_) + sibilanceSignal;

    outLeft = leftAcc;
    outRight = rightAcc;
}

}  // namespace ABDMS2000
