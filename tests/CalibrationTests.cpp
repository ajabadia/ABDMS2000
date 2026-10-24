/**
 * @file tests/CalibrationTests.cpp
 * @brief Herramienta y suite de calibración metrológica de Nivel 2 (sondas DSP por bloque)
 *        para ABDMS2000 frente a las tablas de referencia del firmware Korg MS2000 v1.07
 *        (`x811v107.sys` / Motorola DSP56362).
 *
 * Reutiliza y adapta los algoritmos metrológicos de `ABDAudioLab`:
 *  - `ModulationEstimator::calculateNode` -> Regresión por Mínimos Cuadrados Ordinarios (OLS: pendiente K,
 *    offset C, coeficiente de determinación R^2 y error máximo residual).
 *  - `FilterAnalytics` & `PreScanSpectrumAnalyzer` -> Sondas espectrales Goertzel/DFT de alta precisión,
 *    detección de meseta de banda de paso, frecuencia de corte a -3 dB, pico de resonancia (dB),
 *    pendiente asintótica de caída (dB/oct en escala log2) y distorsión armónica total (THD% H2..H5).
 *  - `EnvelopeAnalytics` & `LabEnvelopeCurveAnalyzer` -> Extracción de envolvente rectificada LPF,
 *    medición de Attack/Decay/Sustain/Release, Integral Retrógrada de Schroeder (EDC dB / RT60)
 *    y detector de picos de eco primario/secundario para líneas de retardo (DelayFX).
 *  - `ModulationAnalytics` -> Estimador de frecuencia cíclica LFO por cruces por cero con interpolación
 *    sub-muestra y autocorrelación, análisis de polaridad unipolar/bipolar y cuadratura estéreo.
 */

#include "CalibrationTests.h"

#include <AudioAnalytics/DspMetrologicalProbes.h>
#include <LutDSP/models/OfficialLutModels.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Core/MS2000FirmwareTables.h"
#include "Core/SynthEngine.h"
#include "Core/Voice.h"
#include "Core/VoiceManager.h"
#include "DSP/Common/DSPUtils.h"
#include "DSP/Effects/DelayFX.h"
#include "DSP/Effects/Equalizer.h"
#include "DSP/Effects/ModFX.h"
#include "DSP/Envelopes/ADSREnvelope.h"
#include "DSP/Envelopes/EnvelopeCurves.h"
#include "DSP/Filters/FilterResonanceComp.h"
#include "DSP/Filters/MultiModeFilter.h"
#include "DSP/Modulation/LFO.h"
#include "DSP/Modulation/VirtualPatchMatrix.h"
#include "DSP/Oscillators/DWGSTables.h"
#include "DSP/Oscillators/OSC2Modulator.h"
#include "DSP/Oscillators/VAOscillator.h"
#include "DSP/Sequencer/ModSequencer.h"
#include "DSP/Vocoder/Vocoder16Band.h"
#include "MIDI/MS2000HardwareProgram.h"
#include "MIDI/SysExManager.h"
#include "State/Conversion.gen.h"
#include "State/MS2000FactoryBank.h"
#include "State/MS2000PatchBuilder.h"
#include "State/ParameterRegistry.gen.h"
#include "Tests/TestCompat.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

namespace ABDMS2000
{
namespace Tests
{

// ============================================================================
// Núcleo Metrológico adaptado de ABDAudioLab (src/math/analytics & ModulationEstimator)
// ============================================================================
namespace AudioLabProbes
{

struct RegressionMetrics
{
    double slope {0.0};
    double intercept {0.0};
    double rSquared {0.0};
    double maxAbsError {0.0};
    double rmsError {0.0};
    bool valid {false};
};

/**
 * Regresión analítica por Mínimos Cuadrados Ordinarios (OLS) adaptada de
 * `abdaudiolab::math::ModulationEstimator::calculateNode` y `LabEnvelopeCurveAnalyzer::analyzeTrajectory`.
 */
static RegressionMetrics computeLinearRegression(const std::vector<double> &x, const std::vector<double> &y)
{
    RegressionMetrics res;
    const size_t n = std::min(x.size(), y.size());
    if (n < 2)
        return res;

    double sumX = 0.0, sumY = 0.0, sumXY = 0.0, sumXX = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        sumXX += x[i] * x[i];
    }

    const double fn = static_cast<double>(n);
    const double denom = (fn * sumXX) - (sumX * sumX);
    if (std::abs(denom) < 1e-12)
        return res;

    res.slope = ((fn * sumXY) - (sumX * sumY)) / denom;
    res.intercept = (sumY - (res.slope * sumX)) / fn;

    const double meanY = sumY / fn;
    double ssTot = 0.0, ssRes = 0.0, maxErr = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double yPred = res.slope * x[i] + res.intercept;
        const double err = std::abs(y[i] - yPred);
        const double devTot = y[i] - meanY;
        const double devRes = y[i] - yPred;
        ssTot += devTot * devTot;
        ssRes += devRes * devRes;
        maxErr = std::max(maxErr, err);
    }

    res.rSquared = (ssTot > 1e-12) ? std::clamp(1.0 - (ssRes / ssTot), 0.0, 1.0) : 1.0;
    res.maxAbsError = maxErr;
    res.rmsError = std::sqrt(ssRes / fn);
    res.valid = true;
    return res;
}

/**
 * Compara directamente una curva medida frente a la curva teórica/firmware de referencia (identidad Y = X).
 */
static RegressionMetrics compareCurvesAgainstReference(const std::vector<double> &reference,
                                                       const std::vector<double> &measured)
{
    RegressionMetrics reg = computeLinearRegression(reference, measured);
    const size_t n = std::min(reference.size(), measured.size());
    if (n == 0)
        return reg;

    double sqErrSum = 0.0;
    double maxDirectErr = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double diff = std::abs(measured[i] - reference[i]);
        sqErrSum += diff * diff;
        maxDirectErr = std::max(maxDirectErr, diff);
    }
    reg.maxAbsError = maxDirectErr;
    reg.rmsError = std::sqrt(sqErrSum / static_cast<double>(n));
    return reg;
}

/**
 * Estimador de frecuencia fundamental con interpolación lineal sub-muestra en cruces por cero ascendentes.
 */
static double measureFundamentalFreqHz(const float *samples, int numSamples, double sampleRate, int skipSamples = 128)
{
    if (!samples || numSamples <= skipSamples + 64 || sampleRate <= 0.0)
        return 0.0;

    std::vector<double> crossings;
    crossings.reserve(256);

    for (int i = std::max(1, skipSamples); i < numSamples; ++i)
    {
        const float s0 = samples[i - 1];
        const float s1 = samples[i];
        if (s0 <= 0.0f && s1 > 0.0f)
        {
            const double frac = (s1 - s0 > 1e-9f) ? static_cast<double>(-s0) / static_cast<double>(s1 - s0) : 0.0;
            crossings.push_back(static_cast<double>(i - 1) + frac);
        }
    }

    if (crossings.size() < 2)
        return 0.0;

    const double totalCycles = static_cast<double>(crossings.size() - 1);
    const double totalSamples = crossings.back() - crossings.front();
    if (totalSamples <= 0.0)
        return 0.0;

    return (sampleRate * totalCycles) / totalSamples;
}

/**
 * Sonda espectral Goertzel de amplitud lineal y dB en una frecuencia arbitraria
 * (adaptado de `PreScanSpectrumAnalyzer` / `DSPCoreTests_VCF`).
 */
static double goertzelMagnitudeLinear(const float *samples, int numSamples, double targetFreqHz, double sampleRate)
{
    if (!samples || numSamples <= 0 || targetFreqHz <= 0.0 || targetFreqHz >= sampleRate * 0.5)
        return 0.0;

    const double omega = 2.0 * juce::MathConstants<double>::pi * targetFreqHz / sampleRate;
    const double coeff = 2.0 * std::cos(omega);
    double s1 = 0.0, s2 = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        // Ventana Hann suave para eliminar fugas espectrales
        const double w =
            0.5 * (1.0 - std::cos(2.0 * juce::MathConstants<double>::pi * static_cast<double>(i) / (numSamples - 1)));
        const double s0 = (static_cast<double>(samples[i]) * w) + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }

    const double power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    // Factor de compensación de ganancia coherente de ventana Hann (x2) y normalización (2/N)
    return (4.0 * std::sqrt(std::max(0.0, power))) / static_cast<double>(numSamples);
}

/**
 * Mide la ganancia en estacionario (dB) de un filtro monofónico ante una sinusoide pura de frecuencia `freqHz`.
 */
static double measureFilterSteadyStateGainDb(MultiModeFilter &filter,
                                             double freqHz,
                                             double sampleRate,
                                             int warmupSamples = 1024,
                                             int measureSamples = 2048)
{
    filter.reset();
    const double twoPiOverSr = 2.0 * juce::MathConstants<double>::pi / sampleRate;
    for (int i = 0; i < warmupSamples; ++i)
    {
        const float in = static_cast<float>(std::sin(twoPiOverSr * freqHz * static_cast<double>(i)));
        filter.process(in);
    }

    double sumSqIn = 0.0;
    double sumSqOut = 0.0;
    for (int i = 0; i < measureSamples; ++i)
    {
        const float in = static_cast<float>(std::sin(twoPiOverSr * freqHz * static_cast<double>(warmupSamples + i)));
        const float out = filter.process(in);
        sumSqIn += static_cast<double>(in) * static_cast<double>(in);
        sumSqOut += static_cast<double>(out) * static_cast<double>(out);
    }

    if (sumSqIn < 1e-12 || sumSqOut < 1e-18)
        return -120.0;
    return 10.0 * std::log10(sumSqOut / sumSqIn);
}

/**
 * Calcula el THD% instantáneo (armónicos H2..H5 respecto a H1) usando sondas Goertzel
 * (adaptado de `abdaudiolab::math::PreScanSpectrumAnalyzer::calculateInstantaneousThd`).
 */
static double computeThdPercent(const float *samples, int numSamples, double fundamentalHz, double sampleRate)
{
    const double h1 = goertzelMagnitudeLinear(samples, numSamples, fundamentalHz, sampleRate);
    if (h1 < 1e-6)
        return 0.0;

    double harmSqSum = 0.0;
    for (int h = 2; h <= 5; ++h)
    {
        const double fh = fundamentalHz * static_cast<double>(h);
        if (fh < sampleRate * 0.48)
        {
            const double mag = goertzelMagnitudeLinear(samples, numSamples, fh, sampleRate);
            harmSqSum += mag * mag;
        }
    }
    return (std::sqrt(harmSqSum) / h1) * 100.0;
}

/**
 * Detector de retardo entre pulso directo y primer eco secundario
 * (adaptado de `abdaudiolab::math::analytics::EnvelopeAnalytics::analyzeDelayImpulses`).
 */
