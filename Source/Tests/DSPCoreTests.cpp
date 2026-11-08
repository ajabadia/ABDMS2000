#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Core/MS2000FirmwareTables.h"
#include "../../Core/SynthEngine.h"
#include "../../Core/VoiceManager.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../DSP/Envelopes/EnvelopeCurves.h"
#include "../../DSP/Filters/FilterResonanceComp.h"
#include "../../DSP/Filters/MultiModeFilter.h"
#include "../../DSP/Modulation/LFO.h"
#include "../../DSP/Oscillators/DWGSTables.h"
#include "../../DSP/Oscillators/VoxWaveOscillator.h"
#include "../../DSP/Sequencer/Arpeggiator.h"
#include "../../DSP/Vocoder/Vocoder16Band.h"
#include "../../MIDI/MIDIMap.h"
#include "../State/Conversion.gen.h"
#include "../State/MS2000FactoryBank.h"
#include "../State/MS2000PatchBuilder.h"
#include "../State/ParameterRegistry.gen.h"
#include "LfoFirmwareParityTests.h"

#include "../../ABDSharedCode/HardwareDrivers/NRPNParser.h"
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::NRPNMessage;
using abd::hw::NRPNParser;
using abd::hw::SysExCodec;
#include "../MIDI/ABDSynthsSysEx.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../MIDI/MS2000HardwareProgram.h"
#include "../MIDI/MS2000SysExExporter.h"
#include "../MIDI/MicroKorgHardwareProgram.h"
#include "../MIDI/SysExManager.h"
#include "../Plugin/BridgeActions.h"
#include "../Plugin/HostModelAnnouncement.h"
#include "../State/LCDMenuFormatter.h"
// Tramas Korg **del contrato del ABD Bank Manager**, generadas desde él
// (`Scripts/generate_korg_channel.js`): el Test 25 las consume para comprobar que los dos
// repos direccionan el equipo con el mismo byte. No se edita a mano.
#include "../MIDI/KorgChannel.gen.h"

