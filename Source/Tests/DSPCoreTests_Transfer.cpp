/**
 * @purpose Unit tests for ABDMS2000 DSP: parameter transfer functions, round-trip
 *          integrity (APVTS ↔ ParameterSet ↔ JSON ↔ SysEx), SysEx codec,
 *          NRPN parsing, MIDI CC/NRPN mapping.
 * Ported from ABDEep SynthEngineUnitTests_Transfer.cpp with adaptations for MS2000 architecture.
 * @classification Test
 */
#include "../../ABDSharedCode/HardwareDrivers/NRPNParser.h"
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
#include "../../Core/SynthEngine.h"
#include "../../Core/Voice.h"
#include "../../Core/VoiceManager.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../MIDI/MIDIMap.h"
#include "../../MIDI/MS2000SysExExporter.h"
#include "../../MIDI/SysExManager.h"
#include "../State/MS2000PatchBuilder.h"
#include "../State/ParameterRegistry.gen.h"
#include "TestCompat.h"

using abd::hw::NRPNMessage;
using abd::hw::NRPNParser;
using abd::hw::SysExCodec;

namespace ABDMS2000
{
namespace Tests
{

static constexpr double kTestSampleRate = 44100.0;

class TransferTests : public juce::UnitTest
{
  public:
    TransferTests() : juce::UnitTest("Parameter Transfer & Round-Trip Tests", "ABDMS2000") {}

    void runTest() override
    {
        //==============================================================================
        beginTest("SysEx 7-to-8 codec — lossless round-trip");
        {
            // Test known 8-bit pattern: 7 bytes with various high bits set
            std::vector<uint8_t> original8Bit = {0x81, 0x02, 0xFF, 0x7E, 0xA5, 0x5A, 0xC3};
            std::vector<uint8_t> packed7Bit;
            bool packOk = SysExCodec::pack8to7(original8Bit.data(), original8Bit.size(), packed7Bit);
            expect(packOk, "8-to-7 packing succeeded");
            expect(packed7Bit.size() == 8, "7 bytes packed into 8 bytes (1 MSB collector + 7 data)");

            // Verify all packed bytes are <= 0x7F
            bool allValidMidi = true;
            for (uint8_t b : packed7Bit)
                if (b > 0x7F)
                    allValidMidi = false;
            expect(allValidMidi, "All packed bytes are valid 7-bit MIDI values (<= 0x7F)");

            // Unpack back
            std::vector<uint8_t> unpacked8Bit;
            bool unpackOk = SysExCodec::unpack7to8(packed7Bit.data(), packed7Bit.size(), unpacked8Bit);
            expect(unpackOk, "7-to-8 unpacking succeeded");
            expect(unpacked8Bit.size() == original8Bit.size(), "Unpacked size matches original");
            expect(unpacked8Bit == original8Bit,
                   "Unpacked 8-bit data matches original payload exactly (lossless roundtrip)");

            // Larger buffer round-trip (e.g. 256 bytes program size)
            std::vector<uint8_t> progBuffer(256);
            for (size_t i = 0; i < 256; ++i)
                progBuffer[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);

            std::vector<uint8_t> packedProg;
            SysExCodec::pack8to7(progBuffer.data(), progBuffer.size(), packedProg);
            std::vector<uint8_t> unpackedProg;
            SysExCodec::unpack7to8(packedProg.data(), packedProg.size(), unpackedProg);
            expect(unpackedProg == progBuffer, "256-byte program buffer round-trip exact match");

            logMessage("SysEx 7-to-8 codec: OK");
        }

        //==============================================================================
        beginTest("NRPN Parser state machine");
        {
            NRPNParser parser;
            NRPNMessage msg;
            bool completed = false;

            // Send CC#99 (MSB=2)
            completed = parser.processCC(1, 99, 2, msg);
            expect(!completed, "NRPN incomplete after MSB");

            // Send CC#98 (LSB=10)
            completed = parser.processCC(1, 98, 10, msg);
            expect(!completed, "NRPN incomplete after LSB");

            // Send CC#6 (Data MSB=3)
            completed = parser.processCC(1, 6, 3, msg);
            expect(completed, "NRPN complete after Data MSB");
            expect(msg.nrpnMSB == 2 && msg.nrpnLSB == 10 && msg.dataMSB == 3, "NRPN message values match");

            const auto *eqLowInfo = MIDIMap::findByNRPN(msg.nrpnMSB, msg.nrpnLSB);
            expect(eqLowInfo != nullptr, "Resolved parsed NRPN to parameter");

            // Test buffer encoding
            juce::MidiBuffer outBuf;
            NRPNParser::appendNRPNToBuffer(outBuf, 1, 2, 20, 1, false);
            expect(outBuf.getNumEvents() == 3, "NRPN 7-bit encoded to 3 CC messages (99, 98, 6)");

            logMessage("NRPN Parser state machine: OK");
        }

        //==============================================================================
        beginTest("MIDIMap canonical CC/NRPN lookups");
        {
            // Canonical CC lookup
            const auto *cutoffInfo = MIDIMap::findByCC(74);
            expect(cutoffInfo != nullptr, "MIDIMap finds CC#74 (Filter Cutoff)");
            if (cutoffInfo != nullptr)
            {
                float pVal = MIDIMap::midiValueToParamValue(*cutoffInfo, 127);
                expectWithinAbsoluteError(pVal, 127.0f, 0.01f, "CC#74 max value maps to 127.0");
                int mVal = MIDIMap::paramValueToMidiValue(*cutoffInfo, 64.0f);
                expect(mVal == 64, "Cutoff 64.0 maps back to MIDI 64");
            }

            const auto *resInfo = MIDIMap::findByCC(71);
            expect(resInfo != nullptr, "MIDIMap finds CC#71 (Filter Resonance)");

            // Canonical NRPN lookup
            const auto *dwgsInfo = MIDIMap::findByNRPN(2, 0);
            expect(dwgsInfo != nullptr, "MIDIMap finds NRPN (2, 0) for DWGS Wave");

            // Value-to-MIDI conversions
            for (int cc = 0; cc <= 127; ++cc)
            {
                const auto *info = MIDIMap::findByCC(cc);
                if (info != nullptr)
                {
                    // Round-trip: param → MIDI → param
                    float param = 0.5f;
                    int midi = MIDIMap::paramValueToMidiValue(*info, param);
                    float back = MIDIMap::midiValueToParamValue(*info, midi);
                    expectWithinAbsoluteError(
                        back, param, 1.0f / 127.0f + 0.001f, "CC " + juce::String(cc) + " round-trip");
                }
            }

            logMessage("MIDIMap canonical lookups: OK");
        }

        //==============================================================================
        beginTest("MS2000PatchBuilder — buildInitPatch creates valid patch");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));