static double detectEchoTapDelayMs(const std::vector<float> &ir, double sampleRate)
{
    if (ir.size() < 128 || sampleRate <= 0.0)
        return 0.0;

    float primaryPeak = 0.0f;
    size_t primaryIdx = 0;
    const size_t directWindow = std::min<size_t>(ir.size(), 64);
    for (size_t i = 0; i < directWindow; ++i)
    {
        const float a = std::abs(ir[i]);
        if (a > primaryPeak)
        {
            primaryPeak = a;
            primaryIdx = i;
        }
    }
    if (primaryPeak < 1e-5f)
        return 0.0;

    const size_t minSep = std::max<size_t>(16, static_cast<size_t>(std::lround(0.004 * sampleRate)));
    const size_t searchStart = primaryIdx + minSep;
    if (searchStart >= ir.size())
        return 0.0;

    float secondaryPeak = 0.0f;
    size_t secondaryIdx = searchStart;
    for (size_t i = searchStart; i < ir.size(); ++i)
    {
        const float a = std::abs(ir[i]);
        if (a > secondaryPeak)
        {
            secondaryPeak = a;
            secondaryIdx = i;
        }
    }

    if (secondaryPeak < primaryPeak * 0.02f)
        return 0.0;

    return (static_cast<double>(secondaryIdx - primaryIdx) / sampleRate) * 1000.0;
}

}  // namespace AudioLabProbes

// ============================================================================
// SUITE 1: Calibración de Pitch, Osciladores y Escalas Microtonales vs ROM v1.07
// ============================================================================
static void calibratePitchAndScales()
{
    printf("\n--- [Calibración Nivel 2.1] Pitch, Osciladores y Escalas vs ROM 0x240F0 / 0x13914 ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;  // 48000.0 Hz

    // 1.1 Verificación de la LUT Maestra de Pitch en ROM (0x240F0): duplicación por octava y pre-warping
    {
        bool octaveDoublingOk = true;
        for (int idx = 12; idx <= 156; idx += 12)
        {
            const double w0 = static_cast<double>(FirmwareTables::kRomMasterPitchQ23[idx]);
            const double w12 = static_cast<double>(FirmwareTables::kRomMasterPitchQ23[idx + 12]);
            const double ratio = w12 / w0;
            if (std::abs(ratio - 2.0) > 0.015)
                octaveDoublingOk = false;
        }
        // Por encima de 180 aplica pre-warping bilineal (ratio < 1.95 en idx 192->204)
        const double preWarpRatio =
            static_cast<double>(FirmwareTables::kRomMasterPitchQ23[204])
            / static_cast<double>(FirmwareTables::kRomMasterPitchQ23[192]);
        check(octaveDoublingOk && preWarpRatio < 1.90 && preWarpRatio > 1.50,
              "Cal 1.1: kRomMasterPitchQ23 (ROM 0x240F0) duplicates every 12 semitones and pre-warps near Nyquist");
    }

    // 1.2 Barrido de notas MIDI (24..96) en Voice y medición de frecuencia fundamental con sonda sub-muestra
    {
        const int testNotes[] = {24, 36, 48, 60, 69, 72, 84, 96};
        std::vector<double> refLog2Hz;
        std::vector<double> measLog2Hz;
        double maxCentsError = 0.0;

        for (int note : testNotes)
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = 1.0f;
            vp.osc2Level = 0.0f;
            vp.noiseLevel = 0.0f;
            vp.filterType = FilterType::LPF24;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Decay = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.eg2Release = 0.1f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(note, 1.0f, false);

            constexpr int kNumSamples = 16384;
            std::vector<float> buf(kNumSamples, 0.0f);
            for (int i = 0; i < kNumSamples; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }

            const double expectedHz = 440.0 * std::pow(2.0, static_cast<double>(note - 69) / 12.0);
            const double measuredHz = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kNumSamples, kSr, 256);
            const double centsErr = (measuredHz > 0.0) ? std::abs(1200.0 * std::log2(measuredHz / expectedHz)) : 999.0;
            maxCentsError = std::max(maxCentsError, centsErr);

            refLog2Hz.push_back(std::log2(expectedHz));
            measLog2Hz.push_back(std::log2(std::max(1.0, measuredHz)));
        }

        const auto reg = AudioLabProbes::computeLinearRegression(refLog2Hz, measLog2Hz);
        printf("  [Pitch Sweep C1..C7] R^2 = %.6f | Slope = %.5f | Max Error = %.3f cents\n",
               reg.rSquared,
               reg.slope,
               maxCentsError);
        check(reg.valid && reg.rSquared > 0.99999 && maxCentsError < 1.5,
              "Cal 1.2: Voice fundamental frequency sweep C1..C7 matches 12-TET within < 1.5 cents (R^2 > 0.99999)");
    }

    // 1.3 Regresión OLS de controles de afinación por timbre (pitchTranspose, pitchTune, pitchBendRange)
    {
        auto renderNoteCentsVsA4 = [&](float transposeSt, float tuneCents, float bendVal, float bendRangeSt) -> double
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            vp.pitchTranspose = transposeSt;
            vp.pitchTune = tuneCents;
            vp.pitchBendValue = bendVal;
            vp.pitchBendRange = bendRangeSt;
            v.applyBlockParams(vp);
            v.noteOn(69, 1.0f, false);

            constexpr int kN = 12000;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }
            const double hz = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kN, kSr, 256);
            return (hz > 0.0) ? (1200.0 * std::log2(hz / 440.0)) : -99999.0;
        };

        std::vector<double> injectedCents;
        std::vector<double> observedCents;
        for (int st : {-24, -12, -5, 0, 7, 12, 24})
        {
            injectedCents.push_back(static_cast<double>(st * 100));
            observedCents.push_back(renderNoteCentsVsA4(static_cast<float>(st), 0.0f, 0.0f, 2.0f));
        }
        const auto regTrans = AudioLabProbes::compareCurvesAgainstReference(injectedCents, observedCents);

        std::vector<double> tuneInjected, tuneObserved;
        for (int c : {-50, -25, 0, 25, 50})
        {
            tuneInjected.push_back(static_cast<double>(c));
            tuneObserved.push_back(renderNoteCentsVsA4(0.0f, static_cast<float>(c), 0.0f, 2.0f));
        }
        const auto regTune = AudioLabProbes::compareCurvesAgainstReference(tuneInjected, tuneObserved);

        const double bendPlus12 = renderNoteCentsVsA4(0.0f, 0.0f, 1.0f, 12.0f);
        const double bendMinus12 = renderNoteCentsVsA4(0.0f, 0.0f, -1.0f, 12.0f);

        printf("  [Transpose/Tune/Bend] Transpose R^2=%.6f (err=%.2fc) | Tune R^2=%.6f (err=%.2fc)\n",
               regTrans.rSquared,
               regTrans.maxAbsError,
               regTune.rSquared,
               regTune.maxAbsError);

        check(regTrans.rSquared > 0.99999 && regTrans.maxAbsError < 2.0 && regTune.rSquared > 0.9999
                  && regTune.maxAbsError < 1.5 && std::abs(bendPlus12 - 1200.0) < 2.0
                  && std::abs(bendMinus12 - (-1200.0)) < 2.0,
              "Cal 1.3: Timbre Transpose (-24..+24st), Fine Tune (-50..+50c) and Pitch Bend Range calibrate linearly");
    }

    // 1.4 Validación acústica de las 9 Escalas Microtonales de la ROM (0x13914 / 0x13980)
    {
        double maxScaleErrCents = 0.0;
        for (int scaleType = 0; scaleType <= 8; ++scaleType)
        {
            for (int note : {60, 62, 64, 65, 67, 69, 71})
            {
                Voice v;
                v.prepare(kSr);
                VoiceParameters vp;
                vp.osc1Type = OSC1Type::Sine;
                vp.osc1Level = 1.0f;
                vp.filterCutoffNorm = 1.0f;
                vp.eg2Sustain = 1.0f;
                vp.ampLevel = 1.0f;
                vp.scaleType = scaleType;
                vp.scaleKey = 0;  // Key of C
                v.applyBlockParams(vp);
                v.noteOn(note, 1.0f, false);

                constexpr int kN = 8192;
                std::vector<float> buf(kN, 0.0f);
                for (int i = 0; i < kN; ++i)
                {
                    float l = 0.0f, r = 0.0f;
                    v.renderNextSample(l, r);
                    buf[i] = l;
                }

                const double measHz = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kN, kSr, 128);
                const double eqTempHz = 440.0 * std::pow(2.0, static_cast<double>(note - 69) / 12.0);
                const double measDetuneCents = 1200.0 * std::log2(measHz / eqTempHz);
                const double romDetuneCents =
                    static_cast<double>(FirmwareTables::getHardwareScaleDetuneSemitones(note, scaleType, 0)) * 100.0;
                maxScaleErrCents = std::max(maxScaleErrCents, std::abs(measDetuneCents - romDetuneCents));
            }
        }
        printf("  [Microtonal Scales 0..8] Max acoustic deviation vs ROM 0x13914 = %.3f cents\n", maxScaleErrCents);
        check(maxScaleErrCents < 1.5,
              "Cal 1.4: Rendered Voice across all 9 ROM microtonal scales matches ROM 0x13914 within < 1.5 cents");
    }
}