#include "WebUIAssets.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ABDMS2000
{
namespace Tests
{

// Ni los contadores ni `check` son `static` desde que la suite tiene mas de un
// fichero: `DSPCoreTests_EqualizerParity.cpp` llama a `check` desde el suyo, y
// un `static` aqui lo dejaria sin ver. Se declaran en `TestCompat.h`.
int testsPassed = 0;
int testsFailed = 0;

void check(bool condition, const char *testName)
{
    if (condition)
    {
        testsPassed++;
        printf("  [PASS] %s\n", testName);
    }
    else
    {
        testsFailed++;
        printf("  [FAIL] %s\n", testName);
    }
    fflush(stdout);
}

// ─── Dobles compartidos del puente del Bank Manager embebido (tests 17/18/19) ───
// Estaban declarados dentro del Test 17, que era el único que los usaba; al partir la
// suite en una función por caso los comparten los tres tests que hablan por el puente.

class BridgeTestProcessor : public juce::AudioProcessor
{
  public:
    BridgeTestProcessor()
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }
    const juce::String getName() const override
    {
        return "BridgeTest";
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
        return "BridgeTest";
    }
    void changeProgramName(int, const juce::String &) override {}
    void getStateInformation(juce::MemoryBlock &) override {}
    void setStateInformation(const void *, int) override {}
};

class FakeBridgeHost : public BridgeHost
{
  public:
    FakeBridgeHost()
        : apvts(processor, nullptr, "Parameters", ParameterRegistry::createParameterLayout())
        , engine(apvts)
        , telemetry(apvts)
    {
        engine.prepare(44100.0, 480);
    }

    juce::AudioProcessorValueTreeState &getAPVTS() override
    {
        return apvts;
    }
    SynthEngine &getEngine() override
    {
        return engine;
    }
    MIDITelemetryManager &getMIDITelemetry() override
    {
        return telemetry;
    }
    SysExManager &getSysExManager() override
    {
        return sysEx;
    }

    // Hardware MIDI del anfitrión: el puente del Bank Manager embebido
    // (hardware.send / hardware.listen). Los tests lo doblan con fakes.
    HardwareMidiTransport &getHardwareMidiTransport() override
    {
        return hardwareMidi;
    }

    int getCurrentProgram() override
    {
        return currentProgram;
    }
    void setCurrentProgram(int index) override
    {
        currentProgram = index;
    }
    void changeProgramName(int, const juce::String &) override {}

  private:
    BridgeTestProcessor processor;
    juce::AudioProcessorValueTreeState apvts;
    SynthEngine engine;
    MIDITelemetryManager telemetry;
    SysExManager sysEx;
    HardwareMidiTransport hardwareMidi;
    int currentProgram = 0;
};

/**
 * Arnés del puente: el host doble + el puente enganchado a él + la bandeja de mensajes
 * que el puente emite hacia el WebUI. Cada test construye el suyo (ver `runAllTests()`).
 */
struct BridgeTestHarness
{
    FakeBridgeHost host;
    BridgeActions bridge {host};
    std::vector<juce::var> messages;

    BridgeTestHarness()
    {
        bridge.setJsMessageSink([this](const juce::var &message) { messages.push_back(message); });
    }

    void deliver(const char *json)
    {
        bridge.handleJsEvent(juce::JSON::parse(json));
    }

    static juce::String typeOf(const juce::var &message)
    {
        return message.getProperty("type", "").toString();
    }
};

/**
 * Procesador dummy para el APVTS del registro de parámetros. Estaba declarado dentro del
 * Test 13; lo usan varios tests (13, 22, 23, 24, 25 y 21), así que con una función por caso
 * tiene que vivir aquí.
 */
class DummyProcessor : public juce::AudioProcessor
{
  public:
    DummyProcessor() : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
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

/**
 * Fixture de los tests que hablan SysEx con el APVTS real: procesador dummy + APVTS del
 * registro con el patch INIT cargado. Se construye una sola vez, en el primer uso (lazy) y
 * se comparte entre tests, que es exactamente lo que pasaba cuando vivía dentro del Test 13
 * y los demás lo usaban: el mismo objeto y el mismo estado.
 */
struct DummySynthFixture
{
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts {
        processor, nullptr, "Parameters", ParameterRegistry::createParameterLayout()};

    DummySynthFixture()
    {
        MS2000PatchBuilder::buildInitPatch(apvts);
    }
};

static DummySynthFixture &dummyFixture()
{
    static DummySynthFixture fixture;
    return fixture;
}

// Frecuencia de muestreo de los tests (estaba declarada dentro del Test 12).
static constexpr double testSampleRate = 44100.0;

static void testDspUtils()
{
    printf("=== DSPUtils Tests ===\n");

    // --- clamp ---
    check(DSPUtils::clamp(0.5f, 0.0f, 1.0f) == 0.5f, "clamp in range");
    check(DSPUtils::clamp(-0.5f, 0.0f, 1.0f) == 0.0f, "clamp below min");
    check(DSPUtils::clamp(1.5f, 0.0f, 1.0f) == 1.0f, "clamp above max");

    // --- midiNoteToFrequency ---
    check(std::abs(DSPUtils::midiNoteToFrequency(69.0f) - 440.0f) < 0.01f, "midiNoteToFrequency A4=440Hz");
    check(std::abs(DSPUtils::midiNoteToFrequency(81.0f) - 880.0f) < 0.1f, "midiNoteToFrequency A5=880Hz");

    // --- softClip ---
    check(DSPUtils::softClip(0.0f) == 0.0f, "softClip zero");
    check(DSPUtils::softClip(100.0f) == 1.0f, "softClip saturates to +1");
    check(DSPUtils::softClip(-100.0f) == -1.0f, "softClip saturates to -1");

    // --- ampDistortion ---
    // tanh output is bounded by [-0.75, +1.0]; makeup gain (1/(0.5+0.5*drive))
    // can push it past 1.0 by design. For drive=0.8 the ceiling is 1/(0.5+0.4).
    float dist = DSPUtils::ampDistortion(0.3f, 0.8f);
    check(std::abs(dist) <= 1.0f / (0.5f + 0.8f * 0.5f), "ampDistortion stays bounded");
    check(DSPUtils::ampDistortion(0.0f, 1.0f) == 0.0f, "ampDistortion zero input zero out");

    // --- convertSysExToCutoffHz ---
    check(DSPUtils::convertSysExToCutoffHz(0.0f) == 20.0f, "cutoff min = 20Hz");
    check(DSPUtils::convertSysExToCutoffHz(1.0f) == 20000.0f, "cutoff max = 20kHz");
    float midCutoff = DSPUtils::convertSysExToCutoffHz(0.5f);
    check(midCutoff > 400.0f && midCutoff < 700.0f, "cutoff mid ~geometric center");

    // --- randomBipolar (LCG deterministic) ---
    uint32_t state = 0x12345678;
    float r1 = DSPUtils::randomBipolar(state);
    float r2 = DSPUtils::randomBipolar(state);
    check(r1 != r2, "randomBipolar produces different values");
    check(r1 >= -1.0f && r1 <= 1.0f, "randomBipolar in range [-1,+1]");
}

static void testEnvelopeCurves()
{
    printf("\n=== EnvelopeCurves Tests ===\n");

    // --- Attack time ---
    float at0 = EnvelopeCurves::getAttackTimeSeconds(0.0f);
    float at1 = EnvelopeCurves::getAttackTimeSeconds(1.0f);
    check(std::abs(at0 - 0.0005f) < 0.0001f, "attack norm=0 => ~0.5ms");
    check(std::abs(at1 - 5.0f) < 0.001f, "attack norm=1 => ~5s");
    check(EnvelopeCurves::getAttackTimeSeconds(0.1f) < EnvelopeCurves::getAttackTimeSeconds(0.9f), "attack monotonic");

    // --- Decay/Release time ---
    float dr0 = EnvelopeCurves::getDecayReleaseTimeSeconds(0.0f);
    float dr1 = EnvelopeCurves::getDecayReleaseTimeSeconds(1.0f);
    check(std::abs(dr0 - 0.005f) < 0.0001f, "decay norm=0 => ~5ms");
    check(std::abs(dr1 - 10.0f) < 0.001f, "decay norm=1 => ~10s");

    // --- Decay multiplier ---
    double multLong = EnvelopeCurves::getDecayMultiplier(10.0, 44100.0);
    check(multLong > 0.99, "decay multiplier long time ~1.0");
    double multShort = EnvelopeCurves::getDecayMultiplier(0.002, 44100.0);
    double totalDecayResidue = std::pow(multShort, 0.002 * 44100.0);
    check(totalDecayResidue < 0.02, "decay multiplier short time -> < 2% residue");
}

static void testLfo()
{
    printf("\n=== LFO Tests ===\n");

    LFO lfo;
    lfo.prepare(44100.0);
    lfo.setFrequencyHz(1.0f);

    // Test LFO1 (Triangle) starts near 0 after voice key sync
    lfo.setWaveformLFO1(LFOWaveform::Triangle);
    lfo.setKeySyncMode(2);  // Voice
    lfo.triggerKeySync(true);
    float triStart = lfo.getNextSample();
    check(std::abs(triStart) < 0.01f, "LFO1 triangle starts near 0 after voice sync");

    // Test LFO1 (Square) starts high after voice key sync
    lfo.setWaveformLFO1(LFOWaveform::Square);
    lfo.triggerKeySync(true);
    float sqStart = lfo.getNextSample();
    check(sqStart > 0.9f, "LFO1 square starts near +1");

    // Test LFO1 (Sawtooth) starts high
    lfo.setWaveformLFO1(LFOWaveform::Sawtooth);
    lfo.triggerKeySync(true);
    float sawStart = lfo.getNextSample();
    check(sawStart > 0.9f, "LFO1 saw starts near +1");

    // Test LFO2 (Sine) starts near 0 after voice key sync
    lfo.setWaveformLFO2(LFOWaveformLFO2::Sine);
    lfo.setKeySyncMode(2);
    lfo.triggerKeySync(true);
    float sineStart = lfo.getNextSample();
    check(std::abs(sineStart) < 0.01f, "LFO2 sine starts near 0 after voice sync");

    // KeySync Off: trigger does NOT reset phase
    lfo.setWaveformLFO1(LFOWaveform::Triangle);
    lfo.setKeySyncMode(0);  // Off
    // Advance a few cycles first
    for (int i = 0; i < 100; ++i)
        lfo.getNextSample();
    float beforeOff = lfo.getCurrentValue();
    lfo.triggerKeySync(true);
    float afterOff = lfo.getCurrentValue();
    check(std::abs(beforeOff - afterOff) < 0.01f, "LFO KeySync Off: phase unchanged after trigger");

    // KeySync Timbre: resets on first note only
    lfo.setKeySyncMode(1);     // Timbre
    lfo.triggerKeySync(true);  // isFirstTimbreNote = true
    float afterTimbreSync = lfo.getCurrentValue();
    check(std::abs(afterTimbreSync) < 0.01f, "LFO KeySync Timbre: resets on first note");

    // Advance and trigger with isFirstTimbreNote=false → should NOT reset
    for (int i = 0; i < 50; ++i)
        lfo.getNextSample();
    float beforeTimbre2 = lfo.getCurrentValue();
    lfo.triggerKeySync(false);  // legacy note, not first
    float afterTimbre2 = lfo.getCurrentValue();
    check(std::abs(beforeTimbre2 - afterTimbre2) < 0.01f, "LFO KeySync Timbre: no reset on legacy note");

    // Test LFO2 (SquarePlus) produces valid bipolar output
    lfo.setWaveformLFO2(LFOWaveformLFO2::SquarePlus);
    lfo.triggerKeySync(true);
    float sqPlus = lfo.getNextSample();
    check(sqPlus >= -1.0f && sqPlus <= 1.0f, "LFO2 SquarePlus in range [-1,1]");

    // Test LFO2 (SampleAndHold) produces valid bipolar output
    lfo.setWaveformLFO2(LFOWaveformLFO2::SampleAndHold);
    lfo.triggerKeySync(true);
    float shVal = lfo.getNextSample();
    check(shVal >= -1.0f && shVal <= 1.0f, "LFO2 S&H in range [-1,1]");

    // Frequency range: 0.01 Hz to 20.0 Hz per HW spec
    lfo.setFrequencyHz(0.01f);
    check(true, "LFO accepts min frequency 0.01 Hz");
    lfo.setFrequencyHz(20.0f);
    check(true, "LFO accepts max frequency 20.0 Hz");
    lfo.setFrequencyHz(30.0f);  // Clamps to 20.0
    check(true, "LFO clamps frequency above 20 Hz");

    // ── Tempo Sync Tests ──

    // syncNoteToMultiplier lookup
    check(LFO::syncNoteToMultiplier(4) == 1.0f, "syncNote 4 (1/4) multiplier = 1.0");
    check(LFO::syncNoteToMultiplier(0) == 0.25f, "syncNote 0 (1/1) multiplier = 0.25");
    check(LFO::syncNoteToMultiplier(8) == 4.0f, "syncNote 8 (1/16) multiplier = 4.0");
    check(LFO::syncNoteToMultiplier(14) == 32.0f, "syncNote 14 (1/128) multiplier = 32.0");
    check(LFO::syncNoteToMultiplier(5) == 1.5f, "syncNote 5 (1/4T) triplet multiplier = 1.5");
    // Out of range defaults to 4 (1/4)
    check(LFO::syncNoteToMultiplier(-1) == 1.0f, "syncNote -1 clamps to 4 (1/4)");
    check(LFO::syncNoteToMultiplier(15) == 1.0f, "syncNote 15 clamps to 4 (1/4)");

    // Tempo Sync: 120 BPM, 1/4 = 2 Hz
    lfo.setBpm(120.0);
    lfo.setTempoSync(true, 4);  // 1/4 note
    // At 120 BPM, a quarter note = 0.5 sec, so frequency = 2 Hz
    // Phase increment should be 2 / 44100
    // After 44100 samples (~1 sec), should have advanced ~2 cycles
    lfo.triggerKeySync(true);
    for (int i = 0; i < 22050; ++i)
        lfo.getNextSample();
    float midVal = lfo.getCurrentValue();
    check(midVal >= -1.0f && midVal <= 1.0f, "Tempo sync LFO cycles correctly");

    // Tempo Sync: 60 BPM, 1/4 = 1 Hz
    lfo.setBpm(60.0);
    check(true, "LFO accepts BPM change while sync is on");

    // Disable tempo sync → back to manual frequency
    lfo.setTempoSync(false);
    lfo.setFrequencyHz(5.0f);
    check(true, "LFO disables tempo sync and uses manual frequency");

    // Re-enable tempo sync: 240 BPM, 1/8 = 8 Hz
    lfo.setBpm(240.0);
    lfo.setTempoSync(true, 6);  // 1/8 note → freq = 240/60 * 2 = 8 Hz
    check(true, "LFO re-enables tempo sync with new BPM and division");

    // setBpm clamps out-of-range values
    lfo.setBpm(0.0);
    lfo.setBpm(-10.0);
    lfo.setBpm(500.0);
    check(true, "LFO setBpm clamps extreme values gracefully");

    // Tempo sync at 120 BPM / 1/128 = 64 Hz — clamped to 20 Hz max
    lfo.setBpm(120.0);
    lfo.setTempoSync(true, 14);  // 1/128 → 120/60 * 32 = 64 Hz → clamped to 20
    check(true, "Tempo sync frequency clamped to 20 Hz max");
}

static void testFilterResonanceComp()
{
    printf("\n=== FilterResonanceComp Tests ===\n");

    // Gain compensation decreases as resonance increases (bass thinning)
    float gc0 = FilterResonanceComp::computeGainCompensation(0.0f);
    float gc1 = FilterResonanceComp::computeGainCompensation(1.0f);
    check(gc0 > gc1, "gain comp decreases with resonance");
    check(std::abs(gc0 - 1.0f) < 0.01f, "gain comp at res=0 is ~1.0");

    // Effective feedback
    float fb0 = FilterResonanceComp::computeEffectiveFeedback(0.0f);
    float fbHalf = FilterResonanceComp::computeEffectiveFeedback(0.5f);
    float fb1 = FilterResonanceComp::computeEffectiveFeedback(1.0f);
    check(fb0 < fbHalf && fbHalf < fb1, "feedback monotonic with resonance");
    check(fb1 > 4.0f, "feedback at max resonance > 4.0 (self-oscillation)");

    // Self-oscillation threshold at ~0.82
    float fbOsc = FilterResonanceComp::computeEffectiveFeedback(0.83f);
    check(fbOsc > 3.9f, "resonance > 0.82 enters self-oscillation zone");
}

static void testVoxWaveOscillator()
{
    printf("\n=== VoxWaveOscillator Tests ===\n");

    VoxWaveOscillator vox;
    vox.prepare(44100.0);
    vox.setFrequency(220.0f);
    vox.setVowel(0.0f);  // 'A'
    float voxA = vox.getNextSample();
    check(voxA >= -1.0f && voxA <= 1.0f, "VoxWave A vowel in range [-1,1]");

    vox.setVowel(0.5f);  // 'I'
    float voxI = vox.getNextSample();
    check(voxI >= -1.0f && voxI <= 1.0f, "VoxWave I vowel in range [-1,1]");

    // Different vowels produce different output
    vox.reset();
    vox.setVowel(0.0f);
    float valA = 0.0f;
    for (int i = 0; i < 10; ++i)
        valA += vox.getNextSample();

    vox.reset();
    vox.setVowel(1.0f);
    float valU = 0.0f;
    for (int i = 0; i < 10; ++i)
        valU += vox.getNextSample();

    check(std::abs(valA - valU) > 0.0001f, "VoxWave A vs U produce different output");
}

static void testMidiMapTelemetry()
{
    // --- 9. MIDI Map & NRPN Telemetry Tests ---
    printf("\n[Test 9] MIDI Map & NRPN Telemetry...\n");

    // Canonical CC lookup
    const auto *cutoffInfo = MIDIMap::findByCC(74);
    check(cutoffInfo != nullptr, "MIDIMap finds CC#74 (Filter Cutoff)");
    if (cutoffInfo != nullptr)
    {
        float pVal = MIDIMap::midiValueToParamValue(*cutoffInfo, 127);
        check(std::abs(pVal - 127.0f) < 0.01f, "CC#74 max value maps to 127.0");
        int mVal = MIDIMap::paramValueToMidiValue(*cutoffInfo, 64.0f);
        check(mVal == 64, "Cutoff 64.0 maps back to MIDI 64");
    }

    const auto *resInfo = MIDIMap::findByCC(71);
    check(resInfo != nullptr, "MIDIMap finds CC#71 (Filter Resonance)");

    // Canonical NRPN lookup
    const auto *dwgsInfo = MIDIMap::findByNRPN(2, 0);
    check(dwgsInfo != nullptr, "MIDIMap finds NRPN (2, 0) for DWGS Wave");

    // NRPN Parser State Machine
    NRPNParser parser;
    NRPNMessage msg;
    bool completed = false;

    // Send CC#99 (MSB=2)
    completed = parser.processCC(1, 99, 2, msg);
    check(!completed, "NRPN incomplete after MSB");

    // Send CC#98 (LSB=10 - EQ Low Freq)
    completed = parser.processCC(1, 98, 10, msg);
    check(!completed, "NRPN incomplete after LSB");

    // Send CC#6 (Data MSB=3)
    completed = parser.processCC(1, 6, 3, msg);
    check(completed, "NRPN complete after Data MSB");
    check(msg.nrpnMSB == 2 && msg.nrpnLSB == 10 && msg.dataMSB == 3, "NRPN message values match");

    const auto *eqLowInfo = MIDIMap::findByNRPN(msg.nrpnMSB, msg.nrpnLSB);
    check(eqLowInfo != nullptr, "Resolved parsed NRPN to EQ Low Freq parameter");

    // Test buffer encoding
    juce::MidiBuffer outBuf;
    NRPNParser::appendNRPNToBuffer(outBuf, 1, 2, 20, 1, false);
    check(outBuf.getNumEvents() == 3, "NRPN 7-bit encoded to 3 CC messages (99, 98, 6)");
}

static void testLcdMenuFormatter()
{
    // --- 10. LCD Menu Formatter Tests ---
    printf("\n[Test 10] LCD Menu Formatter...\n");
    auto playTxt = LCDMenuFormatter::formatPlayMode("A", 11, "Init Synth");
    check(playTxt.line1.length() == 16, "Play Mode Line 1 is exactly 16 chars");
    check(playTxt.line2.length() == 16, "Play Mode Line 2 is exactly 16 chars");
    check(playTxt.line1 == "Prog: A11       ", "Play Mode Line 1 content match");
    check(playTxt.line2 == "[Init Synth]    ", "Play Mode Line 2 content match");

    auto cutoffTxt = LCDMenuFormatter::formatParameter("filterCutoff", 84.0f);
    check(cutoffTxt.line1.length() == 16, "Cutoff Param Line 1 is 16 chars");
    check(cutoffTxt.line2.length() == 16, "Cutoff Param Line 2 is 16 chars");
    check(cutoffTxt.line1 == "6.VCF           ", "Cutoff Page Header match");
    check(cutoffTxt.line2 == "Cutoff     : 84 ", "Cutoff Formatted Value match");

    auto waveTxt = LCDMenuFormatter::formatParameter("osc1Wave", 3.0f);
    check(waveTxt.line1 == "3.OSC 1         ", "OSC1 Wave Page Header match");
    check(waveTxt.line2 == "Wave      :CROSS", "OSC1 Wave 3 displays CROSS matching hardware");

    // Standard DWGS (slot 0)
    auto dwgsTxt = LCDMenuFormatter::formatParameter("osc1DwgsWave", 0.0f);
    check(dwgsTxt.line1 == "3.OSC 1         ", "DWGS Page Header match");
    check(dwgsTxt.line2.length() == 16, "DWGS Param Line 2 is 16 chars");
    check(dwgsTxt.line2 == "DWGS:SynWave 1  ", "DWGS Slot 0 displays authentic SynWave 1");

    // Expanded Mode (Prophet VS slot 64)
    auto pvsTxt = LCDMenuFormatter::formatParameter("osc1DwgsWave", 64.0f);
    check(pvsTxt.line1 == "3.OSC 1         ", "Expanded Mode DWGS Page Header match");
    check(pvsTxt.line2.length() == 16, "Expanded Mode DWGS Line 2 is 16 chars");
}

static void testSysExCodec()
{
    // --- 11. SysEx 7-to-8 Codec Tests ---
    printf("\n[Test 11] SysEx Codec (7-bit <-> 8-bit)...\n");
    // Test known 8-bit pattern: 7 bytes with various high bits set
    std::vector<uint8_t> original8Bit = {0x81, 0x02, 0xFF, 0x7E, 0xA5, 0x5A, 0xC3};
    std::vector<uint8_t> packed7Bit;
    bool packOk = SysExCodec::pack8to7(original8Bit.data(), original8Bit.size(), packed7Bit);
    check(packOk, "8-to-7 packing succeeded");
    check(packed7Bit.size() == 8, "7 bytes packed into 8 bytes (1 MSB collector + 7 data)");

    // Verify all packed bytes are <= 0x7F
    bool allValidMidi = true;
    for (uint8_t b : packed7Bit)
        if (b > 0x7F)
            allValidMidi = false;
    check(allValidMidi, "All packed bytes are valid 7-bit MIDI values (<= 0x7F)");

    // Unpack back
    std::vector<uint8_t> unpacked8Bit;
    bool unpackOk = SysExCodec::unpack7to8(packed7Bit.data(), packed7Bit.size(), unpacked8Bit);
    check(unpackOk, "7-to-8 unpacking succeeded");
    check(unpacked8Bit.size() == original8Bit.size(), "Unpacked size matches original");
    check(unpacked8Bit == original8Bit, "Unpacked 8-bit data matches original payload exactly (lossless roundtrip)");

    // Larger buffer round-trip (e.g. 256 bytes program size)
    std::vector<uint8_t> progBuffer(256);
    for (size_t i = 0; i < 256; ++i)
        progBuffer[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);

    std::vector<uint8_t> packedProg;
    SysExCodec::pack8to7(progBuffer.data(), progBuffer.size(), packedProg);
    std::vector<uint8_t> unpackedProg;
    SysExCodec::unpack7to8(packedProg.data(), packedProg.size(), unpackedProg);
    check(unpackedProg == progBuffer, "256-byte MS2000 program buffer round-trip exact match");
}

static void testFilterVocoderStability()
{
    // --- Test 12: Filter & Vocoder Stability Auditing (Impulse, Resonance Headroom, Nyquist Sanity) ---
    printf("\n[Test 12] Filter & Vocoder Stability Auditing...\n");

    // 12.1: Dirac Impulse Test on Filter
    MultiModeFilter filterAudit;
    filterAudit.prepare(testSampleRate);
    filterAudit.setType(FilterType::LPF24);
    filterAudit.setCutoff(1000.0f);
    filterAudit.setResonance(0.7f);

    bool filterHasNanOrInf = false;
    float diracSample = filterAudit.process(1.0f);
    if (std::isnan(diracSample) || std::isinf(diracSample))
        filterHasNanOrInf = true;

    for (int i = 0; i < 512; ++i)
    {
        float out = filterAudit.process(0.0f);
        if (std::isnan(out) || std::isinf(out))
            filterHasNanOrInf = true;
    }
    check(!filterHasNanOrInf, "Dirac impulse response contains zero NaN/Inf and decays stably");

    // 12.2: Extreme Resonance & Vocoder Loop Headroom Test
    Vocoder16Band vocoderAudit;
    vocoderAudit.prepare(testSampleRate);
    vocoderAudit.setEnabled(true);
    vocoderAudit.setFormantShift(2);  // Max shift +2
    vocoderAudit.setHPFLevel(1.0f);

    float maxVocoderPeak = 0.0f;
    bool vocoderHasNanOrInf = false;
    uint32_t seed = 12345;

    for (int i = 0; i < 512; ++i)
    {
        seed = seed * 1664525u + 1013904223u;
        float noiseMod = (static_cast<float>(seed & 0x7FFFFFFF) / 1073741824.0f) - 1.0f;
        seed = seed * 1664525u + 1013904223u;
        float noiseCar = (static_cast<float>(seed & 0x7FFFFFFF) / 1073741824.0f) - 1.0f;

        float outL = 0.0f, outR = 0.0f;
        vocoderAudit.process(noiseMod, noiseCar, outL, outR);

        if (std::isnan(outL) || std::isnan(outR) || std::isinf(outL) || std::isinf(outR))
        {
            vocoderHasNanOrInf = true;
        }

        maxVocoderPeak = std::max(maxVocoderPeak, std::max(std::abs(outL), std::abs(outR)));
    }
    check(!vocoderHasNanOrInf, "Vocoder 16-band unrolled engine generates zero NaN/Inf under white noise stress");
    check(maxVocoderPeak < 6.0f, "Vocoder dynamic headroom is stably bounded (< +15dB limit)");

    // 12.2a: microKORG 8-band vocoder mode check
    vocoderAudit.set8BandMode(true);
    check(vocoderAudit.is8BandMode(), "Vocoder 8-band mode can be toggled for microKORG profile");
    float out8L = 0.0f, out8R = 0.0f;
    vocoderAudit.process(0.5f, 0.5f, out8L, out8R);
    check(std::isfinite(out8L) && std::isfinite(out8R), "Vocoder 8-band mode generates valid finite output");
    vocoderAudit.set8BandMode(false);
    check(!vocoderAudit.is8BandMode(), "Vocoder restores 16-band mode for MS2000 profile");

    // 12.2b: Internal Formant Self-Modulator & Formant Hold Verification
    Vocoder16Band vocoderSelfMod;
    vocoderSelfMod.prepare(testSampleRate);
    vocoderSelfMod.setEnabled(true);
    vocoderSelfMod.setCutoffOffset(0.25f);
    vocoderSelfMod.setResonance(0.7f);
    vocoderSelfMod.setEfSense(0.5f);
    float selfModRms = 0.0f;
    for (int i = 0; i < 1024; ++i)
    {
        float sawCarrier =
            2.0f * std::fmod(static_cast<float>(i) * 110.0f / static_cast<float>(testSampleRate), 1.0f) - 1.0f;
        float oL = 0.0f, oR = 0.0f;
        vocoderSelfMod.process(0.0f, sawCarrier, oL, oR);
        selfModRms += (oL * oL + oR * oR);
    }
    check(selfModRms > 1.0e-4f, "Vocoder generates audible formant speech modulation when played without external mic");

    // 12.3: Nyquist Sanity Check
    float illegalHighCutoff = 99000.0f;
    float safeCutoff = std::min(illegalHighCutoff, static_cast<float>(testSampleRate * 0.49));
    check(safeCutoff < testSampleRate * 0.5,
          "Cutoff calculation is bounded below Nyquist limit to prevent filter collapse");
}

static void testVoiceAllocation()
{
    // --- Test 13: Multitimbric Dual Voice Allocation & ABD Ultra 32-Voice Capacity ---
    printf("\n[Test 13] Multitimbric Voice Allocation & ABD Ultra 32-Voice Capacity...\n");
    fflush(stdout);
    auto vm = std::make_unique<VoiceManager>();
    printf("  [debug] vm allocated\n");
    fflush(stdout);
    vm->prepare(testSampleRate);
    printf("  [debug] vm prepared\n");
    fflush(stdout);

    // 13.1: Hardware 4-Voice Limit
    vm->setMaxPolyphony(4);
    check(vm->getMaxPolyphony() == 4, "Hardware profile limits active capacity to 4 voices");

    for (int n = 60; n < 68; ++n)
        vm->noteOn(n, 0.8f);
    printf("  [debug] active after 8 notes: %zu\n", vm->getActiveVoiceCount());
    fflush(stdout);
    check(vm->getActiveVoiceCount() <= 4, "Polyphonic voice stealing strictly caps active voices to 4");

    // 13.2: ABD Ultra 32-Voice Mode
    vm->allNotesOff();
    vm->setMaxPolyphony(32);
    check(vm->getMaxPolyphony() == 32, "ABD Ultra profile expands active capacity to 32 voices");

    for (int n = 36; n < 36 + 32; ++n)
        vm->noteOn(n, 0.8f);
    printf("  [debug] active after 32 notes: %zu\n", vm->getActiveVoiceCount());
    fflush(stdout);
    check(vm->getActiveVoiceCount() == 32, "ABD Ultra successfully sustains 32 simultaneous polyphonic voices");

    vm->allNotesOff();
    check(vm->getActiveVoiceCount() == 0, "allNotesOff cleanly terminates all active voices");

    // 13.3: VoiceManager audio rendering check
    vm->setMaxPolyphony(4);
    VoiceParameters vp {};
    vm->applyBlockParams(vp);
    vm->noteOn(60, 0.8f);
    float testL = 0.0f, testR = 0.0f;
    float peakRender = 0.0f;
    for (int i = 0; i < 480; ++i)
    {
        vm->process(testL, testR);
        peakRender = std::max(peakRender, std::max(std::abs(testL), std::abs(testR)));
    }
    printf("  [debug] VoiceManager peak audio rendering: %f\n", peakRender);
    fflush(stdout);
    check(peakRender > 0.01f, "VoiceManager renders non-silent audio on noteOn");

    // 13.4: SynthEngine + APVTS audio rendering check
    juce::AudioProcessorValueTreeState::ParameterLayout layout = ParameterRegistry::createParameterLayout();
    // Procesador dummy + APVTS con el patch INIT cargado: el fixture que comparten todos
    // los tests de SysEx (ver `dummyFixture()`), porque antes vivía aquí y los demás lo usaban.
    auto &apvts = dummyFixture().apvts;

    SynthEngine engine(apvts);
    engine.prepare(testSampleRate, 480);
    engine.noteOn(1, 60, 0.8f);

    juce::AudioBuffer<float> testBuffer(2, 480);
    testBuffer.clear();
    juce::MidiBuffer dummyMidi;
    engine.processBlock(testBuffer, dummyMidi, nullptr);

    float enginePeakL = testBuffer.getMagnitude(0, 0, 480);
    float enginePeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] SynthEngine peak L: %f, R: %f\n", enginePeakL, enginePeakR);
    fflush(stdout);
    check(enginePeakL > 0.01f && enginePeakR > 0.01f, "SynthEngine with Init Patch renders audio in processBlock");

    // Test Point 4: Pre-Filter Diagnostic Tone with NO note active
    engine.allNotesOff();
    engine.setDiagnosticTone(4, 440.0f, 0.25f);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float diag4PeakL = testBuffer.getMagnitude(0, 0, 480);
    float diag4PeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Point 4 (PreFilter) peak L: %f, R: %f\n", diag4PeakL, diag4PeakR);
    fflush(stdout);
    check(diag4PeakL > 0.01f && diag4PeakR > 0.01f, "Diag Point 4 (PreFilter) renders audio when idle");

    // Test Point 5: Direct OSC1 Diagnostic Tone with NO note active
    engine.setDiagnosticTone(5, 440.0f, 0.25f);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float diag5PeakL = testBuffer.getMagnitude(0, 0, 480);
    float diag5PeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Point 5 (OSC1) peak L: %f, R: %f\n", diag5PeakL, diag5PeakR);
    fflush(stdout);
    check(diag5PeakL > 0.01f && diag5PeakR > 0.01f, "Diag Point 5 (OSC1) renders audio when idle");

    engine.setDiagnosticTone(0, 440.0f, 0.0f);

    // Test Diagnostic Bypasses: Filter & VCA bypass
    engine.setDiagnosticBypass("filter", true);
    engine.setDiagnosticBypass("vca", true);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float bypassPeakL = testBuffer.getMagnitude(0, 0, 480);
    float bypassPeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Bypass (Filter+VCA) peak L: %f, R: %f\n", bypassPeakL, bypassPeakR);
    fflush(stdout);
    check(bypassPeakL > 0.01f && bypassPeakR > 0.01f,
          "Diag Bypass renders continuous audio through voice 0 without noteOn");

    engine.resetAllDiagnosticBypasses();
    engine.reset();
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float resetPeakL = testBuffer.getMagnitude(0, 0, 480);
    check(resetPeakL < 0.001f, "Resetting bypasses returns synth to quiet idle state");

    // --- Scope Tap Capture (ABDScope integration) ---
    {
        auto &sc = engine.getScopeCollector();
        check(sc.getTapCount() == 6,
              "SynthEngine registers 6 scope taps (master_out/pre_fx/osc_mix/post_filter/post_vca/lfo1)");
        check(sc.getTap(0) != nullptr && sc.getTap(0)->isActive(), "Scope master_out tap auto-activated on register");
        check(sc.getTap(2) != nullptr && !sc.getTap(2)->isActive(), "Non-first taps start inactive (on-demand policy)");

        // Fresh engine => fresh collector, deterministic empty-buffer baseline.
        SynthEngine scopeEngine(apvts);
        scopeEngine.prepare(testSampleRate, 480);
        scopeEngine.noteOn(1, 55, 0.8f);

        juce::AudioBuffer<float> scopeBuf(2, 480);
        scopeBuf.clear();
        juce::MidiBuffer scopeMidi;
        const auto &sc2 = scopeEngine.getScopeCollector();
        const size_t before = sc2.getTap(0)->getAvailableRead();
        scopeEngine.processBlock(scopeBuf, scopeMidi, nullptr);
        const size_t after = sc2.getTap(0)->getAvailableRead();
        printf("  [debug] Scope(fresh) master_out available samples: before=%zu after=%zu\n", before, after);
        fflush(stdout);
        check(after > 0, "Scope master_out tap captures samples into ring buffer after processBlock");
    }
}

static void testEmbeddedWebUiAssets()
{
    // --- Test 15: Embedded WebUI Binary Data integrity ---
    printf("\n[Test 15] Embedded WebUI Binary Data (WebUIAssets)...\n");
    {
        // Regression test: the ABD Bank Manager WebUI must NOT be embedded in the
        // synth binary. If it sneaks back in (e.g. via a GLOB_RECURSE matching
        // WebUI/abdbank/index.html), the resource provider serves the Bank Manager
        // as the root page of the standalone instead of the synth.
        int indexHtmlCount = 0;
        int abdbankCount = 0;
        juce::String synthIndexResource;

        for (int i = 0; i < WebUIAssets::namedResourceListSize; ++i)
        {
            juce::String orig = juce::String::fromUTF8(WebUIAssets::originalFilenames[i]);
            if (orig.endsWithIgnoreCase("index.html"))
            {
                ++indexHtmlCount;
                synthIndexResource = juce::String::fromUTF8(WebUIAssets::namedResourceList[i]);
            }
            if (orig.containsIgnoreCase("abdbank"))
                ++abdbankCount;
        }

        check(indexHtmlCount == 1, "Embedded binary contains exactly one index.html (the synth root)");
        check(abdbankCount == 0, "Embedded binary contains no abdbank resources");

        // The single index.html must be the synth's (title "ABDMS2000 - Synthesizer"),
        // not the Bank Manager's ("ABD Universal Bank Manager").
        bool isSynthIndex = false;
        if (synthIndexResource.isNotEmpty())
        {
            int dataSize = 0;
            const char *data = WebUIAssets::getNamedResource(synthIndexResource.toRawUTF8(), dataSize);
            if (data != nullptr && dataSize > 0)
            {
                juce::String content = juce::String::fromUTF8(data, dataSize);
                isSynthIndex = content.contains("ABDMS2000 - Synthesizer");
            }
        }
        check(isSynthIndex, "Embedded index.html is the synth root page, not the Bank Manager");
    }
}

static void testHostModelAnnouncement()
{
    // --- Test 16: hostModel announcement (despacho de handleJsEvent) ---
    printf("\n[Test 16] Bridge hostModel announcement (HostModelAnnouncement)...\n");
    {
        // The ABD Bank Manager embedded in this plugin filters its model selector
        // with the modelId the host announces through the cppToWebui action
        // `hostModel`. BridgeActions::handleJsEvent() resolves that announcement
        // with announceHostModelForAction() for every action it receives, so this
        // guards the dispatch: requestState / requestFullState must announce.
        std::vector<juce::var> emitted;
        JsMessageSink sink = [&emitted](const juce::var &message) { emitted.push_back(message); };

        // requestState: sent by the embedded ABD Bank Manager once its bridge is
        // subscribed to hostModel (ABDBankManager/WebUI/src/app.js).
        emitted.clear();
        const bool stateAnnounces = announceHostModelForAction("requestState", sink);
        check(stateAnnounces, "requestState announces the host model");
        check(emitted.size() == 1, "requestState emits exactly one JS message");
        check(emitted.size() == 1 && emitted[0].getProperty("type", "").toString() == "hostModel",
              "requestState emits the cppToWebui action 'hostModel'");
        check(emitted.size() == 1
                  && emitted[0].getProperty("data", juce::var()).getProperty("modelId", "").toString() == kHostModelId,
              "hostModel payload carries the generated host modelId");

        // requestFullState: host WebUI handshake; same idempotent announcement.
        emitted.clear();
        const bool fullStateAnnounces = announceHostModelForAction("requestFullState", sink);
        check(fullStateAnnounces, "requestFullState announces the host model");
        check(emitted.size() == 1 && emitted[0].getProperty("type", "").toString() == "hostModel",
              "requestFullState emits 'hostModel'");

        // Actions that must NOT announce anything (incl. near-misses).
        const char *nonTriggers[] = {
            "setParam", "noteOn", "requestStateX", "requestFullStates", "getRawProgramData", "hostModel", ""};
        int nonTriggerFailures = 0;
        for (const char *action : nonTriggers)
        {
            emitted.clear();
            if (announceHostModelForAction(action, sink) || !emitted.empty())
                ++nonTriggerFailures;
        }
        check(nonTriggerFailures == 0, "unrelated actions do not emit hostModel");

        // Empty sink must not crash and must not report an emission.
        emitted.clear();
        announceHostModelForAction("requestState", {});
        check(emitted.empty(), "a null sink is tolerated");

        // The message must serialise to the shape the WebUI parses:
        // { type: 'hostModel', data: { modelId } }.
        const auto json = juce::JSON::toString(hostModelMessage());
        check(json.contains("\"type\"") && json.contains("hostModel") && json.contains(kHostModelId),
              "hostModel message serialises with type + generated modelId");
        // El *valor* lo guarda el test JS contra el contrato canonico
        // (WebUI/tests/unit/hostModelAnnouncement.test.js): aqui basta comprobar
        // que el C++ lo toma del header generado y no de un literal propio.
        check(hostModelId() == juce::String(kHostModelId) && hostModelId().isNotEmpty(),
              "hostModelId() comes from the generated contract header");

        // --- Ficha del host (requestHostInfo -> hostInfo) ---
        //
        // El sello viaja para que el Bank Manager embebido pueda decir si el
        // binario que lo hospeda es antiguo (y con que nivel de puente habla) en
        // vez de insinuarlo cuando el catalogo queda bloqueado.
        emitted.clear();
        const bool infoAnswered = answerHostInfoForAction("requestHostInfo", sink);
        check(infoAnswered, "requestHostInfo is answered with the host info");
        check(emitted.size() == 1 && emitted[0].getProperty("type", "").toString() == "hostInfo",
              "requestHostInfo emits the cppToWebui action 'hostInfo'");
        check(emitted.size() == 1
                  && emitted[0].getProperty("data", juce::var()).getProperty("modelId", "").toString() == kHostModelId,
              "hostInfo carries the same identity as the announcement");
        check(emitted.size() == 1
                  && static_cast<int>(emitted[0].getProperty("data", juce::var()).getProperty("protocol", 0))
                         == kHostBridgeProtocol,
              "hostInfo reports the bridge protocol level");
        check(emitted.size() == 1
                  && emitted[0].getProperty("data", juce::var()).getProperty("buildStamp", "").toString().isNotEmpty(),
              "hostInfo reports the build stamp of the binary");
        check(
            emitted.size() == 1
                && emitted[0].getProperty("data", juce::var()).getProperty("buildRevision", "").toString().isNotEmpty(),
            "hostInfo reports the code revision");

        // El anuncio lleva el mismo sello (un Bank Manager antiguo lo ignora).
        check(hostModelMessage().getProperty("data", juce::var()).getProperty("buildStamp", "").toString().isNotEmpty()
                  && static_cast<int>(hostModelMessage().getProperty("data", juce::var()).getProperty("protocol", 0))
                         == kHostBridgeProtocol,
              "hostModel also carries protocol + build stamp");

        // El nivel declarado tiene que cubrir lo que el binario habla de verdad:
        // 4 = puente MIDI de hardware (el fetch de un banco real por el bridge).
        check(kHostBridgeProtocol >= 4, "the declared bridge protocol covers the hardware MIDI pipe");

        // Politica: pedir la ficha NO es anunciar el modelo, y al reves.
        check(!actionAnnouncesHostModel("requestHostInfo"), "requestHostInfo does not piggyback the announcement");
        check(!actionAnswersHostInfo("requestState") && !actionAnswersHostInfo("requestFullState")
                  && actionAnswersHostInfo("requestHostInfo"),
              "requestHostInfo is a separate action from the announcement triggers");

        emitted.clear();
        answerHostInfoForAction("requestHostInfo", {});
        check(emitted.empty(), "a null sink is tolerated for the host info too");
    }
}

static void testBridgeActionsDispatch()
{
    // --- Test 17: BridgeActions::handleJsEvent (despacho directo) ---
    printf("\n[Test 17] BridgeActions::handleJsEvent dispatch (BridgeHost + sink inyectados)...\n");
    {
        // El puerto y el sink se inyectan precisamente para poder probar esto: el despacho
        // de eventos del WebUI corre con los motores reales, sin WebView y sin editor.
        // El arnés (host doble + puente + bandeja de mensajes) vive a nivel de namespace en
        // `BridgeTestHarness` porque lo comparten los tests 17/18/19.
        BridgeTestHarness harness;
        auto &host = harness.host;
        auto &bridge = harness.bridge;
        auto &messages = harness.messages;
        auto deliver = [&harness](const char *json) { harness.deliver(json); };
        auto typeOf = [](const juce::var &message) { return BridgeTestHarness::typeOf(message); };
        auto modelOf = [](const juce::var &message)
        { return message.getProperty("data", juce::var()).getProperty("modelId", "").toString(); };
        auto hostModelCount = [&messages, &typeOf]()
        {
            int count = 0;
            for (const auto &message : messages)
                if (typeOf(message) == "hostModel")
                    ++count;
            return count;
        };

        // requestState: the embedded ABD Bank Manager announces it is ready.
        messages.clear();
        deliver(R"({"action":"requestState"})");
        check(messages.size() == 1 && typeOf(messages[0]) == "hostModel",
              "handleJsEvent(requestState) emits hostModel");
        check(messages.size() == 1 && modelOf(messages[0]) == kHostModelId,
              "handleJsEvent(requestState) carries the host modelId");

        // requestFullState: host WebUI handshake -> hostModel (announced first)
        // followed by the parameter sync.
        messages.clear();
        deliver(R"({"action":"requestFullState"})");
        check(messages.size() == 2 && typeOf(messages[0]) == "hostModel",
              "handleJsEvent(requestFullState) emits hostModel");
        check(messages.size() == 2 && typeOf(messages[1]) == "syncAllParams",
              "handleJsEvent(requestFullState) also emits syncAllParams");

        // requestHostInfo: la ficha del host (identidad + sello + nivel de puente).
        messages.clear();
        deliver(R"({"action":"requestHostInfo"})");
        check(messages.size() == 1 && typeOf(messages[0]) == "hostInfo",
              "handleJsEvent(requestHostInfo) emits hostInfo");
        check(messages.size() == 1
                  && messages[0].getProperty("data", juce::var()).getProperty("protocol", 0)
                         == juce::var(kHostBridgeProtocol),
              "handleJsEvent(requestHostInfo) carries the bridge protocol");

        // Unrelated actions (incl. near misses) must not announce anything.
        const char *otherActions[] = {R"({"action":"noteOn","note":60,"velocity":0.8})",
                                      R"({"action":"noteOff","note":60})",
                                      R"({"action":"pitchBend","value":0.5})",
                                      R"({"action":"allNotesOff"})",
                                      R"({"action":"toggleScope"})",
                                      R"({"action":"requestStateX"})"};
        int leaked = 0;
        for (const char *json : otherActions)
        {
            messages.clear();
            deliver(json);
            leaked += hostModelCount();
        }
        check(leaked == 0, "unrelated actions do not emit hostModel");

        // Malformed / non-object messages are ignored without exceptions.
        messages.clear();
        bridge.handleJsEvent(juce::var());
        bridge.handleJsEvent(juce::JSON::parse("\"just a string\""));
        bridge.handleJsEvent(juce::JSON::parse("{ not json"));
        check(messages.empty(), "non-object messages are ignored");

        // setParam reaches the injected APVTS.
        const auto &parameters = ParameterRegistry::getAllParameters();
        if (!parameters.empty())
        {
            juce::DynamicObject::Ptr setMsg = new juce::DynamicObject();
            setMsg->setProperty("action", "setParam");
            setMsg->setProperty("paramId", juce::String(parameters.front().id));
            setMsg->setProperty("value", 1.0);
            bridge.handleJsEvent(juce::var(setMsg.get()));

            auto *parameter = host.getAPVTS().getParameter(juce::String(parameters.front().id));
            check(parameter != nullptr && parameter->getValue() > 0.5,
                  "handleJsEvent(setParam) writes through the injected host APVTS");
        }

        // SysEx round-trip uses the injected SysExManager.
        messages.clear();
        deliver(R"({"action":"getAllProgramsData"})");
        check(messages.size() == 1 && typeOf(messages[0]) == "allProgramsDataResponse",
              "handleJsEvent(getAllProgramsData) emits allProgramsDataResponse");

        // Program selection goes through the host port and re-syncs params.
        messages.clear();
        deliver(R"({"action":"selectProgram","index":3})");
        check(host.getCurrentProgram() == 3, "handleJsEvent(selectProgram) forwards the index to the host");
        check(messages.size() == 1 && typeOf(messages[0]) == "syncAllParams",
              "handleJsEvent(selectProgram) emits syncAllParams");

        // toggleScope callback wiring.
        bool toggled = false;
        bridge.setOnToggleScope([&toggled]() { toggled = true; });
        deliver(R"({"action":"toggleScope"})");
        check(toggled, "handleJsEvent(toggleScope) invokes the registered callback");

        // A bridge without a sink must tolerate actions silently.
        BridgeActions silentBridge(host);
        silentBridge.handleJsEvent(juce::JSON::parse(R"({"action":"requestState"})"));
        check(true, "a bridge without JS sink tolerates actions");
    }
}

static void testHardwareMidiBridge()
{
    // Host doble + puente + bandeja de mensajes: el arnés compartido (`BridgeTestHarness`).
    BridgeTestHarness harness;
    {
        auto &host = harness.host;
        auto &messages = harness.messages;
        auto deliver = [&harness](const char *json) { harness.deliver(json); };
        auto typeOf = [](const juce::var &message) { return BridgeTestHarness::typeOf(message); };

        // --- Test 18: puente MIDI de hardware (el otro extremo del Bank Manager) ---
        printf("\n[Test 18] Hardware MIDI bridge (hardware.listen / hardware.send / hardware.receive)...\n");
        {
            // Doble de hardware: captura lo que sale y deja inyectar lo que entra.
            struct FakeHardware
            {
                std::vector<juce::MemoryBlock> sent;
                bool openOutput = true;
                bool openInput = true;
                juce::String outputName = "Fake MIDI Out";
                juce::String inputName = "Fake MIDI In";
                int listenCalls = 0;
            } hardware;

            std::vector<juce::var> toWebUi;
            auto &transport = host.getHardwareMidiTransport();
            transport.bind(
                [&hardware](const juce::MemoryBlock &bytes) -> HardwareMidiTransport::Outcome
                {
                    if (!hardware.openOutput)
                        return {false, "No MIDI output device available for the hardware transfer"};
                    hardware.sent.push_back(bytes);
                    return {true, hardware.outputName};
                },
                [&hardware]() -> HardwareMidiTransport::Outcome
                {
                    ++hardware.listenCalls;
                    if (!hardware.openInput)
                        return {false, "No MIDI input device available for the hardware transfer"};
                    return {true, hardware.inputName};
                },
                [&toWebUi](const juce::var &message) { toWebUi.push_back(message); });

            auto dataOf = [](const juce::var &message) { return message.getProperty("data", juce::var()); };

            // Enumeración y selección explícita: no se acepta ningún puerto implícito.
            transport.setPortListFunction(
                []
                {
                    juce::DynamicObject::Ptr ports = new juce::DynamicObject();
                    juce::Array<juce::var> outputs;
                    juce::Array<juce::var> inputs;
                    juce::DynamicObject::Ptr out = new juce::DynamicObject();
                    out->setProperty("identifier", "out-test");
                    out->setProperty("name", "Test MIDI Out");
                    outputs.add(juce::var(out.get()));
                    juce::DynamicObject::Ptr in = new juce::DynamicObject();
                    in->setProperty("identifier", "in-test");
                    in->setProperty("name", "Test MIDI In");
                    inputs.add(juce::var(in.get()));
                    ports->setProperty("outputs", outputs);
                    ports->setProperty("inputs", inputs);
                    return juce::var(ports.get());
                });
            messages.clear();
            deliver(R"({"action":"hardware.listPorts"})");
            check(messages.size() == 1 && typeOf(messages[0]) == "hardware.ports"
                      && dataOf(messages[0]).getProperty("outputs", juce::var()).size() == 1
                      && dataOf(messages[0]).getProperty("inputs", juce::var()).size() == 1,
                  "hardware.listPorts returns the host input/output identifiers");

            deliver(R"({"action":"hardware.selectPorts","outputId":"out-test","inputId":"in-test"})");
            check(transport.getSelectedOutputId() == "out-test" && transport.getSelectedInputId() == "in-test",
                  "hardware.selectPorts stores the explicit identifiers");

            // `hardware.listen`: el Bank Manager dice que ya escucha -> ack con el dispositivo.
            messages.clear();
            deliver(R"({"action":"hardware.listen"})");
            check(hardware.listenCalls == 1 && transport.isListening(),
                  "handleJsEvent(hardware.listen) opens the host MIDI input");
            check(messages.size() == 1 && typeOf(messages[0]) == "hardware.listen.ack",
                  "handleJsEvent(hardware.listen) acknowledges the request");
            check(messages.size() == 1 && dataOf(messages[0]).getProperty("listening", juce::var()) == juce::var(true),
                  "the listen ack reports the listening state");
            check(messages.size() == 1
                      && dataOf(messages[0]).getProperty("device", "").toString() == hardware.inputName,
                  "the listen ack names the MIDI input device the host opened");

            // `hardware.send`: bytes base64 -> el mismo bloque en el dispositivo.
            // OJO: el bridge habla base64 *estándar* (el `atob`/`btoa` del WebUI), no
            // `MemoryBlock::toBase64Encoding()`, que en JUCE codifica `<tamaño>.<datos>`.
            // Mezclarlos no falla al compilar: falla en silencio al decodificar.
            const juce::uint8 sysexBytes[] = {0xF0, 0x42, 0x30, 0x00, 0x01, 0xF7};
            const juce::MemoryBlock sysex(sysexBytes, sizeof(sysexBytes));
            const auto sysexPayload = juce::Base64::toBase64(sysex.getData(), sysex.getSize());
            const auto sendJson = "{\"action\":\"hardware.send\",\"payload\":\"" + sysexPayload + "\"}";

            // Ida y vuelta del formato que consume el JS (`atob`): mismo contrato que
            // el core del Bank Manager standalone.
            {
                juce::MemoryOutputStream decoded;
                const bool ok = juce::Base64::convertFromBase64(decoded, sysexPayload);
                const juce::MemoryBlock roundTrip(decoded.getData(), decoded.getDataSize());
                check(ok && roundTrip == sysex, "the bridge payload is plain base64 (decodes back to the same bytes)");
            }

            messages.clear();
            deliver(sendJson.toRawUTF8());
            check(hardware.sent.size() == 1 && hardware.sent.front() == sysex,
                  "handleJsEvent(hardware.send) delivers the exact bytes to the host MIDI output");
            check(messages.size() == 1 && typeOf(messages[0]) == "hardware.sent",
                  "handleJsEvent(hardware.send) reports a successful send");
            check(messages.size() == 1 && dataOf(messages[0]) == juce::var(static_cast<int>(sizeof(sysexBytes))),
                  "the send report carries the byte count");

            // SysEx del dispositivo -> WebUI, con la forma que consume hardwareMidi.js.
            toWebUi.clear();
            transport.deliverIncoming(sysex);
            check(toWebUi.size() == 1 && toWebUi.front().getProperty("type", "").toString() == "hardware.receive",
                  "incoming hardware bytes are forwarded as hardware.receive");
            check(toWebUi.size() == 1 && dataOf(toWebUi.front()).getProperty("payload", "").toString() == sysexPayload,
                  "hardware.receive carries the plain base64 payload the WebUI decodes with atob");
            check(toWebUi.size() == 1
                      && dataOf(toWebUi.front()).getProperty("size", juce::var())
                             == juce::var(static_cast<int>(sizeof(sysexBytes))),
                  "hardware.receive reports the byte count");

            // Payload inválido: motivo explícito y sin tocar el dispositivo.
            messages.clear();
            deliver(R"({"action":"hardware.send","payload":42})");
            deliver(R"({"action":"hardware.send","payload":""})");
            check(hardware.sent.size() == 1 && messages.size() == 2 && typeOf(messages[0]) == "hardware.error"
                      && typeOf(messages[1]) == "hardware.error",
                  "invalid base64 payloads report hardware.error and reach no device");

            // Host sin dispositivos: errores con motivo en vez de silencio (antes se
            // despachaba nada y el fetch del Bank Manager solo podía morir por timeout).
            hardware.openOutput = false;
            hardware.openInput = false;
            messages.clear();
            deliver(R"({"action":"hardware.listen"})");
            deliver(sendJson.toRawUTF8());
            std::vector<juce::String> reasons;
            for (const auto &message : messages)
                if (typeOf(message) == "hardware.error")
                    reasons.push_back(message.getProperty("data", "").toString());
            check(reasons.size() == 2 && reasons[0].containsIgnoreCase("input")
                      && reasons[1].containsIgnoreCase("output"),
                  "a host with no MIDI devices explains why instead of failing silently");
            check(!transport.isListening(), "a failed listen leaves the transport not listening");

            // Editor cerrado: sin hardware enlazado, los bytes que lleguen se ignoran.
            transport.unbind();
            toWebUi.clear();
            transport.deliverIncoming(sysex);
            check(toWebUi.empty(), "an unbound transport drops incoming bytes");
            messages.clear();
            deliver(sendJson.toRawUTF8());
            check(messages.size() == 1 && typeOf(messages[0]) == "hardware.error",
                  "an unbound transport reports hardware.error on send");
        }
    }
}

static void testSoftwarePresetTransport()
{
    // Host doble + puente + bandeja de mensajes: el arnés compartido (`BridgeTestHarness`).
    BridgeTestHarness harness;
    {
        auto &host = harness.host;
        auto &bridge = harness.bridge;
        auto &messages = harness.messages;
        auto typeOf = [](const juce::var &message) { return BridgeTestHarness::typeOf(message); };

        // --- Test 19: transporte de software (el ABD Bank Manager embebido) ---
        printf("\n[Test 19] Software preset transport (preset.read/write, bank.read/write, preset.capture)...\n");
        {
            // El Bank Manager embebido no puede abrir MIDI ni leer la memoria del
            // plugin: usa los MISMOS verbos que con un equipo fisico, por el bridge
            // (ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md §6.2/§6.4). Aqui se
            // fija el contrato del cable: base64 estandar, requestId devuelto, sistema
            // `native`, memoria vs audicion (§7.2) y errores con motivo.
            auto dataOf = [](const juce::var &message) { return message.getProperty("data", juce::var()); };
            auto findOfType = [&messages, &typeOf](const juce::String &type) -> juce::var
            {
                for (const auto &message : messages)
                    if (typeOf(message) == type)
                        return message;
                return juce::var();
            };
            auto send = [&bridge](juce::DynamicObject::Ptr object) { bridge.handleJsEvent(juce::var(object.get())); };
            auto makeRequest = [](const char *action, const char *system, juce::var slot, const juce::String &requestId)
            {
                juce::DynamicObject::Ptr object = new juce::DynamicObject();
                object->setProperty("action", action);
                object->setProperty("system", system);
                if (!slot.isVoid())
                    object->setProperty("slot", slot);
                object->setProperty("requestId", requestId);
                return object;
            };
            auto decodePayloadOf = [](const juce::var &message, juce::MemoryBlock &out)
            {
                return SoftwarePresetProtocol::decodePayload(
                    message.getProperty("data", juce::var()).getProperty("payload", "").toString(), out);
            };

            auto &synthMemory = host.getSysExManager();

            // `preset.read`: el bloque de programa del slot, en base64 estandar.
            {
                const auto expected = synthMemory.getProgram(5);
                const auto expectedName = juce::String(expected.getName()).trim();

                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadPreset, "native", juce::var(5), "r1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kPresetData,
                      "preset.read is answered with preset.data");

                // Guardia: si la respuesta no llego, el check de arriba ya ha
                // fallado; aqui se evita el front() sobre la cola vacia en vez
                // de crashear (los checks de abajo fallaran con valores por defecto).
                const auto data = messages.empty() ? juce::var {} : dataOf(messages.front());
                check(data.getProperty("requestId", "").toString() == "r1",
                      "preset.data echoes the requestId the Bank Manager sent");
                check(static_cast<int>(data.getProperty("slot", -1)) == 5, "preset.data carries the requested slot");
                check(data.getProperty("name", "").toString() == expectedName, "preset.data carries the program name");

                juce::MemoryBlock decoded;
                check(!messages.empty() && decodePayloadOf(messages.front(), decoded)
                          && decoded.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE),
                      "the payload decodes to one program block");
                check(decoded.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                          && std::memcmp(
                                 decoded.getData(), expected.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                                 == 0,
                      "the payload is exactly the bytes in the synth memory");
            }

            // `preset.write` con slot: guarda el programa y lo carga (patch activo).
            {
                MS2000ProgramData incoming;
                incoming.setName("BankPatch");
                incoming.rawData[MS2000ProgramData::VOICE_BYTE] = 2;  // Unison

                auto write = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(9), "w1");
                write->setProperty("name", "BankPatch");
                write->setProperty(
                    "payload", SoftwarePresetProtocol::encodePayload(incoming.rawData.data(), incoming.rawData.size()));

                messages.clear();
                send(write);

                const auto stored = synthMemory.getProgram(9);
                check(std::memcmp(
                          stored.rawData.data(), incoming.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                          == 0,
                      "preset.write stores the block in the synth memory");
                check(juce::String(stored.getName()).trim() == "BankPatch",
                      "preset.write stores the name the Bank Manager sent");
                check(host.getCurrentProgram() == 9, "preset.write with a slot becomes the active program");

                const auto written = findOfType(SoftwarePresetProtocol::kPresetWritten);
                check(written.isObject() && dataOf(written).getProperty("requestId", "").toString() == "w1",
                      "preset.write is acknowledged with preset.written");
                check(written.isObject() && static_cast<int>(dataOf(written).getProperty("slot", -1)) == 9,
                      "preset.written reports the slot it stored");
            }

            // `preset.write` con `audition`: solo el motor, la memoria no se toca (§7.2).
            {
                const auto before = synthMemory.getProgram(9);

                MS2000ProgramData auditioned;
                auditioned.setName("Auditioned");
                auditioned.rawData[MS2000ProgramData::VOICE_BYTE] = 0;  // Mono

                auto write = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(9), "w2");
                write->setProperty("name", "Auditioned");
                write->setProperty("audition", true);
                write->setProperty(
                    "payload",
                    SoftwarePresetProtocol::encodePayload(auditioned.rawData.data(), auditioned.rawData.size()));

                messages.clear();
                send(write);

                const auto after = synthMemory.getProgram(9);
                check(std::memcmp(after.rawData.data(), before.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                          == 0,
                      "an audition leaves the synth memory untouched");
                check(juce::String(after.getName()).trim() == "BankPatch",
                      "an audition does not rename the stored program");

                // El motor si recibe el bloque: el voice mode del APVTS es el del
                // payload auditado (Mono), no el guardado (Unison).
                const auto voiceMode = host.getAPVTS().getRawParameterValue(ParamIDs::voiceMode);
                check(voiceMode != nullptr && voiceMode->load() < 0.5f,
                      "an audition does apply the block to the engine");
                // La audición también refresca la WebUI (`syncAllParams`), así que
                // el acuse se busca por tipo y se comprueba que sea exactamente uno.
                int writtenAcks = 0;
                for (const auto &message : messages)
                    if (typeOf(message) == SoftwarePresetProtocol::kPresetWritten)
                        ++writtenAcks;
                check(writtenAcks == 1 && findOfType(SoftwarePresetProtocol::kPresetWritten).isObject(),
                      "the audition is acknowledged like any other write");
                check(messages.size() > 0 && typeOf(messages.back()) == SoftwarePresetProtocol::kPresetWritten,
                      "the write acknowledgement is the last message of the audition");
            }

            // `bank.read`: los 128 programas de una vez (el Bank Manager cae al verbo
            // por slot solo si el host no lo conoce: aqui si lo conoce).
            {
                const auto expectedFirst = synthMemory.getProgram(0);

                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadBank, "native", juce::var(), "b1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kBankData,
                      "bank.read is answered with bank.data");

                // Guardia: si la respuesta no llego, el check de arriba ya ha
                // fallado; aqui se evita el front() sobre la cola vacia en vez
                // de crashear (los checks de abajo fallaran con valores por defecto).
                const auto data = messages.empty() ? juce::var {} : dataOf(messages.front());
                check(data.getProperty("requestId", "").toString() == "b1", "bank.data echoes the requestId");

                const auto slots = data.getProperty("slots", juce::var());
                check(slots.isArray() && slots.getArray() != nullptr
                          && slots.getArray()->size() == static_cast<int>(SysExManager::BANK_SIZE),
                      "bank.data carries all 128 programs");
                check(slots.isArray() && slots.getArray() != nullptr && slots.getArray()->size() > 0
                          && static_cast<int>(slots.getArray()->getUnchecked(0).getProperty("slot", -1)) == 0,
                      "bank.data entries carry their slot");

                juce::MemoryBlock firstBlob;
                if (slots.isArray() && slots.getArray() != nullptr && slots.getArray()->size() > 0)
                {
                    const auto first = slots.getArray()->getUnchecked(0);
                    check(SoftwarePresetProtocol::decodePayload(first.getProperty("payload", "").toString(), firstBlob)
                              && firstBlob.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                              && std::memcmp(firstBlob.getData(),
                                             expectedFirst.rawData.data(),
                                             MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                                     == 0,
                          "bank.data carries each program's bytes");
                }
            }

            // `bank.write`: escribe memoria, no cambia lo que suena.
            {
                const int activeBefore = host.getCurrentProgram();

                messages.clear();
                juce::Array<juce::var> slots;
                for (int slot : {20, 21})
                {
                    MS2000ProgramData program;
                    program.setName(slot == 20 ? "BatchOne" : "BatchTwo");
                    program.rawData[MS2000ProgramData::VOICE_BYTE] = 1;

                    juce::DynamicObject::Ptr entry = new juce::DynamicObject();
                    entry->setProperty("slot", slot);
                    entry->setProperty("name", juce::String(program.getName()).trim());
                    entry->setProperty(
                        "payload",
                        SoftwarePresetProtocol::encodePayload(program.rawData.data(), program.rawData.size()));
                    slots.add(juce::var(entry.get()));
                }

                auto bulk = makeRequest(SoftwarePresetProtocol::kWriteBank, "native", juce::var(), "bw1");
                bulk->setProperty("slots", slots);
                send(bulk);

                check(juce::String(synthMemory.getProgram(20).getName()).trim() == "BatchOne"
                          && juce::String(synthMemory.getProgram(21).getName()).trim() == "BatchTwo",
                      "bank.write stores every slot it receives");
                check(host.getCurrentProgram() == activeBefore, "a bank write does not change the active program");

                const auto written = findOfType(SoftwarePresetProtocol::kBankWritten);
                check(written.isObject() && static_cast<int>(dataOf(written).getProperty("count", -1)) == 2,
                      "bank.written reports how many slots were stored");
            }

            // `preset.capture`: el patch activo del synth, sin tocar memoria si no
            // se pide un slot.
            {
                const auto memoryBefore = synthMemory.getProgram(11);

                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kCapturePreset, "native", juce::var(), "c1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kPresetCaptured,
                      "preset.capture is answered with preset.captured");

                juce::MemoryBlock captured;
                check(!messages.empty() && decodePayloadOf(messages.front(), captured)
                          && captured.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE),
                      "preset.captured carries the active patch as one program block");
                check(std::memcmp(synthMemory.getProgram(11).rawData.data(),
                                  memoryBefore.rawData.data(),
                                  MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                          == 0,
                      "a capture without a slot does not write memory");

                // Con `slot`: ademas se guarda.
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kCapturePreset, "native", juce::var(11), "c2"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kPresetCaptured,
                      "preset.capture with a slot is answered too");

                juce::MemoryBlock second;
                const bool decodedSecond = !messages.empty() && decodePayloadOf(messages.front(), second);
                check(decodedSecond
                          && std::memcmp(synthMemory.getProgram(11).rawData.data(),
                                         second.getData(),
                                         MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                                 == 0,
                      "preset.capture with a slot stores the active patch in the synth memory");
            }

            // Errores: motivo explicito y codigo que permite al Bank Manager caer al
            // verbo por slot en vez de morir por timeout.
            {
                auto codeOf = [&dataOf](const juce::var &message)
                { return dataOf(message).getProperty("code", "").toString(); };

                // Sistema que este host no habla (`sysex` no se declara: §7.1).
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadPreset, "sysex", juce::var(0), "e1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kError,
                      "an unsupported preset system is answered with preset.error");
                check(messages.size() == 1 && codeOf(messages[0]) == SoftwarePresetProtocol::kUnsupportedAction,
                      "the unsupported system reports 'unsupported-action' (the Bank Manager can degrade)");
                check(messages.size() == 1 && dataOf(messages[0]).getProperty("requestId", "").toString() == "e1",
                      "preset.error echoes the requestId");

                // Slot fuera de la memoria del synth.
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadPreset, "native", juce::var(999), "e2"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kError
                          && codeOf(messages[0]) == SoftwarePresetProtocol::kInvalidSlot,
                      "a slot outside 0..127 reports 'invalid-slot'");

                // Payload vacio y payload que no llega al bloque de programa.
                messages.clear();
                auto empty = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(0), "e3");
                empty->setProperty("payload", "");
                send(empty);

                const juce::uint8 tooShortRaw[] = {0x01, 0x02, 0x03};
                auto shortWrite = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(0), "e4");
                shortWrite->setProperty("payload",
                                        SoftwarePresetProtocol::encodePayload(tooShortRaw, sizeof(tooShortRaw)));
                send(shortWrite);

                int invalidPayloadErrors = 0;
                for (const auto &message : messages)
                    if (typeOf(message) == SoftwarePresetProtocol::kError
                        && codeOf(message) == SoftwarePresetProtocol::kInvalidPayload)
                        ++invalidPayloadErrors;
                check(invalidPayloadErrors == 2 && messages.size() == 2,
                      "an empty or too-short payload reports 'invalid-payload' and reaches no memory");

                // `bank.write` sin array de slots.
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kWriteBank, "native", juce::var(), "e5"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kError
                          && codeOf(messages[0]) == SoftwarePresetProtocol::kInvalidPayload,
                      "bank.write without a slots array reports 'invalid-payload'");
            }

            // El sistema de presets declarado por el host es el que dice el contrato
            // (`transport.software.systems: ['native']`).
            check(juce::String(SoftwarePresetProtocol::kPresetSystem) == "native",
                  "the host exposes the 'native' preset system the contract declares");
        }
    }
}

