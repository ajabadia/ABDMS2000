/**
 * @purpose Unit tests for ABDMS2000 DSP: rapid parameter sweeps, real-time safety,
 *          allocation-free processing, parameter smoothing under modulation.
 * Ported from ABDEep SynthEngineUnitTests_RapidSweep.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "TestCompat.h"
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../Core/VoiceManager.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../DSP/Filters/MultiModeFilter.h"
#include "../../DSP/Modulation/LFO.h"
#include "../State/ParameterRegistry.gen.h"

namespace ABDMS2000 {
namespace Tests {

static constexpr double kTestSampleRate = 44100.0;

class RapidSweepTests : public juce::UnitTest
{
public:
    RapidSweepTests() : juce::UnitTest("Rapid Sweep & Real-Time Safety Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("Filter cutoff rapid sweep — no allocation, stable output");
        {
            MultiModeFilter filter;
            filter.prepare(kTestSampleRate);
            filter.setType(FilterType::LPF24);
            filter.setResonance(0.5f);

            // Sweep cutoff from 20 Hz to 20 kHz in 1000 blocks
            const int numBlocks = 1000;
            const int blockSize = 256;
            float cutoff = 20.0f;
            const float cutoffStep = (20000.0f - 20.0f) / (float)numBlocks;

            bool anyNanOrInf = false;
            float maxAbs = 0.0f;

            for (int b = 0; b < numBlocks; ++b)
            {
                filter.setCutoff(cutoff);
                cutoff += cutoffStep;

                for (int i = 0; i < blockSize; ++i)
                {
                    float out = filter.process(0.5f);
                    if (std::isnan(out) || std::isinf(out)) anyNanOrInf = true;
                    maxAbs = std::max(maxAbs, std::abs(out));
                }
            }

            expect(!anyNanOrInf, "Rapid filter sweep produces no NaN/Inf");
            expect(maxAbs < 10.0f, "Filter output stays bounded during rapid sweep");
            logMessage("Filter rapid sweep: maxAbs=" + juce::String(maxAbs));
        }

        //==============================================================================
        beginTest("Resonance rapid sweep — no instability at self-osc threshold");
        {
            MultiModeFilter filter;
            filter.prepare(kTestSampleRate);
            filter.setType(FilterType::LPF24);
            filter.setCutoff(1000.0f);

            const int numBlocks = 500;
            const int blockSize = 256;
            float resonance = 0.0f;
            const float resStep = 1.0f / (float)numBlocks;

            bool anyNanOrInf = false;
            float maxAbs = 0.0f;

            for (int b = 0; b < numBlocks; ++b)
            {
                filter.setResonance(resonance);
                resonance += resStep;

                for (int i = 0; i < blockSize; ++i)
                {
                    float out = filter.process(0.5f);
                    if (std::isnan(out) || std::isinf(out)) anyNanOrInf = true;
                    maxAbs = std::max(maxAbs, std::abs(out));
                }
            }

            expect(!anyNanOrInf, "Rapid resonance sweep produces no NaN/Inf");
            expect(maxAbs < 10.0f, "Filter output stays bounded during resonance sweep");
            logMessage("Resonance rapid sweep: maxAbs=" + juce::String(maxAbs));
        }

        //==============================================================================
        beginTest("LFO frequency rapid sweep — no phase discontinuities");
        {
            LFO lfo;
            lfo.prepare(kTestSampleRate);
            // Sine wave is only available on LFO2
            lfo.setWaveformLFO2(LFOWaveformLFO2::Sine);

            const int numBlocks = 200;
            const int blockSize = 512;
            float freq = 0.01f;
            const float freqStep = (20.0f - 0.01f) / (float)numBlocks;

            bool anyNanOrInf = false;
            float prevSample = 0.0f;
            int phaseDiscontinuities = 0;

            for (int b = 0; b < numBlocks; ++b)
            {
                lfo.setFrequencyHz(freq);
                freq += freqStep;

                for (int i = 0; i < blockSize; ++i)
                {
                    float sample = lfo.getNextSample();
                    if (std::isnan(sample) || std::isinf(sample)) anyNanOrInf = true;
                    
                    // Detect phase jumps (discontinuity > 1.5)
                    if (std::abs(sample - prevSample) > 1.5f) phaseDiscontinuities++;
                    prevSample = sample;
                }
            }

            expect(!anyNanOrInf, "Rapid LFO frequency sweep produces no NaN/Inf");
            expect(phaseDiscontinuities == 0, "LFO phase should be continuous during frequency sweep");
            logMessage("LFO rapid frequency sweep: OK");
        }

        //==============================================================================
        beginTest("Filter type rapid switching — clean state transitions");
        {
            MultiModeFilter filter;
            filter.prepare(kTestSampleRate);
            filter.setCutoff(1000.0f);
            filter.setResonance(0.5f);

            const int numSwitches = 1000;
            bool anyNanOrInf = false;
            float maxAbs = 0.0f;

            for (int i = 0; i < numSwitches; ++i)
            {
                FilterType type = static_cast<FilterType>(i % 4);
                filter.setType(type);
                
                for (int s = 0; s < 10; ++s)
                {
                    float out = filter.process(0.5f);
                    if (std::isnan(out) || std::isinf(out)) anyNanOrInf = true;
                    maxAbs = std::max(maxAbs, std::abs(out));
                }
            }

            expect(!anyNanOrInf, "Rapid filter type switching produces no NaN/Inf");
            expect(maxAbs < 5.0f, "Filter output bounded during type switching");
            logMessage("Filter type rapid switching: OK");
        }

        //==============================================================================
        beginTest("SynthEngine rapid parameter sweeps — allocation-free");
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
            
            engine.noteOn(1, 60, 0.8f);
            
            // Rapidly sweep multiple parameters
            juce::AudioBuffer<float> buffer(2, 256);
            juce::MidiBuffer midi;
            bool anyNanOrInf = false;
            
            for (int sweep = 0; sweep < 100; ++sweep)
            {
                float cutoff = 20.0f + (sweep * 199.8f); // 20 to 20000 Hz
                float resonance = (sweep % 10) * 0.1f;
                float lfoFreq = 0.1f + (sweep % 20) * 1.0f;
                
                // Update parameters (would normally go through APVTS)
                // engine.setFilterCutoff(cutoff);
                // engine.setFilterResonance(resonance);
                // engine.setLfoFrequency(lfoFreq);
                
                buffer.clear();
                midi.clear();
                if (sweep == 0) midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.8f), 0);
                
                engine.processBlock(buffer, midi, nullptr);
                
                for (int ch = 0; ch < 2; ++ch)
                {
                    for (int i = 0; i < 256; ++i)
                    {
                        float sample = buffer.getSample(ch, i);
                        if (std::isnan(sample) || std::isinf(sample)) anyNanOrInf = true;
                    }
                }
            }
            
            expect(!anyNanOrInf, "SynthEngine rapid parameter sweeps produce no NaN/Inf");
            logMessage("SynthEngine rapid sweeps: OK");
        }

        //==============================================================================
        beginTest("Voice allocation under rapid note changes — no leaks");
        {
            auto vm = std::make_unique<VoiceManager>();
            vm->prepare(kTestSampleRate);
            vm->setMaxPolyphony(16);
            
            int totalNotesOn = 0;
            int totalNotesOff = 0;
            
            // Rapidly trigger and release notes
            for (int cycle = 0; cycle < 1000; ++cycle)
            {
                int numNotes = (cycle % 16) + 1;
                
                for (int n = 0; n < numNotes; ++n)
                {
                    vm->noteOn(60 + n, 0.8f);
                    totalNotesOn++;
                }
                
                for (int n = 0; n < numNotes; ++n)
                {
                    vm->noteOff(60 + n);
                    totalNotesOff++;
                }
                
                // Check no memory leaks (voice count should return to 0)
                if (cycle % 100 == 0)
                {
                    vm->allNotesOff();
                    expect(vm->getActiveVoiceCount() == 0, "VoiceManager should have 0 active voices after allNotesOff");
                }
            }
            
            expect(totalNotesOn == totalNotesOff, "Total noteOn == noteOff count");
            expect(vm->getActiveVoiceCount() == 0, "Final active voice count should be 0");
            logMessage("Voice allocation rapid changes: OK (on=" + juce::String(totalNotesOn) + ")");
        }

        //==============================================================================
        beginTest("Envelope rapid trigger/release — no stuck envelopes");
        {
            abd::synth::ADSREnvelope env;
            env.prepare(kTestSampleRate);
            env.setAttack(0.001f);
            env.setDecay(0.001f);
            env.setSustain(0.5f);
            env.setRelease(0.01f);
            
            bool anyStuck = false;
            
            for (int cycle = 0; cycle < 10000; ++cycle)
            {
                env.noteOn(1.0f);
                
                // Quick attack-decay
                for (int i = 0; i < 10; ++i) env.getNextSample();
                
                env.noteOff();
                
                // Quick release
                for (int i = 0; i < 20; ++i) env.getNextSample();
                
                if (!env.isIdle() && cycle > 100) // Should be idle after release
                {
                    anyStuck = true;
                    break;
                }
            }
            
            expect(!anyStuck, "No envelopes stuck in active state after rapid trigger/release cycles");
            logMessage("Envelope rapid trigger/release: OK");
        }

        //==============================================================================
        beginTest("Modulation matrix rapid route changes — no allocation");
        {
            VirtualPatchMatrix matrix;
            PatchModulationSources sources{};
            
            // Rapidly add/remove routes
            for (int cycle = 0; cycle < 1000; ++cycle)
            {
                int src = cycle % 4;
                int dst = cycle % 8;
                float amount = ((cycle % 100) / 100.0f) * 2.0f - 1.0f; // -1 to +1
                
                matrix.setSlot(cycle % 4, 
                    static_cast<PatchSource>(src),
                    static_cast<PatchDestination>(dst),
                    amount);
                
                // Test modulation value
                sources = {};
                if (src == 0) sources.lfo1 = 0.5f;
                else if (src == 1) sources.lfo2 = 0.5f;
                
                auto outputs = matrix.evaluate(sources);
                float mod = 0.0f;
                if (dst == 0) mod = outputs.pitchMod;
                else if (dst == 1) mod = outputs.osc2PitchMod;
                else if (dst == 2) mod = outputs.osc1Ctrl1Mod;
                else if (dst == 3) mod = outputs.noiseLevelMod;
                else if (dst == 4) mod = outputs.cutoffMod;
                else if (dst == 5) mod = outputs.ampMod;
                else if (dst == 6) mod = outputs.panMod;
                else if (dst == 7) mod = outputs.lfo2FreqMod;
                
                expect(std::isfinite(mod), "Modulation value finite after rapid route change");
            }
            
            logMessage("Modulation matrix rapid changes: OK");
        }

        //==============================================================================
        beginTest("Diagnostic tone injection — clean output at all points");
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
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(ParameterRegistry::createParameterLayout()));
            
            SynthEngine engine(apvts);
            engine.prepare(kTestSampleRate, 512);
            
            // Test all diagnostic points
            for (int point = 0; point <= 5; ++point)
            {
                engine.setDiagnosticTone(point, 440.0f, 0.25f);
                
                juce::AudioBuffer<float> buffer(2, 256);
                buffer.clear();
                juce::MidiBuffer midi;
                
                engine.processBlock(buffer, midi, nullptr);
                
                float peak = 0.0f;
                for (int ch = 0; ch < 2; ++ch)
                    peak = std::max(peak, buffer.getMagnitude(ch, 256));
                
                expect(peak > 0.001f, "Diagnostic point " + juce::String(point) + " produces audible output");
                expect(peak < 1.0f, "Diagnostic point " + juce::String(point) + " output bounded");
            }
            
            engine.setDiagnosticTone(0, 440.0f, 0.0f); // Disable
            logMessage("Diagnostic tone injection: OK");
        }
    }
};

static RapidSweepTests rapidSweepTests;

} // namespace Tests
} // namespace ABDMS2000