// ============================================================================
// SUITE 2: Calibración del Filtro Multimodo (VCF) vs ROM 0x26154 y Pendientes SVF
// ============================================================================
static void calibrateMultiModeFilter()
{
    printf("\n--- [Calibración Nivel 2.2] Filtro Multimodo (VCF) vs ROM 0x26154 y FilterAnalytics ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 2.1 Verificación de la LUT de Cutoff en ROM (0x26154: kRomFilterCutoffQ23).
    //     OJO: esta tabla NO es la que el H8S envía al DSP para el VCF; el cutoff del DSP sale de
    //     kRomMasterPitchQ23[128+knob] con c = min(2w, 0x770000/2^23) y fc = (48000/pi)*asin(c)
    //     (ver `getHardwareFilterCutoffHz` y `cutoff/FINDINGS.md`).
    {
        // En el rango exponencial (índices 70..166), cada +12 pasos (1 octava) duplica el coeficiente Q23
        bool expOctaveOk = true;
        for (int idx = 72; idx <= 154; idx += 12)
        {
            const double c0 = static_cast<double>(FirmwareTables::kRomFilterCutoffQ23[idx]);
            const double c12 = static_cast<double>(FirmwareTables::kRomFilterCutoffQ23[idx + 12]);
            const double ratio = c12 / c0;
            if (std::abs(ratio - 2.0) > 0.02)
                expOctaveOk = false;
        }
        // En los índices 174..255 satura exactamente en el techo hardware 0xFC9536 (20 kHz)
        bool ceilingOk = true;
        for (int idx = 174; idx <= 255; ++idx)
        {
            if (FirmwareTables::kRomFilterCutoffQ23[idx] != 0xFC9536u)
                ceilingOk = false;
        }
        check(expOctaveOk && ceilingOk,
              "Cal 2.1: kRomFilterCutoffQ23 (ROM 0x26154 tuning table, unused by the DSP VCF path) doubles per octave and clamps at 0xFC9536");
    }

    // 2.2 Sonda espectral de punto de corte (-3 dB / -6 dB) y seguimiento logarítmico de frecuencia
    {
        const double targetCutoffs[] = {250.0, 500.0, 1000.0, 2000.0, 4000.0};
        std::vector<double> refLog2Fc;
        std::vector<double> measGainAtFcDb;
        bool cutoffTrackingOk = true;

        for (double fc : targetCutoffs)
        {
            MultiModeFilter flt;
            flt.prepare(kSr);
            flt.setType(FilterType::LPF12);
            flt.setCutoff(static_cast<float>(fc));
            flt.setResonance(0.0f);

            const double passbandDb = AudioLabProbes::measureFilterSteadyStateGainDb(flt, fc * 0.1, kSr);
            const double cornerDb = AudioLabProbes::measureFilterSteadyStateGainDb(flt, fc, kSr) - passbandDb;
            const double stopbandOctUpDb =
                AudioLabProbes::measureFilterSteadyStateGainDb(flt, fc * 2.0, kSr) - passbandDb;

            // En un filtro SVF 2-pole con damping sin resonancia (zeta=1 / Q=0.5..0.707),
            // la atenuación en fc está entre -2.5 dB y -6.5 dB, y una octava arriba cae > 9 dB
            if (cornerDb > -1.5 || cornerDb < -8.0 || stopbandOctUpDb > cornerDb - 6.0)
                cutoffTrackingOk = false;

            refLog2Fc.push_back(std::log2(fc));
            measGainAtFcDb.push_back(cornerDb);
        }

        check(cutoffTrackingOk,
              "Cal 2.2: MultiModeFilter corner attenuation at fc and -12dB/oct octave-up rejection track across 250Hz..4kHz");
    }

    // 2.3 Regresión de pendiente asintótica en banda de rechazo (LPF24 -24 dB/oct vs LPF12 -12 dB/oct vs HPF12 +12 dB/oct)
    {
        auto measureStopbandSlopeDbPerOct = [&](FilterType type, double cutoffHz, const std::vector<double> &probeFreqs)
        {
            MultiModeFilter flt;
            flt.prepare(kSr);
            flt.setType(type);
            flt.setCutoff(static_cast<float>(cutoffHz));
            flt.setResonance(0.0f);

            std::vector<double> log2Freqs;
            std::vector<double> gainsDb;
            for (double f : probeFreqs)
            {
                log2Freqs.push_back(std::log2(f));
                gainsDb.push_back(AudioLabProbes::measureFilterSteadyStateGainDb(flt, f, kSr));
            }
            return AudioLabProbes::computeLinearRegression(log2Freqs, gainsDb);
        };

        const std::vector<double> hfProbes = {2000.0, 2828.0, 4000.0, 5656.0};  // 2 a 3.5 octavas sobre fc=500Hz
        const auto regLpf24 = measureStopbandSlopeDbPerOct(FilterType::LPF24, 500.0, hfProbes);
        const auto regLpf12 = measureStopbandSlopeDbPerOct(FilterType::LPF12, 500.0, hfProbes);

        const std::vector<double> lfProbes = {125.0, 176.8, 250.0, 353.6};  // 2 a 3.5 octavas bajo fc=2000Hz
        const auto regHpf12 = measureStopbandSlopeDbPerOct(FilterType::HPF12, 2000.0, lfProbes);

        printf("  [VCF Roll-Off Slopes] LPF24 = %.2f dB/oct (R^2=%.4f) | LPF12 = %.2f dB/oct (R^2=%.4f) | HPF12 = +%.2f dB/oct (R^2=%.4f)\n",
               regLpf24.slope,
               regLpf24.rSquared,
               regLpf12.slope,
               regLpf12.rSquared,
               regHpf12.slope,
               regHpf12.rSquared);

        check(regLpf24.rSquared > 0.99 && regLpf24.slope < -20.0 && regLpf24.slope > -28.0,
              "Cal 2.3a: LPF24 asymptotic stopband roll-off slope is ~ -24 dB/octave (4-pole cascade)");
        check(regLpf12.rSquared > 0.99 && regLpf12.slope < -10.0 && regLpf12.slope > -15.0,
              "Cal 2.3b: LPF12 asymptotic stopband roll-off slope is ~ -12 dB/octave (2-pole)");
        check(regHpf12.rSquared > 0.99 && regHpf12.slope > 10.0 && regHpf12.slope < 15.0,
              "Cal 2.3c: HPF12 asymptotic stopband roll-off slope is ~ +12 dB/octave (2-pole)");
    }

    // 2.4 Curva de pico de resonancia y BPF12 simétrico
    {
        MultiModeFilter bpf;
        bpf.prepare(kSr);
        bpf.setType(FilterType::BPF12);
        bpf.setCutoff(1000.0f);
        bpf.setResonance(0.6f);

        const double gainCenter = AudioLabProbes::measureFilterSteadyStateGainDb(bpf, 1000.0, kSr);
        const double gainLow = AudioLabProbes::measureFilterSteadyStateGainDb(bpf, 250.0, kSr);
        const double gainHigh = AudioLabProbes::measureFilterSteadyStateGainDb(bpf, 4000.0, kSr);

        check(gainCenter > gainLow + 10.0 && gainCenter > gainHigh + 10.0,
              "Cal 2.4: BPF12 peaks sharply at center frequency (1 kHz) and rejects both 250 Hz and 4 kHz by > 10 dB");
    }
}

// ============================================================================
// SUITE 3: Calibración de Envolventes EG1 / EG2 vs ROM 0x27D54 (kRomEnvelopeIncrementQ23)
// ============================================================================
static void calibrateEnvelopes()
{
    printf("\n--- [Calibración Nivel 2.3] Envolventes EG1/EG2 vs ROM 0x27D54 (kRomEnvelopeIncrementQ23) ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;  // 48000.0 Hz

    // 3.1 Sonda de tiempos de Attack en ADSREnvelope (modo Hardware MS2000) frente a la tabla ROM 0x27D54
    {
        const int testKnobs[] = {0, 1, 10, 20, 32, 48, 64, 80, 96, 112, 127};
        std::vector<double> romAttackMs;
        std::vector<double> measuredAttackMs;

        for (int knob : testKnobs)
        {
            const double expectedSec = static_cast<double>(FirmwareTables::getHardwareEnvelopeTimeSeconds(knob));
            romAttackMs.push_back(expectedSec * 1000.0);

            ADSREnvelope env;
            env.prepare(kSr);
            env.setHardwareMS2000Mode(true);
            env.setAttack(static_cast<float>(knob) / 127.0f);
            env.setDecay(0.5f);
            env.setSustain(1.0f);
            env.setRelease(0.5f);
            env.noteOn();

            int samplesToPeak = 0;
            const int maxSamples = static_cast<int>(kSr * 5.0);
            for (int s = 1; s <= maxSamples; ++s)
            {
                env.getNextSample();
                if (env.getStage() != abd::synth::EnvelopeStage::Attack)
                {
                    samplesToPeak = s;
                    break;
                }
            }
            measuredAttackMs.push_back((static_cast<double>(samplesToPeak) / kSr) * 1000.0);
        }

        const auto reg = AudioLabProbes::compareCurvesAgainstReference(romAttackMs, measuredAttackMs);
        printf("  [EG Attack vs ROM 0x27D54] R^2 = %.6f | Slope = %.5f | RMS Err = %.3f ms | Knob0 = %.2f ms (ref 1.00 ms)\n",
               reg.rSquared,
               reg.slope,
               reg.rmsError,
               measuredAttackMs.front());

        check(reg.valid && reg.rSquared > 0.9999 && std::abs(measuredAttackMs.front() - 1.0) < 0.15
                  && std::abs(measuredAttackMs.back() - 3799.19) < 5.0,
              "Cal 3.1: ADSREnvelope Attack times across 0..127 match ROM 0x27D54 LUT (1.00ms..3799.19ms, R^2 > 0.9999)");
    }

    // 3.2 Sonda de Decay / Sustain / Release y umbral de corte exponencial (< 4% en P:$0A52 / P:$0A74)
    {
        std::vector<double> prevDecaySamples;
        bool monotonicDecay = true;
        double lastSamples = 0.0;

        for (int knob : {0, 16, 32, 48, 64, 80, 96, 112})
        {
            ADSREnvelope env;
            env.prepare(kSr);
            env.setHardwareMS2000Mode(true);
            env.setAttack(0.0f);
            env.setDecay(static_cast<float>(knob) / 127.0f);
            env.setSustain(0.0f);
            env.setRelease(0.2f);
            env.noteOn();

            int totalSamples = 0;
            const int maxSearch = static_cast<int>(kSr * 6.0);
            for (int s = 1; s <= maxSearch; ++s)
            {
                const float v = env.getNextSample();
                if (env.getStage() == abd::synth::EnvelopeStage::Sustain || v <= 0.001f)
                {
                    totalSamples = s;
                    break;
                }
            }
            if (knob > 0 && static_cast<double>(totalSamples) <= lastSamples)
                monotonicDecay = false;
            lastSamples = static_cast<double>(totalSamples);
        }

        // Verificar niveles exactos de meseta Sustain (0%, 25%, 50%, 75%, 100%)
        bool sustainAccurate = true;
        for (float targetSus : {0.0f, 0.25f, 0.50f, 0.75f, 1.0f})
        {
            ADSREnvelope env;
            env.prepare(kSr);
            env.setHardwareMS2000Mode(true);
            env.setAttack(0.0f);
            env.setDecay(0.05f);
            env.setSustain(targetSus);
            env.noteOn();
            float finalVal = 0.0f;
            for (int i = 0; i < 4000; ++i)
                finalVal = env.getNextSample();
            if (std::abs(finalVal - targetSus) > 0.01f)
                sustainAccurate = false;
        }

        check(monotonicDecay && sustainAccurate,
              "Cal 3.2: ADSREnvelope Decay/Release curves scale monotonically with ROM LUT and hold exact Sustain plateaus");
    }
}

// ============================================================================
// SUITE 4: Calibración de LFO1 y LFO2 vs ROM 0x295E8 / 0x29BE8 (P:$0523–$05F4)
// ============================================================================
static void calibrateLfos()
{
    printf("\n--- [Calibración Nivel 2.4] LFO1 y LFO2 vs ROM 0x295E8 / 0x29BE8 (hasta 194.90 Hz) ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 4.1 Verifica el tramo común, el primer desajuste, el cruce de velocidad y el máximo ROM.
    {
        bool identical0to114 = true;
        int firstDifferentIndex = -1;
        for (int k = 0; k < 128; ++k)
        {
            const uint32_t lfo1Increment = FirmwareTables::kRomLfo1IncrementQ23[k];
            const uint32_t lfo2Increment = FirmwareTables::kRomLfo2IncrementQ23[k];
            if (lfo1Increment != lfo2Increment && firstDifferentIndex < 0)
                firstDifferentIndex = k;
            if (k <= 114 && lfo1Increment != lfo2Increment)
                identical0to114 = false;
        }
        const float firstDifferentRatio = FirmwareTables::getHardwareLfo2SpeedRatio(115.0f);
        const float ratio120 = FirmwareTables::getHardwareLfo2SpeedRatio(120.0f);
        const float ratio121 = FirmwareTables::getHardwareLfo2SpeedRatio(121.0f);
        const float ratio127 = FirmwareTables::getHardwareLfo2SpeedRatio(127.0f);
        const float f1Max = FirmwareTables::getHardwareLfo1FreqHz(127.0f);
        const float f2Max = FirmwareTables::getHardwareLfo2FreqHz(127.0f);

        check(identical0to114 && firstDifferentIndex == 115
                  && FirmwareTables::kRomLfo1IncrementQ23[114] == 0x00207Au
                  && FirmwareTables::kRomLfo2IncrementQ23[114] == 0x00207Au
                  && FirmwareTables::kRomLfo1IncrementQ23[115] == 0x0021A0u
                  && FirmwareTables::kRomLfo2IncrementQ23[115] == 0x0020D4u
                  && firstDifferentRatio < 1.0f && ratio120 < 1.0f && ratio121 > 1.0f
                  && std::abs(ratio127 - 5.32385f) < 0.001f && std::abs(f1Max - 36.61f) < 0.05f
                  && std::abs(f2Max - 194.90f) < 0.1f,
              "Cal 4.1: ROM LFO tables match through 114 (0x207A), first differ at 115 (0x21A0/0x20D4), LFO2 crosses above at 121, and reaches 5.32385x at 127");
    }

    // 4.2 Medición de frecuencia real generada por el bloque DSP `LFO` en LFO1 y LFO2 (con MS2000 Mode activo)
    {
        const int testKnobs[] = {48, 64, 80, 96, 108, 114, 120, 127};
        std::vector<double> refLfo2Hz;
        std::vector<double> measLfo2Hz;

        for (int k : testKnobs)
        {
            const float targetHz = FirmwareTables::getHardwareLfo2FreqHz(static_cast<float>(k));
            LFO lfo2;
            lfo2.prepare(kSr);
            lfo2.setHardwareMS2000Mode(true);
            lfo2.setWaveformLFO2(LFOWaveformLFO2::Sine);
            lfo2.setFrequencyHz(targetHz);
            lfo2.setKeySyncMode(2);
            lfo2.triggerKeySync(true);

            constexpr int kN = 144000;  // 3.0 segundos a 48 kHz (cubre múltiples periodos en sub-Hz)
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
                buf[i] = lfo2.getNextSample();

            const double measuredHz = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kN, kSr, 64);
            refLfo2Hz.push_back(static_cast<double>(targetHz));
            measLfo2Hz.push_back(measuredHz);
        }

        const auto reg = AudioLabProbes::compareCurvesAgainstReference(refLfo2Hz, measLfo2Hz);
        printf("  [LFO2 Rendered Osc Rate] R^2 = %.6f | Slope = %.5f | Max Err = %.3f Hz (at 194.90 Hz)\n",
               reg.rSquared,
               reg.slope,
               reg.maxAbsError);

        check(reg.valid && reg.rSquared > 0.9999 && reg.maxAbsError < 0.5,
              "Cal 4.2: LFO2 rendered oscillation frequency matches ROM 0x29BE8 up to 194.90 Hz audio-rate (R^2 > 0.9999)");
    }

    // 4.3 Verificación de polaridad unipolar de LFO2 Square+ (P:$053B) vs bipolar de LFO1 Square (P:$0532)
    {
        LFO lfo1, lfo2;
        lfo1.prepare(kSr);
        lfo1.setWaveformLFO1(LFOWaveform::Square);
        lfo1.setFrequencyHz(10.0f);

        lfo2.prepare(kSr);
        lfo2.setHardwareMS2000Mode(true);
        lfo2.setWaveformLFO2(LFOWaveformLFO2::SquarePlus);
        lfo2.setFrequencyHz(10.0f);

        float min1 = 1.0f, max1 = -1.0f;
        float min2 = 1.0f, max2 = -1.0f;
        for (int i = 0; i < 9600; ++i)
        {
            const float v1 = lfo1.getNextSample();
            const float v2 = lfo2.getNextSample();
            min1 = std::min(min1, v1);
            max1 = std::max(max1, v1);
            min2 = std::min(min2, v2);
            max2 = std::max(max2, v2);
        }
        check(min1 < -0.9f && max1 > 0.9f && min2 >= 0.0f && max2 > 0.9f,
              "Cal 4.3: LFO1 Square is bipolar [-1,+1] (P:$0532) while LFO2 Square+ is unipolar [0,+1] (P:$053B)");
    }
}

// ============================================================================
// SUITE 5: Calibración de Virtual Patch Matrix (ModulationEstimator OLS R^2)
// ============================================================================
static void calibrateVirtualPatchMatrix()
{
    printf("\n--- [Calibración Nivel 2.5] Virtual Patch Matrix (Linealidad OLS R^2 y Rango Hardware) ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 5.1 Linealidad exacta de la matriz VirtualPatchMatrix en sus 8 destinos y neutralidad del byte 64 (0.0)
    {
        VirtualPatchMatrix matrix;
        PatchModulationSources src {};
        src.modWheel = 1.0f;  // Fuente fija = +1.0 para sondar la ganancia del slot

        const float intensities[] = {-1.0f, -0.5f, 0.0f, 0.5f, 1.0f};
        std::vector<double> xInj;
        std::vector<double> yPitch;
        std::vector<double> yCutoff;
        std::vector<double> yAmp;
        std::vector<double> yPan;

        for (float inten : intensities)
        {
            matrix.setSlot(0, PatchSource::ModWheel, PatchDestination::Pitch, inten);
            matrix.setSlot(1, PatchSource::ModWheel, PatchDestination::Cutoff, inten);
            matrix.setSlot(2, PatchSource::ModWheel, PatchDestination::Amp, inten);
            matrix.setSlot(3, PatchSource::ModWheel, PatchDestination::Pan, inten);

            const PatchModulationOutputs out = matrix.evaluate(src);
            xInj.push_back(static_cast<double>(inten));
            yPitch.push_back(static_cast<double>(out.pitchMod) * Constants::kVirtualPatchMaxPitchSemitones);
            yCutoff.push_back(static_cast<double>(out.cutoffMod) * Constants::kFilterModMaxOctaves);
            yAmp.push_back(static_cast<double>(out.ampMod));
            yPan.push_back(static_cast<double>(out.panMod));
        }

        const auto regPitch = AudioLabProbes::computeLinearRegression(xInj, yPitch);
        const auto regCutoff = AudioLabProbes::computeLinearRegression(xInj, yCutoff);
        const auto regAmp = AudioLabProbes::computeLinearRegression(xInj, yAmp);
        const auto regPan = AudioLabProbes::computeLinearRegression(xInj, yPan);

        check(regPitch.rSquared > 0.99999 && std::abs(regPitch.slope - 24.0) < 0.01 && std::abs(regPitch.intercept) < 1e-5,
              "Cal 5.1a: Virtual Patch -> Pitch scales linearly to +/-24 semitones (R^2 = 1.0, zero offset at 64)");
        check(regCutoff.rSquared > 0.99999 && std::abs(regCutoff.slope - 5.0) < 0.01 && std::abs(regCutoff.intercept) < 1e-5,
              "Cal 5.1b: Virtual Patch -> Cutoff scales linearly to +/-5 octaves (R^2 = 1.0, zero offset at 64)");
        check(regAmp.rSquared > 0.99999 && regPan.rSquared > 0.99999,
              "Cal 5.1c: Virtual Patch -> Amp and Pan scale linearly with R^2 = 1.0");
    }

    // 5.2 Sonda acústica de Virtual Patch (ModWheel -> Pitch +/-24 st) en Voice renderizada
    {
        auto measurePatchedPitchSt = [&](float patchIntensityNorm) -> double
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            vp.modWheelValue = 1.0f;
            vp.patchSlots[0].source = PatchSource::ModWheel;
            vp.patchSlots[0].destination = PatchDestination::Pitch;
            vp.patchSlots[0].intensity = patchIntensityNorm;
            v.applyBlockParams(vp);
            v.noteOn(60, 1.0f, false);  // C4 = 261.6256 Hz

            constexpr int kN = 12000;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }
            const double hz = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kN, kSr, 256);
            return (hz > 0.0) ? (12.0 * std::log2(hz / 261.625565)) : -999.0;
        };

        const double stMinus = measurePatchedPitchSt(-0.5f);  // -12 semitonos
        const double stZero = measurePatchedPitchSt(0.0f);    // 0 semitonos
        const double stPlus = measurePatchedPitchSt(0.5f);    // +12 semitonos

        check(std::abs(stMinus - (-12.0)) < 0.05 && std::abs(stZero) < 0.02 && std::abs(stPlus - 12.0) < 0.05,
              "Cal 5.2: Acoustic probe of Virtual Patch (ModWheel -> Pitch) shifts Voice by exact +/-12 semitones at +/-50%");
    }
}

