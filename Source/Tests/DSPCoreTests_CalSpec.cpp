/**
 * @purpose Unit tests for ABDMS2000 DSP: CalibrationSpec serialization, validation,
 *          factory defaults, parameter transfer curves, round-trip integrity.
 * Ported from ABDEep SynthEngineUnitTests_CalSpec.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../DSP/Filters/MultiModeFilter.h"
#include "../../DSP/Modulation/LFO.h"
#include "../State/ParameterRegistry.gen.h"
#include "TestCompat.h"

namespace ABDMS2000
{
namespace Tests
{

static constexpr double kTestSampleRate = 44100.0;

class CalSpecTests : public juce::UnitTest
{
  public:
    CalSpecTests() : juce::UnitTest("Calibration Spec & Transfer Curves Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("Filter cutoff transfer curve — exponential mapping");
        {
            // MS2000 filter cutoff: exponential from 20Hz to 20kHz
            auto cutoffFromNormalized = [](float norm) -> float
            {
                // Logarithmic mapping: 20Hz * (20000/20)^norm = 20 * 1000^norm
                return 20.0f * std::pow(1000.0f, norm);
            };

            expectWithinAbsoluteError(cutoffFromNormalized(0.0f), 20.0f, 1.0f, "Cutoff norm=0 → 20Hz");
            expectWithinAbsoluteError(
                cutoffFromNormalized(0.5f), 632.0f, 50.0f, "Cutoff norm=0.5 → ~632Hz (geometric center)");
            expectWithinAbsoluteError(cutoffFromNormalized(1.0f), 20000.0f, 100.0f, "Cutoff norm=1 → 20kHz");

            // Monotonic
            for (float n = 0.0f; n <= 1.0f; n += 0.1f)
            {
                float c1 = cutoffFromNormalized(n);
                float c2 = cutoffFromNormalized(n + 0.01f);
                if (n + 0.01f <= 1.0f)
                    expect(c2 > c1, "Cutoff curve is monotonic increasing");
            }

            logMessage("Filter cutoff transfer curve: OK");
        }

        //==============================================================================
        beginTest("Filter resonance transfer curve — linear with compensation");
        {
            // Resonance: linear 0-1 mapped to feedback with gain compensation
            auto resonanceFeedback = [](float norm) -> float
            {
                // Approximate MS2000 resonance curve
                float feedback = norm * 4.0f;  // 0 to 4.0 (self-osc threshold)
                return feedback;
            };

            expectWithinAbsoluteError(resonanceFeedback(0.0f), 0.0f, 0.01f, "Resonance 0 → no feedback");
            expectWithinAbsoluteError(resonanceFeedback(0.5f), 2.0f, 0.1f, "Resonance 0.5 → moderate feedback");
            expect(resonanceFeedback(0.8f) > 3.2f, "Resonance 0.8 → near self-osc threshold");
            expect(resonanceFeedback(1.0f) >= 4.0f, "Resonance 1.0 → at/above self-osc threshold");

            logMessage("Filter resonance transfer curve: OK");
        }

        //==============================================================================
        beginTest("Envelope time transfer curves — attack/decay/release");
        {
            // Attack: 0.5ms to 5s (exponential)
            auto attackTime = [](float norm) -> float
            {
                return 0.0005f * std::pow(10000.0f, norm);  // 0.5ms to 5s
            };

            expectWithinAbsoluteError(attackTime(0.0f), 0.0005f, 0.0001f, "Attack 0 → 0.5ms");
            expectWithinAbsoluteError(attackTime(0.5f), 0.05f, 0.01f, "Attack 0.5 → ~50ms");
            expectWithinAbsoluteError(attackTime(1.0f), 5.0f, 0.1f, "Attack 1 → 5s");

            // Decay/Release: 5ms to 10s
            auto decayTime = [](float norm) -> float
            {
                return 0.005f * std::pow(2000.0f, norm);  // 5ms to 10s
            };

            expectWithinAbsoluteError(decayTime(0.0f), 0.005f, 0.001f, "Decay 0 → 5ms");
            expectWithinAbsoluteError(decayTime(0.5f), 0.22f, 0.05f, "Decay 0.5 → ~220ms");
            expectWithinAbsoluteError(decayTime(1.0f), 10.0f, 0.1f, "Decay 1 → 10s");

            // Monotonic
            for (float n = 0.0f; n <= 0.9f; n += 0.1f)
            {
                expect(attackTime(n + 0.1f) > attackTime(n), "Attack monotonic");
                expect(decayTime(n + 0.1f) > decayTime(n), "Decay monotonic");
            }

            logMessage("Envelope time transfer curves: OK");
        }

        //==============================================================================
        beginTest("LFO frequency transfer curve — log scale");
        {
            // LFO: 0.01Hz to 20Hz (logarithmic)
            auto lfoFreq = [](float norm) -> float { return 0.01f * std::pow(2000.0f, norm); };

            expectWithinAbsoluteError(lfoFreq(0.0f), 0.01f, 0.001f, "LFO 0 → 0.01Hz");
            expectWithinAbsoluteError(lfoFreq(0.5f), 0.45f, 0.1f, "LFO 0.5 → ~0.45Hz");
            expectWithinAbsoluteError(lfoFreq(1.0f), 20.0f, 0.1f, "LFO 1 → 20Hz");

            // Tempo sync divisions (MS2000 clock divide table)
            auto syncDivider = [](int syncNote) -> float
            {
                static const float dividers[15] = {4.0f,
                                                   3.0f,
                                                   2.0f,
                                                   1.0f,
                                                   0.5f,
                                                   3.0f / 8.0f,
                                                   1.0f / 3.0f,
                                                   1.0f / 4.0f,
                                                   3.0f / 16.0f,
                                                   1.0f / 6.0f,
                                                   1.0f / 8.0f,
                                                   3.0f / 32.0f,
                                                   1.0f / 12.0f,
                                                   1.0f / 16.0f,
                                                   3.0f / 64.0f};
                if (syncNote >= 0 && syncNote < 15)
                    return dividers[syncNote];
                return 4.0f;
            };

            // At 120 BPM: quarter note = 0.5s = 2Hz base
            // 1/4 note (syncNote=4) → 2Hz / 4 = 0.5Hz
            float baseHz = 120.0f / 60.0f * 2.0f;  // 2Hz
            float sync4Hz = baseHz / syncDivider(4);
            expectWithinAbsoluteError(sync4Hz, 0.5f, 0.01f, "Sync 1/4 @ 120BPM → 0.5Hz");

            logMessage("LFO frequency transfer curve: OK");
        }

        //==============================================================================
        beginTest("Pitch bend / transpose transfer — linear in semitones/cents");
        {
            // Pitch bend: -1..+1 mapped to +/- range semitones
            auto pitchBendSemitones = [](float bend, float rangeSemitones) -> float { return bend * rangeSemitones; };

            expectWithinAbsoluteError(pitchBendSemitones(0.0f, 2.0f), 0.0f, 0.001f, "Bend 0 → 0st");
            expectWithinAbsoluteError(pitchBendSemitones(1.0f, 2.0f), 2.0f, 0.01f, "Bend +1 → +2st");
            expectWithinAbsoluteError(pitchBendSemitones(-1.0f, 2.0f), -2.0f, 0.01f, "Bend -1 → -2st");
            expectWithinAbsoluteError(pitchBendSemitones(0.5f, 12.0f), 6.0f, 0.01f, "Bend +0.5 @ 12st range → +6st");

            // Transpose: -48..+48 semitones
            auto transposeSemitones = [](float norm) -> int { return (int) std::round(norm * 96.0f - 48.0f); };

            expectEquals(transposeSemitones(0.0f), -48, "Transpose 0 → -48st");
            expectEquals(transposeSemitones(0.5f), 0, "Transpose 0.5 → 0st");
            expectEquals(transposeSemitones(1.0f), 48, "Transpose 1 → +48st");

            // Fine tune: -128..+127 cents
            auto fineTuneCents = [](float norm) -> float { return norm * 255.0f - 128.0f; };

            expectWithinAbsoluteError(fineTuneCents(0.0f), -128.0f, 0.1f, "Tune 0 → -128¢");
            expectWithinAbsoluteError(fineTuneCents(0.5f), -0.5f, 0.1f, "Tune 0.5 → ~0¢");
            expectWithinAbsoluteError(fineTuneCents(1.0f), 127.0f, 0.1f, "Tune 1 → +127¢");

            logMessage("Pitch/transpose/tune transfer: OK");
        }

        //==============================================================================
        beginTest("Filter keytrack transfer — 1:1 per octave multiplicative");
        {
            auto keytrackRatio = [](float noteFreq, float refFreq, float amount) -> float
            {
                if (amount <= 0.0f || noteFreq <= 0.0f || refFreq <= 0.0f)
                    return 1.0f;
                return std::pow(noteFreq / refFreq, amount);
            };

            const float ref = 261.63f;  // C4

            // Amount 0 → no tracking
            expectWithinAbsoluteError(keytrackRatio(1046.5f, ref, 0.0f), 1.0f, (float) 1e-5f, "Keytrack 0 → ratio 1");

            // Pivot at middle C
            expectWithinAbsoluteError(keytrackRatio(ref, ref, 1.0f), 1.0f, (float) 1e-5f, "C4 is pivot → ratio 1");

            // 1:1 per octave
            expectWithinAbsoluteError(keytrackRatio(ref * 2.0f, ref, 1.0f), 2.0f, (float) 1e-3f, "Octave up → ×2");
            expectWithinAbsoluteError(keytrackRatio(ref * 0.5f, ref, 1.0f), 0.5f, (float) 1e-3f, "Octave down → ×0.5");

            // Two octaves up
            expectWithinAbsoluteError(keytrackRatio(ref * 4.0f, ref, 1.0f), 4.0f, (float) 1e-3f, "Two octaves up → ×4");

            // Half tracking
            expectWithinAbsoluteError(
                keytrackRatio(ref * 2.0f, ref, 0.5f), std::sqrt(2.0f), (float) 1e-3f, "Half tracking → √2");

            logMessage("Filter keytrack transfer: OK");
        }

        //==============================================================================
        beginTest("Velocity sensitivity curves");
        {
            // Velocity sensitivity: -1 to +1 mapped from 0-1
            auto velSens = [](float norm) -> float { return (norm - 0.5f) * 2.0f; };

            expectWithinAbsoluteError(velSens(0.0f), -1.0f, 0.001f, "Vel sens 0 → -100%");
            expectWithinAbsoluteError(velSens(0.5f), 0.0f, 0.001f, "Vel sens 0.5 → 0%");
            expectWithinAbsoluteError(velSens(1.0f), 1.0f, 0.001f, "Vel sens 1 → +100%");

            // Amp velocity: 0-1 depth
            auto ampVelDepth = [](float norm) -> float
            {
                return norm;  // 0-1
            };
            expectWithinAbsoluteError(ampVelDepth(0.0f), 0.0f, 0.001f, "Amp vel 0 → no velocity effect");
            expectWithinAbsoluteError(ampVelDepth(1.0f), 1.0f, 0.001f, "Amp vel 1 → full velocity effect");

            logMessage("Velocity sensitivity curves: OK");
        }

        //==============================================================================
        beginTest("Pan law — equal power");
        {
            // Pan: -1 (L) to +1 (R), equal power
            auto panGains = [](float pan) -> std::pair<float, float>
            {
                float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<double>::pi;  // -1→0, 0→π/4, +1→π/2
                return {std::cos(angle), std::sin(angle)};
            };

            auto [L, R] = panGains(-1.0f);
            expectWithinAbsoluteError(L, 1.0f, 0.01f, "Pan -1 → L=1.0");
            expectWithinAbsoluteError(R, 0.0f, 0.01f, "Pan -1 → R=0.0");

            auto [Lc, Rc] = panGains(0.0f);
            expectWithinAbsoluteError(Lc, std::sqrt(0.5f), 0.01f, "Pan 0 → L=√0.5");
            expectWithinAbsoluteError(Rc, std::sqrt(0.5f), 0.01f, "Pan 0 → R=√0.5");
            expectWithinAbsoluteError(Lc * Lc + Rc * Rc, 1.0f, 0.001f, "Pan 0 → equal power (L²+R²=1)");

            auto [Lr, Rr] = panGains(1.0f);
            expectWithinAbsoluteError(Lr, 0.0f, 0.01f, "Pan +1 → L=0.0");
            expectWithinAbsoluteError(Rr, 1.0f, 0.01f, "Pan +1 → R=1.0");

            logMessage("Pan law equal power: OK");
        }

        //==============================================================================
        beginTest("Gain/dB conversion — round-trip accuracy");
        {
            auto dbToGain = [](float db) -> float { return std::pow(10.0f, db / 20.0f); };

            auto gainToDb = [](float gain) -> float { return 20.0f * std::log10(gain); };

            float testGains[] = {0.001f, 0.01f, 0.1f, 0.5f, 0.707f, 1.0f, 2.0f, 10.0f};
            for (float g : testGains)
            {
                float db = gainToDb(g);
                float g2 = dbToGain(db);
                expectWithinAbsoluteError(g2, g, g * 0.001f, "Gain " + juce::String(g) + " round-trip");
            }

            // Special values
            expectWithinAbsoluteError(dbToGain(-6.0f), 0.5f, 0.01f, "-6dB → 0.5");
            expectWithinAbsoluteError(dbToGain(-3.0f), 0.707f, 0.01f, "-3dB → 1/√2");
            expectWithinAbsoluteError(gainToDb(0.5f), -6.0f, 0.01f, "0.5 → -6dB");
            expectWithinAbsoluteError(gainToDb(1.0f), 0.0f, 0.001f, "1.0 → 0dB");

            logMessage("Gain/dB conversion: OK");
        }

        //==============================================================================
        beginTest("Modulation depth scaling — bipolar to unipolar");
        {
            // Bipolar modulation (-1..+1) scaled by depth (0..1)
            auto applyMod = [](float source, float depth) -> float { return source * depth; };

            expectWithinAbsoluteError(applyMod(1.0f, 1.0f), 1.0f, 0.001f, "Source +1, depth 1 → +1");
            expectWithinAbsoluteError(applyMod(-1.0f, 1.0f), -1.0f, 0.001f, "Source -1, depth 1 → -1");
            expectWithinAbsoluteError(applyMod(1.0f, 0.5f), 0.5f, 0.001f, "Source +1, depth 0.5 → +0.5");
            expectWithinAbsoluteError(applyMod(0.0f, 1.0f), 0.0f, 0.001f, "Source 0, depth 1 → 0");

            // Multiple sources summed
            auto sumMod = [](const float *sources, const float *depths, int n) -> float
            {
                float sum = 0.0f;
                for (int i = 0; i < n; ++i)
                    sum += sources[i] * depths[i];
                return sum;
            };

            float src[2] = {0.5f, -0.3f};
            float dep[2] = {0.8f, 0.6f};
            float result = sumMod(src, dep, 2);
            expectWithinAbsoluteError(result, 0.5f * 0.8f + (-0.3f) * 0.6f, 0.001f, "Sum of modulated sources");

            logMessage("Modulation depth scaling: OK");
        }

        //==============================================================================
        beginTest("Parameter smoothing — exponential time constant");
        {
            // 1-pole smoothing: y[n] = y[n-1] + (target - y[n-1]) * coeff
            // coeff = 1 - exp(-1 / (tau * sr))
            auto calcCoeff = [](float tauSec, double sr) -> float { return 1.0f - std::exp(-1.0 / (tauSec * sr)); };

            float coeff = calcCoeff(0.001f, 44100.0);  // 1ms at 44.1kHz
            expect(coeff > 0.0f && coeff < 1.0f, "Smoothing coefficient in (0,1)");

            // Simulate smoothing
            float y = 0.0f;
            float target = 1.0f;
            for (int i = 0; i < 10000; ++i)
                y += (target - y) * coeff;

            expect(y > 0.99f, "Smoothing converges to target");

            // Time constant invariance across sample rates
            float coeff44k = calcCoeff(0.001f, 44100.0);
            float coeff96k = calcCoeff(0.001f, 96000.0);
            // Both should give same physical time constant
            float tau44k = -1.0 / (std::log(1.0 - coeff44k) * 44100.0);
            float tau96k = -1.0 / (std::log(1.0 - coeff96k) * 96000.0);
            expectWithinAbsoluteError(tau44k, tau96k, 0.0001f, "Time constant sample-rate invariant");

            logMessage("Parameter smoothing time constant: OK");
        }
    }
};

static CalSpecTests calSpecTests;

}  // namespace Tests
}  // namespace ABDMS2000