static void testHardwareProgramToEngine()
{
    // --- Test 20: programa de hardware del MS2000 (254 B reales) → motor ---
    printf("\n[Test 20] Korg MS2000 hardware program (real 254-byte dump) applied to the engine...\n");
    {
        using Hw = MS2000HardwareProgram;

        // 1. La plantilla embebida es el INIT Program real (nombre en 0x00).
        Hw prog;
        check(prog.getName() == "INIT Program", "the template is the real 'INIT Program' (name at 0x00)");
        check(!prog.isVocoderProgram(), "the INIT Program is a synth program (voice mode Single)");

        // 2. Empaquetado real: 254 B → 291 B (el ultimo grupo parcial NO se rellena).
        std::vector<uint8_t> packed;
        check(prog.packToSysexPayload(packed) && packed.size() == 291,
              "the 254-byte program packs to the real 291-byte payload (no zero padding)");
        const auto frame = prog.buildProgramDump(1, 0x40);
        check(frame.size() == 297 && frame[0] == 0xF0 && frame[3] == 0x58 && frame[4] == 0x40 && frame.back() == 0xF7,
              "the hardware frame is F0 42 3n 58 40 [291 B] F7");

        Hw roundTrip;
        check(roundTrip.unpackFromSysexPayload(packed.data(), packed.size()), "the real payload unpacks");
        check(roundTrip.raw == prog.raw, "the 254 bytes survive pack -> unpack unchanged");

        // 3. Bytes reales → parametros del motor.
        DummyProcessor hwProcessor;
        juce::AudioProcessorValueTreeState hwApvts(
            hwProcessor, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
        auto raw = [&hwApvts](const char *id) -> float
        {
            if (auto *p = hwApvts.getRawParameterValue(id))
                return p->load();
            return -999.0f;
        };
        auto setParam = [&hwApvts](const char *id, float value)
        {
            if (auto *p = hwApvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(value));
        };

        Hw patch;
        patch.setName("From the MS2000");
        patch.timbre(0)[Hw::ti::FLAGS] = 0xA0;  // assign Unison (bits 6,7 = 2)
        patch.timbre(0)[Hw::ti::CUTOFF] = 42;
        patch.timbre(0)[Hw::ti::RESONANCE] = 88;
        patch.timbre(0)[Hw::ti::FILTER_EG1_INT] = 64 - 20;
        patch.timbre(0)[Hw::ti::OSC1_WAVE] = 5;  // DWGS
        patch.timbre(0)[Hw::ti::OSC1_DWGS] = 33;
        patch.timbre(0)[Hw::ti::OSC2_SEMITONE] = 64 + 7;
        patch.timbre(0)[Hw::ti::AMP_PAN] = 64 + 10;
        patch.timbre(0)[Hw::ti::EG2_SUSTAIN] = 99;
        patch.timbre(0)[Hw::ti::PATCH1] = (5 << 4) | 2;  // destino 5, origen 2
        patch.timbre(0)[Hw::ti::PATCH1_INT] = 64 + 31;
        patch.timbre(0)[Hw::ti::PORTAMENTO] = 30;
        patch.set(Hw::ARP_TYPE_RNG, (2 << 4) | 4);  // rango 3 octavas (2), tipo Random (4)
        patch.set(Hw::ARP_GATE, 65);
        patch.set(Hw::MODFX_TYPE, 2);
        patch.set(Hw::EQ_HI_FREQ, 29);  // 30 pasos reales -> 3 en el motor

        check(patch.applyToAPVTS(hwApvts) > 40, "applying the real program writes the mapped engine parameters");
        check(raw(ParamIDs::voiceMode) == 2.0f, "timbre assign mode (bits 6,7) lands on voiceMode = Unison");
        check(raw(ParamIDs::filterCutoff) == 42.0f, "cutoff at the real timbre offset reaches the engine");
        check(raw(ParamIDs::filterResonance) == 88.0f, "resonance reaches the engine");
        check(raw(ParamIDs::filterEg1Int) == -20.0f, "+64-biased bytes land signed on the engine");
        check(raw(ParamIDs::osc1Wave) == 5.0f && raw(ParamIDs::osc1DwgsWave) == 33.0f,
              "OSC1 wave and its DWGS index are two parameters in the dump");
        check(raw(ParamIDs::osc2Semitone) == 7.0f, "OSC2 semitone keeps its +64 bias");
        check(raw(ParamIDs::ampPan) == 10.0f, "pan (64 = center) lands on the bipolar engine parameter");
        check(raw(ParamIDs::eg2Sustain) == 99.0f, "EG2 sustain reaches the engine");
        check(raw(ParamIDs::patch1Source) == 2.0f && raw(ParamIDs::patch1Destination) == 5.0f
                  && raw(ParamIDs::patch1Intensity) == 31.0f,
              "a virtual patch slot is unpacked from its packed byte");
        check(raw(ParamIDs::arpType) == 4.0f && raw(ParamIDs::arpRange) == 3.0f && raw(ParamIDs::arpGate) == 65.0f,
              "arp type/range (0..3 real = 1..4 octaves) and gate reach the engine");
        check(raw(ParamIDs::modFxType) == 2.0f, "Mod FX type reaches the engine");
        check(raw(ParamIDs::eqHighFreq) == 3.0f,
              "the EQ's 30 real steps fold into the engine's 4 (declared approximation)");
        check(raw(ParamIDs::portamentoTime) == 30.0f && raw(ParamIDs::portamentoOn) == 1.0f,
              "portamento time is applied and its switch is derived (the hardware has no on/off byte)");
        check(raw(ParamIDs::synthVocoderMode) == 0.0f, "a synth program leaves the vocoder off");

        // 4. Programa de vocoder: el MISMO bloque de timbre se interpreta de otra forma.
        Hw voc;
        voc.setVoiceMode(Hw::VoiceMode::Vocoder);
        voc.timbre(0)[Hw::ti::MIX_NOISE] = 77;      // en vocoder = HPF level
        voc.timbre(0)[Hw::ti::FILTER_VELO] = 61;    // en vocoder = Gate Sense
        voc.timbre(0)[Hw::ti::FILTER_EG1_INT] = 1;  // en vocoder = Filter Shift (+1)
        voc.timbre(0)[Hw::ti::CUTOFF] = 74;         // en vocoder = Fc Offset (+10 signed)
        voc.timbre(0)[Hw::ti::RESONANCE] = 88;      // en vocoder = Resonance (88)
        // Las 16 bandas con 16 valores DISTINTOS: asi el round-trip demuestra que cada byte
        // llega a su propia banda y que captureFromAPVTS lo devuelve al mismo offset.
        for (int b = 0; b < 16; ++b)
        {
            voc.timbre(0)[Hw::ti::VOCODER_BAND_LEVELS + static_cast<size_t>(b)] = static_cast<uint8_t>(10 + b * 7);
            voc.timbre(0)[Hw::ti::VOCODER_BAND_PANS + static_cast<size_t>(b)] = static_cast<uint8_t>(64 + (-60 + b * 8));
        }
        voc.applyToAPVTS(hwApvts);
        check(raw(ParamIDs::synthVocoderMode) == 1.0f, "a vocoder program switches the engine to vocoder mode");
        check(raw(ParamIDs::vocoderHpfLevel) == 77.0f && raw(ParamIDs::vocoderGateSense) == 61.0f,
              "the vocoder reads those bytes as HPF level / gate sense, not as mixer / velocity sense");
        check(raw(ParamIDs::vocoderFormantShift) == 3.0f,
              "filter shift 0,+1,+2,-1,-2 is reordered to the engine's -2..+2 (real +1 -> engine +1)");
        check(raw(ParamIDs::vocoderCutoff) == 10.0f && raw(ParamIDs::vocoderResonance) == 88.0f,
              "the vocoder Fc Offset and Resonance reach the engine from the timbre block");
        {
            bool levelsMatch = true;
            bool pansMatch = true;
            for (int b = 0; b < 16; ++b)
            {
                static const char *const kLevels[16] = {ParamIDs::vocoderBandLevel1,  ParamIDs::vocoderBandLevel2,
                                                        ParamIDs::vocoderBandLevel3,  ParamIDs::vocoderBandLevel4,
                                                        ParamIDs::vocoderBandLevel5,  ParamIDs::vocoderBandLevel6,
                                                        ParamIDs::vocoderBandLevel7,  ParamIDs::vocoderBandLevel8,
                                                        ParamIDs::vocoderBandLevel9,  ParamIDs::vocoderBandLevel10,
                                                        ParamIDs::vocoderBandLevel11, ParamIDs::vocoderBandLevel12,
                                                        ParamIDs::vocoderBandLevel13, ParamIDs::vocoderBandLevel14,
                                                        ParamIDs::vocoderBandLevel15, ParamIDs::vocoderBandLevel16};
                static const char *const kPans[16] = {ParamIDs::vocoderBandPan1,  ParamIDs::vocoderBandPan2,
                                                      ParamIDs::vocoderBandPan3,  ParamIDs::vocoderBandPan4,
                                                      ParamIDs::vocoderBandPan5,  ParamIDs::vocoderBandPan6,
                                                      ParamIDs::vocoderBandPan7,  ParamIDs::vocoderBandPan8,
                                                      ParamIDs::vocoderBandPan9,  ParamIDs::vocoderBandPan10,
                                                      ParamIDs::vocoderBandPan11, ParamIDs::vocoderBandPan12,
                                                      ParamIDs::vocoderBandPan13, ParamIDs::vocoderBandPan14,
                                                      ParamIDs::vocoderBandPan15, ParamIDs::vocoderBandPan16};
                levelsMatch = levelsMatch && (raw(kLevels[b]) == static_cast<float>(10 + b * 7));
                pansMatch = pansMatch && (raw(kPans[b]) == static_cast<float>(-60 + b * 8));
            }
            check(levelsMatch, "the 16 vocoder band levels (46..61) reach the engine band by band");
            check(pansMatch, "the 16 vocoder band pans (62..77) reach the engine as signed 64-centred values");

            // Y de vuelta al bloque real: cada banda se escribe en SU offset.
            Hw vocBack;
            vocBack.captureFromAPVTS(hwApvts, "VocRoundTrip");
            bool levelsBack = true;
            bool pansBack = true;
            for (int b = 0; b < 16; ++b)
            {
                levelsBack = levelsBack
                             && (vocBack.timbre(0)[Hw::ti::VOCODER_BAND_LEVELS + static_cast<size_t>(b)]
                                 == static_cast<uint8_t>(10 + b * 7));
                pansBack = pansBack
                           && (vocBack.timbre(0)[Hw::ti::VOCODER_BAND_PANS + static_cast<size_t>(b)]
                               == static_cast<uint8_t>(64 + (-60 + b * 8)));
            }
            check(levelsBack, "captureFromAPVTS writes the 16 band levels back to bytes 46..61");
            check(pansBack, "captureFromAPVTS writes the 16 band pans back to bytes 62..77");
        }

        // 5. De vuelta al hardware: los DOS timbres, escala/split y el mod sequence.
        setParam(ParamIDs::synthVocoderMode, 0.0f);  // el paso 4 dejó el motor en vocoder
        Hw back;
        for (size_t i = 0; i < Hw::TIMBRE_SIZE; ++i)
            back.timbre(1)[i] = static_cast<uint8_t>(0xA0 + i);
        back.set(Hw::SPLIT_POINT, 64);
        back.setVoiceMode(Hw::VoiceMode::Single);
        setParam(ParamIDs::filterCutoff, 7.0f);
        setParam(ParamIDs::filterType, 2.0f);
        setParam(ParamIDs::eqLowFreq, 1.0f);
        setParam(ParamIDs::filterVelo, 25.0f);
        setParam(ParamIDs::t2FilterCutoff, 33.0f);
        setParam(ParamIDs::t2Osc1Wave, 1.0f);
        setParam(ParamIDs::t2FilterVelo, -20.0f);
        setParam(ParamIDs::t2Seq2Step16, 40.0f);
        setParam(ParamIDs::splitPoint, 55.0f);
        setParam(ParamIDs::scaleKey, 5.0f);
        setParam(ParamIDs::scaleType, 2.0f);
        setParam(ParamIDs::seq1Dest, 11.0f);   // CUTOFF, el 12º destino real
        setParam(ParamIDs::seq1Motion, 1.0f);  // Smooth
        setParam(ParamIDs::seq1Step1, -63.0f);
        setParam(ParamIDs::seqLastStep, 8.0f);
        back.captureFromAPVTS(hwApvts, "Edited");
        check(back.getName() == "Edited", "capture writes the engine program name back");
        check(back.timbre(0)[Hw::ti::CUTOFF] == 7, "capture writes the engine cutoff into the real timbre slot");
        check(back.timbre(0)[Hw::ti::FILTER_TYPE] == 2,
              "capture writes the engine filter type into the real timbre slot");
        check(back.timbre(0)[Hw::ti::FILTER_VELO] == 64 + 25 && back.timbre(1)[Hw::ti::FILTER_VELO] == 64 - 20,
              "each timbre writes its own filter velocity sense");
        check(back.timbre(1)[Hw::ti::CUTOFF] == 33 && back.timbre(1)[Hw::ti::OSC1_WAVE] == 1,
              "capture writes Timbre 2 into its own 108-byte block");
        check(back.timbre(1)[0] == 0xA0, "the Timbre 2 bytes the engine does not model (MIDI channel) survive capture");
        check(back.get(Hw::SPLIT_POINT) == 55, "the engine's split point is written into the real byte");
        check(Hw::extractBits(back.get(Hw::SCALE_BYTE), 4, 7) == 5
                  && Hw::extractBits(back.get(Hw::SCALE_BYTE), 0, 3) == 2,
              "scale key and type, packed in one real byte, come from the engine");
        check(back.timbre(0)[Hw::ti::SEQ1_KNOB] == 11 && back.timbre(0)[Hw::ti::SEQ1_STEPS] == 1,
              "the sequencer row destination and its first step reach the real bytes");
        check(back.timbre(0)[Hw::ti::SEQ1_MOTION] == 0,
              "the real motion bit says Smooth (0), the inverse of the engine's enum");
        check(Hw::extractBits(back.timbre(0)[Hw::ti::SEQ_FLAGS2], 4, 7) == 7,
              "the last step (1..16 in the engine) is stored as 0..15");
        check(back.timbre(1)[Hw::ti::SEQ2_STEPS + 15] == 104, "Timbre 2 has its own sequencer rows");
        check(back.buildProgramDump(1, 0x40).size() == 297, "the captured program still builds a real hardware frame");

        // 6. Lo que no se puede aplicar se declara, no se esconde.
        const auto &pending = Hw::unmodelled();
        const auto mentions = [&pending](const char *needle)
        {
            return std::any_of(pending.begin(),
                               pending.end(),
                               [needle](const char *s) { return std::string(s).find(needle) != std::string::npos; });
        };
        check(!pending.empty() && mentions("MIDI channel") && !mentions("Timbre 2"),
              "the fields the engine cannot hold are declared in unmodelled(), and Timbre 2 is no longer one of them");

        // 7. Ida y vuelta completa: trama real -> SysExManager -> motor -> trama real.
        SysExManager sysEx;
        Hw fromHardware;
        fromHardware.setName("Hw Patch");  // el nombre real son 12 B
        fromHardware.timbre(0)[Hw::ti::CUTOFF] = 99;
        fromHardware.timbre(1)[0] = 0x11;  // Timbre 2 con datos propios
        const auto incoming = fromHardware.buildProgramDump(1, 0x40);
        check(incoming.size() == 297, "the incoming hardware dump uses the real frame size");

        const auto parseResult = sysEx.parseSysEx(incoming.data(), incoming.size(), hwApvts);
        check(parseResult.success && parseResult.programName == "Hw Patch",
              "a real MS2000 dump is accepted and named from byte 0x00");
        check(raw(ParamIDs::filterCutoff) == 99.0f, "the dump's cutoff reaches the engine");
        check(sysEx.hasHardwareProgram() && sysEx.getHardwareProgram().timbre(1)[0] == 0x11,
              "the hardware bytes the engine does not model are kept for the way back");

        setParam(ParamIDs::filterResonance, 42.0f);
        const auto outgoing = sysEx.createHardwareProgramDump(1, hwApvts, "Edited");
        check(outgoing.size() == 297 && outgoing[3] == 0x58 && outgoing[4] == 0x40,
              "the plugin sends the real MS2000 frame, not its own native block");

        Hw sent;
        std::vector<uint8_t> sentPayload(outgoing.begin() + 5, outgoing.end() - 1);
        check(sent.unpackFromSysexPayload(sentPayload.data(), sentPayload.size())
                  && sent.timbre(0)[Hw::ti::RESONANCE] == 42 && sent.timbre(1)[0] == 0x11 && sent.getName() == "Edited",
              "the engine's edit goes back into its real slot and Timbre 2 travels intact");
        check(sent.timbre(0)[Hw::ti::CUTOFF] == 99 && sent.timbre(0)[Hw::ti::OSC1_WAVE] == 0,
              "the rest of the timbre keeps what the hardware sent");

        // 8. El preset PROPIO viaja con la cabecera de ABDSynths, no con la de Korg.
        MS2000ProgramData nativePreset;
        nativePreset.extractFromAPVTS(hwApvts, "Native One");
        nativePreset.setParam(ParamIDs::filterCutoff, 55.0f);
        const auto nativeFrame = sysEx.createProgramDump(1, nativePreset);
        check(nativeFrame.size() == ABDSynthsSysEx::programFrameSize() && nativeFrame[0] == 0xF0
                  && nativeFrame[1] == 0x7D && nativeFrame[2] == 0x0A
                  && nativeFrame[3] == ABDSynthsSysEx::CMD_PROGRAM_DUMP && nativeFrame.back() == 0xF7,
              "the plugin's own preset uses the ABDSynths frame (0x7D / model 0x0A), not a Korg one");

        setParam(ParamIDs::filterCutoff, 1.0f);
        const auto nativeImport = sysEx.parseSysEx(nativeFrame.data(), nativeFrame.size(), hwApvts);
        check(nativeImport.success && raw(ParamIDs::filterCutoff) == 55.0f,
              "the ABDSynths frame is read back as the plugin's own native preset");
        check(juce::String(nativeImport.programName).trim() == "Native One", "the native preset keeps its name");

        juce::MemoryBlock ownPresetFile;
        MS2000SysExExporter::exportSingleProgram(hwApvts, "Own Preset", ownPresetFile);
        check(ownPresetFile.getSize() > 5
                  && static_cast<const uint8_t *>(ownPresetFile.getData())[1] == ABDSynthsSysEx::MANUFACTURER_ID,
              "the .syx the plugin writes for its own presets is an ABDSynths file, not a Korg one");

        const auto nativeBankFrame = sysEx.createAllDataDump(1);
        check(nativeBankFrame.size() > 5 && nativeBankFrame[1] == 0x7D
                  && nativeBankFrame[3] == ABDSynthsSysEx::CMD_ALL_DUMP,
              "the plugin's full memory also travels as an ABDSynths dump");
        const auto bankImport = sysEx.parseSysEx(nativeBankFrame.data(), nativeBankFrame.size(), hwApvts);
        check(bankImport.success && bankImport.programCount == static_cast<int>(SysExManager::BANK_SIZE),
              "the ABDSynths bank dump fills the host's 128 preset slots");
    }
}

static void testDumpRequests()
{
    // --- Test 22: peticiones 0x10/0x0E — el plugin como servidor de presets ---
    printf("\n[Test 22] Dump requests (0x10/0x0E) — the plugin answering as a preset server...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto &apvts = dummyFixture().apvts;
        SysExManager sysEx;
        sysEx.setActiveProgramIndex(5);

        // 1. Petición ABDSynths de un preset: se reconoce y se puede responder.
        const std::vector<uint8_t> ownReq = {0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_REQUEST, 0xF7};
        const auto ownReqResult = sysEx.parseSysEx(ownReq.data(), ownReq.size(), apvts);
        check(ownReqResult.success && ownReqResult.messageType == SysExMessageType::ProgramDumpRequest,
              "a 5-byte ABDSynths program request is recognized (no channel byte in the house frame)");

        const auto ownResp = sysEx.buildProgramDumpResponse(1);
        check(ownResp.size() == ABDSynthsSysEx::programFrameSize() && ownResp[3] == ABDSynthsSysEx::CMD_PROGRAM_DUMP,
              "the program-dump response is the house frame (444 B, command 0x40)");
        const auto ownRespParse = sysEx.parseSysEx(ownResp.data(), ownResp.size(), apvts);
        check(ownRespParse.success && ownRespParse.programCount == 1,
              "the program-dump response re-imports as one native preset");

        // 2. Petición ABDSynths de la memoria completa: respuesta 0x4C de 128 bloques.
        const std::vector<uint8_t> ownAllReq = {0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_ALL_REQUEST, 0xF7};
        const auto ownAllResult = sysEx.parseSysEx(ownAllReq.data(), ownAllReq.size(), apvts);
        check(ownAllResult.success && ownAllResult.messageType == SysExMessageType::AllDataDumpRequest,
              "an ABDSynths all-data request is recognized");

        const auto ownAllResp = sysEx.buildAllDataDumpResponse(1);
        check(ownAllResp[3] == ABDSynthsSysEx::CMD_ALL_DUMP,
              "the all-data response is the house bank frame (command 0x4C)");
        const auto ownAllParse = sysEx.parseSysEx(ownAllResp.data(), ownAllResp.size(), apvts);
        check(ownAllParse.success && ownAllParse.programCount == static_cast<int>(SysExManager::BANK_SIZE),
              "the all-data response re-imports as the full 128-preset memory");

        // 3. Peticiones Korg reales: se reconocen con su canal y la respuesta sale por
        //    el mapa del hardware (programa real de 254 B, no bloque nativo).
        const std::vector<uint8_t> korgReq = {0xF0, 0x42, 0x30, 0x58, 0x10, 0xF7};  // canal 1
        const auto korgResult = sysEx.parseSysEx(korgReq.data(), korgReq.size(), apvts);
        check(korgResult.success && korgResult.messageType == SysExMessageType::ProgramDumpRequest
                  && korgResult.midiChannel == 1,
              "a real Korg program request (F0 42 3n 58 10 F7) is recognized with its channel");

        if (auto *p = apvts.getParameter(ParamIDs::filterCutoff))
            p->setValueNotifyingHost(p->convertTo0to1(77.0f));
        const auto korgResp = sysEx.createHardwareProgramDump(korgResult.midiChannel, apvts);
        check(korgResp.size() == 297 && korgResp[1] == 0x42 && korgResp[2] == 0x30 && korgResp[3] == 0x58
                  && korgResp[4] == 0x40,
              "the Korg response is the real 297-byte hardware frame on the requested channel");

        const std::vector<uint8_t> korgAllReq = {0xF0, 0x42, 0x32, 0x58, 0x0E, 0xF7};  // canal 3
        const auto korgAllResult = sysEx.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
        check(korgAllResult.success && korgAllResult.messageType == SysExMessageType::AllDataDumpRequest
                  && korgAllResult.midiChannel == 3,
              "a real Korg all-data request is recognized with its channel");

        // 4. Los tamaños mínimos se mantienen: 4 B no significan nada.
        const std::vector<uint8_t> tooShort = {0xF0, 0x7D, 0x0A, 0x40, 0xF7, 0x00};
        const auto shortResult = sysEx.parseSysEx(tooShort.data() + 1, tooShort.size() - 1, apvts);
        check(!shortResult.success && shortResult.messageType == SysExMessageType::Unknown,
              "a 4-byte buffer is still rejected as too small");
    }
}

static void testWriteAcknowledgement()
{
    // --- Test 23: acuse de escritura (0x23/0x24) — el receptor confirma lo que guardó ---
    printf("\n[Test 23] Write acknowledgement (0x23/0x24) — the receiver confirming what it stored...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto &apvts = dummyFixture().apvts;
        SysExManager sysEx;
        sysEx.setActiveProgramIndex(3);

        auto sameBytes = [](const std::vector<uint8_t> &a, const std::vector<uint8_t> &b)
        { return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin()); };
        const std::vector<uint8_t> ownAck = {0xF0, 0x7D, 0x0A, 0x23, 0xF7};
        const std::vector<uint8_t> ownNack = {0xF0, 0x7D, 0x0A, 0x24, 0xF7};

        // 1. Una escritura de la casa que cabe: se guarda y vuelve el acuse de 5 B.
        MS2000ProgramData preset;
        preset.setName("ACK ME");
        const auto ownWrite = sysEx.createProgramDump(1, preset);
        const auto ownWriteRes = sysEx.parseSysEx(ownWrite.data(), ownWrite.size(), apvts);
        check(ownWriteRes.success && ownWriteRes.programCount == 1 && sameBytes(ownWriteRes.reply, ownAck),
              "an ABDSynths program write is stored and answered F0 7D 0A 23 F7");

        // 2. Una escritura que no se puede guardar (ni 384 ni 128 B de bloques): NACK.
        {
            std::vector<uint8_t> oddRaw(300, 0x21);
            std::vector<uint8_t> oddPacked;
            SysExCodec::pack8to7(oddRaw.data(), oddRaw.size(), oddPacked);
            std::vector<uint8_t> oddFrame {0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_DUMP};
            oddFrame.insert(oddFrame.end(), oddPacked.begin(), oddPacked.end());
            oddFrame.push_back(0xF7);

            const auto oddRes = sysEx.parseSysEx(oddFrame.data(), oddFrame.size(), apvts);
            check(!oddRes.success && sameBytes(oddRes.reply, ownNack),
                  "an ABDSynths write that is not a whole number of presets is answered F0 7D 0A 24 F7");
        }

        // 3. Un acuse recibido se reconoce y NO se contesta (no hay ping-pong de acuses).
        const auto rxAck = sysEx.parseSysEx(ownAck.data(), ownAck.size(), apvts);
        check(rxAck.success && rxAck.messageType == SysExMessageType::WriteCompleted && rxAck.reply.empty(),
              "a received F0 7D 0A 23 F7 reads as WriteCompleted and is not answered");

        const auto rxNack = sysEx.parseSysEx(ownNack.data(), ownNack.size(), apvts);
        check(!rxNack.success && rxNack.messageType == SysExMessageType::WriteError && !rxNack.errorMessage.empty()
                  && rxNack.reply.empty(),
              "a received F0 7D 0A 24 F7 reads as WriteError (the peer could not store it)");

        // 4. Un acuse con payload no es un acuse: no se intenta leer como preset.
        const std::vector<uint8_t> badAck {0xF0, 0x7D, 0x0A, 0x23, 0x00, 0xF7};
        const auto badAckRes = sysEx.parseSysEx(badAck.data(), badAck.size(), apvts);
        check(!badAckRes.success && badAckRes.reply.empty(),
              "a 0x23 carrying a payload is rejected instead of read as a preset");

        // 5. Escritura Korg real (254 B): el acuse sale con la cabecera del equipo y su canal.
        const auto hwFrame = sysEx.createHardwareProgramDump(3, apvts, "HW ACK");
        const auto hwRes = sysEx.parseSysEx(hwFrame.data(), hwFrame.size(), apvts);
        const std::vector<uint8_t> hwAck {0xF0, 0x42, 0x32, 0x58, 0x23, 0xF7};  // canal 3 = 0x32
        check(hwRes.success && sameBytes(hwRes.reply, hwAck),
              "a real 254-byte Korg write is answered F0 42 3n 58 23 F7 on the sender's channel");

        // 6. Escritura Korg que no se puede desempaquetar: NACK del equipo.
        const std::vector<uint8_t> emptyKorg {0xF0, 0x42, 0x30, 0x58, 0x40, 0xF7};
        const auto emptyKorgRes = sysEx.parseSysEx(emptyKorg.data(), emptyKorg.size(), apvts);
        const std::vector<uint8_t> hwNack {0xF0, 0x42, 0x30, 0x58, 0x24, 0xF7};
        check(!emptyKorgRes.success && sameBytes(emptyKorgRes.reply, hwNack),
              "a Korg write that cannot be unpacked is answered F0 42 3n 58 24 F7");

        // 7. Peticiones: el volcado pedido viaja en `reply` (el plugin contestando al Banco).
        const std::vector<uint8_t> ownReq = {0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_REQUEST, 0xF7};
        const auto ownReqRes = sysEx.parseSysEx(ownReq.data(), ownReq.size(), apvts);
        check(ownReqRes.reply.size() == ABDSynthsSysEx::programFrameSize()
                  && ownReqRes.reply[3] == ABDSynthsSysEx::CMD_PROGRAM_DUMP,
              "an ABDSynths program request leaves its 444-byte answer in reply");

        const std::vector<uint8_t> ownAllReq = {0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_ALL_REQUEST, 0xF7};
        const auto ownAllRes = sysEx.parseSysEx(ownAllReq.data(), ownAllReq.size(), apvts);
        check(ownAllRes.reply.size() > 5 && ownAllRes.reply[3] == ABDSynthsSysEx::CMD_ALL_DUMP,
              "an ABDSynths all-data request answers with the house bank frame (0x4C)");

        const std::vector<uint8_t> korgReq = {0xF0, 0x42, 0x32, 0x58, 0x10, 0xF7};  // canal 3
        const auto korgReqRes = sysEx.parseSysEx(korgReq.data(), korgReq.size(), apvts);
        check(korgReqRes.reply.size() == 297 && korgReqRes.reply[2] == 0x32 && korgReqRes.reply[4] == 0x40,
              "a real Korg request answers with the 297-byte hardware frame (never the native block)");
    }
}