// ============================================================================
// SUITE 6: Calibración de Efectos Globales y Distorsión (EQ, ModFX, Delay, Distortion)
// ============================================================================
static void calibrateEffectsAndDistortion()
{
    printf("\n--- [Calibración Nivel 2.6] Efectos Globales (EQ, ModFX, Delay) y Distorsión vs DSP56362 ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 6.1 Distorsión por Voz y Biquad de Gabinete (P:$0834–$0890): sonda THD% (H2..H5)
    {
        auto measureVoiceThd = [&](bool distOn) -> double
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 0.9f;
            vp.distortionOn = distOn;
            v.applyBlockParams(vp);
            v.noteOn(69, 1.0f, false);  // 440 Hz

            constexpr int kN = 4096;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < 512; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
            }
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }
            return AudioLabProbes::computeThdPercent(buf.data(), kN, 440.0, kSr);
        };

        const double thdClean = measureVoiceThd(false);
        const double thdDist = measureVoiceThd(true);
        printf("  [Voice Distortion THD%% @ 440Hz] Clean = %.3f%% | Distortion ON = %.2f%%\n", thdClean, thdDist);
        check(thdClean < 1.5 && thdDist > 15.0,
              "Cal 6.1: Voice Distortion (P:$0834) cleanly passes sine when OFF (<1.5% THD) and saturates harmonics when ON (>15% THD)");
    }

    // 6.2 Ecualizador Estéreo de 2 Bandas (P:$08B0–$08E7): neutralidad en 0 dB y curva -12..+12 dB
    {
        auto measureEqGainDbAt = [&](float lowGainDb, float highGainDb, double probeFreqHz) -> double
        {
            Equalizer eq;
            eq.prepare(kSr);
            eq.setLowFreqIndex(1);   // 250 Hz
            eq.setHighFreqIndex(2);  // 8.0 kHz
            eq.setLowGainDB(lowGainDb);
            eq.setHighGainDB(highGainDb);

            const double twoPiOverSr = 2.0 * juce::MathConstants<double>::pi / kSr;
            for (int i = 0; i < 1024; ++i)
            {
                float s = static_cast<float>(std::sin(twoPiOverSr * probeFreqHz * static_cast<double>(i))) * 0.25f;
                float l = s, r = s;
                eq.process(l, r);
            }
            double sqIn = 0.0, sqOut = 0.0;
            for (int i = 0; i < 2048; ++i)
            {
                float s = static_cast<float>(std::sin(twoPiOverSr * probeFreqHz * static_cast<double>(1024 + i))) * 0.25f;
                float l = s, r = s;
                eq.process(l, r);
                sqIn += static_cast<double>(s) * static_cast<double>(s);
                sqOut += static_cast<double>(l) * static_cast<double>(l);
            }
            return 10.0 * std::log10(sqOut / sqIn);
        };

        const double flatLow = measureEqGainDbAt(0.0f, 0.0f, 80.0);
        const double flatHigh = measureEqGainDbAt(0.0f, 0.0f, 14000.0);

        std::vector<double> targetDb = {-12.0, -6.0, 0.0, 6.0, 12.0};
        std::vector<double> measLowDb, measHighDb;
        for (double g : targetDb)
        {
            measLowDb.push_back(measureEqGainDbAt(static_cast<float>(g), 0.0f, 60.0));
            measHighDb.push_back(measureEqGainDbAt(0.0f, static_cast<float>(g), 16000.0));
        }
        const auto regLow = AudioLabProbes::computeLinearRegression(targetDb, measLowDb);
        const auto regHigh = AudioLabProbes::computeLinearRegression(targetDb, measHighDb);

        printf("  [Master 2-Band EQ] Flat(0dB) err = %.4f dB | LowShelf R^2 = %.5f | HighShelf R^2 = %.5f\n",
               std::max(std::abs(flatLow), std::abs(flatHigh)),
               regLow.rSquared,
               regHigh.rSquared);

        check(std::abs(flatLow) < 0.05 && std::abs(flatHigh) < 0.05 && regLow.rSquared > 0.995
                  && regHigh.rSquared > 0.995,
              "Cal 6.2: Master 2-Band EQ is unity flat at 0dB (<0.05dB) and tracks -12..+12dB shelves linearly (R^2 > 0.995)");
    }

    // 6.3 ModFX (Chorus/Flanger, Ensemble, Phaser): transparencia en Depth=0 y ensanchamiento estéreo en Depth>0
    {
        bool allTypesModulateStereo = true;
        for (ModFXType t : {ModFXType::ChorusFlanger, ModFXType::Ensemble, ModFXType::Phaser})
        {
            ModFX fx;
            fx.prepare(kSr);
            fx.setEnabled(true);
            fx.setType(t);
            fx.setSpeed(0.5f);
            fx.setDepth(0.8f);
            fx.setFeedback(40.0f);

            double stereoDiffEnergy = 0.0;
            const double twoPiOverSr = 2.0 * juce::MathConstants<double>::pi / kSr;
            for (int i = 0; i < 4096; ++i)
            {
                const float in = static_cast<float>(std::sin(twoPiOverSr * 440.0 * static_cast<double>(i))) * 0.5f;
                float l = in, r = in;
                fx.process(l, r);
                const double diff = static_cast<double>(l) - static_cast<double>(r);
                stereoDiffEnergy += diff * diff;
            }
            if (stereoDiffEnergy < 1e-3)
                allTypesModulateStereo = false;
        }
        check(allTypesModulateStereo,
              "Cal 6.3: ModFX Chorus/Flanger (P:$0F50), Ensemble (P:$0F96) and Phaser (P:$105D) produce quadrature stereo modulation");
    }

    // 6.4 DelayFX (Stereo, Cross, LeftRight): sonda de eco por impulso (`EnvelopeAnalytics::analyzeDelayImpulses`)
    {
        const double targetTimesMs[] = {50.0, 120.0, 250.0, 500.0};
        std::vector<double> refMs;
        std::vector<double> measMs;

        for (double tMs : targetTimesMs)
        {
            DelayFX delay;
            delay.prepare(kSr);
            delay.setEnabled(true);
            delay.setType(DelayType::Stereo);
            delay.setTimeSeconds(static_cast<float>(tMs * 0.001));
            delay.setDepth(0.8f);
            delay.setFeedback(0.3f);

            // Asentar el filtro de suavizado de tiempo (glide de 50ms) en silencio antes de inyectar el impulso
            for (int i = 0; i < 12000; ++i)
            {
                float zL = 0.0f, zR = 0.0f;
                delay.process(zL, zR);
            }

            const int captureLen = static_cast<int>(kSr * 0.8);
            std::vector<float> irL(captureLen, 0.0f);
            for (int i = 0; i < captureLen; ++i)
            {
                float l = (i == 0) ? 1.0f : 0.0f;
                float r = (i == 0) ? 1.0f : 0.0f;
                delay.process(l, r);
                irL[i] = l;
            }

            const double echoMs = AudioLabProbes::detectEchoTapDelayMs(irL, kSr);
            refMs.push_back(tMs);
            measMs.push_back(echoMs);
        }

        const auto regDelay = AudioLabProbes::compareCurvesAgainstReference(refMs, measMs);

        // Verificar rebote Ping-Pong L->R (1x T en L = 200 ms, 2x T en R = 400 ms según DSP56362 P:$1025–$105C)
        DelayFX lrDelay;
        lrDelay.prepare(kSr);
        lrDelay.setEnabled(true);
        lrDelay.setType(DelayType::LeftRight);
        lrDelay.setTimeSeconds(0.200f);  // 200 ms L (1x T), 400 ms R (2x T ping-pong)
        lrDelay.setDepth(0.8f);
        lrDelay.setFeedback(0.0f);
        for (int i = 0; i < 12000; ++i)
        {
            float zL = 0.0f, zR = 0.0f;
            lrDelay.process(zL, zR);
        }
        const int lrLen = static_cast<int>(kSr * 0.6);  // 600 ms para capturar el tap L (200ms) y el tap R (400ms)
        std::vector<float> lrOutL(lrLen, 0.0f), lrOutR(lrLen, 0.0f);
        for (int i = 0; i < lrLen; ++i)
        {
            float l = (i == 0) ? 1.0f : 0.0f;
            float r = (i == 0) ? 1.0f : 0.0f;
            lrDelay.process(l, r);
            lrOutL[i] = l;
            lrOutR[i] = r;
        }
        const double tapLeftMs = AudioLabProbes::detectEchoTapDelayMs(lrOutL, kSr);
        const double tapRightMs = AudioLabProbes::detectEchoTapDelayMs(lrOutR, kSr);

        printf("  [DelayFX Echo Probe] R^2 = %.6f | Max Tap Error = %.2f ms | L/R Ping-Pong Taps = %.1f ms (L) / %.1f ms (R)\n",
               regDelay.rSquared,
               regDelay.maxAbsError,
               tapLeftMs,
               tapRightMs);

        check(regDelay.valid && regDelay.rSquared > 0.9999 && regDelay.maxAbsError < 1.5
                  && std::abs(tapLeftMs - 200.0) < 1.5 && std::abs(tapRightMs - 400.0) < 1.5,
              "Cal 6.4: DelayFX echo tap timing matches configured delay within <1.5ms (R^2 > 0.9999) and L/R Ping-Pong is 1x/2x (P:$1025)");
    }
}