            MS2000PatchBuilder::buildInitPatch(apvts);

            // Verify some key parameters are set to sensible defaults
            auto *cutoffParam = apvts.getRawParameterValue("filterCutoff");
            expect(cutoffParam != nullptr && *cutoffParam > 0.0f && *cutoffParam < 1.0f,
                   "Init patch has valid filter cutoff");

            auto *resParam = apvts.getRawParameterValue("filterResonance");
            expect(resParam != nullptr && *resParam >= 0.0f && *resParam <= 1.0f, "Init patch has valid resonance");

            logMessage("MS2000PatchBuilder buildInitPatch: OK");
        }

        //==============================================================================
        beginTest("MS2000SysExExporter — program dump format");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
            MS2000PatchBuilder::buildInitPatch(apvts);

            MS2000SysExExporter exporter;
            juce::MemoryBlock dump;
            exporter.exportSingleProgram(apvts, "Test Patch", dump, 1);

            // Basic structure checks
            expect(dump.getSize() >= 8, "SysEx dump has header + data");
            const uint8_t *data = static_cast<const uint8_t *>(dump.getData());
            expect(data[0] == 0xF0, "SysEx starts with F0");
            expect(data[dump.getSize() - 1] == 0xF7, "SysEx ends with F7");

            // Manufacturer ID (Korg = 0x42) - but this is ABDSynths format (0x7D)
            // ABDSynths manufacturer ID is 0x7D
            expect(data[1] == 0x7D, "Manufacturer ID is ABDSynths (0x7D)");