static void testAllDataDumpRequest()
{
    // --- Test 24: All Data Dump Request de Korg (0x0E) — la memoria real del equipo ---
    printf("\n[Test 24] Korg All Data Dump Request (0x0E) — answering with the real machine memory...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto &apvts = dummyFixture().apvts;
        // Convención del canal `3n` (ROADMAP §4): ambos extremos usan
        // `0x30 | (canal - 1)`, es decir canal 1 -> 0x30 y canal 16 -> 0x3F.
        // La prueba física frente al equipo real queda pendiente; aquí se verifica
        // únicamente que el plugin conserva la convención compartida.
        {
            const auto ackChannel1 = MS2000HardwareProgram::buildWriteAcknowledgement(1, true);
            const auto ackChannel16 = MS2000HardwareProgram::buildWriteAcknowledgement(16, true);
            check(ackChannel1.size() == 6 && ackChannel1[2] == 0x30 && ackChannel16[2] == 0x3F,
                  "the plugin addresses channel 1 as 0x30 and channel 16 as 0x3F (0x30 | (channel - 1))");
        }

        // Petición tal cual la manda un MS2000/R: `F0 42 3n 58 0E F7` (canal 2).
        const std::vector<uint8_t> korgAllReq = {0xF0, 0x42, 0x31, 0x58, 0x0E, 0xF7};

        // 1. Sin memoria de equipo el plugin hace de equipo con la suya: convierte sus 128
        //    presets nativos a programas reales de 254 B (§6.4). El formato y el tamaño son
        //    los del equipo; la conversión está **declarada como aproximada** (lo que el
        //    motor no modela sale del INIT Program y falta verificarla contra un MS2000).
        {
            SysExManager ownMemory;
            const auto ownAnswer = ownMemory.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
            check(ownAnswer.success && ownAnswer.programCount == static_cast<int>(SysExManager::BANK_SIZE)
                      && ownAnswer.reply.size() == 37163
                      && ownAnswer.reply[4] == MS2000HardwareProgram::CMD_ALL_DATA_DUMP,
                  "without machine memory the plugin answers with its own 128 presets as real 254-byte programs");

            // Fidelidad de la conversión: los parámetros que el motor modela llegan a su byte
            // real (con su sesgo: `filterEg1Int` es -63..+63 y viaja como +64), y el byte de
            // voz (portamento time) llega a los bits reales de su byte.
            const auto *cutoffMeta = ParameterRegistry::getParameter(ParamIDs::filterCutoff);
            const auto *egIntMeta = ParameterRegistry::getParameter(ParamIDs::filterEg1Int);
            MS2000ProgramData native;
            native.setName("NATIVE 7");
            // En el bloque nativo cada parámetro vive en `TIMBRE_START + su sysexOffset`.
            native.setByte(static_cast<uint8_t>(MS2000ProgramData::TIMBRE_START + cutoffMeta->sysexOffset), 77);
            native.setByte(static_cast<uint8_t>(MS2000ProgramData::TIMBRE_START + egIntMeta->sysexOffset), 100);  // +36
            native.setByte(MS2000ProgramData::VOICE_BYTE + 2, 42);  // portamento time

            const auto converted = MS2000HardwareProgram::fromNativeProgram(native);
            const size_t t1 = MS2000HardwareProgram::TIMBRE1_START;
            check(converted.get(t1 + MS2000HardwareProgram::ti::CUTOFF) == 77
                      && converted.get(t1 + MS2000HardwareProgram::ti::FILTER_EG1_INT) == 100
                      && MS2000HardwareProgram::extractBits(
                             converted.get(t1 + MS2000HardwareProgram::ti::PORTAMENTO), 0, 6)
                             == 42
                      && converted.getName() == "NATIVE 7",
                  "the conversion lands the modelled parameters in their real bytes (bias and voice byte included)");

            MS2000HardwareProgram initTemplate;
            check(converted.get(t1 + MS2000HardwareProgram::ti::TUNE)
                      == initTemplate.get(t1 + MS2000HardwareProgram::ti::TUNE),
                  "what the engine does not model stays at the machine's INIT Program (declared approximation)");
        }

        // 2. Llega la memoria real del equipo: 128 programas de 254 B, distinguibles por
        //    su byte de cutoff (así se ve luego que solo se refresca el activo).
        std::vector<MS2000HardwareProgram> machine;
        machine.reserve(SysExManager::BANK_SIZE);
        for (size_t i = 0; i < SysExManager::BANK_SIZE; ++i)
        {
            MS2000HardwareProgram program;  // plantilla "INIT Program" del equipo real
            program.setName("MEM " + std::to_string(i));
            program.set(MS2000HardwareProgram::TIMBRE1_START + MS2000HardwareProgram::ti::CUTOFF,
                        static_cast<uint8_t>(i));
            machine.push_back(program);
        }

        const auto machineDump = MS2000HardwareProgram::buildAllDataDump(1, machine);
        const size_t dumpedPayload = MS2000HardwareProgram::allDataDumpPayloadSize(machine.size());
        printf("      real memory: 128 programs -> payload %d B, frame %d B\n",
               static_cast<int>(dumpedPayload),
               static_cast<int>(machineDump.size()));
        check(machineDump.size() == dumpedPayload + 6 && machineDump.size() == 37163 && dumpedPayload == 37157,
              "the Korg all-data frame packs the whole 32 512 B stream (37 157 B payload, 37 163 B frame)");

        SysExManager sysEx;
        sysEx.setActiveProgramIndex(3);
        const auto stored = sysEx.parseSysEx(machineDump.data(), machineDump.size(), apvts);
        check(stored.success && stored.messageType == SysExMessageType::AllDataDump
                  && sysEx.getHardwareBank().size() == SysExManager::BANK_SIZE,
              "the real machine memory lands as the full 128-program hardware bank");
        check(stored.reply.size() == 6 && stored.reply[4] == MS2000HardwareProgram::CMD_WRITE_COMPLETED,
              "the all-data write is acknowledged with F0 42 3n 58 23 F7");

        // 3. Ahora sí: la petición se contesta con esa memoria y en su canal.
        const auto answer = sysEx.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
        check(answer.success && answer.messageType == SysExMessageType::AllDataDumpRequest && answer.midiChannel == 2
                  && answer.programCount == static_cast<int>(SysExManager::BANK_SIZE),
              "a real Korg all-data request is recognized with its channel and the slots held");
        check(answer.reply.size() == machineDump.size() && answer.reply.front() == 0xF0 && answer.reply[1] == 0x42
                  && answer.reply[2] == 0x31 && answer.reply[3] == 0x58
                  && answer.reply[4] == MS2000HardwareProgram::CMD_ALL_DATA_DUMP && answer.reply.back() == 0xF7,
              "the answer is the machine memory as F0 42 3n 58 4C ... F7 on the requested channel");

        // 4. El volcado se relee entero y las plazas que no se tocan viajan intactas.
        {
            SysExManager roundTrip;
            const auto reimported = roundTrip.parseSysEx(answer.reply.data(), answer.reply.size(), apvts);
            const auto &slots = roundTrip.getHardwareBank();
            check(reimported.success && slots.size() == machine.size(), "the answer re-imports as the whole memory");

            int preserved = 0;
            for (size_t i = 0; i < slots.size(); ++i)
                if (i != 3 && slots[i].raw == machine[i].raw)
                    ++preserved;
            check(preserved == static_cast<int>(machine.size()) - 1,
                  "every slot but the active one comes back byte for byte (nothing is corrupted)");
            check(slots[3].getName() == machine[3].getName(), "the active slot keeps its name");
        }

        // 5. El programa activo lo manda el motor: si se edita, el volcado lo refleja.
        if (auto *cutoff = apvts.getRawParameterValue(ParamIDs::filterCutoff))
            cutoff->store(77.0f);

        const auto edited = sysEx.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
        {
            SysExManager roundTrip;
            roundTrip.parseSysEx(edited.reply.data(), edited.reply.size(), apvts);
            const auto &slots = roundTrip.getHardwareBank();
            const size_t cutoffOffset = MS2000HardwareProgram::TIMBRE1_START + MS2000HardwareProgram::ti::CUTOFF;
            check(slots[3].raw[cutoffOffset] == 77,
                  "the active program in the dump follows the engine (cutoff 77 in its byte)");
            check(slots[2].raw[cutoffOffset] == machine[2].raw[cutoffOffset],
                  "editing the engine does not rewrite the other slots");
        }

        // 6. El acuse del equipo se reconoce y no se contesta (no hay ping-pong).
        const auto ackOnly = sysEx.parseSysEx(stored.reply.data(), stored.reply.size(), apvts);
        check(ackOnly.success && ackOnly.messageType == SysExMessageType::WriteCompleted && ackOnly.reply.empty(),
              "a received Korg F0 42 3n 58 23 F7 reads as WriteCompleted and is not answered");

        const std::vector<uint8_t> korgNack = {0xF0, 0x42, 0x31, 0x58, 0x24, 0xF7};
        const auto nackOnly = sysEx.parseSysEx(korgNack.data(), korgNack.size(), apvts);
        check(!nackOnly.success && nackOnly.messageType == SysExMessageType::WriteError
                  && !nackOnly.errorMessage.empty() && nackOnly.reply.empty(),
              "a received Korg F0 42 3n 58 24 F7 reads as WriteError (the machine refused it)");
    }
}

