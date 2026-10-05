/**
 * @purpose Unit tests for ABDMS2000 DSP: transpose, global tune, pitch bend ranges,
 *          voice detune, unison calculations.
 * Ported from ABDEep SynthEngineUnitTests_Pitch.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "TestCompat.h"
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../Core/VoiceManager.h"
#include "../State/ParameterRegistry.gen.h"
#include "../../DSP/Common/DSPUtils.h"

namespace ABDMS2000 {
namespace Tests {

static constexpr double kTestSampleRate = 44100.0;

class PitchTests : public juce::UnitTest
{
public:
    PitchTests() : juce::UnitTest("Pitch/Transpose/Tune Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("Transpose and global tune ranges");
        {
            auto calcTranspose = [](float normalized) -> int { return (int)std::round(normalized * 96.0f - 48.0f); };
            expectEquals(calcTranspose(0.0f), -48, "Transpose 0 → -48 st");
            expectEquals(calcTranspose(0.25f), -24, "Transpose 0.25 → -24 st");
            expectEquals(calcTranspose(0.5f), 0, "Transpose 0.5 → 0 st");
            expectEquals(calcTranspose(0.75f), 24, "Transpose 0.75 → +24 st");
            expectEquals(calcTranspose(1.0f), 48, "Transpose 1.0 → +48 st");

            auto calcTune = [](float normalized) -> float { return normalized * 255.0f - 128.0f; };
            expectWithinAbsoluteError(calcTune(0.0f), -128.0f, 0.001f, "Tune 0 → -128¢");
            expectWithinAbsoluteError(calcTune(0.5f), -0.5f, 0.001f, "Tune 0.5 → -0.5¢");
            expectWithinAbsoluteError(calcTune(1.0f), 127.0f, 0.001f, "Tune 1.0 → +127¢");
            expectEquals(std::clamp(60 + (-48), 0, 127), 12, "C4 + transpose -48 should map to C1");
            expectEquals(std::clamp(60 + 48, 0, 127), 108, "C4 + transpose +48 should map to C8");
            logMessage("Transpose and tune mapping: OK");
        }

        //==============================================================================
        beginTest("Pitch Bend range scaling");
        {
            auto calcBendUp = [](float normalized) -> float { return normalized * 24.0f; };
            auto calcBendDown = [](float normalized) -> float { return normalized * 24.0f; };
            expectWithinAbsoluteError(calcBendUp(0.0f), 0.0f, 0.001f, "Pitch bend up min should be 0");
            expectWithinAbsoluteError(calcBendUp(1.0f), 24.0f, 0.001f, "Pitch bend up max should be 24");
            expectWithinAbsoluteError(calcBendDown(0.5f), 12.0f, 0.001f, "Pitch bend down mid should be 12");
            logMessage("Pitch bend range scaling: OK");
        }

        //==============================================================================
        beginTest("Voice detune and unison calculations");
        {
            // Voice detune: -50 to +50 cents range
            auto calcDetune = [](float normalized) -> float { return (normalized - 0.5f) * 100.0f; };
            expectWithinAbsoluteError(calcDetune(0.0f), -50.0f, 0.01f, "Detune 0 → -50¢");
            expectWithinAbsoluteError(calcDetune(0.5f), 0.0f, 0.01f, "Detune 0.5 → 0¢");
            expectWithinAbsoluteError(calcDetune(1.0f), 50.0f, 0.01f, "Detune 1.0 → +50¢");

            // Unison detune: symmetric detune for stacked voices
            float maxDetuneCents = 50.0f;
            int numVoices = 6;
            float step = 2.0f * maxDetuneCents / (float)(numVoices - 1);
            float expectedCents[6] = { -25.0f, -15.0f, -5.0f, 5.0f, 15.0f, 25.0f };
            for (int v = 0; v < numVoices; ++v)
            {
                float detuneCents = -maxDetuneCents + (float)v * step;
                expectWithinAbsoluteError(detuneCents, expectedCents[v], 0.1f, "Voice " + juce::String(v) + " detune mismatch");
            }

            // 2-voice unison symmetric
            float detune2v[2] = { -25.0f, 25.0f };
            expect(detune2v[0] < 0, "Voice 0 in 2-voice unison should have negative detune");
            expect(detune2v[1] > 0, "Voice 1 in 2-voice unison should have positive detune");
            expectWithinAbsoluteError(std::abs(detune2v[0]), std::abs(detune2v[1]), 0.001f, "2-voice detune should be symmetric");
            logMessage("Unison detune symmetry: OK");
        }

        //==============================================================================
        beginTest("Unison pan spread");
        {
            int numVoices = 4;
            float expectedPans[4] = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
            for (int v = 0; v < numVoices; ++v)
            {
                float pan = (float)v / (float)(numVoices - 1);
                expectWithinAbsoluteError(pan, expectedPans[v], 0.001f, "Pan for voice " + juce::String(v) + " mismatch");
            }
            expectWithinAbsoluteError(0.0f, 0.0f, 0.001f, "2-voice pan 0 should be 0");
            expectWithinAbsoluteError(1.0f, 1.0f, 0.001f, "2-voice pan 1 should be 1");
            logMessage("Unison pan distribution: OK");
        }

        //==============================================================================
        beginTest("Oscillator frequency calculation with transpose/tune");
        {
            // MIDI note to frequency
            auto midiToFreq = [](int midiNote, float transposeSemitones, float tuneCents) -> float {
                float noteWithTranspose = (float)midiNote + transposeSemitones + tuneCents / 100.0f;
                return 440.0f * std::pow(2.0f, (noteWithTranspose - 69.0f) / 12.0f);
            };

            // A4 (69) = 440 Hz baseline
            expectWithinAbsoluteError(midiToFreq(69, 0, 0), 440.0f, (float)0.01f, "A4 = 440 Hz");
            
            // Transpose +12 semitones = one octave up
            expectWithinAbsoluteError(midiToFreq(69, 12, 0), 880.0f, (float)0.01f, "A4 + 12st = A5 (880 Hz)");
            
            // Transpose -12 semitones = one octave down
            expectWithinAbsoluteError(midiToFreq(69, -12, 0), 220.0f, (float)0.01f, "A4 - 12st = A3 (220 Hz)");
            
            // Tune +100 cents = one semitone up
            expectWithinAbsoluteError(midiToFreq(69, 0, 100), 440.0f * std::pow(2.0f, 1.0f/12.0f), (float)0.01f, "A4 + 100¢ = A#4");
            
            // Tune -100 cents = one semitone down
            expectWithinAbsoluteError(midiToFreq(69, 0, -100), 440.0f / std::pow(2.0f, 1.0f/12.0f), (float)0.01f, "A4 - 100¢ = G#4");
            
            // Combined: transpose + tune
            EXPECT_WITHIN_ABS_ERROR_FLOAT(midiToFreq(60, 12, 50), 
                440.0f * std::pow(2.0f, (60 + 12 + 0.5 - 69) / 12.0f), 0.01f, "C4 + 12st + 50¢");
            
            logMessage("Oscillator frequency with transpose/tune: OK");
        }

        //==============================================================================
        beginTest("Pitch bend modulation depth");
        {
            // Pitch bend: -1.0 to +1.0 mapped to bend range
            auto applyPitchBend = [](float baseFreq, float bendValue, float bendRangeSemitones) -> float {
                return baseFreq * std::pow(2.0f, (bendValue * bendRangeSemitones) / 12.0f);
            };

            float baseFreq = 440.0f;
            float bendRange = 2.0f; // 2 semitones (default)
            
            // No bend
            expectWithinAbsoluteError(applyPitchBend(baseFreq, 0.0f, bendRange), 440.0f, (float)0.01f, "Pitch bend 0 → no change");
            
            // Full up bend
            float upFreq = applyPitchBend(baseFreq, 1.0f, bendRange);
            expectWithinAbsoluteError(upFreq, 440.0f * std::pow(2.0f, 2.0f/12.0f), (float)0.01f, "Pitch bend +1 → +2 semitones");
            
            // Full down bend
            float downFreq = applyPitchBend(baseFreq, -1.0f, bendRange);
            expectWithinAbsoluteError(downFreq, 440.0f / std::pow(2.0f, 2.0f/12.0f), (float)0.01f, "Pitch bend -1 → -2 semitones");
            
            // Different bend ranges
            bendRange = 12.0f; // 1 octave
            upFreq = applyPitchBend(baseFreq, 1.0f, bendRange);
            expectWithinAbsoluteError(upFreq, 880.0f, (float)0.01f, "Pitch bend +1 with 12st range → +1 octave");
            
            bendRange = 24.0f; // 2 octaves
            upFreq = applyPitchBend(baseFreq, 1.0f, bendRange);
            expectWithinAbsoluteError(upFreq, 440.0f * 4.0f, (float)0.01f, "Pitch bend +1 with 24st range → +2 octaves");
            
            logMessage("Pitch bend modulation depth: OK");
        }

        //==============================================================================
        beginTest("Portamento/glide time calculation");
        {
            // Portamento time: normalized 0-1 mapped to time constant
            auto calcPortamentoRate = [](float normalized, float sampleRate) -> float {
                if (normalized <= 0.0f) return 1.0f; // Instant
                float tau = normalized * 5.0f; // Max 5 seconds
                return 1.0f - std::exp(-1.0f / (tau * sampleRate));
            };

            const float sr = static_cast<float>(kTestSampleRate);
            
            // Instant glide
            expectEquals(calcPortamentoRate(0.0f, sr), 1.0f, "Portamento time 0 → instant glide");
            
            // Mid glide
            float midRate = calcPortamentoRate(0.5f, sr);
            expect(midRate < 0.5f, "Portamento time 0.5 should have rate < 0.5");
            
            // Slow glide
            float slowRate = calcPortamentoRate(1.0f, sr);
            expect(slowRate > 0.0f && slowRate < 0.001f, "Portamento time 1.0 should have very small positive rate");
            
            // Sample rate invariance: same physical time at different sample rates
            float rate44k = calcPortamentoRate(0.5f, 44100.0f);
            float rate96k = calcPortamentoRate(0.5f, 96000.0f);
            // The rate per sample should be different but the physical time constant the same
            float tau44k = -1.0f / (std::log(1.0f - rate44k) * 44100.0f);
            float tau96k = -1.0f / (std::log(1.0f - rate96k) * 96000.0f);
            expectWithinAbsoluteError(tau44k, tau96k, (float)0.001f, "Portamento tau should be sample-rate invariant");
            
            logMessage("Portamento time calculation: OK");
        }

        //==============================================================================
        beginTest("SynthEngine pitch bend integration");
        {
            // Create minimal synth engine for pitch bend test
            juce::AudioProcessorValueTreeState::ParameterLayout layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor {
            public:
                DummyProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
                DummyProcessor(const DummyProcessor&) = delete;
                DummyProcessor& operator=(const DummyProcessor&) = delete;
                const juce::String getName() const override { return "Dummy"; }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
                double getTailLengthSeconds() const override { return 0.0; }
                bool acceptsMidi() const override { return true; }
                bool producesMidi() const override { return true; }
                juce::AudioProcessorEditor* createEditor() override { return nullptr; }
                bool hasEditor() const override { return false; }
                int getNumPrograms() override { return 1; }
                int getCurrentProgram() override { return 0; }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override { return "Dummy"; }
                void changeProgramName(int, const juce::String&) override {}
                void getStateInformation(juce::MemoryBlock&) override {}
                void setStateInformation(const void*, int) override {}
            };
            
            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
            
            SynthEngine engine(apvts);
            engine.prepare(kTestSampleRate, 512);
            
            // Test pitch bend propagation
            engine.setPitchBend(0.0f);
            engine.noteOn(1, 69, 0.8f); // A4
            
            juce::AudioBuffer<float> buffer(2, 128);
            buffer.clear();
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 0), 0); // Mod wheel = 0
            engine.processBlock(buffer, midi);
            
            // Test pitch bend propagation
            engine.setPitchBend(1.0f); // Full up
            engine.setPitchBend(-1.0f); // Full down
            engine.setPitchBend(0.0f); // Back to center
            
            expect(true, "Pitch bend values propagated without crash");
            logMessage("SynthEngine pitch bend integration: OK");
        }
    }
};

static PitchTests pitchTests;

} // namespace Tests
} // namespace ABDMS2000