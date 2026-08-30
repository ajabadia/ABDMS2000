#include "Vocoder16Band.h"
#include "../Common/DSPUtils.h"
#include <cmath>
#include <algorithm>

namespace ABDMS2000 {

// Exact 16 MS2000 measured bandpass center frequencies
static const float kMS2000VocoderFreqs[16] = {
    125.0f, 180.0f, 250.0f, 340.0f, 460.0f, 620.0f, 820.0f, 1050.0f,
    1350.0f, 1700.0f, 2150.0f, 2700.0f, 3350.0f, 4100.0f, 4900.0f, 5700.0f
};

void BiquadBPF::setBandpass(float centerFreq, float q, double sampleRate) noexcept
{
    float w0 = centerFreq * DSPUtils::TWO_PI / static_cast<float>(sampleRate);
    float sin_w0 = std::sin(w0);
    float cos_w0 = std::cos(w0);
    float alpha = sin_w0 / (2.0f * q);

    float a0 = 1.0f + alpha;
    b0 = (sin_w0 * 0.5f) / a0;
    b1 = 0.0f;
    b2 = (-sin_w0 * 0.5f) / a0;
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

void Vocoder16Band::prepare(double sampleRate) noexcept
{
    sampleRate_ = (sampleRate > 1000.0) ? sampleRate : 44100.0;
    whiteNoiseGen_.reset(0xABCDEF12);

    for (size_t i = 0; i < NUM_BANDS; ++i)
    {
        followers_[i].prepare(sampleRate_);
        bandLevels_[i] = 1.0f;
        bandPans_[i] = 0.5f; // Center
    }

    updateFilterFrequencies();
    updateHPFFilter();
    sibilanceFollower_.prepare(sampleRate_);
    sibilanceFollower_.setReleaseTime(0.010f); // 10ms fast release
    reset();
}

void Vocoder16Band::reset() noexcept
{
    for (auto& f : analysisFilters_) f.reset();
    for (auto& f : synthesisFilters_) f.reset();
    for (auto& f : followers_) f.reset();
    sibilanceFollower_.reset();
    hpfS1_ = hpfS2_ = 0.0f;
}


void Vocoder16Band::setFormantShift(int shiftValue) noexcept
{
    formantShift_ = std::max(-2, std::min(2, shiftValue));
    updateFilterFrequencies();
}

void Vocoder16Band::setGateSense(float gateSense0to1) noexcept
{
    float norm = DSPUtils::clamp(gateSense0to1, 0.0f, 1.0f);
    // Gate Sense maps to 0.005s .. 0.200s
    float releaseSec = 0.005f + (0.195f * norm);
    for (auto& f : followers_)
    {
        f.setReleaseTime(releaseSec);
    }
}

void Vocoder16Band::setHPFLevel(float level0to1) noexcept
{
    hpfLevel_ = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void Vocoder16Band::setHPFThreshold(float thresh0to1) noexcept
{
    hpfThreshold_ = DSPUtils::clamp(thresh0to1, 0.001f, 1.0f);
}

void Vocoder16Band::setDirectLevel(float level0to1) noexcept
{
    directLevel_ = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void Vocoder16Band::setBandLevel(size_t bandIndex, float level0to1) noexcept
{
    bandLevels_[bandIndex % NUM_BANDS] = DSPUtils::clamp(level0to1, 0.0f, 1.0f);
}

void Vocoder16Band::setBandPan(size_t bandIndex, float pan0to1) noexcept
{
    bandPans_[bandIndex % NUM_BANDS] = DSPUtils::clamp(pan0to1, 0.0f, 1.0f);
}

void Vocoder16Band::updateFilterFrequencies() noexcept
{
    const float Q = 6.5f; // Constant-Q bandwidth for sharp band isolation

    for (size_t i = 0; i < NUM_BANDS; ++i)
    {
        // 1. Modulator analysis filters: fixed frequencies
        analysisFilters_[i].setBandpass(kMS2000VocoderFreqs[i], Q, sampleRate_);

        // 2. Carrier synthesis filters: Formant Shift offset
        int synthIdx = std::max(0, std::min(15, static_cast<int>(i) + formantShift_));
        synthesisFilters_[i].setBandpass(kMS2000VocoderFreqs[synthIdx], Q, sampleRate_);
    }
}

void Vocoder16Band::updateHPFFilter() noexcept
{
    // 8.0 kHz High-Pass Butterworth Filter for Sibilance Detector
    float w0 = 8000.0f * DSPUtils::TWO_PI / static_cast<float>(sampleRate_);
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

void Vocoder16Band::process(float modSample, float carrierSample, float& outLeft, float& outRight) noexcept
{
    if (!enabled_) return;

    // 1. Sibilance Detection (8kHz High-Pass on Modulator)
    float modHighFreqs = hpfB0_ * modSample + hpfS1_;
    hpfS1_ = hpfB1_ * modSample - hpfA1_ * modHighFreqs + hpfS2_;
    hpfS2_ = hpfB2_ * modSample - hpfA2_ * modHighFreqs;

    float sibilanceEnvelope = sibilanceFollower_.process(modHighFreqs);
    float sibilanceSignal = 0.0f;

    // Comprobar si supera el umbral configurado (Gate Sense)
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

    // 2. Unrolled 16-Stage Vocoder Engine (Zero Branch Overhead)
    #define PROCESS_VOCODER_BAND(idx) do { \
        float fMod = analysisFilters_[idx].process(modSample); \
        float vEnv = followers_[idx].process(fMod); \
        float fCar = synthesisFilters_[idx].process(carrierSample); \
        float bAudio = fCar * vEnv * bandLevels_[idx] * 0.5f; \
        float pNorm = bandPans_[idx]; \
        leftAcc  += bAudio * (1.0f - pNorm); \
        rightAcc += bAudio * pNorm; \
    } while(0)

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

    #undef PROCESS_VOCODER_BAND

    // 3. Dry Modulator blend (Direct Level) + Sibilance parallel bus (centered)
    leftAcc  += (modSample * directLevel_) + sibilanceSignal;
    rightAcc += (modSample * directLevel_) + sibilanceSignal;

    outLeft = leftAcc;
    outRight = rightAcc;
}


} // namespace ABDMS2000