static void testKorgChannelEndToEnd()
{
    // --- Test 25: E2E del canal `3n` entre los dos repos ---
    // Las tramas de este test NO se escriben aquí: las emite el contrato del ABD Bank
    // Manager (`korg-ms2000.ts` → `buildDumpRequest`) y llegan compiladas en
    // `Source/MIDI/KorgChannel.gen.h`. Es a propósito — repetir la fórmula en el test es
    // justo como se cuela un desplazamiento de uno (pasó: el Banco emitía `0x30 | canal` y
    // el plugin `0x30 | (canal - 1)`) —, así que lo que se comprueba es que la trama del
    // Banco **entra** en el parser y su respuesta **sale en el mismo byte**.
    printf("\n[Test 25] Korg channel end-to-end (the Bank Manager's own frames into the SysExManager)...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto &apvts = dummyFixture().apvts;
        SysExManager sysEx;
        int shaped = 0, recognised = 0, echoed = 0, rightSize = 0;

        for (int channel = 1; channel <= kKorgChannelCount; ++channel)
        {
            const uint8_t *frame = kKorgProgramDumpRequest[channel - 1];

            if (frame[0] == 0xF0 && frame[1] == 0x42 && frame[3] == 0x58 && frame[5] == 0xF7
                && (frame[2] & 0xF0) == 0x30)
                ++shaped;

            const auto result = sysEx.parseSysEx(frame, 6, apvts);
            if (result.success && result.messageType == SysExMessageType::ProgramDumpRequest
                && result.midiChannel == channel)
                ++recognised;
            // El eco: la respuesta sale en el **mismo** byte que entró, que es lo que ata las
            // dos convenciones sin que ninguna repita la del otro.
            if (result.reply.size() == 297 && result.reply.size() > 2 && result.reply[2] == frame[2])
                ++echoed;
            if (result.reply.size() == 297 && result.reply[4] == 0x40)  // 0x40 = Program Data Dump
                ++rightSize;
        }

        check(shaped == kKorgChannelCount,
              "the contract's 16 program requests are well-formed Korg frames (F0 42 3n 58 10 F7)");
        check(recognised == kKorgChannelCount,
              "every channel the Bank Manager addresses is recognised by the plugin, on that channel");
        check(echoed == kKorgChannelCount && rightSize == kKorgChannelCount,
              "the plugin answers each request on the byte it received (297-byte program dump, command 0x40)");

        // El caso concreto, para que un fallo diga cuál: canal 1 -> F0 42 30 58 10 F7.
        const auto channelOne = sysEx.parseSysEx(kKorgProgramDumpRequest[0], 6, apvts);
        check(kKorgProgramDumpRequest[0][2] == 0x30 && kKorgChannelByte[0] == 0x30
                  && kKorgChannelByte[kKorgChannelCount - 1] == 0x3F && channelOne.reply[2] == 0x30
                  && channelOne.midiChannel == 1,
              "channel 1 is 0x30 and channel 16 is 0x3F on both sides (the whole range is addressable)");

        // Y la petición de memoria completa, también con la trama del contrato: el
        // 0x0E de cada canal recibe la memoria del plugin (128 programas) en su byte.
        int allRecognised = 0, allEchoed = 0;
        for (int channel = 1; channel <= kKorgChannelCount; ++channel)
        {
            const uint8_t *frame = kKorgAllDataDumpRequest[channel - 1];
            const auto result = sysEx.parseSysEx(frame, 6, apvts);

            if (result.success && result.messageType == SysExMessageType::AllDataDumpRequest
                && result.midiChannel == channel)
                ++allRecognised;
            if (result.reply.size() == 37163 && result.reply[2] == frame[2]
                && result.reply[4] == MS2000HardwareProgram::CMD_ALL_DATA_DUMP)
                ++allEchoed;
        }
        check(allRecognised == kKorgChannelCount,
              "the contract's 16 all-data requests (0x0E) are recognised on their channel");
        check(allEchoed == kKorgChannelCount,
              "each 0x0E is answered with the whole memory (37 163 B) on the byte it came in");
    }
}

