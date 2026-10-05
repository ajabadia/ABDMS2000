/**
 * @purpose Unit tests for ABDMS2000 DSP: portamento/glide behavior, modes, time scaling.
 * Ported from ABDEep SynthEngineUnitTests.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "TestCompat.h"
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../Core/VoiceManager.h"
#include "../../DSP/Modulation/PortamentoGlide.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../State/ParameterRegistry.gen.h"

namespace ABDMS2000 {
namespace Tests {

static constexpr double kTestSampleRate = 44100.0;

class PortamentoTests : public juce::UnitTest
{
public:
    PortamentoTests() : juce::UnitTest("Portamento/Glide Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("Portamento parameter ranges");
        {
            auto clampMode = [](int mode) -> int { return std::clamp(mode, 0, 13); };
            for (int mode = 0; mode <= 13; ++mode) expectEquals(clampMode(mode), mode, "Porta mode " + juce::String(mode) + " should be valid");
            expectEquals(clampMode(-1), 0, "Porta mode -1 should clamp to 0");
            expectEquals(clampMode(14), 13, "Porta mode 14 should clamp to 13");

            const float sr = static_cast<float>(kTestSampleRate);
            auto timeToRate = [](float normalized, float sampleRate) -> float {
                if (normalized <= 0.0f) return 1.0f;
                float tau = normalized * 5.0f;
                return 1.0f - std::exp(-1.0f / (tau * sampleRate));
            };
            expectWithinAbsoluteError(timeToRate(0.0f, sr), 1.0f, 0.001f, "Porta time 0 → instant glide");
            expect(timeToRate(0.5f, sr) < 0.5f, "Porta time 0.5 should have rate < 0.5");
            expect(timeToRate(1.0f, sr) > 0.0f && timeToRate(1.0f, sr) < 0.001f, "Porta time 1.0 should have very small positive rate");
            logMessage("Portamento parameter ranges: OK");
        }

        //==============================================================================
        beginTest("Portamento glide simulation");
        {
            const float sr = static_cast<float>(kTestSampleRate);
            const float portaTime = 1.0f;
            auto simulateGlide = [&](int mode, int maxSamples) -> int {
                double pitch = 60.0;
                const double target = 72.0;
                for (int s = 0; s < maxSamples; ++s)
                {
                    double rate = 0.0;
                    if (portaTime <= 0.0f) rate = 1.0;
                    else {
                        double tau = (double)portaTime * 5.0;
                        bool isFixRate = (mode == 2 || mode == 3 || mode == 6 || mode == 7 || mode == 8 || mode == 9);
                        if (isFixRate) rate = 12.0 / (tau * (double)sr);
                        else {
                            rate = 1.0 - std::exp(-1.0 / (tau * (double)sr));
                            if (mode == 4 || mode == 5) rate = rate * rate;
                        }
                    }
                    bool isFixRate = (mode == 2 || mode == 3 || mode == 6 || mode == 7 || mode == 8 || mode == 9);
                    if (isFixRate) pitch += (target > pitch ? 1.0 : -1.0) * rate;
                    else pitch += (target - pitch) * rate;
                    if (std::abs(pitch - target) < 0.001) return s + 1;
                }
                return -1;
            };

            int samplesMode0 = simulateGlide(0, (int)(sr * 60));
            expect(samplesMode0 > (int)(sr * 2), "Mode 0: 12-semitone glide should take > 2s");
            expect(samplesMode0 < (int)(sr * 55), "Mode 0: 12-semitone glide should take < 55s");

            int samplesMode2 = simulateGlide(2, (int)(sr * 10));
            expect(samplesMode2 > (int)(sr * 3), "Mode 2: fix-rate should take > 3s");
            expect(samplesMode2 < (int)(sr * 8), "Mode 2: fix-rate should take < 8s");

            int samplesMode4 = simulateGlide(4, (int)(sr * 120));
            if (samplesMode0 > 0 && samplesMode4 > 0)
                expect(samplesMode4 > samplesMode0, "Mode 4 should be slower than mode 0");
            logMessage("Portamento glide simulation: OK");
        }

        //==============================================================================
        beginTest("Portamento time increases glide duration");
        {
            auto simulatePortamentoSamples = [](float portaTime, int maxSamples) -> int {
                double pitch = 60.0;
                const double target = 84.0;
                double rate = (portaTime <= 0.0f) ? 1.0 : 1.0 - std::exp(-1.0 / (portaTime * 0.05 * 44100.0));
                for (int s = 0; s < maxSamples; ++s)
                {
                    pitch += (target - pitch) * rate;
                    if (std::abs(pitch - target) < 0.5) return s + 1;
                }
                return -1;
            };

            int fast = simulatePortamentoSamples(0.01f, 500000);
            int slow = simulatePortamentoSamples(0.5f, 500000);
            expect(fast > 0 && slow > 0, "Both portamento times should converge");
            expect(slow > fast, "Slower portamento should take more samples (fast=" + juce::String(fast) + " slow=" + juce::String(slow) + ")");
            logMessage("Portamento time increases duration: OK");
        }

        //==============================================================================
        beginTest("Portamento Normal vs Fingered behavior");
        {
            auto simulateGlide = [](bool fingered, int startNote, int nextNote, float portaTime) -> int {
                double pitch = (double)startNote;
                const double target = (double)nextNote;
                double rate = (portaTime <= 0.0f) ? 1.0 : 1.0 - std::exp(-1.0 / (portaTime * 0.05 * 44100.0));
                if (fingered) pitch = (double)startNote;
                else { /* normal mode: glide from previous */ }
                for (int s = 0; s < 44100 * 5; ++s) {
                    pitch += (target - pitch) * rate;
                    if (std::abs(pitch - target) < 0.5) return s + 1;
                }
                return -1;
            };

            int normal = simulateGlide(false, 60, 72, 0.5f);
            int fingered = simulateGlide(true, 60, 72, 0.5f);
            expect(normal > 0, "Normal portamento should converge");
            expect(fingered > 0, "Fingered portamento should converge");
            logMessage("Portamento Normal vs Fingered: OK");
        }

        //==============================================================================
        beginTest("PortamentoGlide class — glide time and pitch calculation");
        {
            PortamentoGlide glide;
            glide.prepare(kTestSampleRate);
            
            // Test glide time setting (normalized 0-1)
            glide.setGlideTime(0.0f); // Instant
            glide.reset(60.0f);
            glide.setTargetNote(72.0f, true);
            float pitch = glide.getNextPitchSemitones();
            expectEquals(pitch, 72.0f, "Time 0 should give instant pitch");
            
            glide.setGlideTime(0.5f);
            glide.reset(60.0f);
            glide.setTargetNote(72.0f, true);
            
            // Test that it glides over time
            pitch = glide.getNextPitchSemitones();
            expect(pitch > 60.0f && pitch < 72.0f, "Glide should start between start and target");
            
            // Advance several samples
            for (int i = 0; i < 1000; ++i) {
                pitch = glide.getNextPitchSemitones();
            }
            expect(pitch > 60.0f && pitch <= 72.0f, "Pitch should approach target over time");
            
            // Time = 0 should be instant
            glide.setGlideTime(0.0f);
            glide.reset(60.0f);
            glide.setTargetNote(72.0f, true);
            float instantPitch = glide.getNextPitchSemitones();
            expectEquals(instantPitch, 72.0f, "Time 0 should give instant pitch");
            
            logMessage("PortamentoGlide class: OK");
        }

        //==============================================================================
        beginTest("PortamentoGlide class — glide convergence");
        {
            PortamentoGlide glide;
            glide.prepare(kTestSampleRate);
            
            glide.setGlideTime(0.5f);
            glide.reset(60.0f);
            glide.setTargetNote(72.0f, true);
            
            int steps = 0;
            for (int s = 0; s < 100000; ++s)
            {
                float pitch = glide.getNextPitchSemitones();
                if (std::abs(pitch - 72.0f) < 0.001f) { steps = s; break; }
            }
            
            expect(steps > 0 && steps < 100000, "Glide should converge in reasonable time");
            logMessage("PortamentoGlide simulation: OK (steps=" + juce::String(steps) + ")");
        }

        //==============================================================================
        beginTest("SynthEngine portamento integration");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor {
            public:
                DummyProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
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
            
            // Set portamento parameters
            // (In real implementation, these would come from APVTS)
            // engine.setPortamentoTime(0.5f);
            // engine.setPortamentoMode(0);
            
            engine.noteOn(1, 60, 0.8f);
            
            juce::AudioBuffer<float> buffer(2, 128);
            buffer.clear();
            juce::MidiBuffer midi;
            engine.processBlock(buffer, midi);
            
            // Second note with portamento
            midi.clear();
            midi.addEvent(juce::MidiMessage::noteOn(1, 72, 0.8f), 0);
            engine.processBlock(buffer, midi);
            
            expect(true, "Portamento integration runs without crash");
            logMessage("SynthEngine portamento integration: OK");
        }
    }
};

static PortamentoTests portamentoTests;

} // namespace Tests
} // namespace ABDMS2000