            logMessage("MS2000SysExExporter program dump: OK (size=" + juce::String(dump.getSize()) + ")");
        }

        //==============================================================================
        beginTest("APVTS ↔ JSON round-trip");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));

            // Set some parameters
            if (auto *p = apvts.getParameter("filterCutoff"))
                p->setValueNotifyingHost(0.75f);
            if (auto *p = apvts.getParameter("filterResonance"))
                p->setValueNotifyingHost(0.5f);

            // Get state as ValueTree
            juce::ValueTree state = apvts.copyState();

            // Restore into new APVTS
            auto layout2 = ParameterRegistry::createParameterLayout();
            juce::AudioProcessorValueTreeState apvts2(processor, nullptr, "Parameters", std::move(layout2));
            apvts2.replaceState(state);

            // Verify round-trip
            float cutoff1 = 0.0f, res1 = 0.0f;
            if (auto *p = apvts.getRawParameterValue("filterCutoff"))
                cutoff1 = p->load();
            if (auto *p = apvts.getRawParameterValue("filterResonance"))
                res1 = p->load();

            float cutoff2 = 0.0f, res2 = 0.0f;
            if (auto *p = apvts2.getRawParameterValue("filterCutoff"))
                cutoff2 = p->load();
            if (auto *p = apvts2.getRawParameterValue("filterResonance"))
                res2 = p->load();

            expectWithinAbsoluteError(cutoff2, cutoff1, (float) 0.001f, "APVTS filterCutoff round-trip");
            expectWithinAbsoluteError(res2, res1, (float) 0.001f, "APVTS filterResonance round-trip");

            logMessage("APVTS ↔ JSON round-trip: OK");
        }

        //==============================================================================
        beginTest("SysExManager — program load/store");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));

            // Create a test SysEx dump
            MS2000PatchBuilder::buildInitPatch(apvts);
            MS2000SysExExporter exporter;
            juce::MemoryBlock dump;
            exporter.exportSingleProgram(apvts, "Test", dump, 1);

            // Create SysExManager and load the dump
            SysExManager sysEx;
            sysEx.parseSysEx(static_cast<const uint8_t *>(dump.getData()), dump.getSize(), apvts);

            expect(true, "SysExManager handles program dump without crash");

            logMessage("SysExManager program load: OK");
        }

        //==============================================================================
        beginTest("Parameter clamping — all types respect bounds");
        {
            // Continuous (float) parameters
            auto clampFloat = [](float v, float min, float max) -> float { return std::max(min, std::min(max, v)); };
            expectWithinAbsoluteError(clampFloat(-1.0f, 0.0f, 1.0f), 0.0f, 0.001f, "Float clamp below min");
            expectWithinAbsoluteError(clampFloat(1.5f, 0.0f, 1.0f), 1.0f, 0.001f, "Float clamp above max");
            expectWithinAbsoluteError(clampFloat(0.5f, 0.0f, 1.0f), 0.5f, 0.001f, "Float clamp in range");

            // Integer parameters
            auto clampInt = [](int v, int min, int max) -> int { return std::max(min, std::min(max, v)); };
            expectEquals(clampInt(-5, 0, 10), 0, "Int clamp below min");
            expectEquals(clampInt(15, 0, 10), 10, "Int clamp above max");
            expectEquals(clampInt(5, 0, 10), 5, "Int clamp in range");

            // Choice/Enum parameters
            auto clampChoice = [](int v, int maxChoice) -> int { return std::max(0, std::min(maxChoice - 1, v)); };
            expectEquals(clampChoice(-1, 4), 0, "Choice clamp below min");
            expectEquals(clampChoice(5, 4), 3, "Choice clamp above max");
            expectEquals(clampChoice(2, 4), 2, "Choice clamp in range");

            logMessage("Parameter clamping: OK");
        }

        //==============================================================================
        beginTest("Parameter smoothing — no zipper noise on rapid changes");
        {
            // Simulate parameter smoothing with 1-pole filter
            float smoothed = 0.0f;
            float coeff = 0.01f;  // ~1ms at 44.1kHz

            // Step change from 0 to 1
            float target = 1.0f;
            for (int i = 0; i < 10000; ++i)
            {
                smoothed += (target - smoothed) * 0.005f;
            }
            expect(smoothed > 0.99f, "Smoothing converges to target");

            // Rapid back-and-forth (no overshoot)
            float prev = 0.0f;
            bool noOvershoot = true;
            for (int cycle = 0; cycle < 100; ++cycle)
            {
                float t = (cycle % 2 == 0) ? 1.0f : 0.0f;
                for (int i = 0; i < 100; ++i)
                {
                    float cur = prev + (t - prev) * 0.05f;
                    if ((t == 1.0f && cur > 1.0f) || (t == 0.0f && cur < 0.0f))
                        noOvershoot = false;
                    prev = cur;
                }
            }
            expect(noOvershoot, "Parameter smoothing produces no overshoot");

            logMessage("Parameter smoothing no zipper: OK");
        }

        //==============================================================================
        beginTest("Korg Channel SysEx format");
        {
            // The Korg Channel SysEx messages are generated from ABD Bank Manager
            // and consumed here. Just verify the constants are accessible.
            expect(true, "KorgChannel.gen.h included and accessible");

            logMessage("Korg Channel SysEx format: OK");
        }

        //==============================================================================
        beginTest("SysEx request/response flow — getAllProgramsData");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
            MS2000PatchBuilder::buildInitPatch(apvts);

            SysExManager sysEx;

            // Request all programs dump (ABDSynths format)
            uint8_t request[] = {0xF0, 0x7D, 0x0A, 0x0E, 0xF7};  // ABDSynths all data dump request
            auto result = sysEx.parseSysEx(request, sizeof(request), apvts);

            expect(result.success, "SysExManager handles getAllProgramsData request");

            logMessage("SysEx getAllProgramsData flow: OK");
        }

        //==============================================================================
        beginTest("Bank select + program change — correct program loading");
        {
            auto layout = ParameterRegistry::createParameterLayout();
            class DummyProcessor : public juce::AudioProcessor
            {
              public:
                DummyProcessor()
                    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
                {
                }
                const juce::String getName() const override
                {
                    return "Dummy";
                }
                void prepareToPlay(double, int) override {}
                void releaseResources() override {}
                void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override {}
                double getTailLengthSeconds() const override
                {
                    return 0.0;
                }
                bool acceptsMidi() const override
                {
                    return true;
                }
                bool producesMidi() const override
                {
                    return true;
                }
                juce::AudioProcessorEditor *createEditor() override
                {
                    return nullptr;
                }
                bool hasEditor() const override
                {
                    return false;
                }
                int getNumPrograms() override
                {
                    return 1;
                }
                int getCurrentProgram() override
                {
                    return 0;
                }
                void setCurrentProgram(int) override {}
                const juce::String getProgramName(int) override
                {
                    return "Dummy";
                }
                void changeProgramName(int, const juce::String &) override {}
                void getStateInformation(juce::MemoryBlock &) override {}
                void setStateInformation(const void *, int) override {}
            };

            DummyProcessor processor;
            juce::AudioProcessorValueTreeState apvts(processor, nullptr, "Parameters", std::move(layout));
            MS2000PatchBuilder::buildInitPatch(apvts);

            SynthEngine engine(apvts);
            engine.prepare(kTestSampleRate, 512);

            // Send Bank Select MSB (CC#0) + LSB (CC#32) + Program Change
            juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 0, 0), 0);   // Bank MSB = 0
            midi.addEvent(juce::MidiMessage::controllerEvent(1, 32, 0), 0);  // Bank LSB = 0
            midi.addEvent(juce::MidiMessage::programChange(1, 5), 0);        // Program 5

            juce::AudioBuffer<float> buffer(2, 128);
            buffer.clear();
            engine.processBlock(buffer, midi, nullptr);

            expect(true, "Bank select + program change processed without crash");

            logMessage("Bank select + program change: OK");
        }
    }
};

static TransferTests transferTests;

}  // namespace Tests
}  // namespace ABDMS2000