static void testEngineExtension()
{
    // --- Test 21: extensión del motor bajo el modelo ABDSynths (Timbre 2, velocidades,
    //             escala/split y el mod sequence real) ---
    printf("\n[Test 21] Engine extension under the ABDSynths model (Timbre 2, velocity sense, scale/split, real mod "
           "sequence)...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto &apvts = dummyFixture().apvts;
        auto &dummyProc = dummyFixture().processor;
        // 1. El registro declara el espejo del Timbre 2 y los 3×16 pasos por timbre.
        const auto &allParams = ParameterRegistry::getAllParameters();
        int timbre2Params = 0, seqStepParams = 0, withSysex = 0, maxOffset = -1;
        for (const auto &m : allParams)
        {
            const std::string id(m.id);
            if (id.rfind("t2", 0) == 0)
                ++timbre2Params;
            // Pasos de fila: `seq2Step7` / `t2Seq3Step16` (no `seqLastStep`, que no es un paso)
            const size_t stepPos = id.find("Step");
            if (stepPos != std::string::npos && stepPos > 0
                && std::isdigit(static_cast<unsigned char>(id[stepPos - 1])))
                ++seqStepParams;
            if (m.sysexOffset >= 0)
            {
                ++withSysex;
                maxOffset = std::max(maxOffset, static_cast<int>(m.sysexOffset));
            }
        }
        // 123 desde UX-4: se añadió t2TriggerMode (espejo del trigger mode del Timbre 1).
        // 126: 124 (t2TriggerMode + t2KeyPriority) + t2Eg1Reset + t2Eg2Reset.
        check(timbre2Params == 126, "the registry declares the Timbre 2 mirror (126 parameters)");
        check(seqStepParams == 96, "each timbre's 3 sequencer rows have their 16 real steps (96 parameters)");
        check(withSysex == 253 && maxOffset == 252,
              "every parameter the engine models has its own byte in the program");

        // 2. Los defaults del espejo son los del Timbre 1: el INIT real tiene los dos
        //    bloques de 108 B byte a byte idénticos, así que el Timbre 2 no arranca distinto.
        const char *mirroredIds[6] = {ParamIDs::osc1Wave,
                                      ParamIDs::filterCutoff,
                                      ParamIDs::eg2Sustain,
                                      ParamIDs::lfo1Freq,
                                      ParamIDs::patch1Intensity,
                                      ParamIDs::mixNoiseLevel};
        bool sameDefaults = true;
        for (const char *id : mirroredIds)
        {
            const juce::String baseId(id);
            const juce::String twin = "t2" + baseId.substring(0, 1).toUpperCase() + baseId.substring(1);
            const auto *base = ParameterRegistry::getParameter(id);
            const auto *mirror = ParameterRegistry::getParameter(twin.toRawUTF8());
            if (base == nullptr || mirror == nullptr || base->defaultValue != mirror->defaultValue)
                sameDefaults = false;
        }
        check(sameDefaults,
              "the Timbre 2 defaults equal the Timbre 1 ones (the real program has two identical blocks)");

        // 3. La memoria nativa es v2 (384 B) y cabe todo; una trama v1 (128 B) se lee igual.
        check(MS2000ProgramData::UNPACKED_PROGRAM_SIZE == 384
                  && MS2000ProgramData::UNPACKED_PROGRAM_SIZE % MS2000ProgramData::UNPACKED_PROGRAM_SIZE_V1 == 0,
              "the native program block is v2 (384 B), a multiple of the 128 B v1");
        check(static_cast<int>(MS2000ProgramData::TIMBRE_START) + withSysex
                  <= static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE),
              "the v2 block can hold every parameter with a byte (253 at the last one)");

        MS2000ProgramData v1Preset;
        v1Preset.setName("Legacy V1");
        std::vector<uint8_t> v1Payload;
        std::vector<uint8_t> v1Frame = {
            0xF0, ABDSynthsSysEx::MANUFACTURER_ID, ABDSynthsSysEx::MODEL_BYTE, ABDSynthsSysEx::CMD_PROGRAM_DUMP};
        if (SysExCodec::pack8to7(v1Preset.rawData.data(), MS2000ProgramData::UNPACKED_PROGRAM_SIZE_V1, v1Payload))
        {
            v1Frame.insert(v1Frame.end(), v1Payload.begin(), v1Payload.end());
            v1Frame.push_back(0xF7);
        }
        SysExManager legacySysEx;
        const auto v1Result = legacySysEx.parseSysEx(v1Frame.data(), v1Frame.size(), apvts);
        check(v1Result.success && v1Result.programName == "Legacy V1",
              "a v1 native preset (128 B) is still read, the rest of the block at its defaults");

        // 4. El Timbre 2 suena con SUS parámetros: antes era una copia del Timbre 1.
        juce::AudioProcessorValueTreeState extApvts(
            dummyProc, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
        MS2000PatchBuilder::buildInitPatch(extApvts);
        auto setExt = [&extApvts](const char *id, float value)
        {
            if (auto *p = extApvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        auto renderPeak = [](SynthEngine &eng)
        {
            juce::AudioBuffer<float> buf(2, 512);
            buf.clear();
            juce::MidiBuffer noMidi;
            eng.processBlock(buf, noMidi, nullptr);
            return buf.getMagnitude(0, 0, 512);
        };

        setExt(ParamIDs::timbreMode, 2.0f);  // Layer: los dos timbres a la vez
        setExt(ParamIDs::t2AmpLevel, 0.0f);  // Timbre 2 mudo
        SynthEngine mutedEngine(extApvts);
        mutedEngine.prepare(48000.0, 512);
        mutedEngine.updateParametersFromAPVTS();  // el modo de programa lo lee el propio motor
        mutedEngine.noteOn(1, 60, 1.0f);
        const float mutedPeak = renderPeak(mutedEngine);

        setExt(ParamIDs::t2AmpLevel, 100.0f);  // nivel del INIT para el Timbre 2
        SynthEngine layeredEngine(extApvts);
        layeredEngine.prepare(48000.0, 512);
        layeredEngine.updateParametersFromAPVTS();
        layeredEngine.noteOn(1, 60, 1.0f);
        const float layeredPeak = renderPeak(layeredEngine);

        check(mutedPeak > 0.001f, "in Layer mode the Timbre 1 keeps sounding on its own");
        check(layeredPeak > mutedPeak * 1.4f, "the Timbre 2 has its own amplitude: its level adds to the Layer output");

        // 5. Sensibilidad a la velocidad del VCA (byte 28 real).
        auto renderVoicePeak = [](float velocity, float ampVeloSens)
        {
            Voice voice;
            voice.prepare(48000.0);
            VoiceParameters p;
            p.ampLevel = 1.0f;
            p.ampVeloSens = ampVeloSens;
            p.eg2Attack = 0.0f;
            p.eg2Decay = 0.0f;
            p.eg2Sustain = 1.0f;
            p.eg2Release = 0.0f;
            p.filterCutoffNorm = 1.0f;
            voice.applyBlockParams(p);
            voice.noteOn(60, velocity, false);
            float peak = 0.0f;
            for (int i = 0; i < 512; ++i)
            {
                float l = 0.0f, r = 0.0f;
                voice.renderNextSample(l, r);
                peak = std::max(peak, std::abs(l));
            }
            return peak;
        };
        check(renderVoicePeak(1.0f, 1.0f) > renderVoicePeak(0.2f, 1.0f) * 2.0f,
              "amp velocity sense (+63) opens the VCA with velocity");
        check(renderVoicePeak(0.3f, 0.0f) == renderVoicePeak(1.0f, 0.0f),
              "with amp velocity sense at 0 the level does not depend on velocity (as the hardware)");
        check(renderVoicePeak(1.0f, -1.0f) < renderVoicePeak(0.0f, -1.0f),
              "a negative amp velocity sense inverts the response");

        // 6. El mod sequence: filas, destinos y pasos reales llegan al motor.
        juce::AudioProcessorValueTreeState seqApvts(
            dummyProc, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
        MS2000PatchBuilder::buildInitPatch(seqApvts);
        auto setSeq = [&seqApvts](const char *id, float value)
        {
            if (auto *p = seqApvts.getParameter(id))
                p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        setSeq(ParamIDs::modSeqOn, 1.0f);
        setSeq(ParamIDs::modSeqResolution, 3.0f);
        setSeq(ParamIDs::seqLastStep, 8.0f);
        setSeq(ParamIDs::seq1Dest, 11.0f);   // CUTOFF: el 12º destino real
        setSeq(ParamIDs::seq1Motion, 0.0f);  // Step
        setSeq(ParamIDs::seq1Step1, -63.0f);
        setSeq(ParamIDs::t2ModSeqOn, 1.0f);
        setSeq(ParamIDs::t2Seq3Dest, 15.0f);  // AMP LEVEL en el Timbre 2
        setSeq(ParamIDs::t2Seq3Step1, 63.0f);

        ABDMS2000::SynthEngine seqEngine(seqApvts);
        seqEngine.prepare(48000.0, 512);
        seqEngine.updateParametersFromAPVTS();
        seqEngine.testGetModSequencer().advanceClock(512);

        const auto &rowA = seqEngine.testGetModSequencer().getTrack(ModSequencer::TIMBRE1_TRACK);
        const auto &rowC2 = seqEngine.testGetModSequencer().getTrack(ModSequencer::TIMBRE2_TRACK + 2);
        check(static_cast<int>(rowA.destination) == 11 && rowA.length == 8 && rowA.motion == ModSeqMotion::Step,
              "the engine reads each row's destination, motion and last step from its parameters");
        check(std::abs(rowA.steps[0] - 0.0f) < 1e-6f && std::abs(rowC2.steps[0] - 1.0f) < 1e-6f,
              "the −63..+63 step bytes land as the row's 0..1 step values");
        check(seqEngine.testGetModSequencer().getTrack(ModSequencer::TIMBRE1_TRACK + 1).steps[0] == 0.5f,
              "a step byte of 0 (the INIT value) is the centre of the row");

        VoiceParameters tmpA = seqEngine.testGetVoiceParamsA();
        VoiceParameters tmpB = seqEngine.testGetVoiceParamsB();
        seqEngine.testApplySeqModulation(false, tmpA);
        seqEngine.testApplySeqModulation(true, tmpB);
        check(tmpA.seq.cutoff == -1.0f, "the Timbre 1 row drives its own cutoff (full −1 at the bottom step)");
        check(tmpB.seq.amp == 1.0f && tmpA.seq.amp == 0.0f,
              "the Timbre 2 rows drive the Timbre 2 parameters, not the Timbre 1 ones");
        (void) rowC2;
    }
}

static void testMicrokorgHardwareParity()
{
    printf("\n[Test 28] microKORG Hardware Parity (SysEx & Rhythm Step Mask)...\n");

    // 1. Validar MicroKorgHardwareProgram: Last Step y Step Mask
    MicroKorgHardwareProgram mkProg;
    mkProg.setName("MK_TEST");
    check(mkProg.getName() == "MK_TEST", "microKORG program stores and returns its name");

    mkProg.setLastStep(6);
    check(mkProg.getLastStep() == 6, "microKORG Last Step (1..8) stores correctly");

    mkProg.setStepMaskByte(0b10101010);  // Pasos 2, 4, 6, 8 ON
    check(!mkProg.getStepMask(1) && mkProg.getStepMask(2) && !mkProg.getStepMask(3) && mkProg.getStepMask(4),
          "microKORG 8-step rhythm mask flags unpack correctly");
    check(mkProg.getStepMaskByte() == 0b10101010, "microKORG bitmask byte roundtrip matches");

    // 2. Empaquetado en volcado SysEx Korg microKORG (Model ID 0x71)
    const auto dump71 = mkProg.buildProgramDump(1, MicroKorgHardwareProgram::MODEL_ID_MICROKORG, 0x40);
    check(dump71.size() == 297, "microKORG program dump produces a valid 297-byte frame");
    check(dump71[0] == 0xF0 && dump71[1] == 0x42 && dump71[2] == 0x30 && dump71[3] == 0x71 && dump71[4] == 0x40,
          "microKORG dump header has Model ID 0x71 and Function 0x40");
    check(dump71.back() == 0xF7, "microKORG dump ends with 0xF7");

    // 3. SysExManager parseSysEx con trama microKORG (0x71)
    auto &apvts = dummyFixture().apvts;
    MS2000PatchBuilder::buildInitPatch(apvts);

    SysExManager sysEx;
    const auto parseRes = sysEx.parseSysEx(dump71.data(), dump71.size(), apvts);
    check(parseRes.success && parseRes.programName == "MK_TEST",
          "SysExManager successfully decodes and loads microKORG dump (0x71)");
    check(sysEx.hasMicrokorgProgram(), "SysExManager records valid microKORG program");

    // Verificar que los parámetros del APVTS se actualizaron
    if (auto *pLastStep = apvts.getParameter("arpLastStep"))
        check(std::round(pLastStep->convertFrom0to1(pLastStep->getValue())) == 6.0f,
              "arpLastStep APVTS parameter is updated to 6 from microKORG dump");
    if (auto *pStep2 = apvts.getParameter("arpStep2"))
        check(pStep2->convertFrom0to1(pStep2->getValue()) > 0.5f, "arpStep2 APVTS parameter is ON");
    if (auto *pStep1 = apvts.getParameter("arpStep1"))
        check(pStep1->convertFrom0to1(pStep1->getValue()) < 0.5f, "arpStep1 APVTS parameter is OFF (rest)");
    if (auto *pModSeq = apvts.getParameter("modSeqOn"))
        check(pModSeq->convertFrom0to1(pModSeq->getValue()) == 0.0f,
              "modSeqOn APVTS parameter is forced to OFF on microKORG load");

    // 4. SysExManager respuesta a Dump Request (0x10) en modo microKORG
    sysEx.setSynthMode(1);  // Perfil microKORG
    const uint8_t reqDump[] = {0xF0, 0x42, 0x30, 0x71, 0x10, 0xF7};
    const auto reqRes = sysEx.parseSysEx(reqDump, sizeof(reqDump), apvts);
    check(reqRes.success && reqRes.reply.size() == 297,
          "Dump request with 0x71 replies with a microKORG dump frame (297 bytes)");
    check(reqRes.reply[3] == 0x71, "Dump reply header preserves microKORG Model ID 0x71");
}

// ============================================================
// Case 29: Schema-Driven DSP Conversions & Extreme Clamping (DSP.7 - DSP.10)
// ============================================================
static void testDspConversionsAndClamping()
{
    printf("=== Schema-Driven DSP Conversion Tests (DSP.7-10) ===\n");

    // 1. Clamping de dominio en normalizedToDomain
    if (const auto *cutoff = ParameterRegistry::getParameter("filterCutoff"))
    {
        float hz0 = conversion::normalizedToDomain(*cutoff, 0.0f);
        float hz1 = conversion::normalizedToDomain(*cutoff, 1.0f);
        float hzUnder = conversion::normalizedToDomain(*cutoff, -10.0f);
        float hzOver = conversion::normalizedToDomain(*cutoff, 10.0f);

        check(hz0 >= cutoff->domainMin && hz0 <= cutoff->domainMax, "cutoff t=0 within domain [20, 20000]");
        check(hz1 >= cutoff->domainMin && hz1 <= cutoff->domainMax, "cutoff t=1 within domain [20, 20000]");
        check(hzUnder == cutoff->domainMin, "cutoff negative t clamped to domainMin (20Hz)");
        check(hzOver == cutoff->domainMax, "cutoff excess t clamped to domainMax (20000Hz)");

        // Inverso domainToNormalized
        check(conversion::domainToNormalized(*cutoff, 20.0f) == 0.0f, "cutoff domainToNormalized(20Hz) == 0.0");
        check(conversion::domainToNormalized(*cutoff, 20000.0f) == 1.0f, "cutoff domainToNormalized(20kHz) == 1.0");
        check(conversion::domainToNormalized(*cutoff, -100.0f) == 0.0f,
              "cutoff domainToNormalized negative clamped to 0.0");
        check(conversion::domainToNormalized(*cutoff, 50000.0f) == 1.0f,
              "cutoff domainToNormalized excess clamped to 1.0");

        // Paridad musical exacta: punto medio del knob (t=0.5) ~ 632Hz (media geometrica)
        float hzMid = conversion::normalizedToDomain(*cutoff, 0.5f);
        check(hzMid > 600.0f && hzMid < 650.0f, "cutoff normalizedToDomain mid ~ 632Hz (Exp curve parity)");
    }

    // 1b. Resonancia de filtro (Linear [0.0 .. 1.0])
    if (const auto *res = ParameterRegistry::getParameter("filterResonance"))
    {
        float r0 = conversion::normalizedToDomain(*res, 0.0f);
        float rMid = conversion::normalizedToDomain(*res, 0.5f);
        float r1 = conversion::normalizedToDomain(*res, 1.0f);
        check(r0 == 0.0f, "resonance t=0 is 0.0");
        check(std::abs(rMid - 0.5f) < 1e-4f, "resonance t=0.5 is 0.5 (Linear)");
        check(r1 == 1.0f, "resonance t=1 is 1.0");
    }

    // 2. Curva Exp (LFO1 Freq: 0.01Hz .. 20Hz)
    if (const auto *lfoFreq = ParameterRegistry::getParameter("lfo1Freq"))
    {
        float f0 = conversion::normalizedToDomain(*lfoFreq, 0.0f);
        float f1 = conversion::normalizedToDomain(*lfoFreq, 1.0f);
        float fUnder = conversion::normalizedToDomain(*lfoFreq, -5.0f);
        float fOver = conversion::normalizedToDomain(*lfoFreq, 5.0f);

        check(f0 >= lfoFreq->domainMin && f0 <= lfoFreq->domainMax, "lfo1Freq t=0 within [0.01, 20.0]");
        check(f1 >= lfoFreq->domainMin && f1 <= lfoFreq->domainMax, "lfo1Freq t=1 clamped to domainMax 20.0Hz");
        check(fUnder == f0, "lfo1Freq negative t clamped safely");
        check(fOver == lfoFreq->domainMax, "lfo1Freq excess t clamped to domainMax");
    }

    // 2b. ModFX Speed (Exp [0.02Hz .. 15.0Hz])
    if (const auto *mSpeed = ParameterRegistry::getParameter("modFxSpeed"))
    {
        float s0 = conversion::normalizedToDomain(*mSpeed, 0.0f);
        float sMid = conversion::normalizedToDomain(*mSpeed, 0.5f);
        float s1 = conversion::normalizedToDomain(*mSpeed, 1.0f);
        check(s0 == 0.02f, "modFxSpeed t=0 is 0.02Hz");
        check(sMid > 0.5f && sMid < 0.6f, "modFxSpeed t=0.5 ~ 0.55Hz (geometric mean)");
        check(s1 == 15.0f, "modFxSpeed t=1 is 15.0Hz");
    }

    // 2c. EQ Gains (Bipolar Linear [-12.0dB .. +12.0dB])
    if (const auto *eqG = ParameterRegistry::getParameter("eqLowGain"))
    {
        float g0 = conversion::normalizedToDomain(*eqG, 0.0f);
        float gMid = conversion::normalizedToDomain(*eqG, 0.5f);
        float g1 = conversion::normalizedToDomain(*eqG, 1.0f);
        check(g0 == -12.0f, "eqLowGain t=0 is -12dB");
        check(std::abs(gMid - 0.0f) < 1e-4f, "eqLowGain t=0.5 is 0dB");
        check(g1 == 12.0f, "eqLowGain t=1 is +12dB");
    }

    // 2d. Mixer Levels & Amp (Linear [0.0 .. 1.0], ampPan Bipolar [-1.0 .. +1.0])
    if (const auto *mix1 = ParameterRegistry::getParameter("mixOsc1Level"))
    {
        float m0 = conversion::normalizedToDomain(*mix1, 0.0f);
        float mMid = conversion::normalizedToDomain(*mix1, 0.5f);
        float m1 = conversion::normalizedToDomain(*mix1, 1.0f);
        check(m0 == 0.0f, "mixOsc1Level t=0 is 0.0");
        check(std::abs(mMid - 0.5f) < 1e-4f, "mixOsc1Level t=0.5 is 0.5");
        check(m1 == 1.0f, "mixOsc1Level t=1 is 1.0");
    }

    if (const auto *aLevel = ParameterRegistry::getParameter("ampLevel"))
    {
        float a0 = conversion::normalizedToDomain(*aLevel, 0.0f);
        float a1 = conversion::normalizedToDomain(*aLevel, 1.0f);
        check(a0 == 0.0f, "ampLevel t=0 is 0.0");
        check(a1 == 1.0f, "ampLevel t=1 is 1.0");
    }

    if (const auto *aPan = ParameterRegistry::getParameter("ampPan"))
    {
        float p0 = conversion::normalizedToDomain(*aPan, 0.0f);
        float pMid = conversion::normalizedToDomain(*aPan, 0.5f);
        float p1 = conversion::normalizedToDomain(*aPan, 1.0f);
        check(p0 == -1.0f, "ampPan t=0 is -1.0 (L)");
        check(std::abs(pMid - 0.0f) < 1e-4f, "ampPan t=0.5 is 0.0 (C)");
        check(p1 == 1.0f, "ampPan t=1 is 1.0 (R)");
    }

    // 2e. Envelopes EG1 & EG2 (Linear [0.0 .. 1.0])
    if (const auto *eg1A = ParameterRegistry::getParameter("eg1Attack"))
    {
        float a0 = conversion::normalizedToDomain(*eg1A, 0.0f);
        float aMid = conversion::normalizedToDomain(*eg1A, 0.5f);
        float a1 = conversion::normalizedToDomain(*eg1A, 1.0f);
        check(a0 == 0.0f, "eg1Attack t=0 is 0.0");
        check(std::abs(aMid - 0.5f) < 1e-4f, "eg1Attack t=0.5 is 0.5");
        check(a1 == 1.0f, "eg1Attack t=1 is 1.0");
    }

    if (const auto *eg2S = ParameterRegistry::getParameter("eg2Sustain"))
    {
        float s0 = conversion::normalizedToDomain(*eg2S, 0.0f);
        float sMid = conversion::normalizedToDomain(*eg2S, 0.5f);
        float s1 = conversion::normalizedToDomain(*eg2S, 1.0f);
        check(s0 == 0.0f, "eg2Sustain t=0 is 0.0");
        check(std::abs(sMid - 0.5f) < 1e-4f, "eg2Sustain t=0.5 is 0.5");
        check(s1 == 1.0f, "eg2Sustain t=1 is 1.0");
    }

    // 2f. OSC1 Ctrl & Portamento (Linear [0.0 .. 1.0])
    if (const auto *pTime = ParameterRegistry::getParameter("portamentoTime"))
    {
        float p0 = conversion::normalizedToDomain(*pTime, 0.0f);
        float pMid = conversion::normalizedToDomain(*pTime, 0.5f);
        float p1 = conversion::normalizedToDomain(*pTime, 1.0f);
        check(p0 == 0.0f, "portamentoTime t=0 is 0.0");
        check(std::abs(pMid - 0.5f) < 1e-4f, "portamentoTime t=0.5 is 0.5");
        check(p1 == 1.0f, "portamentoTime t=1 is 1.0");
    }

    if (const auto *c1 = ParameterRegistry::getParameter("osc1Ctrl1"))
    {
        float c0 = conversion::normalizedToDomain(*c1, 0.0f);
        float c1Val = conversion::normalizedToDomain(*c1, 1.0f);
        check(c0 == 0.0f, "osc1Ctrl1 t=0 is 0.0");
        check(c1Val == 1.0f, "osc1Ctrl1 t=1 is 1.0");
    }

    // 2g. Bipolar Modulations & Sensitivities (Bipolar Linear [-1.0 .. +1.0])
    if (const auto *fInt = ParameterRegistry::getParameter("filterEg1Int"))
    {
        float i0 = conversion::normalizedToDomain(*fInt, 0.0f);
        float iMid = conversion::normalizedToDomain(*fInt, 0.5f);
        float i1 = conversion::normalizedToDomain(*fInt, 1.0f);
        check(i0 == -1.0f, "filterEg1Int t=0 is -1.0");
        check(std::abs(iMid - 0.0f) < 1e-4f, "filterEg1Int t=0.5 is 0.0");
        check(i1 == 1.0f, "filterEg1Int t=1 is +1.0");
    }

    if (const auto *fKbd = ParameterRegistry::getParameter("filterKeyTrack"))
    {
        float k0 = conversion::normalizedToDomain(*fKbd, 0.0f);
        float kMid = conversion::normalizedToDomain(*fKbd, 0.5f);
        float k1 = conversion::normalizedToDomain(*fKbd, 1.0f);
        check(k0 == -1.0f, "filterKeyTrack t=0 is -1.0");
        check(std::abs(kMid - 0.0f) < 1e-4f, "filterKeyTrack t=0.5 is 0.0");
        check(k1 == 1.0f, "filterKeyTrack t=1 is +1.0");
    }

    if (const auto *aVel = ParameterRegistry::getParameter("ampVelo"))
    {
        float v0 = conversion::normalizedToDomain(*aVel, 0.0f);
        float vMid = conversion::normalizedToDomain(*aVel, 0.5f);
        float v1 = conversion::normalizedToDomain(*aVel, 1.0f);
        check(v0 == -1.0f, "ampVelo t=0 is -1.0");
        check(std::abs(vMid - 0.0f) < 1e-4f, "ampVelo t=0.5 is 0.0");
        check(v1 == 1.0f, "ampVelo t=1 is +1.0");
    }

    // 2h. Virtual Patch Intensity, Delay Depth/Feedback & Arp Gate
    if (const auto *p1Int = ParameterRegistry::getParameter("patch1Intensity"))
    {
        float pi0 = conversion::normalizedToDomain(*p1Int, 0.0f);
        float piMid = conversion::normalizedToDomain(*p1Int, 0.5f);
        float pi1 = conversion::normalizedToDomain(*p1Int, 1.0f);
        check(pi0 == -1.0f, "patch1Intensity t=0 is -1.0");
        check(std::abs(piMid - 0.0f) < 1e-4f, "patch1Intensity t=0.5 is 0.0");
        check(pi1 == 1.0f, "patch1Intensity t=1 is +1.0");
    }

    if (const auto *dDep = ParameterRegistry::getParameter("delayDepth"))
    {
        float d0 = conversion::normalizedToDomain(*dDep, 0.0f);
        float d1 = conversion::normalizedToDomain(*dDep, 1.0f);
        check(d0 == 0.0f, "delayDepth t=0 is 0.0");
        check(d1 == 1.0f, "delayDepth t=1 is 1.0");
    }

    if (const auto *dFb = ParameterRegistry::getParameter("delayFeedback"))
    {
        float fb0 = conversion::normalizedToDomain(*dFb, 0.0f);
        float fb1 = conversion::normalizedToDomain(*dFb, 1.0f);
        check(fb0 == 0.0f, "delayFeedback t=0 is 0.0");
        check(fb1 == 1.0f, "delayFeedback t=1 is 1.0");
    }

    if (const auto *aGate = ParameterRegistry::getParameter("arpGate"))
    {
        float g0 = conversion::normalizedToDomain(*aGate, 0.0f);
        float g1 = conversion::normalizedToDomain(*aGate, 1.0f);
        check(g0 == 0.0f, "arpGate t=0 is 0.0");
        check(g1 == 1.0f, "arpGate t=1 is 1.0");
    }

    // 3. Robustez masiva sobre todos los parámetros registrados (DSP.10)
    const auto &allParams = ParameterRegistry::getAllParameters();
    bool allValid = true;
    for (const auto &p : allParams)
    {
        const float testValues[] = {0.0f, 0.5f, 1.0f, -1.0f, 2.0f};
        for (float t : testValues)
        {
            float dom = conversion::normalizedToDomain(p, t);
            if (std::isnan(dom) || std::isinf(dom))
                allValid = false;
            if (p.domainMin < p.domainMax && (dom < p.domainMin - 0.001f || dom > p.domainMax + 0.001f))
                allValid = false;

            float norm = conversion::domainToNormalized(p, dom);
            if (std::isnan(norm) || std::isinf(norm) || norm < 0.0f || norm > 1.0f)
                allValid = false;
        }
    }
    check(allValid, "All registered parameters produce bounded non-NaN values under extreme inputs (DSP.10)");
}

#if defined(_MSC_VER)
#define ABD_TEST_NOINLINE __declspec(noinline)
#else
#define ABD_TEST_NOINLINE __attribute__((noinline))
#endif

static ABD_TEST_NOINLINE void testFirmwareHardwareParity()
{
    printf("\n[Test 29] Firmware v1.07 Hardware Tables & DSP Signal Path Parity...\n");

    check(runLfoFirmwareParityTests(),
          "Both LFO ROM tables match embedded v1.07 firmware; LFO1/LFO2 base and modulated limits are 500 Hz");

    // 1. ROM Envelope table (0x27D54) calibrated at 48 kHz
    float envMinSec = FirmwareTables::getHardwareEnvelopeTimeSeconds(0);
    float envMaxSec = FirmwareTables::getHardwareEnvelopeTimeSeconds(127);
    check(std::abs(envMinSec - 0.001f) < 0.0001f, "ROM Envelope index 0 equals 1.000 ms (48 samples @ 48 kHz)");
    check(std::abs(envMaxSec - 3.7992f) < 0.01f, "ROM Envelope index 127 equals 3.7992 s (182,361 samples @ 48 kHz)");

    // 2. ROM LFO1 (0x295E8) vs LFO2 (0x29BE8): first differ at knob 115
    check(std::abs(FirmwareTables::getHardwareLfo2SpeedRatio(0.0f) - 1.0f) < 1e-5f,
          "LFO2/LFO1 speed ratio is 1.0x at knob 0");
    check(std::abs(FirmwareTables::getHardwareLfo2SpeedRatio(114.0f) - 1.0f) < 1e-5f,
          "LFO2/LFO1 speed ratio is 1.0x through knob 114");
    check(FirmwareTables::getHardwareLfo2SpeedRatio(115.0f) < 1.0f
              && FirmwareTables::getHardwareLfo2SpeedRatio(120.0f) < 1.0f,
          "LFO2 ROM increments are lower than LFO1 at knobs 115 and 120");
    check(FirmwareTables::getHardwareLfo2SpeedRatio(121.0f) > 1.0f,
          "LFO2 ROM increments first exceed LFO1 at knob 121");
    check(std::abs(FirmwareTables::getHardwareLfo2SpeedRatio(127.0f) - 5.32385f) < 0.001f,
          "LFO2/LFO1 ROM increment ratio is 5.32385x at knob 127");

    // 2b. CDD 4.5: `curve: "Lut"` resuelve la tabla de firmware del contrato, no una formula
    int lutParamCount = 0;
    bool lutRegistryConsistent = true;
    for (const auto &entry : ParameterRegistry::getAllParameters())
    {
        if (entry.curve != 3)
            continue;
        ++lutParamCount;
        if (FirmwareTables::getLutIncrementsQ23(entry.lutId) == nullptr
            || std::abs(FirmwareTables::getLutValueHz(entry.lutId, 0.0f) - entry.domainMin) > 1e-7f
            || std::abs(FirmwareTables::getLutValueHz(entry.lutId, 1.0f) - entry.domainMax) > 1e-4f)
            lutRegistryConsistent = false;
    }
    check(lutParamCount == 4 && lutRegistryConsistent,
          "The 4 curve=Lut params resolve their lutId and their domain equals the ROM table extremes");

    const auto lutKnobHz = [](bool useLfo2, int knob) {
        const int clamped = std::clamp(knob, 0, 127);
        return useLfo2 ? FirmwareTables::getHardwareLfo2FreqHz(static_cast<float>(clamped))
                       : FirmwareTables::getHardwareLfo1FreqHz(static_cast<float>(clamped));
    };

    bool lutCurveSamplesTable = true;
    bool lutExactAnchors = true;
    bool lutInverseRoundTrips = true;
    for (const char *paramId : {"lfo1Freq", "lfo2Freq", "t2Lfo1Freq", "t2Lfo2Freq"})
    {
        const auto *meta = ParameterRegistry::getParameter(paramId);
        if (meta == nullptr || meta->curve != 3)
        {
            lutCurveSamplesTable = false;
            continue;
        }
        const bool useLfo2 = std::strcmp(meta->lutId, "ms2000_lfo2_hz") == 0;

        for (int knob = 0; knob < 128; ++knob)
        {
            const float normalized = static_cast<float>(knob) / 127.0f;
            const float lower = lutKnobHz(useLfo2, knob - 1);
            const float upper = lutKnobHz(useLfo2, knob + 1);

            // El normalizado en float puede caer a medio paso de la entrada `knob`:
            // la curva debe quedar entre las entradas vecinas de la tabla, nunca fuera.
            const float value = conversion::normalizedToDomain(*meta, normalized);
            if (value < lower - 1e-4f * lower || value > upper + 1e-4f * upper)
                lutCurveSamplesTable = false;

            const float back = conversion::domainToNormalized(*meta, lutKnobHz(useLfo2, knob));
            const float again = conversion::normalizedToDomain(*meta, back);
            if (again < lower - 1e-4f * lower || again > upper + 1e-4f * upper)
                lutInverseRoundTrips = false;
        }

        // 127 * t es exacto en float para estos tres puntos: la curva debe devolver
        // bit a bit la misma interpolacion que el helper de la tabla ROM.
        for (const float t : {0.25f, 0.5f, 0.75f})
        {
            const float expected = useLfo2 ? FirmwareTables::getHardwareLfo2FreqHz(t * 127.0f)
                                           : FirmwareTables::getHardwareLfo1FreqHz(t * 127.0f);
            if (conversion::normalizedToDomain(*meta, t) != expected)
                lutExactAnchors = false;
        }
    }
    check(lutCurveSamplesTable,
          "normalizedToDomain samples the ROM LFO tables over all 128 knobs (timbre 1 and 2)");
    check(lutExactAnchors,
          "Lut curve returns the ROM table interpolation bit-exactly at t = 0.25 / 0.5 / 0.75");
    check(lutInverseRoundTrips,
          "domainToNormalized returns a normalized value that resolves back to the same table frequency");

    const auto *lfo1Meta = ParameterRegistry::getParameter("lfo1Freq");
    const auto *lfo2Meta = ParameterRegistry::getParameter("lfo2Freq");
    check(lfo1Meta != nullptr && lfo2Meta != nullptr
              && std::abs(conversion::normalizedToDomain(*lfo1Meta, 64.0f / 127.0f) - 2.40325927734375f) < 1e-4f
              && std::abs(conversion::normalizedToDomain(*lfo1Meta, 1.0f) - 36.609649658203125f) < 1e-3f
              && std::abs(conversion::normalizedToDomain(*lfo2Meta, 1.0f) - 194.90432739257812f) < 1e-3f,
          "Lut anchors: LFO1 knob 64 = 2.4033 Hz, LFO1 max = 36.61 Hz, LFO2 max = 194.90 Hz (neither 20 nor 63 Hz)");

    // 3. ROM Master Pitch table (0x240F0) 12-TET octave doubling in linear region
    uint32_t p60 = FirmwareTables::kRomMasterPitchQ23[60];
    uint32_t p72 = FirmwareTables::kRomMasterPitchQ23[72];
    check(std::abs(static_cast<float>(p72) / static_cast<float>(p60) - 2.0f) < 0.01f,
          "ROM Master Pitch table doubles every 12 semitones");

    // 4. VAOscillator Control 1 wave-shaping & Sine CROSS FM
    VAOscillator osc;
    osc.prepare(48000.0);
    osc.setFrequency(440.0f);

    // Pulse: Control 1 = 0 -> 50% duty cycle (mean over 1 period ~ 0)
    osc.setWaveform(VAWaveform::Pulse);
    osc.setControl1(0.0f);
    osc.reset();
    int negCount50 = 0;
    const int N = 4800;
    for (int i = 0; i < N; ++i)
        if (osc.getNextSample() < 0.0f)
            ++negCount50;
    osc.setControl1(0.8f);
    osc.reset();
    int negCountNarrow = 0;
    for (int i = 0; i < N; ++i)
        if (osc.getNextSample() < 0.0f)
            ++negCountNarrow;
    check(std::abs(negCount50 - (N / 2)) < 60 && negCountNarrow < (N / 4),
          "OSC1 Pulse Control 1 sweeps duty cycle from 50% square to narrow pulse");

    // Sine CROSS FM modulation via getNextSample(phaseModulation)
    osc.setWaveform(VAWaveform::Sine);
    osc.setControl1(0.8f);
    osc.reset();
    float cleanSine = osc.getNextSample(0.0f);
    osc.reset();
    float fmSine = osc.getNextSample(0.25f);
    check(std::abs(fmSine - cleanSine) > 0.1f, "OSC1 Sine (CROSS) phase-modulates with OSC2 signal");

    // 5. OSC2Modulator RingMod and Ring+Sync (CrossMod)
    OSC2Modulator mod;
    check(std::abs(mod.process(0.5f, -0.5f, OSC2ModulationMode::Off) - (-0.5f)) < 1e-6f,
          "OSC2Modulator Off passes raw OSC2");
    check(std::abs(mod.process(0.5f, -0.5f, OSC2ModulationMode::RingMod) - (-0.25f)) < 1e-6f,
          "OSC2Modulator RingMod multiplies OSC1 * OSC2");
    check(std::abs(mod.process(0.5f, -0.5f, OSC2ModulationMode::CrossMod) - (-0.25f)) < 1e-6f,
          "OSC2Modulator CrossMod (Ring+Sync) also multiplies OSC1 * OSC2");

    // 6. SysEx 0x1C All Program Data Dump Request
    auto sysex = std::make_unique<SysExManager>();
    const uint8_t req1C[] = {0xF0, 0x42, 0x30, 0x58, 0x1C, 0xF7};
    auto res1C = sysex->parseSysEx(req1C, sizeof(req1C), dummyFixture().apvts);
    check(res1C.success && res1C.messageType == SysExMessageType::AllDataDumpRequest,
          "SysEx 0x1C is recognized as All Program Data Dump Request");

    // 7. 128 Authentic Factory Preload Presets (extracted from flash.bin @ 0x0B0000)
    auto factoryBank = std::make_unique<MS2000FactoryBank>();
    check(factoryBank->getNumPresets() == 128,
          "MS2000FactoryBank contains all 128 authentic factory presets (A01..H16)");
    check(factoryBank->getPreset(0).name == "Stab Saw", "Factory preset A01 (index 0) is 'Stab Saw'");
    check(factoryBank->getPreset(127).name == "INIT Program", "Factory preset H16 (index 127) is 'INIT Program'");

    // 8. 64 Authentic DWGS Hardware Wavetables (extracted from flash.bin @ 0x040000)
    const float *dwgsTableData = DWGSTables::getTableData();
    bool all64DwgsHaveEnergy = (dwgsTableData != nullptr);
    for (size_t w = 0; w < 64 && all64DwgsHaveEnergy; ++w)
    {
        float energy = 0.0f;
        const float *wavePtr = dwgsTableData + w * DWGSTables::SAMPLES_PER_TABLE;
        for (size_t i = 0; i < DWGSTables::SAMPLES_PER_TABLE; i += 32)
        {
            float sample = wavePtr[i];
            energy += sample * sample;
        }
        if (energy < 1e-4f)
            all64DwgsHaveEnergy = false;
    }
    check(all64DwgsHaveEnergy, "All 64 authentic DWGS hardware wavetables (slots 0..63) have non-zero waveform energy");

    // 9. Parameter Translation Layer Parity (LFO ROM Hz, VCF ROM Cutoff Hz, Bipolar Wire Codec +64)
    check(std::abs(FirmwareTables::getHardwareLfo1FreqHz(0.0f) - 0.04578f) < 0.001f,
          "ROM LFO1 knob 0 equals 0.0458 Hz (inc=0x000010 @ 48 kHz)");
    check(std::abs(FirmwareTables::getHardwareLfo1FreqHz(127.0f) - 36.610f) < 0.05f,
          "ROM LFO1 knob 127 equals 36.61 Hz without premature clamping at knob 108");
    check(std::abs(FirmwareTables::getHardwareLfo2FreqHz(127.0f) - 194.904f) < 0.1f,
          "ROM LFO2 knob 127 reaches 194.90 Hz audio-rate modulation (~5.3239x LFO1)");
    check(std::abs(FirmwareTables::getHardwareFilterCutoffHz(0.0f) - 65.399f) < 0.1f,
          "VCF Cutoff knob 0 (kRomMasterPitchQ23[128] = 0x004621, c = 0.00428) equals 65.40 Hz");
    check(std::abs(FirmwareTables::getHardwareFilterCutoffHz(1.0f) - 18236.31f) < 0.5f,
          "VCF Cutoff knob 127 saturates at the $770000 coefficient ceiling (18,236 Hz, not 20 kHz)");
    check(std::abs(FirmwareTables::getHardwareFilterCutoffHz(0.0f, -4.0f) - 5.0f) < 0.01f,
          "VCF Cutoff negative modulation reaches sub-20Hz closing region (5.0 Hz floor)");
    {
        // El coeficiente del DSP c = min(2w, 0x770000/2^23) * w3 y fc = (48000/pi)*asin(c): +12 índices
        // de la tabla de pitch duplican c, pero asin comprime, así que la octava NO es exacta
        // (1.911x en el knob 64). En modOctaves pasamos +1.0 octava para medir esa ley de modulación.
        const float fcBase = FirmwareTables::getHardwareFilterCutoffHz(64.0f / 127.0f, 0.0f);
        const float fcPlus1Oct = FirmwareTables::getHardwareFilterCutoffHz(64.0f / 127.0f, 1.0f);
        const float fcKnob108 = FirmwareTables::getHardwareFilterCutoffHz(108.0f / 127.0f, 0.0f);
        const float fcKnob110 = FirmwareTables::getHardwareFilterCutoffHz(110.0f / 127.0f, 0.0f);
        check(std::abs(fcBase - 2636.77f) < 0.5f, "VCF Cutoff knob 64 equals 2,636.8 Hz (c = 0.1717, asin law)");
        check(std::abs((fcPlus1Oct / fcBase) - 2.020f) < 0.12f,
              "VCF Cutoff +1.0 octave follows the asin law (non-linear near Nyquist)");
        check(fcKnob108 < fcKnob110 && std::abs(fcKnob110 - 18236.31f) < 0.5f
                  && fcKnob110 == FirmwareTables::getHardwareFilterCutoffHz(1.0f),
              "VCF Cutoff is flat above knob 109 ($770000 coefficient clamp)");
    }
    check(std::abs(conversion::wireToDomain("eqLowGain", 64) - 0.0f) < 1e-4f
              && conversion::domainToWire("eqLowGain", 0.0f) == 64,
          "conversion::wireToDomain/domainToWire maps Korg bipolar center byte 64 <-> 0.0 dB");

    // 10. Embedded Hardware Firmware Updater v1.07 (x811v107.sys) & IPL Protocol Verification
    auto iplUpdater = std::make_unique<MS2000IplUpdater>();
    check(iplUpdater->isEmbeddedFirmwareValid(),
          "Embedded x811v107.sys (256 KB) passes SHA-1 and 16-bit word sum verification");
    check(iplUpdater->getEmbeddedSha1() == ms2kipl::kV107Sha1,
          "Embedded x811v107.sys SHA-1 matches official Korg v1.07 (945a7555...)");
    check(iplUpdater->getDumpSysSha1() == ms2kipl::kDumpSysSha1,
          "Patched Flash-Dump IPL v1.07 SHA-1 matches verified image (cd85d579...)");
    const auto blk0Msg = ms2kipl::msgBlock(0, iplUpdater->getEmbeddedV107().data());
    check(blk0Msg.size() == 9372 && blk0Msg.front() == 0xF0 && blk0Msg.back() == 0xF7,
          "ms2kipl::msgBlock packs 8192 bytes into 9372-byte Korg IPL SysEx frame (MSB-first)");
    ms2kipl::Version parsedVer;
    const ms2kipl::Bytes verReply = {0xF0, 0x42, 0x23, 0x58, 0x01, 0x07, 0x01, 0x00, 0x01, 0x00, 0x01, 0xF7};
    check(ms2kipl::parseVersion(verReply, parsedVer) && parsedVer.sys == 0x0107
              && ms2kipl::versionText(parsedVer.sys) == "1.07",
          "ms2kipl::parseVersion decodes System v1.07 / PCM v1.00 / User v1.00 IPL reply");
}

static ABD_TEST_NOINLINE void testHardwareMidiSysEx41AndTimbreOps()
{
    printf("\n[Test 30] Official MS2000 MIDI CCs, NRPNs, SysEx 0x41/0x11/0x4E & Timbre 1<->2 Ops...\n");

    // 1. Official Korg MS2000 MIDI CC Table Verification
    const auto *ccModType = MIDIMap::findByCC(82);
    check(ccModType != nullptr && juce::String(ccModType->paramId) == "osc2ModType",
          "CC#82 maps to osc2ModType (Official Korg MS2000 MIDI Implementation)");
    const auto *ccSemitone = MIDIMap::findByCC(18);
    check(ccSemitone != nullptr && juce::String(ccSemitone->paramId) == "osc2Semitone", "CC#18 maps to osc2Semitone");
    const auto *ccTune = MIDIMap::findByCC(19);
    check(ccTune != nullptr && juce::String(ccTune->paramId) == "osc2Tune", "CC#19 maps to osc2Tune");
    const auto *ccKbdTrack = MIDIMap::findByCC(85);
    check(ccKbdTrack != nullptr && juce::String(ccKbdTrack->paramId) == "filterKeyTrack",
          "CC#85 maps to filterKeyTrack");
    const auto *ccAmpLevel = MIDIMap::findByCC(7);
    check(ccAmpLevel != nullptr && juce::String(ccAmpLevel->paramId) == "ampLevel", "CC#7 maps to ampLevel");
    const auto *ccDistortion = MIDIMap::findByCC(92);
    check(ccDistortion != nullptr && juce::String(ccDistortion->paramId) == "ampDistortion",
          "CC#92 maps to ampDistortion");
    const auto *ccEg2Sustain = MIDIMap::findByCC(70);
    check(ccEg2Sustain != nullptr && juce::String(ccEg2Sustain->paramId) == "eg2Sustain", "CC#70 maps to eg2Sustain");
    const auto *ccLfo1Wave = MIDIMap::findByCC(87);
    check(ccLfo1Wave != nullptr && juce::String(ccLfo1Wave->paramId) == "lfo1Wave", "CC#87 maps to lfo1Wave");
    const auto *ccLfo1Freq = MIDIMap::findByCC(27);
    check(ccLfo1Freq != nullptr && juce::String(ccLfo1Freq->paramId) == "lfo1Freq", "CC#27 maps to lfo1Freq");
    const auto *ccLfo2Freq = MIDIMap::findByCC(76);
    check(ccLfo2Freq != nullptr && juce::String(ccLfo2Freq->paramId) == "lfo2Freq", "CC#76 maps to lfo2Freq");
    const auto *ccPatch1Int = MIDIMap::findByCC(28);
    check(ccPatch1Int != nullptr && juce::String(ccPatch1Int->paramId) == "patch1Intensity",
          "CC#28 maps to patch1Intensity");
    const auto *ccModSeqOn = MIDIMap::findByCC(89);
    check(ccModSeqOn != nullptr && juce::String(ccModSeqOn->paramId) == "modSeqOn", "CC#89 maps to modSeqOn");

    // 2. Official Korg MS2000 Hardware NRPN Table (MSB 0 Arp, MSB 4 Virtual Patch & Mod Seq)
    const auto *nrpnArpOn = MIDIMap::findByNRPN(0, 2);
    check(nrpnArpOn != nullptr && juce::String(nrpnArpOn->paramId) == "arpOn", "NRPN (0, 2) maps to arpOn");
    const auto *nrpnArpType = MIDIMap::findByNRPN(0, 7);
    check(nrpnArpType != nullptr && juce::String(nrpnArpType->paramId) == "arpType", "NRPN (0, 7) maps to arpType");
    const auto *nrpnPatch1Src = MIDIMap::findByNRPN(4, 0);
    check(nrpnPatch1Src != nullptr && juce::String(nrpnPatch1Src->paramId) == "patch1Source",
          "NRPN (4, 0) maps to patch1Source");
    const auto *nrpnPatch1Dest = MIDIMap::findByNRPN(4, 8);
    check(nrpnPatch1Dest != nullptr && juce::String(nrpnPatch1Dest->paramId) == "patch1Destination",
          "NRPN (4, 8) maps to patch1Destination");
    const auto *nrpnSeq1Step1 = MIDIMap::findByNRPN(4, 16);
    check(nrpnSeq1Step1 != nullptr && juce::String(nrpnSeq1Step1->paramId) == "seq1Step1",
          "NRPN (4, 16) maps to seq1Step1");
    const auto *nrpnSeq3Step16 = MIDIMap::findByNRPN(4, 63);
    check(nrpnSeq3Step16 != nullptr && juce::String(nrpnSeq3Step16->paramId) == "seq3Step16",
          "NRPN (4, 63) maps to seq3Step16");

    // Discrete Choice CC mapping verification (e.g. Filter Type CC#83: 0,32,64,96 <-> 0..3)
    const auto *ccFilterType = MIDIMap::findByCC(83);
    check(ccFilterType != nullptr && std::round(MIDIMap::midiValueToParamValue(*ccFilterType, 96)) == 3.0f,
          "CC#83 value 96 maps to Filter Type index 3 (HPF12)");

    // 3. SysEx Parameter Change (0x41), Program Write Request (0x11), Mode Change (0x4E)
    auto &apvts = dummyFixture().apvts;
    MS2000PatchBuilder::buildInitPatch(apvts);
    SysExManager sysex;

    // Test outgoing buildParameterChangeForEngineParam & incoming parseSysEx(0x41)
    auto msgDwgs = MS2000HardwareProgram::buildParameterChangeForEngineParam(1, "osc1DwgsWave", 42.0f);
    check(msgDwgs.size() == 10 && msgDwgs[4] == 0x41 && msgDwgs.back() == 0xF7,
          "buildParameterChangeForEngineParam(osc1DwgsWave) builds a 10-byte 0x41 SysEx frame");
    auto res41 = sysex.parseSysEx(msgDwgs.data(), msgDwgs.size(), apvts);
    check(res41.success && res41.messageType == SysExMessageType::ParameterChange,
          "SysExManager::parseSysEx decodes 0x41 Parameter Change");
    if (auto *pDwgs = apvts.getParameter("osc1DwgsWave"))
        check(std::round(pDwgs->convertFrom0to1(pDwgs->getValue())) == 42.0f,
              "0x41 Parameter Change updates osc1DwgsWave in APVTS to 42");

    // Timbre 2 Parameter Change (+272 offset shift)
    auto msgT2Dwgs = MS2000HardwareProgram::buildParameterChangeForEngineParam(1, "t2Osc1DwgsWave", 19.0f);
    check(msgT2Dwgs.size() == 10 && msgT2Dwgs[4] == 0x41,
          "buildParameterChangeForEngineParam(t2Osc1DwgsWave) builds 0x41 with Timbre 2 +272 offset");
    auto res41T2 = sysex.parseSysEx(msgT2Dwgs.data(), msgT2Dwgs.size(), apvts);
    check(res41T2.success, "SysExManager::parseSysEx applies Timbre 2 0x41 Parameter Change");
    if (auto *pT2Dwgs = apvts.getParameter("t2Osc1DwgsWave"))
        check(std::round(pT2Dwgs->convertFrom0to1(pT2Dwgs->getValue())) == 19.0f,
              "Timbre 2 0x41 Parameter Change updates t2Osc1DwgsWave in APVTS to 19");

    // Bipolar signed parameter via 0x41 (filterEg1Int = -25)
    auto msgEgInt = MS2000HardwareProgram::buildParameterChangeForEngineParam(1, "filterEg1Int", -25.0f);
    check(msgEgInt.size() == 10, "buildParameterChangeForEngineParam(filterEg1Int, -25.0f) encodes negative value");
    sysex.parseSysEx(msgEgInt.data(), msgEgInt.size(), apvts);
    if (auto *pEgInt = apvts.getParameter("filterEg1Int"))
        check(std::round(pEgInt->convertFrom0to1(pEgInt->getValue())) == -25.0f,
              "Negative 0x41 Parameter Change updates filterEg1Int to -25");

    // Program Write Request (0x11) and Mode Change (0x4E)
    auto msgWrite = MS2000HardwareProgram::buildProgramWriteRequest(1, 15);
    check(msgWrite.size() == 8 && msgWrite[4] == 0x11 && msgWrite[6] == 15,
          "buildProgramWriteRequest builds 8-byte 0x11 frame with program index");
    auto res11 = sysex.parseSysEx(msgWrite.data(), msgWrite.size(), apvts);
    check(res11.success && res11.messageType == SysExMessageType::WriteCompleted && sysex.getActiveProgramIndex() == 15,
          "SysExManager::parseSysEx recognizes 0x11 Program Write Request and updates active slot");

    auto msgMode = MS2000HardwareProgram::buildModeChange(1, 0);
    check(msgMode.size() == 8 && msgMode[4] == 0x4E && msgMode[5] == 0x00, "buildModeChange builds 8-byte 0x4E frame");
    auto res4E = sysex.parseSysEx(msgMode.data(), msgMode.size(), apvts);
    check(res4E.success && res4E.messageType == SysExMessageType::ParameterChange,
          "SysExManager::parseSysEx recognizes 0x4E Mode Change");

    // 4. Timbre 1 <-> Timbre 2 Copy & Swap Operations
    auto setApvtsParam = [&apvts](const char *id, float val)
    {
        if (auto *p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(val));
    };
    auto getApvtsParam = [&apvts](const char *id) -> float
    {
        if (auto *p = apvts.getParameter(id))
            return p->convertFrom0to1(p->getValue());
        return 0.0f;
    };

    setApvtsParam("filterCutoff", 88.0f);
    setApvtsParam("t2FilterCutoff", 22.0f);
    setApvtsParam("osc1Wave", 5.0f);
    setApvtsParam("t2Osc1Wave", 1.0f);

    MS2000PatchBuilder::swapTimbres(apvts);
    check(std::round(getApvtsParam("filterCutoff")) == 22.0f && std::round(getApvtsParam("t2FilterCutoff")) == 88.0f,
          "MS2000PatchBuilder::swapTimbres exchanges Timbre 1 and Timbre 2 parameters");

    MS2000PatchBuilder::copyTimbre2ToTimbre1(apvts);
    check(std::round(getApvtsParam("filterCutoff")) == 88.0f,
          "MS2000PatchBuilder::copyTimbre2ToTimbre1 copies Timbre 2 into Timbre 1");

    setApvtsParam("filterCutoff", 55.0f);
    MS2000PatchBuilder::copyTimbre1ToTimbre2(apvts);
    check(std::round(getApvtsParam("t2FilterCutoff")) == 55.0f,
          "MS2000PatchBuilder::copyTimbre1ToTimbre2 copies Timbre 1 into Timbre 2");

    // 4. Musical Randomizer (ReMS2000 bounds & HPF protection across Timbre 1, Timbre 2 & Global)
    MS2000PatchBuilder::buildMusicalRandomPatch(apvts);
    check(getApvtsParam("osc1Wave") <= 6.0f && getApvtsParam("t2Osc1Wave") <= 6.0f,
          "buildMusicalRandomPatch skips AudioIn (7) on both Timbre 1 and Timbre 2");
    check(getApvtsParam("mixOsc1Level") >= 65.0f && getApvtsParam("t2MixOsc1Level") >= 65.0f,
          "buildMusicalRandomPatch ensures audible OSC1 level on both timbres");
    check(getApvtsParam("filterResonance") <= 38.0f && getApvtsParam("t2FilterResonance") <= 38.0f,
          "buildMusicalRandomPatch caps resonance at ReMS2000 sweet spot (<= 38)");
    check(getApvtsParam("ampLevel") >= 110.0f && getApvtsParam("t2AmpLevel") >= 110.0f,
          "buildMusicalRandomPatch keeps ampLevel in ReMS2000 range (110..127)");
    check(getApvtsParam("arpGate") >= 33.0f,
          "buildMusicalRandomPatch keeps arpGate in ReMS2000 audible range (33..127)");

    // 5. OSC1 AudioIn (Wave 7) external audio routing through Voice & VoiceManager
    {
        VoiceManager audioInVm;
        audioInVm.prepare(44100.0);
        VoiceParameters vp;
        vp.osc1Type = OSC1Type::AudioIn;
        vp.osc1Level = 1.0f;
        vp.osc2Level = 0.0f;
        vp.noiseLevel = 0.0f;
        vp.filterType = FilterType::LPF12;
        vp.filterCutoffNorm = 1.0f;
        vp.filterResonance = 0.0f;
        vp.ampLevel = 1.0f;
        vp.eg2Attack = 0.0f;
        vp.eg2Decay = 0.0f;
        vp.eg2Sustain = 1.0f;
        vp.eg2Release = 0.1f;
        audioInVm.applyBlockParams(vp);
        audioInVm.noteOn(60, 1.0f);

        float silentRms = 0.0f;
        float activeRms = 0.0f;
        for (int i = 0; i < 256; ++i)
        {
            float l = 0.0f, r = 0.0f;
            audioInVm.process(l, r, 0, 0.0f, 0.0f);
            silentRms += l * l;
        }
        for (int i = 0; i < 256; ++i)
        {
            float l = 0.0f, r = 0.0f;
            const float extSample = 0.5f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f);
            audioInVm.process(l, r, 0, 0.0f, extSample);
            activeRms += l * l;
        }
        check(silentRms < 1e-6f && activeRms > 0.01f,
              "OSC1 = AudioIn (Wave 7) routes external audio input through the voice VCF/VCA path");
    }

    // 6. ModSequencer 1-Shot mode (seqRunMode = 1) and sync resolution
    {
        ModSequencer seq;
        seq.prepare(44100.0);
        seq.setEnabled(true);
        seq.setTempoBPM(120.0f);
        seq.setSyncResolution(3);  // 1/16
        auto &trk = seq.getTrack(0);
        trk.destination = ModSeqDest::Cutoff;
        trk.mode = ModSeqMode::Forward;
        trk.length = 4;  // 4 steps: 0, 1, 2, 3
        trk.oneShot = true;
        trk.steps[0] = 0.1f;
        trk.steps[1] = 0.3f;
        trk.steps[2] = 0.6f;
        trk.steps[3] = 0.9f;
        seq.reset();

        // Advance well past 10 steps worth of samples at 120 BPM 1/16 (5512.5 samples per step)
        seq.advanceClock(60000);
        check(seq.getCurrentStepIndex(0) == 3 && std::abs(seq.getTrackOutput(0) - 0.9f) < 1e-4f,
              "ModSequencer oneShot mode stops and holds at lastStep instead of looping");
    }

    // 7. LCDMenuFormatter Timbre 2 normalization and expanded parameter pages
    {
        LCDScreenText t1Cutoff = LCDMenuFormatter::formatParameter("filterCutoff", 64.0f);
        LCDScreenText t2Cutoff = LCDMenuFormatter::formatParameter("t2FilterCutoff", 64.0f);
        check(t1Cutoff.line1 == t2Cutoff.line1 && t1Cutoff.line2 == t2Cutoff.line2,
              "LCDMenuFormatter normalizes Timbre 2 (t2*) parameter IDs identically to Timbre 1");
        LCDScreenText panCenter = LCDMenuFormatter::formatParameter("t2AmpPan", 0.0f);
        check(panCenter.line2.find("CNT") != std::string::npos,
              "LCDMenuFormatter displays CNT for centered ampPan (0.0f)");
    }

    // 8. ROM v1.07 Microtonal Scales (0x13914 / 0x13980: Pure Major, Pure Minor, Arabic, Slendro, Pelog)
    {
        bool a4Always440 = true;
        for (int key = 0; key < 12; ++key)
        {
            if (std::abs(FirmwareTables::getHardwareScaleDetuneSemitones(69, 1, key)) > 1e-6f
                || std::abs(FirmwareTables::getHardwareScaleDetuneSemitones(69, 2, key)) > 1e-6f)
            {
                a4Always440 = false;
            }
        }
        check(
            a4Always440,
            "ROM Pure Major (1) and Pure Minor (2) keep A4 (note 69) at 0 cents (440.00 Hz) across all 12 Scale Keys");

        const float cPureMaj = FirmwareTables::getHardwareScaleDetuneSemitones(60, 1, 0);
        const float ePureMaj = FirmwareTables::getHardwareScaleDetuneSemitones(64, 1, 0);
        const float gPureMaj = FirmwareTables::getHardwareScaleDetuneSemitones(67, 1, 0);
        check(std::abs((ePureMaj - cPureMaj) - (-0.14f)) < 1e-5f && std::abs((gPureMaj - cPureMaj) - 0.02f) < 1e-5f,
              "ROM Pure Major intervals relative to tonic are -14 cents (M3) and +2 cents (P5)");

        const float eArabic = FirmwareTables::getHardwareScaleDetuneSemitones(64, 3, 0);
        const float bArabic = FirmwareTables::getHardwareScaleDetuneSemitones(71, 3, 0);
        check(std::abs(eArabic - (-0.52f)) < 1e-5f && std::abs(bArabic - (-0.52f)) < 1e-5f,
              "ROM Arabic scale (3) detunes 3rd and 7th degrees by -52 cents (quarter-tone Rast)");
    }

    // 9. Hardware 254B SysEx seqRunMode (byte 52 bit 6) & DELAY_DEPTH preservation
    {
        MS2000HardwareProgram hw;
        setApvtsParam("seqRunMode", 1.0f);    // Loop (bit 6 = 1)
        setApvtsParam("t2SeqRunMode", 0.0f);  // 1Shot (bit 6 = 0)
        setApvtsParam("delayDepth", 73.0f);
        setApvtsParam("delayFeedback", 19.0f);
        hw.captureFromAPVTS(apvts, "SeqRunTest");
        check(MS2000HardwareProgram::extractBits(hw.timbre(0)[MS2000HardwareProgram::ti::SEQ_FLAGS], 6, 6) == 1
                  && MS2000HardwareProgram::extractBits(hw.timbre(1)[MS2000HardwareProgram::ti::SEQ_FLAGS], 6, 6) == 0,
              "MS2000HardwareProgram captures seqRunMode (bit 6 of SEQ_FLAGS) independently for Timbre 1 and Timbre 2");
        check(hw.get(MS2000HardwareProgram::DELAY_DEPTH) == 73,
              "MS2000HardwareProgram::captureFromAPVTS preserves delayDepth in byte 0x15 without overwrite by "
              "delayFeedback");
    }
}

static void testCalibrationAndPresetValidation()
{
    printf("\n[Test 28] 3-Tier DSP Calibration, Hardware Routing & Factory Preset Validation Suite...\n");
    auto &apvts = dummyFixture().apvts;
    using Hw = MS2000HardwareProgram;

    auto setParam = [&apvts](const char *id, float value)
    {
        if (auto *p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(value));
    };
    auto getRaw = [&apvts](const char *id) -> float
    {
        if (auto *v = apvts.getRawParameterValue(id))
            return v->load(std::memory_order_relaxed);
        return -9999.0f;
    };

    // =========================================================================
    // TIER 1: Parameter Contract & 254-Byte Hardware Mapping Integrity
    // =========================================================================
    {
        // 1.1 EQ Gain neutral (64 = 0 dB), +12 dB (76 -> 127), -12 dB (52 -> 1)
        Hw hwEq;
        hwEq.set(Hw::EQ_LO_GAIN, 64);
        hwEq.set(Hw::EQ_HI_GAIN, 64);
        hwEq.applyToAPVTS(apvts);
        check(std::abs(getRaw(ParamIDs::eqLowGain) - 64.0f) < 0.01f
                  && std::abs(getRaw(ParamIDs::eqHighGain) - 64.0f) < 0.01f,
              "Tier 1.1a: Hardware EQ byte 64 (0 dB) maps to engine neutral 64.0f (not 0.0f/-12 dB)");

        hwEq.set(Hw::EQ_LO_GAIN, 76);  // +12 dB
        hwEq.set(Hw::EQ_HI_GAIN, 52);  // -12 dB
        hwEq.applyToAPVTS(apvts);
        check(std::abs(getRaw(ParamIDs::eqLowGain) - 127.0f) < 0.5f
                  && std::abs(getRaw(ParamIDs::eqHighGain) - 1.0f) < 0.5f,
              "Tier 1.1b: Hardware EQ bytes 76 (+12 dB) and 52 (-12 dB) map to 127 and 1");

        Hw capturedEq;
        capturedEq.captureFromAPVTS(apvts, "EQRoundTrip");
        check(capturedEq.get(Hw::EQ_LO_GAIN) == 76 && capturedEq.get(Hw::EQ_HI_GAIN) == 52,
              "Tier 1.1c: EQ Gain round-trips symmetrically back to hardware bytes 76 and 52");

        // 1.2 Single / Vocoder polyphony vs Split / Layer polyphony
        MS2000PatchBuilder::buildInitPatch(apvts);
        setParam(ParamIDs::synthMode, 0.0f);     // MS2000 (4 hardware voices)
        setParam(ParamIDs::timbreMode, 0.0f);    // Single mode
        setParam(ParamIDs::timbreVoices, 0.0f);  // 1+3 setting in byte 0x10 (ignored in Single!)
        setParam(ParamIDs::voiceMode, 1.0f);     // Poly
        SynthEngine polyEngine(apvts);
        polyEngine.prepare(48000.0, 256);
        polyEngine.updateParametersFromAPVTS();
        polyEngine.noteOn(1, 60, 0.9f);
        polyEngine.noteOn(1, 64, 0.9f);
        polyEngine.noteOn(1, 67, 0.9f);
        polyEngine.noteOn(1, 72, 0.9f);
        juce::AudioBuffer<float> chordBuf(2, 256);
        chordBuf.clear();
        juce::MidiBuffer emptyMidi;
        polyEngine.processBlock(chordBuf, emptyMidi, nullptr);
        check(polyEngine.getSnapshot().activeVoiceCount == 4,
              "Tier 1.2: Single mode allocates all 4 hardware voices to Timbre 1 regardless of timbreVoices byte");

        // 1.3 Timbre Transpose, Tune, BendRange, VibratoInt, and AmpGate hardware mapping
        Hw hwPitch;
        hwPitch.timbre(0)[Hw::ti::TRANSPOSE] = 64 - 12;    // -12 st
        hwPitch.timbre(0)[Hw::ti::TUNE] = 64 + 25;         // +25 cents
        hwPitch.timbre(0)[Hw::ti::BEND_RANGE] = 64 + 7;    // +7 st
        hwPitch.timbre(0)[Hw::ti::VIBRATO] = 64 + 30;      // +30
        hwPitch.timbre(0)[Hw::ti::AMP_FLAGS] = (1u << 6);  // Amp Gate = Gate (bit 6 = 1)
        hwPitch.timbre(1)[Hw::ti::TRANSPOSE] = 64 + 12;    // +12 st on Timbre 2
        hwPitch.timbre(1)[Hw::ti::AMP_FLAGS] = 0;          // Amp Gate = EG2 on Timbre 2
        hwPitch.applyToAPVTS(apvts);

        check(std::abs(getRaw(ParamIDs::pitchTranspose) - (-12.0f)) < 0.01f
                  && std::abs(getRaw(ParamIDs::pitchTune) - 25.0f) < 0.01f
                  && std::abs(getRaw(ParamIDs::pitchBendRange) - 7.0f) < 0.01f
                  && std::abs(getRaw(ParamIDs::vibratoInt) - 30.0f) < 0.01f && getRaw(ParamIDs::ampGate) > 0.5f
                  && std::abs(getRaw(ParamIDs::t2PitchTranspose) - 12.0f) < 0.01f && getRaw(ParamIDs::t2AmpGate) < 0.5f,
              "Tier 1.3: Timbre 1/2 Transpose, Tune, BendRange, VibratoInt, and AmpGate map from 254B dump");

        // 1.4 ModFX bypass when Depth = 0 (no static comb filter)
        ModFX modFx;
        modFx.prepare(48000.0);
        modFx.setEnabled(true);
        modFx.setType(ModFXType::ChorusFlanger);
        modFx.setDepth(0.0f);
        float maxCombDiff = 0.0f;
        for (int i = 0; i < 256; ++i)
        {
            const float inSample = std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 48000.0f);
            float l = inSample, r = inSample;
            modFx.process(l, r);
            maxCombDiff = std::max(maxCombDiff, std::max(std::abs(l - inSample), std::abs(r - inSample)));
        }
        check(maxCombDiff < 1e-6f, "Tier 1.4: ModFX with depth=0 is bit-transparent (zero static comb filtering)");
    }

    // =========================================================================
    // TIER 2: Atomic Block-by-Block DSP Probes (Pitch, Gate, Voice Steal)
    // =========================================================================
    {
        // 2.1 Fundamental pitch probe with pitchTranspose (-12, 0, +12 semitones)
        auto measureFreqHz = [](float transposeSemitones) -> float
        {
            Voice v;
            v.prepare(48000.0);
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
            vp.pitchTranspose = transposeSemitones;
            v.applyBlockParams(vp);
            v.noteOn(69, 1.0f, false);  // A4

            // Skip transient (200 samples), then measure zero-crossings over 8000 samples
            float prev = 0.0f;
            for (int i = 0; i < 200; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                prev = l;
            }
            int crossings = 0;
            int firstCross = -1, lastCross = -1;
            for (int i = 0; i < 8000; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                if (prev <= 0.0f && l > 0.0f)
                {
                    if (firstCross < 0)
                        firstCross = i;
                    lastCross = i;
                    ++crossings;
                }
                prev = l;
            }
            if (crossings < 2 || lastCross <= firstCross)
                return 0.0f;
            const float avgPeriodSamples =
                static_cast<float>(lastCross - firstCross) / static_cast<float>(crossings - 1);
            return 48000.0f / avgPeriodSamples;
        };

        const float f0 = measureFreqHz(0.0f);
        const float fDown12 = measureFreqHz(-12.0f);
        const float fUp12 = measureFreqHz(12.0f);
        check(std::abs(f0 - 440.0f) < 1.5f && std::abs(fDown12 - 220.0f) < 1.0f && std::abs(fUp12 - 880.0f) < 3.0f,
              "Tier 2.1: Voice fundamental pitch tracks pitchTranspose accurately (220 Hz, 440 Hz, 880 Hz)");

        // 2.2 Amp Gate mode (AMP_FLAGS bit 6 = 1) sustains even when EG2 Sustain = 0
        {
            Voice v;
            v.prepare(48000.0);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Saw;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Decay = 0.0f;
            vp.eg2Sustain = 0.0f;  // Zero sustain in EG2!
            vp.eg2Release = 0.0f;
            vp.ampLevel = 1.0f;
            vp.ampGate = true;  // Organ Gate bypasses EG2
            v.applyBlockParams(vp);
            v.noteOn(60, 1.0f, false);

            float latePeak = 0.0f;
            for (int i = 0; i < 4000; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                if (i > 3000)
                    latePeak = std::max(latePeak, std::abs(l));
            }
            check(latePeak > 0.12f,
                  "Tier 2.2: Amp Gate mode (AMP_FLAGS bit 6 = 1) sustains organ patches when EG2 sustain is 0");
        }

        // 2.3 Voice Steal Continuity Probe (no hard reset discontinuity jump on active voice steal)
        {
            Voice v;
            v.prepare(48000.0);
            VoiceParameters vp;
            vp.osc1Type = OSC1Type::Sine;
            vp.osc1Level = 1.0f;
            vp.filterCutoffNorm = 1.0f;
            vp.eg2Attack = 0.0f;
            vp.eg2Decay = 0.5f;
            vp.eg2Sustain = 1.0f;
            vp.eg2Release = 0.5f;
            vp.ampLevel = 1.0f;
            v.applyBlockParams(vp);
            v.noteOn(48, 1.0f, false);  // C3 (~130.8 Hz)

            // Advance until sine wave is near peak amplitude (> 0.4)
            float prevL = 0.0f;
            for (int i = 0; i < 500; ++i)
            {
                float l = 0.0f, r = 0.0f;
                v.renderNextSample(l, r);
                prevL = l;
                if (i > 50 && std::abs(l) > 0.35f)
                    break;
            }
            // Retrigger active voice (steal)
            v.noteOn(60, 1.0f, false);
            float nextL = 0.0f, nextR = 0.0f;
            v.renderNextSample(nextL, nextR);
            const float stepJump = std::abs(nextL - prevL);
            check(stepJump < 0.15f,
                  "Tier 2.3: Active voice retrigger preserves oscillator/filter state without hard-zero click");
        }
    }

    // =========================================================================
    // TIER 3: Automated Factory Bank Preset Validation (128 Presets)
    // =========================================================================
    {
        MS2000FactoryBank factoryBank;
        check(factoryBank.getNumPresets() == 128, "Tier 3.1: Factory bank contains 128 MS2000 presets");

        // Check representative presets (0: TeraByte, 1: Standchen, 2: DwgsOrgan, 4: Stab Saw, 16: Clean Funk)
        const int probeIndices[] = {0, 1, 2, 4, 8, 16, 32, 64, 96, 127};
        bool allValid = true;
        for (int idx : probeIndices)
        {
            const auto &prog = factoryBank.getPresetProgramData(idx);
            prog.applyToAPVTS(apvts);

            SynthEngine eng(apvts);
            eng.prepare(44100.0, 256);
            eng.updateParametersFromAPVTS();
            eng.noteOn(1, 60, 0.9f);

            juce::AudioBuffer<float> buf(2, 256);
            juce::MidiBuffer midi;
            float maxAbs = 0.0f;
            bool hasNaN = false;

            for (int block = 0; block < 8; ++block)
            {
                buf.clear();
                eng.processBlock(buf, midi, nullptr);
                for (int ch = 0; ch < 2; ++ch)
                {
                    const float *ptr = buf.getReadPointer(ch);
                    for (int s = 0; s < 256; ++s)
                    {
                        if (!std::isfinite(ptr[s]))
                            hasNaN = true;
                        maxAbs = std::max(maxAbs, std::abs(ptr[s]));
                    }
                }
            }
            if (hasNaN || maxAbs > 4.0f)
                allValid = false;
        }
        check(allValid, "Tier 3.2: Representative factory presets render finite, bounded audio without NaN/runaway");
    }
}

/**
 * Horquilla de la suite. Cada caso vive en su propia función a propósito: cuando todos
 * compartían un único `runAllTests()`, el compilador reservaba de golpe los locales de
 * todos los casos (incluidos los dobles de host, con `SynthEngine` +
 * `MIDITelemetryManager` + `SysExManager` en la pila) y el binario moría con
 * `STATUS_STACK_OVERFLOW` en la pila por defecto de Windows. Con una función por caso la
 * pila de cada uno se libera al volver, así que no hace falta tocar el tamaño de pila del
 * ejecutable (ver DOCS/ABDSYNTHS_SYSEX_GUIDE.md §10).
 */
/** Los casos que viven en OTRO fichero, y por eso no son `static` como los de
    arriba. La paridad del ecualizador esta en
    `DSPCoreTests_EqualizerParity.cpp` porque lleva consigo una copia congelada
    de la implementacion anterior, y esa copia no tiene nada que ver con esta
    horquilla. */
void testEqualizerParity();
void testTier2FirmwareBlockCalibration();

static void testArpeggiatorSwing()
{
    printf("\n=== Arpeggiator Swing Tests ===\n");
    {
        constexpr double kSr = 48000.0;
        Arpeggiator arp;
        arp.prepare(kSr);
        arp.setEnabled(true);
        arp.setTempoBPM(120.0);
        arp.setSyncResolution(3);  // 1/16 = 0.125 s (6000 samples @ 48 kHz)
        arp.setGateTime(0.5f);
        arp.setSwing(0.0f);

        arp.noteOn(60, 0.8f);
        arp.noteOn(64, 0.8f);

        std::vector<ArpNoteEvent> events;
        std::vector<double> timesZero;
        for (int b = 0; b < 2000; ++b)
        {
            events.clear();
            if (arp.processStep(48, events))
            {
                for (const auto &ev : events)
                    if (ev.isNoteOn)
                        timesZero.push_back((b + 1) * 48.0 / kSr);
            }
        }
        check(timesZero.size() >= 12, "swing=0%: genera multiples pasos en 2 segundos");

        double avgIntervalZero = (timesZero.back() - timesZero.front()) / static_cast<double>(timesZero.size() - 1);

        arp.reset();
        arp.setSwing(50.0f);
        arp.noteOn(60, 0.8f);
        arp.noteOn(64, 0.8f);

        std::vector<double> timesPlus50;
        for (int b = 0; b < 2000; ++b)
        {
            events.clear();
            if (arp.processStep(48, events))
            {
                for (const auto &ev : events)
                    if (ev.isNoteOn)
                        timesPlus50.push_back((b + 1) * 48.0 / kSr);
            }
        }
        check(timesPlus50.size() >= 12, "swing=+50%: genera multiples pasos");

        std::vector<double> intervalsPlus50;
        for (size_t i = 1; i < timesPlus50.size(); ++i)
            intervalsPlus50.push_back(timesPlus50[i] - timesPlus50[i - 1]);

        double meanPlus50 = 0.0;
        for (double iv : intervalsPlus50)
            meanPlus50 += iv;
        meanPlus50 /= static_cast<double>(intervalsPlus50.size());

        double variance = 0.0;
        for (double iv : intervalsPlus50)
            variance += (iv - meanPlus50) * (iv - meanPlus50);
        variance /= static_cast<double>(intervalsPlus50.size());
        double stddev = std::sqrt(variance);

        check(std::abs(meanPlus50 - avgIntervalZero) < 0.005, "swing=+50%: promedio conservado (dentro de 5ms)");
        check(stddev > 0.020, "swing=+50%: variabilidad significativa entre pasos pares e impares");
    }

    {
        MS2000HardwareProgram prog;
        prog.set(MS2000HardwareProgram::ARP_SWING, 0x1C);
        check(prog.get(MS2000HardwareProgram::ARP_SWING) == 0x1C, "round-trip ARP_SWING +28 (0x1C) en complemento a 2");

        prog.set(MS2000HardwareProgram::ARP_SWING, 0xCE);
        check(prog.get(MS2000HardwareProgram::ARP_SWING) == 0xCE, "round-trip ARP_SWING -50 (0xCE) en complemento a 2");

        float engineValuePlus28 =
            MS2000HardwareProgram::toEngineValue(0x1C, MS2000HardwareProgram::Xform::Signed2sComplement);
        check(std::abs(engineValuePlus28 - 28.0f) < 0.001f, "toEngineValue(0x1C) = +28%");

        float engineValueMinus50 =
            MS2000HardwareProgram::toEngineValue(0xCE, MS2000HardwareProgram::Xform::Signed2sComplement);
        check(std::abs(engineValueMinus50 - (-50.0f)) < 0.001f, "toEngineValue(0xCE) = -50%");

        uint8_t byteFromPlus28 =
            MS2000HardwareProgram::fromEngineValue(28.0f, MS2000HardwareProgram::Xform::Signed2sComplement);
        check(byteFromPlus28 == 0x1C, "fromEngineValue(+28%) = 0x1C");

        uint8_t byteFromMinus50 =
            MS2000HardwareProgram::fromEngineValue(-50.0f, MS2000HardwareProgram::Xform::Signed2sComplement);
        check(byteFromMinus50 == 0xCE, "fromEngineValue(-50%) = 0xCE");
    }
}

static void runAllTests()
{
    testDspUtils();
    testEnvelopeCurves();
    testLfo();
    testEqualizerParity();
    testFilterResonanceComp();
    testVoxWaveOscillator();
    testMidiMapTelemetry();
    testLcdMenuFormatter();
    testSysExCodec();
    testFilterVocoderStability();
    testVoiceAllocation();
    testEmbeddedWebUiAssets();
    testHostModelAnnouncement();
    testBridgeActionsDispatch();
    testHardwareMidiBridge();
    testSoftwarePresetTransport();
    testHardwareProgramToEngine();
    testDumpRequests();
    testWriteAcknowledgement();
    testAllDataDumpRequest();
    testKorgChannelEndToEnd();
    testEngineExtension();
    testMicrokorgHardwareParity();
    testDspConversionsAndClamping();
    testFirmwareHardwareParity();
    testHardwareMidiSysEx41AndTimbreOps();
    testCalibrationAndPresetValidation();
    testTier2FirmwareBlockCalibration();
    testArpeggiatorSwing();

    printf("\n=== Test Summary ===\n");
    printf("  Passed: %d\n", testsPassed);
    printf("  Failed: %d\n", testsFailed);
    printf("===================\n");
}

}  // namespace Tests
}
int main()
{
    ABDMS2000::Tests::runAllTests();
    return ABDMS2000::Tests::testsFailed > 0 ? 1 : 0;
}
  // namespace ABDMS2000