// ============================================================================
// SUITE 7: Síntesis No Lineal de Osciladores (PWM, Cross-Mod FM, Hard Sync, Ring Mod, DWGS & Mixer)
// ============================================================================
static void calibrateOscillatorShapingAndMixer()
{
    printf("\n--- [Calibración Nivel 2.7] Síntesis de Osciladores (PWM, Cross-Mod FM, Sync/Ring, DWGS & Mixer) ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 7.1 Pulse PWM Duty Cycle (Control 1: 0.0 -> 50% square .. 0.8 -> 90% narrow pulse)
    {
        const float ctrl1Sweep[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f};
        std::vector<double> xCtrl;
        std::vector<double> yDuty;

        for (float c1 : ctrl1Sweep)
        {
            VAOscillator osc;
            osc.prepare(kSr);
            osc.setWaveform(VAWaveform::Pulse);
            osc.setFrequency(250.0f);
            osc.setControl1(c1);
            osc.reset();

            constexpr int kN = 19200;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
                buf[i] = osc.getNextSample();

            const double duty = abd::analytics::OscillatorShapingAnalytics::estimatePulseDutyCycle(buf.data(), kN, 128);
            xCtrl.push_back(static_cast<double>(c1));
            yDuty.push_back(duty);
        }

        const auto regPwm = AudioLabProbes::computeLinearRegression(xCtrl, yDuty);
        printf("  [OSC1 Pulse PWM] Duty(0.0) = %.3f | Duty(0.8) = %.3f | Linearity R^2 = %.6f\n",
               yDuty.front(),
               yDuty.back(),
               regPwm.rSquared);

        check(std::abs(yDuty.front() - 0.50) < 0.02 && regPwm.rSquared > 0.995,
              "Cal 7.1: OSC1 Pulse Control 1 sweeps duty cycle linearly from 50% square (R^2 > 0.995)");
    }

    // 7.2 Sine Cross-Mod (FM OSC2 -> OSC1 via Control 1 & Control 2, DSP56362 P:$0621–$064D)
    {
        const float fmDepths[] = {0.0f, 0.08f, 0.16f, 0.24f};
        std::vector<double> sidebandRatios;
        bool monotonicSidebands = true;

        for (float depth : fmDepths)
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;  // Sine / Cross-Mod
            vp.osc1Ctrl1 = depth;          // FM depth from OSC2
            vp.osc1Ctrl2 = 0.0f;           // Sin modulación LFO1 adicional para medir puro Control 1
            vp.osc1Level = 1.0f;
            vp.osc2Wave = VAWaveform::Sine;
            vp.osc2Semitone = 7.0f;        // Quinta justa (659.25 Hz) para separar bandas laterales fc +/- k*fm
            vp.osc2Level = 0.0f;           // Escuchamos únicamente la portadora OSC1 modulada por OSC2
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(69, 1.0f, false);  // A4 = 440 Hz

            constexpr int kN = 16384;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }

            const double ratio = abd::analytics::OscillatorShapingAnalytics::estimateFmSidebandToCarrierRatio(
                buf.data() + 1024, kN - 1024, 440.0, 659.255, kSr);
            if (!sidebandRatios.empty() && ratio < sidebandRatios.back() - 1e-4)
                monotonicSidebands = false;
            sidebandRatios.push_back(ratio);
        }

        printf("  [OSC1 Sine Cross-Mod FM] Sideband/Carrier Ratio: Ctrl1=0 -> %.4f | Ctrl1=0.24 -> %.4f\n",
               sidebandRatios.front(),
               sidebandRatios.back());

        check(sidebandRatios.front() < 0.02 && sidebandRatios.back() > 0.30 && monotonicSidebands,
              "Cal 7.2: OSC1 Sine Cross-Mod (OSC2 -> OSC1 FM) grows Bessel sidebands monotonically with Control 1");
    }

    // 7.3 Hard Sync (desplazamiento de formante armónico con pitch clavado a OSC1) y Ring Mod (supresión de portadoras)
    {
        auto measureSyncVoice = [&](float osc2Semi, double &outF0, double &outCentroidHz)
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Saw;
            vp.osc1Level = 0.0f;  // Silenciamos OSC1 en el mixer para medir puro OSC2 sincronizado
            vp.osc2Wave = VAWaveform::Sawtooth;
            vp.osc2Semitone = osc2Semi;
            vp.osc2ModMode = OSC2ModulationMode::Sync;
            vp.osc2Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(57, 1.0f, false);  // A3 = 220 Hz master

            constexpr int kN = 16384;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }
            outF0 = AudioLabProbes::measureFundamentalFreqHz(buf.data(), kN, kSr, 512);
            outCentroidHz = abd::analytics::OscillatorShapingAnalytics::computeHarmonicCentroidHz(
                buf.data() + 1024, kN - 1024, 220.0, kSr, 24);
        };

        double f0Sync0 = 0.0, cent0 = 0.0;
        double f0Sync7 = 0.0, cent7 = 0.0;
        double f0Sync19 = 0.0, cent19 = 0.0;
        measureSyncVoice(0.0f, f0Sync0, cent0);
        measureSyncVoice(7.0f, f0Sync7, cent7);
        measureSyncVoice(19.0f, f0Sync19, cent19);

        // Ring Mod: f1 = 440 Hz (A4), f2 = 659.25 Hz (E5, +7 st) -> suma = 1099.25 Hz, diferencia = 219.25 Hz
        Voice vRing;
        vRing.prepare(kSr);
        VoiceParameters vpRing;
        vpRing.osc1Type = OSC1Type::Sine;
        vpRing.osc1Level = 0.0f;
        vpRing.osc2Wave = VAWaveform::Triangle;
        vpRing.osc2Semitone = 7.0f;
        vpRing.osc2ModMode = OSC2ModulationMode::RingMod;
        vpRing.osc2Level = 1.0f;
        vpRing.filterCutoffNorm = 1.0f;
        vpRing.eg2Attack = 0.0f;
        vpRing.eg2Sustain = 1.0f;
        vpRing.ampLevel = 1.0f;
        vRing.applyBlockParams(vpRing);
        vRing.noteOn(69, 1.0f, false);  // A4 = 440 Hz

        constexpr int kNRing = 16384;
        std::vector<float> ringBuf(kNRing, 0.0f);
        for (int i = 0; i < kNRing; ++i)
        {
            float l = 0.0f, r = 0.0f;
            vRing.renderNextSample(l, r);
            ringBuf[i] = l;
        }
        const double magCarrier440 =
            AudioLabProbes::goertzelMagnitudeLinear(ringBuf.data() + 1024, kNRing - 1024, 440.0, kSr);
        const double magSumSideband =
            AudioLabProbes::goertzelMagnitudeLinear(ringBuf.data() + 1024, kNRing - 1024, 440.0 + 659.255, kSr);
        const double magDiffSideband =
            AudioLabProbes::goertzelMagnitudeLinear(ringBuf.data() + 1024, kNRing - 1024, 659.255 - 440.0, kSr);

        printf("  [Hard Sync & Ring Mod] Sync Centroid (0/+7/+19st) = %.0f / %.0f / %.0f Hz | Ring Sidebands/Carrier = %.2f\n",
               cent0,
               cent7,
               cent19,
               (magSumSideband + magDiffSideband) / std::max(1e-5, magCarrier440));

        check( cent7 > cent0 && cent19 > cent7 && magSumSideband > magCarrier440 * 5.0
                  && magDiffSideband > magCarrier440 * 5.0,
              "Cal 7.3: Hard Sync shifts harmonic formant centroid upward with slave pitch and RingMod suppresses carrier");
    }

    // 7.4 Linealidad del Mixer (osc1Level, osc2Level) y auditoría de las 64 ondas DWGS ROM (0x040000)
    {
        const float levels[] = {0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
        std::vector<double> xLvl, yRms;
        for (float lv : levels)
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = lv;
            vp.osc2Level = 0.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(69, 1.0f, false);

            double sumSq = 0.0;
            constexpr int kN = 4800;
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                if (i >= 800)
                    sumSq += static_cast<double>(l) * static_cast<double>(l);
            }
            xLvl.push_back(static_cast<double>(lv));
            yRms.push_back(std::sqrt(sumSq / 4000.0));
        }
        const auto regMix = AudioLabProbes::computeLinearRegression(xLvl, yRms);

        // Verificar que las 64 ondas DWGS renderizadas en Voice producen energía acotada y espectros diferenciados
        int validDwgsCount = 0;
        double minDwgsCentroid = 1e9, maxDwgsCentroid = 0.0;
        for (int w = 0; w < 64; ++w)
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::DWGS;
            vp.osc1DwgsIndex = w;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(60, 1.0f, false);  // C4 = 261.63 Hz

            constexpr int kN = 4096;
            std::vector<float> buf(kN, 0.0f);
            double sq = 0.0;
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
                if (i >= 512)
                    sq += static_cast<double>(l) * static_cast<double>(l);
            }
            const double rms = std::sqrt(sq / static_cast<double>(kN - 512));
            const double cHz = abd::analytics::OscillatorShapingAnalytics::computeHarmonicCentroidHz(
                buf.data() + 512, kN - 512, 261.6256, kSr, 16);
            if (rms > 0.02 && rms < 1.5 && cHz >= 250.0)
            {
                ++validDwgsCount;
                minDwgsCentroid = std::min(minDwgsCentroid, cHz);
                maxDwgsCentroid = std::max(maxDwgsCentroid, cHz);
            }
        }

        printf("  [Mixer & 64 DWGS ROM] Mixer R^2 = %.6f | Valid DWGS = %d/64 | Centroid Span = %.0f..%.0f Hz\n",
               regMix.rSquared,
               validDwgsCount,
               minDwgsCentroid,
               maxDwgsCentroid);

        check(regMix.rSquared > 0.9999 && validDwgsCount == 64 && maxDwgsCentroid > minDwgsCentroid * 2.0,
              "Cal 7.4: Mixer level scales linearly (R^2 > 0.9999) and all 64 DWGS ROM wavetables render distinct harmonic spectra");
    }
}

