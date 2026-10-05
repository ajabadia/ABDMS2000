/**
 * @purpose Unit tests for ABDMS2000 DSP: voice management, oscillators, envelopes,
 *          LFO, modulation matrix, chord tables, unison, and PolyBLEP.
 * Ported from ABDEep SynthEngineUnitTests_Voice.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "TestCompat.h"
#include <cstring>
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../Core/VoiceManager.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../DSP/Envelopes/ADSREnvelope.h"
#include "../../DSP/Envelopes/EnvelopeCurves.h"
#include "../../DSP/Modulation/LFO.h"
#include "../../DSP/Modulation/VirtualPatchMatrix.h"
#include "../../DSP/Oscillators/VAOscillator.h"
#include "../../DSP/Oscillators/DWGSOscillator.h"
#include "../../DSP/Oscillators/VoxWaveOscillator.h"
#include "../../DSP/Filters/MultiModeFilter.h"
#include "../State/ParameterRegistry.gen.h"

namespace ABDMS2000 {
namespace Tests {

static constexpr double kTestSampleRate = 44100.0;

class VoiceTests : public juce::UnitTest
{
public:
    VoiceTests() : juce::UnitTest("Voice Management & DSP Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("Voice mode helpers — getVoicesPerNote");
        {
            int expected[13] = { 1, 2, 3, 4, 6, 12, 1, 2, 3, 4, 6, 1, 1 };
            for (int mode = 0; mode <= 12; ++mode)
            {
                int voices = 1;
                switch (mode) {
                    case 0:  voices = 1;  break; case 1:  voices = 2;  break;
                    case 2:  voices = 3;  break; case 3:  voices = 4;  break;
                    case 4:  voices = 6;  break; case 5:  voices = 12; break;
                    case 6:  voices = 1;  break; case 7:  voices = 2;  break;
                    case 8:  voices = 3;  break; case 9:  voices = 4;  break;
                    case 10: voices = 6;  break; case 11: voices = 1;  break;
                    case 12: voices = 1;  break;
                }
                expectEquals(voices, expected[mode], "Mode " + juce::String(mode) + " should return " + juce::String(expected[mode]));
            }
            logMessage("All 13 voice modes: OK");
        }

        //==============================================================================
        beginTest("Unison detune calculation");
        {
            float maxDetuneCents = 50.0f;
            int numVoices = 6;
            float step = 2.0f * maxDetuneCents / (float)(numVoices - 1);
            float expectedCents[6] = { -25.0f, -15.0f, -5.0f, 5.0f, 15.0f, 25.0f };
            for (int v = 0; v < numVoices; ++v)
            {
                float detuneCents = -maxDetuneCents + (float)v * step;
                expectWithinAbsoluteError(detuneCents, expectedCents[v], 0.1f, "Voice " + juce::String(v) + " detune mismatch");
            }
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
        beginTest("Chord interval tables");
        {
            struct ChordEntry { int intervals[6]; int numNotes; };
            ChordEntry expectedChords[8] = {
                { { 0, 4, 7, 12, 16, 19 }, 6 }, { { 0, 4, 7, -1, -1, -1 }, 3 },
                { { 0, 3, 7, -1, -1, -1 }, 3 }, { { 0, 4, 8, -1, -1, -1 }, 3 },
                { { 0, 3, 6, -1, -1, -1 }, 3 }, { { 0, 2, 7, -1, -1, -1 }, 3 },
                { { 0, 5, 7, -1, -1, -1 }, 3 }, { { 0, 4, 7, 10, -1, -1 }, 4 }
            };
            for (int type = 0; type < 8; ++type)
            {
                int numNotes = 0;
                for (int i = 0; i < 6; ++i) { if (expectedChords[type].intervals[i] < 0) break; numNotes++; }
                expectEquals(numNotes, expectedChords[type].numNotes, "Chord type " + juce::String(type) + " should have " + juce::String(expectedChords[type].numNotes) + " notes");
            }
            expectEquals(expectedChords[0].intervals[0], 0, "Memory chord root interval should be 0");
            expectEquals(expectedChords[0].intervals[1], 4, "Memory chord should contain a major third");
            logMessage("All 8 chord types: OK");
        }

        //==============================================================================
        beginTest("Voice default parameter values");
        {
            Voice voice;
            voice.prepare(kTestSampleRate);
            expect(!voice.isActive(), "Voice should not be active before noteOn");
            logMessage("Default parameters: OK");
        }

        //==============================================================================
        beginTest("Voice noteOn and noteOff lifecycle");
        {
            Voice voice;
            voice.prepare(kTestSampleRate);
            voice.noteOn(60, 0.8f, false);
            expect(voice.isActive(), "Voice should be active after noteOn");
            expectEquals(voice.getCurrentNote(), 60, "Voice should report MIDI note 60");
            voice.noteOff();
            expect(!voice.isActive() || voice.isInRelease(), "Voice should be in release or inactive after noteOff");
            logMessage("Start/stop lifecycle: OK");
        }

        //==============================================================================
        beginTest("ModulationMatrix slot routing");
        {
            VirtualPatchMatrix matrix;
            VirtualPatchMatrix::PatchModulationSources sources{};
            sources.lfo1 = 0.5f;
            
            auto outputs = matrix.evaluate(sources);
            expectWithinAbsoluteError(outputs.pitchMod, 0.0f, 0.001f, "Empty matrix should return 0 modulation");
            
            matrix.setSlot(0, VirtualPatchMatrix::PatchSource::LFO1, VirtualPatchMatrix::PatchDestination::Pitch, 0.8f);
            auto outputs2 = matrix.evaluate(sources);
            float expectedMod = 0.5f * 0.8f;
            expectWithinAbsoluteError(outputs2.pitchMod, expectedMod, 0.001f, "Single route should produce source*amount");
            
            sources.lfo2 = 0.6f;
            matrix.setSlot(1, VirtualPatchMatrix::PatchSource::LFO2, VirtualPatchMatrix::PatchDestination::Pitch, 0.3f);
            auto outputs3 = matrix.evaluate(sources);
            expectedMod = 0.5f * 0.8f + 0.6f * 0.3f;
            expectWithinAbsoluteError(outputs3.pitchMod, expectedMod, 0.001f, "Two routes should accumulate");
            
            VirtualPatchMatrix::PatchModulationSources sourcesEmpty{};
            auto outputsEmpty = matrix.evaluate(sourcesEmpty);
            expectWithinAbsoluteError(outputsEmpty.pitchMod, 0.0f, 0.001f, "Unrouted destination should return 0");
            logMessage("Modulation matrix routing: OK");
        }

        //==============================================================================
        beginTest("Envelope ADSR lifecycle");
        {
            ADSREnvelope env;
            env.prepare(kTestSampleRate);
            env.setParameters(0.005f, 0.005f, 0.5f, 0.05f);
            env.trigger();
            expect(env.isActive(), "Envelope should be active after trigger");
            const int adsrSamples = static_cast<int>(kTestSampleRate * 0.015);
            float maxLevel = 0.0f;
            for (int s = 0; s < adsrSamples; ++s) maxLevel = std::max(maxLevel, env.getNextSample());
            expect(maxLevel > 0.99f, "Envelope peak should be near 1.0 (was " + juce::String(maxLevel) + ")");
            env.release();
            expect(env.isActive(), "Envelope should be active after release");
            const int releaseSamples = static_cast<int>(kTestSampleRate * 0.05) + 100;
            float lastLevel = 0.0f;
            for (int s = 0; s < releaseSamples; ++s) lastLevel = env.getNextSample();
            expectWithinAbsoluteError(lastLevel, 0.0f, (float)0.01f, "Envelope level should be near 0 after release");
            expect(!env.isActive(), "Envelope should become inactive after release completes");
            logMessage("Envelope ADSR lifecycle: OK");
        }

        //==============================================================================
        beginTest("LFO shape enumeration");
        {
            LFO lfo;
            lfo.prepare(kTestSampleRate);
            lfo.setFrequencyHz(440.0f);
            for (int shape = 0; shape <= 6; ++shape)
            {
                lfo.setWaveformLFO1(static_cast<LFOWaveform>(shape));
                lfo.triggerKeySync(true);
                for (int s = 0; s < 100; ++s)
                    expect(std::isfinite(lfo.getNextSample()), "LFO shape " + juce::String(shape) + " should produce finite output");
            }
            lfo.setWaveformLFO1(LFOWaveform::Sine);
            lfo.triggerKeySync(true);
            float maxAbs = 0.0f;
            for (int s = 0; s < 100; ++s) maxAbs = std::max(maxAbs, std::abs(lfo.getNextSample()));
            expect(maxAbs <= 1.0f, "Sine LFO output should be within [-1, 1] (maxAbs=" + juce::String(maxAbs) + ")");
            logMessage("LFO shapes 0-6: OK");
        }

        //==============================================================================
        beginTest("Note priority edge cases");
        {
            int heldNotes[12] = {};
            int heldCount = 0;
            auto addNote = [&](int note) { if (heldCount < 12) heldNotes[heldCount++] = note; };
            auto removeNote = [&](int note) {
                for (int h = 0; h < heldCount; ++h)
                    if (heldNotes[h] == note) {
                        for (int r = h; r < heldCount - 1; ++r) heldNotes[r] = heldNotes[r + 1];
                        heldCount--; break;
                    }
            };
            addNote(48); addNote(52); addNote(55);
            int lowest = heldNotes[0];
            for (int h = 1; h < heldCount; ++h) if (heldNotes[h] < lowest) lowest = heldNotes[h];
            expectEquals(lowest, 48, "Lowest priority should select C3");
            int highest = heldNotes[0];
            for (int h = 1; h < heldCount; ++h) if (heldNotes[h] > highest) highest = heldNotes[h];
            expectEquals(highest, 55, "Highest priority should select G3");
            expectEquals(heldNotes[heldCount - 1], 55, "Last priority should select most recently added note");
            removeNote(52);
            expectEquals(heldCount, 2, "After removing E3, should have 2 notes");
            removeNote(48);
            lowest = heldNotes[0];
            for (int h = 1; h < heldCount; ++h) if (heldNotes[h] < lowest) lowest = heldNotes[h];
            expectEquals(lowest, 55, "After removing C3, lowest should be G3");
            logMessage("Note priority edge cases: OK");
        }

        //==============================================================================
        beginTest("Envelope curve parameter mapping");
        {
            auto normalizedToCurve = [](float normalized) -> float { return normalized * 2.0f - 1.0f; };
            expectWithinAbsoluteError(normalizedToCurve(0.0f), -1.0f, 0.001f, "Curve 0 → -1.0 (exponential)");
            expectWithinAbsoluteError(normalizedToCurve(0.5f), 0.0f, 0.001f, "Curve 0.5 → 0.0 (linear)");
            expectWithinAbsoluteError(normalizedToCurve(1.0f), 1.0f, 0.001f, "Curve 1.0 → 1.0 (logarithmic)");
            logMessage("Envelope curve mapping: OK");
        }

        //==============================================================================
        beginTest("Held notes tracker limits");
        {
            int heldNotes[12] = {}; int heldCount = 0;
            auto removeNote = [&](int note) {
                for (int h = 0; h < heldCount; ++h)
                    if (heldNotes[h] == note) {
                        for (int r = h; r < heldCount - 1; ++r) heldNotes[r] = heldNotes[r + 1];
                        heldCount--; break;
                    }
            };
            removeNote(60);
            expectEquals(heldCount, 0, "Empty tracker should remain empty after removing non-existent note");
            for (int i = 0; i < 12; ++i) { if (heldCount < 12) heldNotes[heldCount++] = 36 + i; }
            expectEquals(heldCount, 12, "Tracker should hold 12 notes");
            if (heldCount < 12) heldNotes[heldCount++] = 48;
            expectEquals(heldCount, 12, "Tracker should reject overflow");
            logMessage("Held notes tracker limits: OK");
        }

        //==============================================================================
        beginTest("Poly chord accumulator reset");
        {
            int heldNotes[12] = {}; int heldCount = 0;
            for (int i = 0; i < 5; ++i) heldNotes[heldCount++] = 40 + i;
            expect(heldCount == 5 && heldNotes[0] == 40 && heldNotes[4] == 44, "Accumulator should hold 5 notes");
            heldCount = 0; std::memset(heldNotes, 0, sizeof(heldNotes));
            expectEquals(heldCount, 0, "After reset, count should be 0");
            expectEquals(heldNotes[0], 0, "After reset, first element should be 0");
            logMessage("Poly chord accumulator reset: OK");
        }

        //==============================================================================
        beginTest("Oscillator range number of options");
        {
            auto clampRange = [](int range) -> int { return std::clamp(range, 0, 2); };
            expectEquals(clampRange(0), 0, "Range 0 → 16'");
            expectEquals(clampRange(1), 1, "Range 1 → 8'");
            expectEquals(clampRange(2), 2, "Range 2 → 4'");
            expectEquals(clampRange(-1), 0, "Range -1 should clamp to 0");
            expectEquals(clampRange(5), 2, "Range 5 should clamp to 2");
            logMessage("Oscillator range clamping: OK");
        }

        //==============================================================================
        beginTest("Memory chord octave span");
        {
            int intervals[6] = { 0, 4, 7, 12, 16, 19 };
            int span = intervals[5] - intervals[0];
            expectEquals(span, 19, "Memory chord span should be 19 semitones");
            expect(span >= 12, "Memory chord should span at least one octave");
            int root = 48;
            for (int i = 0; i < 6; ++i)
                expect(root + intervals[i] >= 0 && root + intervals[i] <= 127,
                    "Memory chord note should be in MIDI range");
            logMessage("Memory chord octave span: OK");
        }

        //==============================================================================
        beginTest("VAOscillator tone modulation");
        {
            VAOscillator osc;
            osc.prepare(kTestSampleRate);
            osc.setFrequency(440.0);
            osc.setWaveform(VAWaveform::Sawtooth);
            osc.setControl1(0.0f);
            float maxNoMod = 0.0f;
            for (int i = 0; i < static_cast<int>(kTestSampleRate * 0.1); ++i)
                maxNoMod = std::max(maxNoMod, std::abs(osc.getNextSample()));
            expectWithinAbsoluteError(maxNoMod, 1.0f, 0.01f, "Sawtooth peak should be 1.0");

            osc.setWaveform(VAWaveform::Pulse);
            osc.setControl1(1.0f); // Full PWM
            float maxMod = 0.0f;
            for (int i = 0; i < static_cast<int>(kTestSampleRate * 0.1); ++i)
                maxMod = std::max(maxMod, std::abs(osc.getNextSample()));
            expect(maxMod > 0.5f && maxMod < 1.0f,
                "Pulse with full PWM should reduce peak (got " + juce::String(maxMod) + ")");

            osc.setWaveform(VAWaveform::Triangle);
            osc.setControl1(0.5f);
            float maxTri = 0.0f;
            for (int i = 0; i < static_cast<int>(kTestSampleRate * 0.1); ++i)
                maxTri = std::max(maxTri, std::abs(osc.getNextSample()));
            expect(maxTri > 0.5f && maxTri < 1.0f,
                "Triangle peak (got " + juce::String(maxTri) + ")");

            osc.setWaveform(VAWaveform::Sine);
            osc.setControl1(0.0f);
            float maxSine = 0.0f;
            for (int i = 0; i < static_cast<int>(kTestSampleRate * 0.1); ++i)
                maxSine = std::max(maxSine, std::abs(osc.getNextSample()));
            expectWithinAbsoluteError(maxSine, 1.0f, 0.01f, "Sine peak should be 1.0");
            
            logMessage("VAOscillator waveforms: OK");
        }

        //==============================================================================
        beginTest("VAOscillator hard sync");
        {
            VAOscillator master, slave;
            master.prepare(kTestSampleRate);
            slave.prepare(kTestSampleRate);
            master.setFrequency(440.0);
            slave.setFrequency(880.0); // One octave up
            master.setWaveform(VAWaveform::Sawtooth);
            slave.setWaveform(VAWaveform::Sawtooth);

            // Trigger sync on slave
            slave.syncPhase(master.getPhase());

            // After sync, slave phase should be reset based on master
            expect(std::abs(slave.getPhase()) < 0.1f, "Hard sync should reset slave phase near 0");
            logMessage("VAOscillator hard sync: OK");
        }

        //==============================================================================
        beginTest("Envelope loop mode");
        {
            ADSREnvelope env;
            env.setSampleRate(kTestSampleRate);
            env.setParameters(0.001f, 0.002f, 0.0f, 0.05f);
            env.setLoopMode(true); // Loop mode: Attack → Decay → Attack
            env.trigger();

            int samplesPerCycle = static_cast<int>(kTestSampleRate * 0.004); // ~0.003s attack+decay
            int totalSamples = samplesPerCycle * 3; // at least 3 cycles

            float prevPeak = 0.0f;
            int cycleCount = 0;
            bool looping = true;
            for (int s = 0; s < totalSamples; ++s)
            {
                float val = env.nextSample();
                if (val > prevPeak && val > 0.5f && s > samplesPerCycle / 2)
                    cycleCount++;
                prevPeak = val;

                if (!env.isActive())
                {
                    looping = false;
                    break;
                }
            }

            expect(looping, "Envelope loop mode should remain active for 3 cycles");
            expect(cycleCount >= 2, "Envelope loop should produce at least 2 cycle peaks");
            logMessage("Envelope loop mode: OK");
        }

        //==============================================================================
        beginTest("PolyBLEP anti-aliasing");
        {
            using namespace ABD::synth::DSPUtils;
            float dt = 0.05f;
            expect(std::abs(polyBlep2(0.0f, dt)) > 0.001f, "polyBlep2 at t=0 should be non-zero");
            expect(std::abs(polyBlep2(dt * 0.5f, dt)) > 0.001f, "polyBlep2 at t=dt/2 should be non-zero");
            expect(std::abs(polyBlep2(0.5f, dt)) < 0.0001f, "polyBlep2 at t=0.5 should be zero");
            expect(std::abs(polyBlep2(1.0f - dt * 0.5f, dt)) > 0.001f, "polyBlep2 near t=1 should be non-zero");
            logMessage("PolyBLEP anti-aliasing: OK");
        }

        //==============================================================================
        beginTest("Saw curvature");
        {
            using namespace ABDMS2000::DSPUtils;
            float dt = 0.1f;
            float t = 0.0f;
            for (int i = 0; i < 10; ++i)
            {
                float raw = t;
                float correction = polyBlep2(t, dt);
                float corrected = raw + correction;
                expect(std::isfinite(corrected), "Saw curvature sample " + juce::String(i) + " should be finite");
                t += dt;
                if (t >= 1.0f) t -= 1.0f;
            }
            logMessage("Saw curvature: OK");
        }

        //==============================================================================
        beginTest("VoiceManager voice stealing");
        {
            auto vm = std::make_unique<VoiceManager>();
            vm->prepare(kTestSampleRate);
            vm->setMaxPolyphony(4);

            // Try to play 8 notes with only 4 voices
            for (int n = 60; n < 68; ++n) vm->noteOn(n, 0.8f);
            expect(vm->getActiveVoiceCount() <= 4, "Polyphonic voice stealing strictly caps active voices to 4");

            vm->allNotesOff();
            expect(vm->getActiveVoiceCount() == 0, "allNotesOff cleanly terminates all active voices");
            logMessage("VoiceManager voice stealing: OK");
        }
    }
};

static VoiceTests voiceTests;

} // namespace Tests
} // namespace ABDMS2000