// ============================================================================
// SUITE 8: Interacción Dinámica del VCF (Key Track, EG1 Bipolar, Auto-Oscilación y OfficialLutModels)
// ============================================================================
static void calibrateDynamicFilterModulation()
{
    printf("\n--- [Calibración Nivel 2.8] VCF Dinámico (KeyTrack, EG1 Int, Auto-Oscilación y OfficialLutModels) ---\n");
    constexpr double kSr = FirmwareTables::kHardwareSampleRate;

    // 8.1 Paridad de la tabla oficial compartida `abd::lutdsp::models::korg_ms2000_dsp56362_vcf` (ABDAudioLab)
    //     frente a la tabla ROM 0x26154 (`FirmwareTables::getHardwareFilterCutoffHz`)
    {
        bool lutMatchesRom = true;
        for (size_t i = 0; i < abd::lutdsp::models::korg_ms2000_dsp56362_vcf_SIZE; ++i)
        {
            const auto &pt = abd::lutdsp::models::korg_ms2000_dsp56362_vcf[i];
            const float romHz = FirmwareTables::getHardwareFilterCutoffHz(pt.p1, 0.0f);
            if (std::abs(pt.mu - romHz) / std::max(1.0f, romHz) > 0.02f)
                lutMatchesRom = false;
        }
        check(lutMatchesRom,
              "Cal 8.1: Shared ABDAudioLab LUT (korg_ms2000_dsp56362_vcf) matches the asin(2w) VCF cutoff law within < 2%");
    }

    // 8.2 Seguimiento de teclado del VCF (`filterKbdTrack`) y excursión bipolar de `eg1FilterIntensity` en Voice
    {
        auto measureVoiceBrightness = [&](int midiNote, float baseCutoffNorm, float kbdTrack, float eg1Int) -> double
        {
            Voice v;
            v.prepare(kSr);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Saw;
            vp.osc1Level = 1.0f;
            vp.filterType = FilterType::LPF24;
            vp.filterCutoffNorm = baseCutoffNorm;
            vp.filterResonance = 0.25f;
            vp.filterKbdTrack = kbdTrack;
            vp.eg1FilterIntensity = eg1Int;
            vp.eg1Attack = 0.0f;
            vp.eg1Decay = 1.0f;
            vp.eg1Sustain = 1.0f;  // EG1 mantenido a +1.0 para medir el desplazamiento estático
            vp.eg2Attack = 0.0f;
            vp.eg2Sustain = 1.0f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(midiNote, 1.0f, false);

            constexpr int kN = 8192;
            std::vector<float> buf(kN, 0.0f);
            for (int i = 0; i < kN; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                buf[i] = l;
            }
            const double f0 = DSPUtils::midiNoteToFrequency(static_cast<float>(midiNote));
            return abd::analytics::OscillatorShapingAnalytics::computeHarmonicCentroidHz(
                buf.data() + 1024, kN - 1024, f0, kSr, 20);
        };

        // KeyTrack se mide lejos de C4, donde el desplazamiento es nulo por definicion. En C5
        // (nota 72, +1 octava sobre C4=60) el firmware aplica +-2 octavas de indice (kbdTrack * 2).
        // La base se elige en ~1.8 kHz (knob 0.45 con la ley fc = asin(2w)) para que las DOS
        // direcciones sean medibles: +2 octavas llega a ~6.5 kHz (por debajo del techo 0x770000
        // de 18.2 kHz, que antes se alcanzaba con bases mas altas) y -2 octavas cae a ~440 Hz,
        // por debajo del fundamental de C5 (523 Hz), sin saturar el centroide en el fundamental.
        constexpr float kKeyTrackBaseNorm = 0.45f;
        const double centKeyTrackOff = measureVoiceBrightness(72, kKeyTrackBaseNorm, 0.0f, 0.0f);
        const double centKeyTrackPos = measureVoiceBrightness(72, kKeyTrackBaseNorm, 1.0f, 0.0f);
        const double centKeyTrackNeg = measureVoiceBrightness(72, kKeyTrackBaseNorm, -1.0f, 0.0f);

        // En C4 (nota 60), EG1 Int = +0.5 abre el filtro y EG1 Int = -0.5 lo cierra respecto a 0.0.
        // Base ~2.6 kHz: con el span de 16 octavas del firmware, +0.5 llega al techo 0x770000
        // (18.2 kHz) y -0.5 cierra por debajo del fundamental (0 Hz de centroide).
        const double centEgNeg = measureVoiceBrightness(60, 0.50f, 0.0f, -0.5f);
        const double centEgZero = measureVoiceBrightness(60, 0.50f, 0.0f, 0.0f);
        const double centEgPos = measureVoiceBrightness(60, 0.50f, 0.0f, +0.5f);

        printf("  [VCF KeyTrack & EG1 Int] KeyTrack(-1/0/+1 @ C5) = %.0f / %.0f / %.0f Hz | EG1(-0.5/0/+0.5 @ C4) = %.0f / %.0f / %.0f Hz\n",
               centKeyTrackNeg,
               centKeyTrackOff,
               centKeyTrackPos,
               centEgNeg,
               centEgZero,
               centEgPos);

        check(centKeyTrackPos > centKeyTrackOff * 1.15 && centKeyTrackOff > centKeyTrackNeg * 1.06
                  && centEgPos > centEgZero * 1.25 && centEgZero > centEgNeg * 1.25,
              "Cal 8.2: VCF Keyboard Tracking (-1..+1 around C4) and bipolar EG1 Intensity (-1..+1) shift filter cutoff accurately");
    }

    // 8.3 Auto-oscilación del VCF (Resonance > 0.82 / knob > 105) y estabilidad acotada
    {
        MultiModeFilter flt;
        flt.prepare(kSr);
        flt.setType(FilterType::LPF24);
        flt.setCutoff(1000.0f);
        flt.setResonance(0.95f);  // Zona de auto-oscilación (> 0.82)

        // Excitación por impulso unitario y caída libre de 4096 muestras
        flt.process(1.0f);
        constexpr int kN = 4096;
        std::vector<float> ringBuf(kN, 0.0f);
        float maxPeak = 0.0f;
        double lateEnergy = 0.0;
        for (int i = 0; i < kN; ++i)
        {
            ringBuf[i] = flt.process(0.0f);
            maxPeak = std::max(maxPeak, std::abs(ringBuf[i]));
            if (i >= kN - 1024)
                lateEnergy += static_cast<double>(ringBuf[i]) * static_cast<double>(ringBuf[i]);
        }
        const double lateRms = std::sqrt(lateEnergy / 1024.0);
        check(lateRms > 0.01 && maxPeak < 4.0f,
              "Cal 8.3: VCF enters sustained self-oscillation above Resonance 0.82 (knob 105) and remains smoothly bounded");
    }
}

// ============================================================================
// SUITE 9: Auditoría Acústica de los 128 Presets de Fábrica (A01..H16) y Mod Sequencer
// ============================================================================
static void calibrateFull128FactoryBankAudit()
{
    printf("\n--- [Calibración Nivel 3] Auditoría Completa de los 128 Presets de Fábrica (A01..H16) y ModSeq ---\n");
    constexpr double kSr = 48000.0;

    // 9.1 Mod Sequencer: diferenciación de movimiento Step vs Smooth y rango exacto por destino
    {
        ModSequencer seqStep, seqSmooth;
        seqStep.prepare(kSr);
        seqStep.setEnabled(true);
        seqStep.setTempoBPM(120.0f);
        seqStep.setSyncResolution(3);  // 1/16

        seqSmooth.prepare(kSr);
        seqSmooth.setEnabled(true);
        seqSmooth.setTempoBPM(120.0f);
        seqSmooth.setSyncResolution(3);

        auto &tStep = seqStep.getTrack(0);
        tStep.destination = ModSeqDest::Cutoff;
        tStep.motion = ModSeqMotion::Step;
        tStep.length = 4;
        tStep.steps = {0.0f, 1.0f, 0.0f, 1.0f};

        auto &tSmooth = seqSmooth.getTrack(0);
        tSmooth.destination = ModSeqDest::Cutoff;
        tSmooth.motion = ModSeqMotion::Smooth;
        tSmooth.length = 4;
        tSmooth.steps = {0.0f, 1.0f, 0.0f, 1.0f};

        seqStep.triggerKeySync();
        seqSmooth.triggerKeySync();

        float maxJumpStep = 0.0f;
        float maxJumpSmooth = 0.0f;
        float prevStep = seqStep.getTrackOutput(0);
        float prevSmooth = seqSmooth.getTrackOutput(0);

        for (int i = 0; i < 16000; ++i)
        {
            seqStep.advanceClock(1);
            seqSmooth.advanceClock(1);
            const float curStep = seqStep.getTrackOutput(0);
            const float curSmooth = seqSmooth.getTrackOutput(0);
            maxJumpStep = std::max(maxJumpStep, std::abs(curStep - prevStep));
            maxJumpSmooth = std::max(maxJumpSmooth, std::abs(curSmooth - prevSmooth));
            prevStep = curStep;
            prevSmooth = curSmooth;
        }

        check(maxJumpStep > 0.5f && maxJumpSmooth < 0.05f,
              "Cal 9.1: ModSequencer Step mode produces discrete jumps while Smooth mode slew-interpolates transitions");
    }

    // 9.2 Barrido acústico de los 128 Presets de Fábrica del Korg MS2000 (A01..H16 en flash.bin @ 0x0B0000)
    {
        struct CalibrationEngineHost : public juce::AudioProcessor
        {
            CalibrationEngineHost()
                : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
            {
            }
            const juce::String getName() const override { return "CalHost"; }
            void prepareToPlay(double, int) override {}
            void releaseResources() override {}
            void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
            double getTailLengthSeconds() const override { return 0.0; }
            bool acceptsMidi() const override { return true; }
            bool producesMidi() const override { return false; }
            juce::AudioProcessorEditor *createEditor() override { return nullptr; }
            bool hasEditor() const override { return false; }
            int getNumPrograms() override { return 1; }
            int getCurrentProgram() override { return 0; }
            void setCurrentProgram(int) override {}
            const juce::String getProgramName(int) override { return {}; }
            void changeProgramName(int, const juce::String &) override {}
            void getStateInformation(juce::MemoryBlock &) override {}
            void setStateInformation(const void *, int) override {}
        };

        CalibrationEngineHost host;
        juce::AudioProcessorValueTreeState apvts(
            host, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
        SynthEngine engine(apvts);
        engine.prepare(kSr, 256);

        MS2000FactoryBank bank;

        int finiteBoundedPresets = 0;
        int audiblePresets = 0;
        float globalMaxPeak = 0.0f;

        juce::AudioBuffer<float> block(2, 256);
        juce::MidiBuffer emptyMidi;

        for (int idx = 0; idx < bank.getNumPresets(); ++idx)
        {
            const auto &prog = bank.getPresetProgramData(idx);
            prog.applyToAPVTS(apvts);

            engine.reset();
            engine.updateParametersFromAPVTS();
            engine.noteOn(1, 48, 0.9f);  // C3
            engine.noteOn(1, 60, 0.9f);  // C4

            bool hasNanOrInf = false;
            float presetPeak = 0.0f;
            double presetEnergy = 0.0;

            // Renderizar 12 bloques de 256 muestras (3072 muestras = 64 ms)
            for (int b = 0; b < 12; ++b)
            {
                block.clear();
                if (b == 10)
                {
                    engine.noteOff(1, 48, 0.0f);
                    engine.noteOff(1, 60, 0.0f);
                }
                engine.processBlock(block, emptyMidi, nullptr);

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float *ptr = block.getReadPointer(ch);
                    for (int s = 0; s < 256; ++s)
                    {
                        const float v = ptr[s];
                        if (!std::isfinite(v))
                            hasNanOrInf = true;
                        presetPeak = std::max(presetPeak, std::abs(v));
                        presetEnergy += static_cast<double>(v) * static_cast<double>(v);
                    }
                }
            }

            globalMaxPeak = std::max(globalMaxPeak, presetPeak);
            if (!hasNanOrInf && presetPeak < 8.0f)
                ++finiteBoundedPresets;
            if (presetEnergy > 1e-5)
                ++audiblePresets;
        }

        printf("  [128 Factory Presets Audit A01..H16] Finite & Bounded = %d/128 | Audible = %d/128 | Max Peak = %.3f\n",
               finiteBoundedPresets,
               audiblePresets,
               globalMaxPeak);

        check(finiteBoundedPresets == 128 && audiblePresets >= 120,
              "Cal 9.2: All 128 factory presets (A01..H16) render finite, bounded audio without NaN/Inf or runaway clipping");
    }
}

void testTier2FirmwareBlockCalibration()
{
    printf("\n================================================================================\n");
    printf("  BATERÍA METROLÓGICA DE CALIBRACIÓN DSP NIVEL 2 Y 3 (ABDAudioLab Probes vs ROM v1.07)\n");
    printf("================================================================================\n");

    calibratePitchAndScales();
    calibrateMultiModeFilter();
    calibrateEnvelopes();
    calibrateLfos();
    calibrateVirtualPatchMatrix();
    calibrateEffectsAndDistortion();
    calibrateOscillatorShapingAndMixer();
    calibrateDynamicFilterModulation();
    calibrateFull128FactoryBankAudit();
}

static void compareIdenticalPresetsWithEmu(const char *scratchDir, int noteOffFrame = -1)
{
    printf("\n================================================================================\n");
    printf("  COMPARATIVA ACÚSTICA 1-A-1: ABDMS2000 vs ms2000r-emu-main (H8S + DSP56362)\n");
    printf("  (Presets 100%% idénticos byte a byte en 0x0A0400 y 0x0B0000 | Nota C4 vel 100)\n");
    printf("================================================================================\n");

    constexpr double kSr = 48000.0;
    const int presetIndices[] = {2, 5, 9, 16, 20, 22, 23, 24};

    // 24 bandas de 1/3 de octava de 63 Hz a 10 kHz
    constexpr int kNumBands = 24;
    double bandFreqs[kNumBands];
    for (int b = 0; b < kNumBands; ++b)
        bandFreqs[b] = 63.0 * std::pow(2.0, static_cast<double>(b) / 3.0);

    struct AcousticProfile
    {
        double rmsDb = -120.0;
        double peakDb = -120.0;
        double spectralCentroidHz = 0.0;
        double bassPct = 0.0;     // 63..250 Hz
        double lowMidPct = 0.0;   // 250..1000 Hz
        double highMidPct = 0.0;  // 1000..4000 Hz
        double highPct = 0.0;     // 4000..10000 Hz
        double envSlopeDb = 0.0;  // Late RMS (200..400ms) - Early RMS (10..110ms)
        double stereoWidthPct = 0.0;
        double bandEnergy[kNumBands]{};
    };

    auto analyzeStereoBuffer = [&](const std::vector<float> &L, const std::vector<float> &R) -> AcousticProfile
    {
        AcousticProfile prof{};
        const size_t N = std::min(L.size(), R.size());
        if (N < 4096)
            return prof;

        // Buscar onset acústico (umbral 1e-3) para alinear frente a latencia UART MIDI (~3-5 ms en H8S)
        size_t onset = 0;
        for (size_t i = 0; i < N / 2; ++i)
        {
            if (std::max(std::abs(L[i]), std::abs(R[i])) > 1e-3f)
            {
                onset = i;
                break;
            }
        }

        const size_t winLen = std::min<size_t>(19200, N - onset);  // 400 ms desde onset
        double sumSq = 0.0, peak = 0.0;
        double midSq = 0.0, sideSq = 0.0;
        std::vector<float> mono(winLen, 0.0f);

        for (size_t i = 0; i < winLen; ++i)
        {
            const double l = L[onset + i];
            const double r = R[onset + i];
            const double m = 0.5 * (l + r);
            const double s = 0.5 * (l - r);
            mono[i] = static_cast<float>(m);
            sumSq += 0.5 * (l * l + r * r);
            peak = std::max(peak, std::max(std::abs(l), std::abs(r)));
            midSq += m * m;
            sideSq += s * s;
        }

        const double rms = std::sqrt(sumSq / static_cast<double>(winLen));
        prof.rmsDb = 20.0 * std::log10(std::max(1e-6, rms));
        prof.peakDb = 20.0 * std::log10(std::max(1e-6, peak));
        prof.stereoWidthPct = 100.0 * std::sqrt(sideSq / std::max(1e-12, midSq));

        // Envolvente temporal: Early (10..110 ms = muestras 480..5280) vs Late (200..400 ms = muestras 9600..19200)
        double earlySq = 0.0, lateSq = 0.0;
        const size_t e0 = std::min<size_t>(480, winLen), e1 = std::min<size_t>(5280, winLen);
        const size_t l0 = std::min<size_t>(9600, winLen), l1 = std::min<size_t>(19200, winLen);
        for (size_t i = e0; i < e1; ++i)
            earlySq += static_cast<double>(mono[i]) * static_cast<double>(mono[i]);
        for (size_t i = l0; i < l1; ++i)
            lateSq += static_cast<double>(mono[i]) * static_cast<double>(mono[i]);
        const double earlyRms = std::sqrt(earlySq / std::max<size_t>(1, e1 - e0));
        const double lateRms = std::sqrt(lateSq / std::max<size_t>(1, l1 - l0));
        prof.envSlopeDb = 20.0 * std::log10(std::max(1e-6, lateRms) / std::max(1e-6, earlyRms));

        // Análisis espectral en 24 bandas de 1/3 de octava integrando la potencia Welch (ventanas de 1024 muestras)
        // dentro de cada banda [fc * 2^(-1/6), fc * 2^(+1/6)] para no perder armónicos entre agujas discretas.
        double totalEnergy = 0.0, weightedFreq = 0.0;
        double eBass = 0.0, eLowMid = 0.0, eHighMid = 0.0, eHigh = 0.0;
        const int analysisN = static_cast<int>(std::min<size_t>(16384, winLen));
        constexpr int kFrameLen = 1024;
        constexpr int kHopLen = 512;

        for (int b = 0; b < kNumBands; ++b)
        {
            const double fc = bandFreqs[b];
            const double fLo = fc * 0.8908987;  // 2^(-1/6)
            const double fHi = fc * 1.1224620;  // 2^(+1/6)
            constexpr int kSubPoints = 7;
            double bandSum = 0.0;
            int frameCount = 0;
            for (int start = 0; start + kFrameLen <= analysisN; start += kHopLen)
            {
                double frameSubSum = 0.0;
                for (int sp = 0; sp < kSubPoints; ++sp)
                {
                    const double frac = static_cast<double>(sp) / static_cast<double>(kSubPoints - 1);
                    const double fProbe = fLo + frac * (fHi - fLo);
                    const double mag =
                        AudioLabProbes::goertzelMagnitudeLinear(mono.data() + start, kFrameLen, fProbe, kSr);
                    frameSubSum += mag * mag;
                }
                bandSum += frameSubSum / static_cast<double>(kSubPoints);
                ++frameCount;
            }
            const double e = (frameCount > 0) ? (bandSum / static_cast<double>(frameCount)) : 0.0;
            prof.bandEnergy[b] = e;
            totalEnergy += e;
            weightedFreq += fc * e;

            if (fc < 250.0)
                eBass += e;
            else if (fc < 1000.0)
                eLowMid += e;
            else if (fc < 4000.0)
                eHighMid += e;
            else
                eHigh += e;
        }

        if (totalEnergy > 1e-12)
        {
            prof.spectralCentroidHz = weightedFreq / totalEnergy;
            prof.bassPct = 100.0 * eBass / totalEnergy;
            prof.lowMidPct = 100.0 * eLowMid / totalEnergy;
            prof.highMidPct = 100.0 * eHighMid / totalEnergy;
            prof.highPct = 100.0 * eHigh / totalEnergy;
        }
        return prof;
    };

    struct CompareEngineHost : public juce::AudioProcessor
    {
        CompareEngineHost()
            : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
        {
        }
        const juce::String getName() const override { return "CmpHost"; }
        void prepareToPlay(double, int) override {}
        void releaseResources() override {}
        void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
        double getTailLengthSeconds() const override { return 0.0; }
        bool acceptsMidi() const override { return true; }
        bool producesMidi() const override { return false; }
        juce::AudioProcessorEditor *createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram(int) override {}
        const juce::String getProgramName(int) override { return {}; }
        void changeProgramName(int, const juce::String &) override {}
        void getStateInformation(juce::MemoryBlock &) override {}
        void setStateInformation(const void *, int) override {}
    };

    CompareEngineHost host;
    juce::AudioProcessorValueTreeState apvts(
        host, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
    SynthEngine engine(apvts);
    engine.prepare(kSr, 256);

    MS2000FactoryBank bank;
    for (int progIdx : presetIndices)
    {
        char binPath[512];
        std::snprintf(binPath, sizeof(binPath), "%s\\emu_preset_%d.bin", scratchDir, progIdx);
        FILE *f = nullptr;
        fopen_s(&f, binPath, "rb");
        if (!f)
        {
            printf("  [WARN] No se encontró %s\n", binPath);
            continue;
        }
        uint32_t nFrames = 0;
        std::fread(&nFrames, sizeof(uint32_t), 1, f);
        std::vector<float> emuL(nFrames), emuR(nFrames);
        std::fread(emuL.data(), sizeof(float), nFrames, f);
        std::fread(emuR.data(), sizeof(float), nFrames, f);
        std::fclose(f);

        // Renderizar exactamente el mismo preset en ABDMS2000::SynthEngine (24000 muestras @ 48 kHz, C4 vel 100)
        const auto &preset = bank.getPreset(progIdx);
        preset.programData.applyToAPVTS(apvts);
        engine.reset();

        std::vector<float> abdL(nFrames, 0.0f), abdR(nFrames, 0.0f);
        juce::AudioBuffer<float> block(2, 256);
        juce::MidiBuffer midiBuf;
        midiBuf.addEvent(juce::MidiMessage::noteOn(1, 60, static_cast<juce::uint8>(100)), 0);
        uint32_t done = 0;
        while (done < nFrames)
        {
            const int chunk = static_cast<int>(std::min<uint32_t>(256, nFrames - done));
            if (noteOffFrame >= 0 && done <= static_cast<uint32_t>(noteOffFrame)
                && static_cast<uint32_t>(noteOffFrame) < done + static_cast<uint32_t>(chunk))
                midiBuf.addEvent(juce::MidiMessage::noteOff(1, 60, static_cast<juce::uint8>(0)),
                                 noteOffFrame - static_cast<int>(done));
            block.setSize(2, chunk, false, true, true);
            block.clear();
            engine.processBlock(block, midiBuf, nullptr);
            midiBuf.clear();
            std::memcpy(abdL.data() + done, block.getReadPointer(0), static_cast<size_t>(chunk) * sizeof(float));
            std::memcpy(abdR.data() + done, block.getReadPointer(1), static_cast<size_t>(chunk) * sizeof(float));
            done += static_cast<uint32_t>(chunk);
        }

        const AcousticProfile pEmu = analyzeStereoBuffer(emuL, emuR);
        const AcousticProfile pAbd = analyzeStereoBuffer(abdL, abdR);

        {
            char abdPath[512];
            std::snprintf(abdPath, sizeof(abdPath), "%s\\abd_preset_%d.bin", scratchDir, progIdx);
            FILE *fa = nullptr;
            fopen_s(&fa, abdPath, "wb");
            if (fa)
            {
                std::fwrite(&nFrames, sizeof(uint32_t), 1, fa);
                std::fwrite(abdL.data(), sizeof(float), nFrames, fa);
                std::fwrite(abdR.data(), sizeof(float), nFrames, fa);
                std::fclose(fa);
            }
        }

        // Similitud coseno de la envolvente espectral en 24 bandas
        double dot = 0.0, normEmu = 0.0, normAbd = 0.0;
        for (int b = 0; b < kNumBands; ++b)
        {
            const double a = std::sqrt(pEmu.bandEnergy[b]);
            const double c = std::sqrt(pAbd.bandEnergy[b]);
            dot += a * c;
            normEmu += a * a;
            normAbd += c * c;
        }
        const double specCosSim =
            (normEmu > 1e-12 && normAbd > 1e-12) ? (100.0 * dot / (std::sqrt(normEmu) * std::sqrt(normAbd))) : 0.0;

        const char bankChar = static_cast<char>('A' + (progIdx / 16));
        const int patchNum = (progIdx % 16) + 1;
        printf("\n  Preset %c%02d (#%02d): \"%s\" | Similitud Espectral = %.1f%%\n",
               bankChar,
               patchNum,
               progIdx,
               preset.name.c_str(),
               specCosSim);
        printf("    ms2000r-emu : RMS=%6.1f dB | Peak=%6.1f dB | Centroid=%5.0f Hz | Bands(B/LM/HM/H)=%4.1f/%4.1f/%4.1f/%4.1f%% | EnvSlope=%+5.1f dB | Width=%5.1f%%\n",
               pEmu.rmsDb,
               pEmu.peakDb,
               pEmu.spectralCentroidHz,
               pEmu.bassPct,
               pEmu.lowMidPct,
               pEmu.highMidPct,
               pEmu.highPct,
               pEmu.envSlopeDb,
               pEmu.stereoWidthPct);
        printf("    ABDMS2000   : RMS=%6.1f dB | Peak=%6.1f dB | Centroid=%5.0f Hz | Bands(B/LM/HM/H)=%4.1f/%4.1f/%4.1f/%4.1f%% | EnvSlope=%+5.1f dB | Width=%5.1f%%\n",
               pAbd.rmsDb,
               pAbd.peakDb,
               pAbd.spectralCentroidHz,
               pAbd.bassPct,
               pAbd.lowMidPct,
               pAbd.highMidPct,
               pAbd.highPct,
               pAbd.envSlopeDb,
               pAbd.stereoWidthPct);
    }
}

}  // namespace Tests
}  // namespace ABDMS2000

#if defined(ABDMS2000_CALIBRATION_STANDALONE_MAIN)
namespace ABDMS2000
{
namespace Tests
{
int testsPassed = 0;
int testsFailed = 0;

void check(bool condition, const char *testName)
{
    if (condition)
    {
        printf("  [PASS] %s\n", testName);
        ++testsPassed;
    }
    else
    {
        printf("  [FAIL] %s\n", testName);
        ++testsFailed;
    }
}
}  // namespace Tests
}  // namespace ABDMS2000

int main(int argc, char **argv)
{
    if (argc >= 3 && std::strcmp(argv[1], "--compare-emu") == 0)
    {
        const int noteOffFrame = (argc >= 4) ? std::atoi(argv[3]) : -1;
        ABDMS2000::Tests::compareIdenticalPresetsWithEmu(argv[2], noteOffFrame);
        return 0;
    }
    ABDMS2000::Tests::testTier2FirmwareBlockCalibration();
    printf("\n=== Calibration Summary ===\n");
    printf("  Passed: %d\n", ABDMS2000::Tests::testsPassed);
    printf("  Failed: %d\n", ABDMS2000::Tests::testsFailed);
    printf("===========================\n");
    return ABDMS2000::Tests::testsFailed > 0 ? 1 : 0;
}
#endif
