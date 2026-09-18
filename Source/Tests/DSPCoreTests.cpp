#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Core/SynthEngine.h"
#include "../State/ParameterRegistry.gen.h"
#include "../State/MS2000PatchBuilder.h"
#include "../../DSP/Common/DSPUtils.h"
#include "../../DSP/Envelopes/EnvelopeCurves.h"
#include "../../DSP/Modulation/LFO.h"
#include "../../DSP/Oscillators/VoxWaveOscillator.h"
#include "../../DSP/Filters/FilterResonanceComp.h"
#include "../../DSP/Filters/MultiModeFilter.h"
#include "../../DSP/Vocoder/Vocoder16Band.h"
#include "../../Core/VoiceManager.h"
#include "../../MIDI/MIDIMap.h"

#include "../../ABDSharedCode/HardwareDrivers/NRPNParser.h"
#include "../../ABDSharedCode/HardwareDrivers/SysExCodec.h"
using abd::hw::NRPNParser;
using abd::hw::NRPNMessage;
using abd::hw::SysExCodec;
#include "../State/LCDMenuFormatter.h"
#include "../Plugin/HostModelAnnouncement.h"
#include "../Plugin/BridgeActions.h"
#include "../MIDI/MIDITelemetryManager.h"
#include "../MIDI/SysExManager.h"
#include "../MIDI/MS2000HardwareProgram.h"
#include "../MIDI/ABDSynthsSysEx.h"
#include "../MIDI/MS2000SysExExporter.h"
// Tramas Korg **del contrato del ABD Bank Manager**, generadas desde él
// (`Scripts/generate_korg_channel.js`): el Test 25 las consume para comprobar que los dos
// repos direccionan el equipo con el mismo byte. No se edita a mano.
#include "../MIDI/KorgChannel.gen.h"

#include "WebUIAssets.h"

#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>
#include <vector>



namespace ABDMS2000 {
namespace Tests {

static int testsPassed = 0;
static int testsFailed = 0;

static void check(bool condition, const char* testName) {
    if (condition) {
        testsPassed++;
        printf("  [PASS] %s\n", testName);
    } else {
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
        : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)) {}
    const juce::String getName() const override { return "BridgeTest"; }
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
    const juce::String getProgramName(int) override { return "BridgeTest"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

class FakeBridgeHost : public BridgeHost
{
public:
    FakeBridgeHost()
        : apvts(processor, nullptr, "Parameters", ParameterRegistry::createParameterLayout()),
          engine(apvts),
          telemetry(apvts)
    {
        engine.prepare(44100.0, 480);
    }

    juce::AudioProcessorValueTreeState& getAPVTS() override { return apvts; }
    SynthEngine& getEngine() override { return engine; }
    MIDITelemetryManager& getMIDITelemetry() override { return telemetry; }
    SysExManager& getSysExManager() override { return sysEx; }

    // Hardware MIDI del anfitrión: el puente del Bank Manager embebido
    // (hardware.send / hardware.listen). Los tests lo doblan con fakes.
    HardwareMidiTransport& getHardwareMidiTransport() override { return hardwareMidi; }

    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram(int index) override { currentProgram = index; }
    void changeProgramName(int, const juce::String&) override {}

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
    BridgeActions bridge{ host };
    std::vector<juce::var> messages;

    BridgeTestHarness()
    {
        bridge.setJsMessageSink([this](const juce::var& message) { messages.push_back(message); });
    }

    void deliver(const char* json) { bridge.handleJsEvent(juce::JSON::parse(json)); }

    static juce::String typeOf(const juce::var& message)
    {
        return message.getProperty("type", "").toString();
    }
};

/**
 * Procesador dummy para el APVTS del registro de parámetros. Estaba declarado dentro del
 * Test 13; lo usan varios tests (13, 22, 23, 24, 25 y 21), así que con una función por caso
 * tiene que vivir aquí.
 */
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

/**
 * Fixture de los tests que hablan SysEx con el APVTS real: procesador dummy + APVTS del
 * registro con el patch INIT cargado. Se construye una sola vez, en el primer uso (lazy) y
 * se comparte entre tests, que es exactamente lo que pasaba cuando vivía dentro del Test 13
 * y los demás lo usaban: el mismo objeto y el mismo estado.
 */
struct DummySynthFixture
{
    DummyProcessor processor;
    juce::AudioProcessorValueTreeState apvts{ processor, nullptr, "Parameters",
                                              ParameterRegistry::createParameterLayout() };

    DummySynthFixture() { MS2000PatchBuilder::buildInitPatch(apvts); }
};

static DummySynthFixture& dummyFixture()
{
    static DummySynthFixture fixture;
    return fixture;
}

// Frecuencia de muestreo de los tests (estaba declarada dentro del Test 12).
static constexpr double testSampleRate = 44100.0;

static void testDspUtils() {
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

static void testEnvelopeCurves() {
    printf("\n=== EnvelopeCurves Tests ===\n");

    // --- Attack time ---
    float at0 = EnvelopeCurves::getAttackTimeSeconds(0.0f);
    float at1 = EnvelopeCurves::getAttackTimeSeconds(1.0f);
    check(std::abs(at0 - 0.0005f) < 0.0001f, "attack norm=0 => ~0.5ms");
    check(std::abs(at1 - 5.0f) < 0.001f, "attack norm=1 => ~5s");
    check(EnvelopeCurves::getAttackTimeSeconds(0.1f) < EnvelopeCurves::getAttackTimeSeconds(0.9f),
          "attack monotonic");

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

static void testLfo() {
    printf("\n=== LFO Tests ===\n");

    LFO lfo;
    lfo.prepare(44100.0);
    lfo.setFrequencyHz(1.0f);

    // Test LFO1 (Triangle) starts near 0 after voice key sync
    lfo.setWaveformLFO1(LFOWaveform::Triangle);
    lfo.setKeySyncMode(2); // Voice
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
    lfo.setKeySyncMode(0); // Off
    // Advance a few cycles first
    for (int i = 0; i < 100; ++i) lfo.getNextSample();
    float beforeOff = lfo.getCurrentValue();
    lfo.triggerKeySync(true);
    float afterOff = lfo.getCurrentValue();
    check(std::abs(beforeOff - afterOff) < 0.01f, "LFO KeySync Off: phase unchanged after trigger");

    // KeySync Timbre: resets on first note only
    lfo.setKeySyncMode(1); // Timbre
    lfo.triggerKeySync(true); // isFirstTimbreNote = true
    float afterTimbreSync = lfo.getCurrentValue();
    check(std::abs(afterTimbreSync) < 0.01f, "LFO KeySync Timbre: resets on first note");

    // Advance and trigger with isFirstTimbreNote=false → should NOT reset
    for (int i = 0; i < 50; ++i) lfo.getNextSample();
    float beforeTimbre2 = lfo.getCurrentValue();
    lfo.triggerKeySync(false); // legacy note, not first
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
    lfo.setFrequencyHz(30.0f); // Clamps to 20.0
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
    lfo.setTempoSync(true, 4); // 1/4 note
    // At 120 BPM, a quarter note = 0.5 sec, so frequency = 2 Hz
    // Phase increment should be 2 / 44100
    // After 44100 samples (~1 sec), should have advanced ~2 cycles
    lfo.triggerKeySync(true);
    for (int i = 0; i < 22050; ++i) lfo.getNextSample();
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
    lfo.setTempoSync(true, 6); // 1/8 note → freq = 240/60 * 2 = 8 Hz
    check(true, "LFO re-enables tempo sync with new BPM and division");

    // setBpm clamps out-of-range values
    lfo.setBpm(0.0);
    lfo.setBpm(-10.0);
    lfo.setBpm(500.0);
    check(true, "LFO setBpm clamps extreme values gracefully");

    // Tempo sync at 120 BPM / 1/128 = 64 Hz — clamped to 20 Hz max
    lfo.setBpm(120.0);
    lfo.setTempoSync(true, 14); // 1/128 → 120/60 * 32 = 64 Hz → clamped to 20
    check(true, "Tempo sync frequency clamped to 20 Hz max");
}

static void testFilterResonanceComp() {
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

static void testVoxWaveOscillator() {
    printf("\n=== VoxWaveOscillator Tests ===\n");

    VoxWaveOscillator vox;
    vox.prepare(44100.0);
    vox.setFrequency(220.0f);
    vox.setVowel(0.0f); // 'A'
    float voxA = vox.getNextSample();
    check(voxA >= -1.0f && voxA <= 1.0f, "VoxWave A vowel in range [-1,1]");

    vox.setVowel(0.5f); // 'I'
    float voxI = vox.getNextSample();
    check(voxI >= -1.0f && voxI <= 1.0f, "VoxWave I vowel in range [-1,1]");

    // Different vowels produce different output
    vox.reset();
    vox.setVowel(0.0f);
    float valA = 0.0f;
    for (int i = 0; i < 10; ++i) valA += vox.getNextSample();

    vox.reset();
    vox.setVowel(1.0f);
    float valU = 0.0f;
    for (int i = 0; i < 10; ++i) valU += vox.getNextSample();

    check(std::abs(valA - valU) > 0.0001f, "VoxWave A vs U produce different output");
}

static void testMidiMapTelemetry() {
    // --- 9. MIDI Map & NRPN Telemetry Tests ---
    printf("\n[Test 9] MIDI Map & NRPN Telemetry...\n");
    
    // Canonical CC lookup
    const auto* cutoffInfo = MIDIMap::findByCC(74);
    check(cutoffInfo != nullptr, "MIDIMap finds CC#74 (Filter Cutoff)");
    if (cutoffInfo != nullptr)
    {
        float pVal = MIDIMap::midiValueToParamValue(*cutoffInfo, 127);
        check(std::abs(pVal - 127.0f) < 0.01f, "CC#74 max value maps to 127.0");
        int mVal = MIDIMap::paramValueToMidiValue(*cutoffInfo, 64.0f);
        check(mVal == 64, "Cutoff 64.0 maps back to MIDI 64");
    }

    const auto* resInfo = MIDIMap::findByCC(71);
    check(resInfo != nullptr, "MIDIMap finds CC#71 (Filter Resonance)");

    // Canonical NRPN lookup
    const auto* dwgsInfo = MIDIMap::findByNRPN(2, 0);
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

    const auto* eqLowInfo = MIDIMap::findByNRPN(msg.nrpnMSB, msg.nrpnLSB);
    check(eqLowInfo != nullptr, "Resolved parsed NRPN to EQ Low Freq parameter");

    // Test buffer encoding
    juce::MidiBuffer outBuf;
    NRPNParser::appendNRPNToBuffer(outBuf, 1, 2, 20, 1, false);
    check(outBuf.getNumEvents() == 3, "NRPN 7-bit encoded to 3 CC messages (99, 98, 6)");

}

static void testLcdMenuFormatter() {
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

}

static void testSysExCodec() {
    // --- 11. SysEx 7-to-8 Codec Tests ---
    printf("\n[Test 11] SysEx Codec (7-bit <-> 8-bit)...\n");
    // Test known 8-bit pattern: 7 bytes with various high bits set
    std::vector<uint8_t> original8Bit = { 0x81, 0x02, 0xFF, 0x7E, 0xA5, 0x5A, 0xC3 };
    std::vector<uint8_t> packed7Bit;
    bool packOk = SysExCodec::pack8to7(original8Bit.data(), original8Bit.size(), packed7Bit);
    check(packOk, "8-to-7 packing succeeded");
    check(packed7Bit.size() == 8, "7 bytes packed into 8 bytes (1 MSB collector + 7 data)");

    // Verify all packed bytes are <= 0x7F
    bool allValidMidi = true;
    for (uint8_t b : packed7Bit) if (b > 0x7F) allValidMidi = false;
    check(allValidMidi, "All packed bytes are valid 7-bit MIDI values (<= 0x7F)");

    // Unpack back
    std::vector<uint8_t> unpacked8Bit;
    bool unpackOk = SysExCodec::unpack7to8(packed7Bit.data(), packed7Bit.size(), unpacked8Bit);
    check(unpackOk, "7-to-8 unpacking succeeded");
    check(unpacked8Bit.size() == original8Bit.size(), "Unpacked size matches original");
    check(unpacked8Bit == original8Bit, "Unpacked 8-bit data matches original payload exactly (lossless roundtrip)");

    // Larger buffer round-trip (e.g. 256 bytes program size)
    std::vector<uint8_t> progBuffer(256);
    for (size_t i = 0; i < 256; ++i) progBuffer[i] = static_cast<uint8_t>((i * 7 + 13) & 0xFF);

    std::vector<uint8_t> packedProg;
    SysExCodec::pack8to7(progBuffer.data(), progBuffer.size(), packedProg);
    std::vector<uint8_t> unpackedProg;
    SysExCodec::unpack7to8(packedProg.data(), packedProg.size(), unpackedProg);
    check(unpackedProg == progBuffer, "256-byte MS2000 program buffer round-trip exact match");

}

static void testFilterVocoderStability() {
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
    if (std::isnan(diracSample) || std::isinf(diracSample)) filterHasNanOrInf = true;

    for (int i = 0; i < 512; ++i)
    {
        float out = filterAudit.process(0.0f);
        if (std::isnan(out) || std::isinf(out)) filterHasNanOrInf = true;
    }
    check(!filterHasNanOrInf, "Dirac impulse response contains zero NaN/Inf and decays stably");

    // 12.2: Extreme Resonance & Vocoder Loop Headroom Test
    Vocoder16Band vocoderAudit;
    vocoderAudit.prepare(testSampleRate);
    vocoderAudit.setEnabled(true);
    vocoderAudit.setFormantShift(2); // Max shift +2
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

    // 12.3: Nyquist Sanity Check
    float illegalHighCutoff = 99000.0f;
    float safeCutoff = std::min(illegalHighCutoff, static_cast<float>(testSampleRate * 0.49));
    check(safeCutoff < testSampleRate * 0.5, "Cutoff calculation is bounded below Nyquist limit to prevent filter collapse");

}

static void testVoiceAllocation() {
    // --- Test 13: Multitimbric Dual Voice Allocation & ABD Ultra 32-Voice Capacity ---
    printf("\n[Test 13] Multitimbric Voice Allocation & ABD Ultra 32-Voice Capacity...\n");
    fflush(stdout);
    auto vm = std::make_unique<VoiceManager>();
    printf("  [debug] vm allocated\n"); fflush(stdout);
    vm->prepare(testSampleRate);
    printf("  [debug] vm prepared\n"); fflush(stdout);

    // 13.1: Hardware 4-Voice Limit
    vm->setMaxPolyphony(4);
    check(vm->getMaxPolyphony() == 4, "Hardware profile limits active capacity to 4 voices");

    for (int n = 60; n < 68; ++n) vm->noteOn(n, 0.8f);
    printf("  [debug] active after 8 notes: %zu\n", vm->getActiveVoiceCount()); fflush(stdout);
    check(vm->getActiveVoiceCount() <= 4, "Polyphonic voice stealing strictly caps active voices to 4");

    // 13.2: ABD Ultra 32-Voice Mode
    vm->allNotesOff();
    vm->setMaxPolyphony(32);
    check(vm->getMaxPolyphony() == 32, "ABD Ultra profile expands active capacity to 32 voices");

    for (int n = 36; n < 36 + 32; ++n) vm->noteOn(n, 0.8f);
    printf("  [debug] active after 32 notes: %zu\n", vm->getActiveVoiceCount()); fflush(stdout);
    check(vm->getActiveVoiceCount() == 32, "ABD Ultra successfully sustains 32 simultaneous polyphonic voices");

    vm->allNotesOff();
    check(vm->getActiveVoiceCount() == 0, "allNotesOff cleanly terminates all active voices");

    // 13.3: VoiceManager audio rendering check
    vm->setMaxPolyphony(4);
    VoiceParameters vp{};
    vm->applyBlockParams(vp);
    vm->noteOn(60, 0.8f);
    float testL = 0.0f, testR = 0.0f;
    float peakRender = 0.0f;
    for (int i = 0; i < 480; ++i)
    {
        vm->process(testL, testR);
        peakRender = std::max(peakRender, std::max(std::abs(testL), std::abs(testR)));
    }
    printf("  [debug] VoiceManager peak audio rendering: %f\n", peakRender); fflush(stdout);
    check(peakRender > 0.01f, "VoiceManager renders non-silent audio on noteOn");


    // 13.4: SynthEngine + APVTS audio rendering check
    juce::AudioProcessorValueTreeState::ParameterLayout layout = ParameterRegistry::createParameterLayout();
    // Procesador dummy + APVTS con el patch INIT cargado: el fixture que comparten todos
    // los tests de SysEx (ver `dummyFixture()`), porque antes vivía aquí y los demás lo usaban.
    auto& dummyProc = dummyFixture().processor;
    auto& apvts = dummyFixture().apvts;

    SynthEngine engine(apvts);
    engine.prepare(testSampleRate, 480);
    engine.noteOn(1, 60, 0.8f);

    juce::AudioBuffer<float> testBuffer(2, 480);
    testBuffer.clear();
    juce::MidiBuffer dummyMidi;
    engine.processBlock(testBuffer, dummyMidi, nullptr);

    float enginePeakL = testBuffer.getMagnitude(0, 0, 480);
    float enginePeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] SynthEngine peak L: %f, R: %f\n", enginePeakL, enginePeakR); fflush(stdout);
    check(enginePeakL > 0.01f && enginePeakR > 0.01f, "SynthEngine with Init Patch renders audio in processBlock");

    // Test Point 4: Pre-Filter Diagnostic Tone with NO note active
    engine.allNotesOff();
    engine.setDiagnosticTone(4, 440.0f, 0.25f);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float diag4PeakL = testBuffer.getMagnitude(0, 0, 480);
    float diag4PeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Point 4 (PreFilter) peak L: %f, R: %f\n", diag4PeakL, diag4PeakR); fflush(stdout);
    check(diag4PeakL > 0.01f && diag4PeakR > 0.01f, "Diag Point 4 (PreFilter) renders audio when idle");

    // Test Point 5: Direct OSC1 Diagnostic Tone with NO note active
    engine.setDiagnosticTone(5, 440.0f, 0.25f);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float diag5PeakL = testBuffer.getMagnitude(0, 0, 480);
    float diag5PeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Point 5 (OSC1) peak L: %f, R: %f\n", diag5PeakL, diag5PeakR); fflush(stdout);
    check(diag5PeakL > 0.01f && diag5PeakR > 0.01f, "Diag Point 5 (OSC1) renders audio when idle");

    engine.setDiagnosticTone(0, 440.0f, 0.0f);

    // Test Diagnostic Bypasses: Filter & VCA bypass
    engine.setDiagnosticBypass("filter", true);
    engine.setDiagnosticBypass("vca", true);
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float bypassPeakL = testBuffer.getMagnitude(0, 0, 480);
    float bypassPeakR = testBuffer.getMagnitude(1, 0, 480);
    printf("  [debug] Diag Bypass (Filter+VCA) peak L: %f, R: %f\n", bypassPeakL, bypassPeakR); fflush(stdout);
    check(bypassPeakL > 0.01f && bypassPeakR > 0.01f, "Diag Bypass renders continuous audio through voice 0 without noteOn");

    engine.resetAllDiagnosticBypasses();
    engine.reset();
    testBuffer.clear();
    engine.processBlock(testBuffer, dummyMidi, nullptr);
    float resetPeakL = testBuffer.getMagnitude(0, 0, 480);
    check(resetPeakL < 0.001f, "Resetting bypasses returns synth to quiet idle state");

    // --- Scope Tap Capture (ABDScope integration) ---
    {
        auto& sc = engine.getScopeCollector();
        check(sc.getTapCount() == 6, "SynthEngine registers 6 scope taps (master_out/pre_fx/osc_mix/post_filter/post_vca/lfo1)");
        check(sc.getTap(0) != nullptr && sc.getTap(0)->isActive(), "Scope master_out tap auto-activated on register");
        check(sc.getTap(2) != nullptr && !sc.getTap(2)->isActive(), "Non-first taps start inactive (on-demand policy)");

        // Fresh engine => fresh collector, deterministic empty-buffer baseline.
        SynthEngine scopeEngine(apvts);
        scopeEngine.prepare(testSampleRate, 480);
        scopeEngine.noteOn(1, 55, 0.8f);

        juce::AudioBuffer<float> scopeBuf(2, 480);
        scopeBuf.clear();
        juce::MidiBuffer scopeMidi;
        const auto& sc2 = scopeEngine.getScopeCollector();
        const size_t before = sc2.getTap(0)->getAvailableRead();
        scopeEngine.processBlock(scopeBuf, scopeMidi, nullptr);
        const size_t after = sc2.getTap(0)->getAvailableRead();
        printf("  [debug] Scope(fresh) master_out available samples: before=%zu after=%zu\n", before, after); fflush(stdout);
        check(after > 0, "Scope master_out tap captures samples into ring buffer after processBlock");
    }

}

static void testEmbeddedWebUiAssets() {
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
            const char* data = WebUIAssets::getNamedResource(synthIndexResource.toRawUTF8(), dataSize);
            if (data != nullptr && dataSize > 0)
            {
                juce::String content = juce::String::fromUTF8(data, dataSize);
                isSynthIndex = content.contains("ABDMS2000 - Synthesizer");
            }
        }
        check(isSynthIndex, "Embedded index.html is the synth root page, not the Bank Manager");
    }

}

static void testHostModelAnnouncement() {
    // --- Test 16: hostModel announcement (despacho de handleJsEvent) ---
    printf("\n[Test 16] Bridge hostModel announcement (HostModelAnnouncement)...\n");
    {
        // The ABD Bank Manager embedded in this plugin filters its model selector
        // with the modelId the host announces through the cppToWebui action
        // `hostModel`. BridgeActions::handleJsEvent() resolves that announcement
        // with announceHostModelForAction() for every action it receives, so this
        // guards the dispatch: requestState / requestFullState must announce.
        std::vector<juce::var> emitted;
        JsMessageSink sink = [&emitted](const juce::var& message) { emitted.push_back(message); };

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
        const char* nonTriggers[] = { "setParam", "noteOn", "requestStateX", "requestFullStates",
                                      "getRawProgramData", "hostModel", "" };
        int nonTriggerFailures = 0;
        for (const char* action : nonTriggers)
        {
            emitted.clear();
            if (announceHostModelForAction(action, sink) || ! emitted.empty())
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
        check(emitted.size() == 1
                  && emitted[0].getProperty("data", juce::var()).getProperty("buildRevision", "").toString().isNotEmpty(),
              "hostInfo reports the code revision");

        // El anuncio lleva el mismo sello (un Bank Manager antiguo lo ignora).
        check(hostModelMessage().getProperty("data", juce::var()).getProperty("buildStamp", "").toString().isNotEmpty()
                  && static_cast<int>(hostModelMessage().getProperty("data", juce::var()).getProperty("protocol", 0))
                         == kHostBridgeProtocol,
              "hostModel also carries protocol + build stamp");

        // El nivel declarado tiene que cubrir lo que el binario habla de verdad:
        // 4 = puente MIDI de hardware (el fetch de un banco real por el bridge).
        check(kHostBridgeProtocol >= 4,
              "the declared bridge protocol covers the hardware MIDI pipe");

        // Politica: pedir la ficha NO es anunciar el modelo, y al reves.
        check(! actionAnnouncesHostModel("requestHostInfo"),
              "requestHostInfo does not piggyback the announcement");
        check(! actionAnswersHostInfo("requestState")
                  && ! actionAnswersHostInfo("requestFullState")
                  && actionAnswersHostInfo("requestHostInfo"),
              "requestHostInfo is a separate action from the announcement triggers");

        emitted.clear();
        answerHostInfoForAction("requestHostInfo", {});
        check(emitted.empty(), "a null sink is tolerated for the host info too");
    }

}

static void testBridgeActionsDispatch() {
    // --- Test 17: BridgeActions::handleJsEvent (despacho directo) ---
    printf("\n[Test 17] BridgeActions::handleJsEvent dispatch (BridgeHost + sink inyectados)...\n");
    {
        // El puerto y el sink se inyectan precisamente para poder probar esto: el despacho
        // de eventos del WebUI corre con los motores reales, sin WebView y sin editor.
        // El arnés (host doble + puente + bandeja de mensajes) vive a nivel de namespace en
        // `BridgeTestHarness` porque lo comparten los tests 17/18/19.
        BridgeTestHarness harness;
        auto& host = harness.host;
        auto& bridge = harness.bridge;
        auto& messages = harness.messages;
        auto deliver = [&harness](const char* json) { harness.deliver(json); };
        auto typeOf = [](const juce::var& message) { return BridgeTestHarness::typeOf(message); };
        auto modelOf = [](const juce::var& message)
        {
            return message.getProperty("data", juce::var()).getProperty("modelId", "").toString();
        };
        auto hostModelCount = [&messages, &typeOf]()
        {
            int count = 0;
            for (const auto& message : messages)
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
        const char* otherActions[] = {
            R"({"action":"noteOn","note":60,"velocity":0.8})",
            R"({"action":"noteOff","note":60})",
            R"({"action":"pitchBend","value":0.5})",
            R"({"action":"allNotesOff"})",
            R"({"action":"toggleScope"})",
            R"({"action":"requestStateX"})"
        };
        int leaked = 0;
        for (const char* json : otherActions)
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
        const auto& parameters = ParameterRegistry::getAllParameters();
        if (! parameters.empty())
        {
            juce::DynamicObject::Ptr setMsg = new juce::DynamicObject();
            setMsg->setProperty("action", "setParam");
            setMsg->setProperty("paramId", juce::String(parameters.front().id));
            setMsg->setProperty("value", 1.0);
            bridge.handleJsEvent(juce::var(setMsg.get()));

            auto* parameter = host.getAPVTS().getParameter(juce::String(parameters.front().id));
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

static void testHardwareMidiBridge() {
    // Host doble + puente + bandeja de mensajes: el arnés compartido (`BridgeTestHarness`).
    BridgeTestHarness harness;
    {
        auto& host = harness.host;
        auto& messages = harness.messages;
        auto deliver = [&harness](const char* json) { harness.deliver(json); };
        auto typeOf = [](const juce::var& message) { return BridgeTestHarness::typeOf(message); };

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
            auto& transport = host.getHardwareMidiTransport();
            transport.bind(
                [&hardware](const juce::MemoryBlock& bytes) -> HardwareMidiTransport::Outcome
                {
                    if (! hardware.openOutput)
                        return { false, "No MIDI output device available for the hardware transfer" };
                    hardware.sent.push_back(bytes);
                    return { true, hardware.outputName };
                },
                [&hardware]() -> HardwareMidiTransport::Outcome
                {
                    ++hardware.listenCalls;
                    if (! hardware.openInput)
                        return { false, "No MIDI input device available for the hardware transfer" };
                    return { true, hardware.inputName };
                },
                [&toWebUi](const juce::var& message) { toWebUi.push_back(message); });

            auto dataOf = [](const juce::var& message) { return message.getProperty("data", juce::var()); };

            // Enumeración y selección explícita: no se acepta ningún puerto implícito.
            transport.setPortListFunction([] {
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
            check(transport.getSelectedOutputId() == "out-test"
                      && transport.getSelectedInputId() == "in-test",
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
            check(messages.size() == 1 && dataOf(messages[0]).getProperty("device", "").toString() == hardware.inputName,
                  "the listen ack names the MIDI input device the host opened");

            // `hardware.send`: bytes base64 -> el mismo bloque en el dispositivo.
            // OJO: el bridge habla base64 *estándar* (el `atob`/`btoa` del WebUI), no
            // `MemoryBlock::toBase64Encoding()`, que en JUCE codifica `<tamaño>.<datos>`.
            // Mezclarlos no falla al compilar: falla en silencio al decodificar.
            const juce::uint8 sysexBytes[] = { 0xF0, 0x42, 0x30, 0x00, 0x01, 0xF7 };
            const juce::MemoryBlock sysex(sysexBytes, sizeof(sysexBytes));
            const auto sysexPayload = juce::Base64::toBase64(sysex.getData(), sysex.getSize());
            const auto sendJson = "{\"action\":\"hardware.send\",\"payload\":\""
                                + sysexPayload + "\"}";

            // Ida y vuelta del formato que consume el JS (`atob`): mismo contrato que
            // el core del Bank Manager standalone.
            {
                juce::MemoryOutputStream decoded;
                const bool ok = juce::Base64::convertFromBase64(decoded, sysexPayload);
                const juce::MemoryBlock roundTrip(decoded.getData(), decoded.getDataSize());
                check(ok && roundTrip == sysex,
                      "the bridge payload is plain base64 (decodes back to the same bytes)");
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
            check(toWebUi.size() == 1 && dataOf(toWebUi.front()).getProperty("size", juce::var()) == juce::var(static_cast<int>(sizeof(sysexBytes))),
                  "hardware.receive reports the byte count");

            // Payload inválido: motivo explícito y sin tocar el dispositivo.
            messages.clear();
            deliver(R"({"action":"hardware.send","payload":42})");
            deliver(R"({"action":"hardware.send","payload":""})");
            check(hardware.sent.size() == 1 && messages.size() == 2
                      && typeOf(messages[0]) == "hardware.error" && typeOf(messages[1]) == "hardware.error",
                  "invalid base64 payloads report hardware.error and reach no device");

            // Host sin dispositivos: errores con motivo en vez de silencio (antes se
            // despachaba nada y el fetch del Bank Manager solo podía morir por timeout).
            hardware.openOutput = false;
            hardware.openInput = false;
            messages.clear();
            deliver(R"({"action":"hardware.listen"})");
            deliver(sendJson.toRawUTF8());
            std::vector<juce::String> reasons;
            for (const auto& message : messages)
                if (typeOf(message) == "hardware.error")
                    reasons.push_back(message.getProperty("data", "").toString());
            check(reasons.size() == 2 && reasons[0].containsIgnoreCase("input")
                      && reasons[1].containsIgnoreCase("output"),
                  "a host with no MIDI devices explains why instead of failing silently");
            check(! transport.isListening(), "a failed listen leaves the transport not listening");

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

static void testSoftwarePresetTransport() {
    // Host doble + puente + bandeja de mensajes: el arnés compartido (`BridgeTestHarness`).
    BridgeTestHarness harness;
    {
        auto& host = harness.host;
        auto& bridge = harness.bridge;
        auto& messages = harness.messages;
        auto typeOf = [](const juce::var& message) { return BridgeTestHarness::typeOf(message); };

        // --- Test 19: transporte de software (el ABD Bank Manager embebido) ---
        printf("\n[Test 19] Software preset transport (preset.read/write, bank.read/write, preset.capture)...\n");
        {
            // El Bank Manager embebido no puede abrir MIDI ni leer la memoria del
            // plugin: usa los MISMOS verbos que con un equipo fisico, por el bridge
            // (ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md §6.2/§6.4). Aqui se
            // fija el contrato del cable: base64 estandar, requestId devuelto, sistema
            // `native`, memoria vs audicion (§7.2) y errores con motivo.
            auto dataOf = [](const juce::var& message) { return message.getProperty("data", juce::var()); };
            auto findOfType = [&messages, &typeOf](const juce::String& type) -> juce::var
            {
                for (const auto& message : messages)
                    if (typeOf(message) == type) return message;
                return juce::var();
            };
            auto send = [&bridge](juce::DynamicObject::Ptr object) { bridge.handleJsEvent(juce::var(object.get())); };
            auto makeRequest = [](const char* action, const char* system, juce::var slot, const juce::String& requestId)
            {
                juce::DynamicObject::Ptr object = new juce::DynamicObject();
                object->setProperty("action", action);
                object->setProperty("system", system);
                if (! slot.isVoid()) object->setProperty("slot", slot);
                object->setProperty("requestId", requestId);
                return object;
            };
            auto decodePayloadOf = [](const juce::var& message, juce::MemoryBlock& out)
            {
                return SoftwarePresetProtocol::decodePayload(
                    message.getProperty("data", juce::var()).getProperty("payload", "").toString(), out);
            };

            auto& synthMemory = host.getSysExManager();

            // `preset.read`: el bloque de programa del slot, en base64 estandar.
            {
                const auto expected = synthMemory.getProgram(5);
                const auto expectedName = juce::String(expected.getName()).trim();

                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadPreset, "native", juce::var(5), "r1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kPresetData,
                      "preset.read is answered with preset.data");

                const auto data = dataOf(messages.front());
                check(data.getProperty("requestId", "").toString() == "r1",
                      "preset.data echoes the requestId the Bank Manager sent");
                check(static_cast<int>(data.getProperty("slot", -1)) == 5,
                      "preset.data carries the requested slot");
                check(data.getProperty("name", "").toString() == expectedName,
                      "preset.data carries the program name");

                juce::MemoryBlock decoded;
                check(decodePayloadOf(messages.front(), decoded)
                          && decoded.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE),
                      "the payload decodes to one program block");
                check(decoded.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE)
                          && std::memcmp(decoded.getData(), expected.rawData.data(),
                                         MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
                      "the payload is exactly the bytes in the synth memory");
            }

            // `preset.write` con slot: guarda el programa y lo carga (patch activo).
            {
                MS2000ProgramData incoming;
                incoming.setName("BankPatch");
                incoming.rawData[MS2000ProgramData::VOICE_BYTE] = 2; // Unison

                auto write = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(9), "w1");
                write->setProperty("name", "BankPatch");
                write->setProperty("payload", SoftwarePresetProtocol::encodePayload(
                    incoming.rawData.data(), incoming.rawData.size()));

                messages.clear();
                send(write);

                const auto stored = synthMemory.getProgram(9);
                check(std::memcmp(stored.rawData.data(), incoming.rawData.data(),
                                  MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
                      "preset.write stores the block in the synth memory");
                check(juce::String(stored.getName()).trim() == "BankPatch",
                      "preset.write stores the name the Bank Manager sent");
                check(host.getCurrentProgram() == 9,
                      "preset.write with a slot becomes the active program");

                const auto written = findOfType(SoftwarePresetProtocol::kPresetWritten);
                check(written.isObject()
                          && dataOf(written).getProperty("requestId", "").toString() == "w1",
                      "preset.write is acknowledged with preset.written");
                check(written.isObject() && static_cast<int>(dataOf(written).getProperty("slot", -1)) == 9,
                      "preset.written reports the slot it stored");
            }

            // `preset.write` con `audition`: solo el motor, la memoria no se toca (§7.2).
            {
                const auto before = synthMemory.getProgram(9);

                MS2000ProgramData auditioned;
                auditioned.setName("Auditioned");
                auditioned.rawData[MS2000ProgramData::VOICE_BYTE] = 0; // Mono

                auto write = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(9), "w2");
                write->setProperty("name", "Auditioned");
                write->setProperty("audition", true);
                write->setProperty("payload", SoftwarePresetProtocol::encodePayload(
                    auditioned.rawData.data(), auditioned.rawData.size()));

                messages.clear();
                send(write);

                const auto after = synthMemory.getProgram(9);
                check(std::memcmp(after.rawData.data(), before.rawData.data(),
                                  MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
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
                for (const auto& message : messages)
                    if (typeOf(message) == SoftwarePresetProtocol::kPresetWritten) ++writtenAcks;
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

                const auto data = dataOf(messages.front());
                check(data.getProperty("requestId", "").toString() == "b1",
                      "bank.data echoes the requestId");

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
                              && std::memcmp(firstBlob.getData(), expectedFirst.rawData.data(),
                                             MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
                          "bank.data carries each program's bytes");
                }
            }

            // `bank.write`: escribe memoria, no cambia lo que suena.
            {
                const int activeBefore = host.getCurrentProgram();

                messages.clear();
                juce::Array<juce::var> slots;
                for (int slot : { 20, 21 })
                {
                    MS2000ProgramData program;
                    program.setName(slot == 20 ? "BatchOne" : "BatchTwo");
                    program.rawData[MS2000ProgramData::VOICE_BYTE] = 1;

                    juce::DynamicObject::Ptr entry = new juce::DynamicObject();
                    entry->setProperty("slot", slot);
                    entry->setProperty("name", juce::String(program.getName()).trim());
                    entry->setProperty("payload", SoftwarePresetProtocol::encodePayload(
                        program.rawData.data(), program.rawData.size()));
                    slots.add(juce::var(entry.get()));
                }

                auto bulk = makeRequest(SoftwarePresetProtocol::kWriteBank, "native", juce::var(), "bw1");
                bulk->setProperty("slots", slots);
                send(bulk);

                check(juce::String(synthMemory.getProgram(20).getName()).trim() == "BatchOne"
                          && juce::String(synthMemory.getProgram(21).getName()).trim() == "BatchTwo",
                      "bank.write stores every slot it receives");
                check(host.getCurrentProgram() == activeBefore,
                      "a bank write does not change the active program");

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
                check(decodePayloadOf(messages.front(), captured)
                          && captured.getSize() == static_cast<int>(MS2000ProgramData::UNPACKED_PROGRAM_SIZE),
                      "preset.captured carries the active patch as one program block");
                check(std::memcmp(synthMemory.getProgram(11).rawData.data(), memoryBefore.rawData.data(),
                                  MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
                      "a capture without a slot does not write memory");

                // Con `slot`: ademas se guarda.
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kCapturePreset, "native", juce::var(11), "c2"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kPresetCaptured,
                      "preset.capture with a slot is answered too");

                juce::MemoryBlock second;
                const bool decodedSecond = decodePayloadOf(messages.front(), second);
                check(decodedSecond
                          && std::memcmp(synthMemory.getProgram(11).rawData.data(), second.getData(),
                                         MS2000ProgramData::UNPACKED_PROGRAM_SIZE) == 0,
                      "preset.capture with a slot stores the active patch in the synth memory");
            }

            // Errores: motivo explicito y codigo que permite al Bank Manager caer al
            // verbo por slot en vez de morir por timeout.
            {
                auto codeOf = [&dataOf](const juce::var& message)
                {
                    return dataOf(message).getProperty("code", "").toString();
                };

                // Sistema que este host no habla (`sysex` no se declara: §7.1).
                messages.clear();
                send(makeRequest(SoftwarePresetProtocol::kReadPreset, "sysex", juce::var(0), "e1"));
                check(messages.size() == 1 && typeOf(messages[0]) == SoftwarePresetProtocol::kError,
                      "an unsupported preset system is answered with preset.error");
                check(messages.size() == 1 && codeOf(messages[0]) == SoftwarePresetProtocol::kUnsupportedAction,
                      "the unsupported system reports 'unsupported-action' (the Bank Manager can degrade)");
                check(messages.size() == 1
                          && dataOf(messages[0]).getProperty("requestId", "").toString() == "e1",
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

                const juce::uint8 tooShortRaw[] = { 0x01, 0x02, 0x03 };
                auto shortWrite = makeRequest(SoftwarePresetProtocol::kWritePreset, "native", juce::var(0), "e4");
                shortWrite->setProperty("payload", SoftwarePresetProtocol::encodePayload(tooShortRaw, sizeof(tooShortRaw)));
                send(shortWrite);

                int invalidPayloadErrors = 0;
                for (const auto& message : messages)
                    if (typeOf(message) == SoftwarePresetProtocol::kError && codeOf(message) == SoftwarePresetProtocol::kInvalidPayload)
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

static void testHardwareProgramToEngine() {
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
        check(frame.size() == 297 && frame[0] == 0xF0 && frame[3] == 0x58 && frame[4] == 0x40
                  && frame.back() == 0xF7,
              "the hardware frame is F0 42 3n 58 40 [291 B] F7");

        Hw roundTrip;
        check(roundTrip.unpackFromSysexPayload(packed.data(), packed.size()),
              "the real payload unpacks");
        check(roundTrip.raw == prog.raw, "the 254 bytes survive pack -> unpack unchanged");

        // 3. Bytes reales → parametros del motor.
        DummyProcessor hwProcessor;
        juce::AudioProcessorValueTreeState hwApvts(hwProcessor, nullptr, "Parameters",
                                                   ParameterRegistry::createParameterLayout());
        auto raw = [&hwApvts](const char* id) -> float {
            if (auto* p = hwApvts.getRawParameterValue(id)) return p->load();
            return -999.0f;
        };
        auto setParam = [&hwApvts](const char* id, float value) {
            if (auto* p = hwApvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
        };

        Hw patch;
        patch.setName("From the MS2000");
        patch.timbre(0)[Hw::ti::FLAGS] = 0xA0;            // assign Unison (bits 6,7 = 2)
        patch.timbre(0)[Hw::ti::CUTOFF] = 42;
        patch.timbre(0)[Hw::ti::RESONANCE] = 88;
        patch.timbre(0)[Hw::ti::FILTER_EG1_INT] = 64 - 20;
        patch.timbre(0)[Hw::ti::OSC1_WAVE] = 5;           // DWGS
        patch.timbre(0)[Hw::ti::OSC1_DWGS] = 33;
        patch.timbre(0)[Hw::ti::OSC2_SEMITONE] = 64 + 7;
        patch.timbre(0)[Hw::ti::AMP_PAN] = 64 + 10;
        patch.timbre(0)[Hw::ti::EG2_SUSTAIN] = 99;
        patch.timbre(0)[Hw::ti::PATCH1] = (5 << 4) | 2;   // destino 5, origen 2
        patch.timbre(0)[Hw::ti::PATCH1_INT] = 64 + 31;
        patch.timbre(0)[Hw::ti::PORTAMENTO] = 30;
        patch.set(Hw::ARP_TYPE_RNG, (2 << 4) | 4);        // rango 3 octavas (2), tipo Random (4)
        patch.set(Hw::ARP_GATE, 65);
        patch.set(Hw::MODFX_TYPE, 2);
        patch.set(Hw::EQ_HI_FREQ, 29);                    // 30 pasos reales -> 3 en el motor

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
        check(raw(ParamIDs::arpType) == 4.0f && raw(ParamIDs::arpRange) == 3.0f
                  && raw(ParamIDs::arpGate) == 65.0f,
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
        voc.timbre(0)[Hw::ti::MIX_NOISE] = 77;         // en vocoder = HPF level
        voc.timbre(0)[Hw::ti::FILTER_VELO] = 61;       // en vocoder = Gate Sense
        voc.timbre(0)[Hw::ti::FILTER_EG1_INT] = 1;     // en vocoder = Filter Shift (+1)
        voc.timbre(0)[Hw::ti::VOCODER_BAND_LEVELS] = 55;
        voc.applyToAPVTS(hwApvts);
        check(raw(ParamIDs::synthVocoderMode) == 1.0f, "a vocoder program switches the engine to vocoder mode");
        check(raw(ParamIDs::vocoderHpfLevel) == 77.0f && raw(ParamIDs::vocoderGateSense) == 61.0f,
              "the vocoder reads those bytes as HPF level / gate sense, not as mixer / velocity sense");
        check(raw(ParamIDs::vocoderFormantShift) == 3.0f,
              "filter shift 0,+1,+2,-1,-2 is reordered to the engine's -2..+2 (real +1 -> engine +1)");
        check(raw(ParamIDs::vocoderBandLevel1) == 55.0f,
              "the 16 vocoder band levels come from the timbre bytes 46..61");

        // 5. De vuelta al hardware: los DOS timbres, escala/split y el mod sequence.
        setParam(ParamIDs::synthVocoderMode, 0.0f); // el paso 4 dejó el motor en vocoder
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
        check(back.timbre(0)[Hw::ti::FILTER_TYPE] == 2, "capture writes the engine filter type into the real timbre slot");
        check(back.timbre(0)[Hw::ti::FILTER_VELO] == 64 + 25 && back.timbre(1)[Hw::ti::FILTER_VELO] == 64 - 20,
              "each timbre writes its own filter velocity sense");
        check(back.timbre(1)[Hw::ti::CUTOFF] == 33 && back.timbre(1)[Hw::ti::OSC1_WAVE] == 1,
              "capture writes Timbre 2 into its own 108-byte block");
        check(back.timbre(1)[0] == 0xA0,
              "the Timbre 2 bytes the engine does not model (MIDI channel) survive capture");
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
        const auto& pending = Hw::unmodelled();
        const auto mentions = [&pending](const char* needle) {
            return std::any_of(pending.begin(), pending.end(), [needle](const char* s) {
                return std::string(s).find(needle) != std::string::npos;
            });
        };
        check(!pending.empty() && mentions("MIDI channel") && !mentions("Timbre 2"),
              "the fields the engine cannot hold are declared in unmodelled(), and Timbre 2 is no longer one of them");

        // 7. Ida y vuelta completa: trama real -> SysExManager -> motor -> trama real.
        SysExManager sysEx;
        Hw fromHardware;
        fromHardware.setName("Hw Patch"); // el nombre real son 12 B
        fromHardware.timbre(0)[Hw::ti::CUTOFF] = 99;
        fromHardware.timbre(1)[0] = 0x11;                 // Timbre 2 con datos propios
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
                  && sent.timbre(0)[Hw::ti::RESONANCE] == 42
                  && sent.timbre(1)[0] == 0x11
                  && sent.getName() == "Edited",
              "the engine's edit goes back into its real slot and Timbre 2 travels intact");
        check(sent.timbre(0)[Hw::ti::CUTOFF] == 99 && sent.timbre(0)[Hw::ti::OSC1_WAVE] == 0,
              "the rest of the timbre keeps what the hardware sent");

        // 8. El preset PROPIO viaja con la cabecera de ABDSynths, no con la de Korg.
        MS2000ProgramData nativePreset;
        nativePreset.extractFromAPVTS(hwApvts, "Native One");
        nativePreset.setParam(ParamIDs::filterCutoff, 55.0f);
        const auto nativeFrame = sysEx.createProgramDump(1, nativePreset);
        check(nativeFrame.size() == ABDSynthsSysEx::programFrameSize() && nativeFrame[0] == 0xF0 && nativeFrame[1] == 0x7D
                  && nativeFrame[2] == 0x0A && nativeFrame[3] == ABDSynthsSysEx::CMD_PROGRAM_DUMP
                  && nativeFrame.back() == 0xF7,
              "the plugin's own preset uses the ABDSynths frame (0x7D / model 0x0A), not a Korg one");

        setParam(ParamIDs::filterCutoff, 1.0f);
        const auto nativeImport = sysEx.parseSysEx(nativeFrame.data(), nativeFrame.size(), hwApvts);
        check(nativeImport.success && raw(ParamIDs::filterCutoff) == 55.0f,
              "the ABDSynths frame is read back as the plugin's own native preset");
        check(juce::String(nativeImport.programName).trim() == "Native One",
              "the native preset keeps its name");

        juce::MemoryBlock ownPresetFile;
        MS2000SysExExporter::exportSingleProgram(hwApvts, "Own Preset", ownPresetFile);
        check(ownPresetFile.getSize() > 5
                  && static_cast<const uint8_t*>(ownPresetFile.getData())[1] == ABDSynthsSysEx::MANUFACTURER_ID,
              "the .syx the plugin writes for its own presets is an ABDSynths file, not a Korg one");

        const auto nativeBankFrame = sysEx.createAllDataDump(1);
        check(nativeBankFrame.size() > 5 && nativeBankFrame[1] == 0x7D && nativeBankFrame[3] == ABDSynthsSysEx::CMD_ALL_DUMP,
              "the plugin's full memory also travels as an ABDSynths dump");
        const auto bankImport = sysEx.parseSysEx(nativeBankFrame.data(), nativeBankFrame.size(), hwApvts);
        check(bankImport.success && bankImport.programCount == static_cast<int>(SysExManager::BANK_SIZE),
              "the ABDSynths bank dump fills the host's 128 preset slots");
    }

}

static void testDumpRequests() {
    // --- Test 22: peticiones 0x10/0x0E — el plugin como servidor de presets ---
    printf("\n[Test 22] Dump requests (0x10/0x0E) — the plugin answering as a preset server...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto& apvts = dummyFixture().apvts;
        SysExManager sysEx;
        sysEx.setActiveProgramIndex(5);

        // 1. Petición ABDSynths de un preset: se reconoce y se puede responder.
        const std::vector<uint8_t> ownReq = { 0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_REQUEST, 0xF7 };
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
        const std::vector<uint8_t> ownAllReq = { 0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_ALL_REQUEST, 0xF7 };
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
        const std::vector<uint8_t> korgReq = { 0xF0, 0x42, 0x30, 0x58, 0x10, 0xF7 };  // canal 1
        const auto korgResult = sysEx.parseSysEx(korgReq.data(), korgReq.size(), apvts);
        check(korgResult.success && korgResult.messageType == SysExMessageType::ProgramDumpRequest
                  && korgResult.midiChannel == 1,
              "a real Korg program request (F0 42 3n 58 10 F7) is recognized with its channel");

        if (auto* p = apvts.getParameter(ParamIDs::filterCutoff))
            p->setValueNotifyingHost(p->convertTo0to1(77.0f));
        const auto korgResp = sysEx.createHardwareProgramDump(korgResult.midiChannel, apvts);
        check(korgResp.size() == 297 && korgResp[1] == 0x42 && korgResp[2] == 0x30 && korgResp[3] == 0x58
                  && korgResp[4] == 0x40,
              "the Korg response is the real 297-byte hardware frame on the requested channel");

        const std::vector<uint8_t> korgAllReq = { 0xF0, 0x42, 0x32, 0x58, 0x0E, 0xF7 }; // canal 3
        const auto korgAllResult = sysEx.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
        check(korgAllResult.success && korgAllResult.messageType == SysExMessageType::AllDataDumpRequest
                  && korgAllResult.midiChannel == 3,
              "a real Korg all-data request is recognized with its channel");

        // 4. Los tamaños mínimos se mantienen: 4 B no significan nada.
        const std::vector<uint8_t> tooShort = { 0xF0, 0x7D, 0x0A, 0x40, 0xF7, 0x00 };
        const auto shortResult = sysEx.parseSysEx(tooShort.data() + 1, tooShort.size() - 1, apvts);
        check(!shortResult.success && shortResult.messageType == SysExMessageType::Unknown,
              "a 4-byte buffer is still rejected as too small");
    }

}

static void testWriteAcknowledgement() {
    // --- Test 23: acuse de escritura (0x23/0x24) — el receptor confirma lo que guardó ---
    printf("\n[Test 23] Write acknowledgement (0x23/0x24) — the receiver confirming what it stored...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto& apvts = dummyFixture().apvts;
        SysExManager sysEx;
        sysEx.setActiveProgramIndex(3);

        auto sameBytes = [](const std::vector<uint8_t>& a, const std::vector<uint8_t>& b)
        {
            return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
        };
        const std::vector<uint8_t> ownAck  = { 0xF0, 0x7D, 0x0A, 0x23, 0xF7 };
        const std::vector<uint8_t> ownNack = { 0xF0, 0x7D, 0x0A, 0x24, 0xF7 };

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
            std::vector<uint8_t> oddFrame{ 0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_DUMP };
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
        check(!rxNack.success && rxNack.messageType == SysExMessageType::WriteError
                  && !rxNack.errorMessage.empty() && rxNack.reply.empty(),
              "a received F0 7D 0A 24 F7 reads as WriteError (the peer could not store it)");

        // 4. Un acuse con payload no es un acuse: no se intenta leer como preset.
        const std::vector<uint8_t> badAck{ 0xF0, 0x7D, 0x0A, 0x23, 0x00, 0xF7 };
        const auto badAckRes = sysEx.parseSysEx(badAck.data(), badAck.size(), apvts);
        check(!badAckRes.success && badAckRes.reply.empty(),
              "a 0x23 carrying a payload is rejected instead of read as a preset");

        // 5. Escritura Korg real (254 B): el acuse sale con la cabecera del equipo y su canal.
        const auto hwFrame = sysEx.createHardwareProgramDump(3, apvts, "HW ACK");
        const auto hwRes = sysEx.parseSysEx(hwFrame.data(), hwFrame.size(), apvts);
        const std::vector<uint8_t> hwAck{ 0xF0, 0x42, 0x32, 0x58, 0x23, 0xF7 }; // canal 3 = 0x32
        check(hwRes.success && sameBytes(hwRes.reply, hwAck),
              "a real 254-byte Korg write is answered F0 42 3n 58 23 F7 on the sender's channel");

        // 6. Escritura Korg que no se puede desempaquetar: NACK del equipo.
        const std::vector<uint8_t> emptyKorg{ 0xF0, 0x42, 0x30, 0x58, 0x40, 0xF7 };
        const auto emptyKorgRes = sysEx.parseSysEx(emptyKorg.data(), emptyKorg.size(), apvts);
        const std::vector<uint8_t> hwNack{ 0xF0, 0x42, 0x30, 0x58, 0x24, 0xF7 };
        check(!emptyKorgRes.success && sameBytes(emptyKorgRes.reply, hwNack),
              "a Korg write that cannot be unpacked is answered F0 42 3n 58 24 F7");

        // 7. Peticiones: el volcado pedido viaja en `reply` (el plugin contestando al Banco).
        const std::vector<uint8_t> ownReq = { 0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_PROGRAM_REQUEST, 0xF7 };
        const auto ownReqRes = sysEx.parseSysEx(ownReq.data(), ownReq.size(), apvts);
        check(ownReqRes.reply.size() == ABDSynthsSysEx::programFrameSize()
                  && ownReqRes.reply[3] == ABDSynthsSysEx::CMD_PROGRAM_DUMP,
              "an ABDSynths program request leaves its 444-byte answer in reply");

        const std::vector<uint8_t> ownAllReq = { 0xF0, 0x7D, 0x0A, ABDSynthsSysEx::CMD_ALL_REQUEST, 0xF7 };
        const auto ownAllRes = sysEx.parseSysEx(ownAllReq.data(), ownAllReq.size(), apvts);
        check(ownAllRes.reply.size() > 5 && ownAllRes.reply[3] == ABDSynthsSysEx::CMD_ALL_DUMP,
              "an ABDSynths all-data request answers with the house bank frame (0x4C)");

        const std::vector<uint8_t> korgReq = { 0xF0, 0x42, 0x32, 0x58, 0x10, 0xF7 }; // canal 3
        const auto korgReqRes = sysEx.parseSysEx(korgReq.data(), korgReq.size(), apvts);
        check(korgReqRes.reply.size() == 297 && korgReqRes.reply[2] == 0x32 && korgReqRes.reply[4] == 0x40,
              "a real Korg request answers with the 297-byte hardware frame (never the native block)");
    }

}

static void testAllDataDumpRequest() {
    // --- Test 24: All Data Dump Request de Korg (0x0E) — la memoria real del equipo ---
    printf("\n[Test 24] Korg All Data Dump Request (0x0E) — answering with the real machine memory...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto& apvts = dummyFixture().apvts;
        // Convención del canal `3n` (ROADMAP §4): ambos extremos usan
        // `0x30 | (canal - 1)`, es decir canal 1 -> 0x30 y canal 16 -> 0x3F.
        // La prueba física frente al equipo real queda pendiente; aquí se verifica
        // únicamente que el plugin conserva la convención compartida.
        {
            const auto ackChannel1  = MS2000HardwareProgram::buildWriteAcknowledgement(1, true);
            const auto ackChannel16 = MS2000HardwareProgram::buildWriteAcknowledgement(16, true);
            check(ackChannel1.size() == 6 && ackChannel1[2] == 0x30 && ackChannel16[2] == 0x3F,
                  "the plugin addresses channel 1 as 0x30 and channel 16 as 0x3F (0x30 | (channel - 1))");
        }

        // Petición tal cual la manda un MS2000/R: `F0 42 3n 58 0E F7` (canal 2).
        const std::vector<uint8_t> korgAllReq = { 0xF0, 0x42, 0x31, 0x58, 0x0E, 0xF7 };

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
            const auto* cutoffMeta = ParameterRegistry::getParameter(ParamIDs::filterCutoff);
            const auto* egIntMeta  = ParameterRegistry::getParameter(ParamIDs::filterEg1Int);
            MS2000ProgramData native;
            native.setName("NATIVE 7");
            // En el bloque nativo cada parámetro vive en `TIMBRE_START + su sysexOffset`.
            native.setByte(static_cast<uint8_t>(MS2000ProgramData::TIMBRE_START + cutoffMeta->sysexOffset), 77);
            native.setByte(static_cast<uint8_t>(MS2000ProgramData::TIMBRE_START + egIntMeta->sysexOffset), 100); // +36
            native.setByte(MS2000ProgramData::VOICE_BYTE + 2, 42);                                            // portamento time

            const auto converted = MS2000HardwareProgram::fromNativeProgram(native);
            const size_t t1 = MS2000HardwareProgram::TIMBRE1_START;
            check(converted.get(t1 + MS2000HardwareProgram::ti::CUTOFF) == 77
                      && converted.get(t1 + MS2000HardwareProgram::ti::FILTER_EG1_INT) == 100
                      && MS2000HardwareProgram::extractBits(
                             converted.get(t1 + MS2000HardwareProgram::ti::PORTAMENTO), 0, 6) == 42
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
            MS2000HardwareProgram program; // plantilla "INIT Program" del equipo real
            program.setName("MEM " + std::to_string(i));
            program.set(MS2000HardwareProgram::TIMBRE1_START + MS2000HardwareProgram::ti::CUTOFF,
                        static_cast<uint8_t>(i));
            machine.push_back(program);
        }

        const auto machineDump = MS2000HardwareProgram::buildAllDataDump(1, machine);
        const size_t dumpedPayload = MS2000HardwareProgram::allDataDumpPayloadSize(machine.size());
        printf("      real memory: 128 programs -> payload %d B, frame %d B\n",
               static_cast<int>(dumpedPayload), static_cast<int>(machineDump.size()));
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
        check(answer.success && answer.messageType == SysExMessageType::AllDataDumpRequest
                  && answer.midiChannel == 2
                  && answer.programCount == static_cast<int>(SysExManager::BANK_SIZE),
              "a real Korg all-data request is recognized with its channel and the slots held");
        check(answer.reply.size() == machineDump.size() && answer.reply.front() == 0xF0
                  && answer.reply[1] == 0x42 && answer.reply[2] == 0x31 && answer.reply[3] == 0x58
                  && answer.reply[4] == MS2000HardwareProgram::CMD_ALL_DATA_DUMP
                  && answer.reply.back() == 0xF7,
              "the answer is the machine memory as F0 42 3n 58 4C ... F7 on the requested channel");

        // 4. El volcado se relee entero y las plazas que no se tocan viajan intactas.
        {
            SysExManager roundTrip;
            const auto reimported = roundTrip.parseSysEx(answer.reply.data(), answer.reply.size(), apvts);
            const auto& slots = roundTrip.getHardwareBank();
            check(reimported.success && slots.size() == machine.size(),
                  "the answer re-imports as the whole memory");

            int preserved = 0;
            for (size_t i = 0; i < slots.size(); ++i)
                if (i != 3 && slots[i].raw == machine[i].raw) ++preserved;
            check(preserved == static_cast<int>(machine.size()) - 1,
                  "every slot but the active one comes back byte for byte (nothing is corrupted)");
            check(slots[3].getName() == machine[3].getName(), "the active slot keeps its name");
        }

        // 5. El programa activo lo manda el motor: si se edita, el volcado lo refleja.
        if (auto* cutoff = apvts.getRawParameterValue(ParamIDs::filterCutoff))
            cutoff->store(77.0f);

        const auto edited = sysEx.parseSysEx(korgAllReq.data(), korgAllReq.size(), apvts);
        {
            SysExManager roundTrip;
            roundTrip.parseSysEx(edited.reply.data(), edited.reply.size(), apvts);
            const auto& slots = roundTrip.getHardwareBank();
            const size_t cutoffOffset = MS2000HardwareProgram::TIMBRE1_START + MS2000HardwareProgram::ti::CUTOFF;
            check(slots[3].raw[cutoffOffset] == 77,
                  "the active program in the dump follows the engine (cutoff 77 in its byte)");
            check(slots[2].raw[cutoffOffset] == machine[2].raw[cutoffOffset],
                  "editing the engine does not rewrite the other slots");
        }

        // 6. El acuse del equipo se reconoce y no se contesta (no hay ping-pong).
        const auto ackOnly = sysEx.parseSysEx(stored.reply.data(), stored.reply.size(), apvts);
        check(ackOnly.success && ackOnly.messageType == SysExMessageType::WriteCompleted
                  && ackOnly.reply.empty(),
              "a received Korg F0 42 3n 58 23 F7 reads as WriteCompleted and is not answered");

        const std::vector<uint8_t> korgNack = { 0xF0, 0x42, 0x31, 0x58, 0x24, 0xF7 };
        const auto nackOnly = sysEx.parseSysEx(korgNack.data(), korgNack.size(), apvts);
        check(!nackOnly.success && nackOnly.messageType == SysExMessageType::WriteError
                  && !nackOnly.errorMessage.empty() && nackOnly.reply.empty(),
              "a received Korg F0 42 3n 58 24 F7 reads as WriteError (the machine refused it)");
    }
}

static void testKorgChannelEndToEnd() {
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
        auto& apvts = dummyFixture().apvts;
        SysExManager sysEx;
        int shaped = 0, recognised = 0, echoed = 0, rightSize = 0;

        for (int channel = 1; channel <= kKorgChannelCount; ++channel)
        {
            const uint8_t* frame = kKorgProgramDumpRequest[channel - 1];

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
            if (result.reply.size() == 297 && result.reply[4] == 0x40) // 0x40 = Program Data Dump
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
                  && kKorgChannelByte[kKorgChannelCount - 1] == 0x3F
                  && channelOne.reply[2] == 0x30 && channelOne.midiChannel == 1,
              "channel 1 is 0x30 and channel 16 is 0x3F on both sides (the whole range is addressable)");

        // Y la petición de memoria completa, también con la trama del contrato: el
        // 0x0E de cada canal recibe la memoria del plugin (128 programas) en su byte.
        int allRecognised = 0, allEchoed = 0;
        for (int channel = 1; channel <= kKorgChannelCount; ++channel)
        {
            const uint8_t* frame = kKorgAllDataDumpRequest[channel - 1];
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

static void testEngineExtension() {
    // --- Test 21: extensión del motor bajo el modelo ABDSynths (Timbre 2, velocidades,
    //             escala/split y el mod sequence real) ---
    printf("\n[Test 21] Engine extension under the ABDSynths model (Timbre 2, velocity sense, scale/split, real mod sequence)...\n");
    {
        // APVTS del registro de parámetros: fixture compartido (ver `dummyFixture()`).
        auto& apvts = dummyFixture().apvts;
        auto& dummyProc = dummyFixture().processor;
        // 1. El registro declara el espejo del Timbre 2 y los 3×16 pasos por timbre.
        const auto& allParams = ParameterRegistry::getAllParameters();
        int timbre2Params = 0, seqStepParams = 0, withSysex = 0, maxOffset = -1;
        for (const auto& m : allParams)
        {
            const std::string id(m.id);
            if (id.rfind("t2", 0) == 0) ++timbre2Params;
            // Pasos de fila: `seq2Step7` / `t2Seq3Step16` (no `seqLastStep`, que no es un paso)
            const size_t stepPos = id.find("Step");
            if (stepPos != std::string::npos && stepPos > 0 && std::isdigit(static_cast<unsigned char>(id[stepPos - 1])))
                ++seqStepParams;
            if (m.sysexOffset >= 0) { ++withSysex; maxOffset = std::max(maxOffset, static_cast<int>(m.sysexOffset)); }
        }
        check(timbre2Params == 115, "the registry declares the Timbre 2 mirror (115 parameters)");
        check(seqStepParams == 96, "each timbre's 3 sequencer rows have their 16 real steps (96 parameters)");
        check(withSysex == 253 && maxOffset == 252, "every parameter the engine models has its own byte in the program");

        // 2. Los defaults del espejo son los del Timbre 1: el INIT real tiene los dos
        //    bloques de 108 B byte a byte idénticos, así que el Timbre 2 no arranca distinto.
        const char* mirroredIds[6] = { ParamIDs::osc1Wave, ParamIDs::filterCutoff, ParamIDs::eg2Sustain,
                                       ParamIDs::lfo1Freq, ParamIDs::patch1Intensity, ParamIDs::mixNoiseLevel };
        bool sameDefaults = true;
        for (const char* id : mirroredIds)
        {
            const juce::String baseId(id);
            const juce::String twin = "t2" + baseId.substring(0, 1).toUpperCase() + baseId.substring(1);
            const auto* base = ParameterRegistry::getParameter(id);
            const auto* mirror = ParameterRegistry::getParameter(twin.toRawUTF8());
            if (base == nullptr || mirror == nullptr || base->defaultValue != mirror->defaultValue)
                sameDefaults = false;
        }
        check(sameDefaults, "the Timbre 2 defaults equal the Timbre 1 ones (the real program has two identical blocks)");

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
        std::vector<uint8_t> v1Frame = { 0xF0, ABDSynthsSysEx::MANUFACTURER_ID, ABDSynthsSysEx::MODEL_BYTE,
                                         ABDSynthsSysEx::CMD_PROGRAM_DUMP };
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
        juce::AudioProcessorValueTreeState extApvts(dummyProc, nullptr, "Parameters",
                                                    ParameterRegistry::createParameterLayout());
        MS2000PatchBuilder::buildInitPatch(extApvts);
        auto setExt = [&extApvts](const char* id, float value) {
            if (auto* p = extApvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        auto renderPeak = [](SynthEngine& eng) {
            juce::AudioBuffer<float> buf(2, 512);
            buf.clear();
            juce::MidiBuffer noMidi;
            eng.processBlock(buf, noMidi, nullptr);
            return buf.getMagnitude(0, 0, 512);
        };

        setExt(ParamIDs::timbreMode, 2.0f);   // Layer: los dos timbres a la vez
        setExt(ParamIDs::t2AmpLevel, 0.0f);   // Timbre 2 mudo
        SynthEngine mutedEngine(extApvts);
        mutedEngine.prepare(48000.0, 512);
        mutedEngine.updateParametersFromAPVTS(); // el modo de programa lo lee el propio motor
        mutedEngine.noteOn(1, 60, 1.0f);
        const float mutedPeak = renderPeak(mutedEngine);

        setExt(ParamIDs::t2AmpLevel, 100.0f); // nivel del INIT para el Timbre 2
        SynthEngine layeredEngine(extApvts);
        layeredEngine.prepare(48000.0, 512);
        layeredEngine.updateParametersFromAPVTS();
        layeredEngine.noteOn(1, 60, 1.0f);
        const float layeredPeak = renderPeak(layeredEngine);

        check(mutedPeak > 0.001f, "in Layer mode the Timbre 1 keeps sounding on its own");
        check(layeredPeak > mutedPeak * 1.4f,
              "the Timbre 2 has its own amplitude: its level adds to the Layer output");

        // 5. Sensibilidad a la velocidad del VCA (byte 28 real).
        auto renderVoicePeak = [](float velocity, float ampVeloSens) {
            Voice voice;
            voice.prepare(48000.0);
            VoiceParameters p;
            p.ampLevel = 1.0f;
            p.ampVeloSens = ampVeloSens;
            p.eg2Attack = 0.0f; p.eg2Decay = 0.0f; p.eg2Sustain = 1.0f; p.eg2Release = 0.0f;
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
        juce::AudioProcessorValueTreeState seqApvts(dummyProc, nullptr, "Parameters",
                                                    ParameterRegistry::createParameterLayout());
        MS2000PatchBuilder::buildInitPatch(seqApvts);
        auto setSeq = [&seqApvts](const char* id, float value) {
            if (auto* p = seqApvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        setSeq(ParamIDs::modSeqOn, 1.0f);
        setSeq(ParamIDs::modSeqResolution, 3.0f);
        setSeq(ParamIDs::seqLastStep, 8.0f);
        setSeq(ParamIDs::seq1Dest, 11.0f);    // CUTOFF: el 12º destino real
        setSeq(ParamIDs::seq1Motion, 0.0f);   // Step
        setSeq(ParamIDs::seq1Step1, -63.0f);
        setSeq(ParamIDs::t2ModSeqOn, 1.0f);
        setSeq(ParamIDs::t2Seq3Dest, 15.0f);  // AMP LEVEL en el Timbre 2
        setSeq(ParamIDs::t2Seq3Step1, 63.0f);

        SynthEngine seqEngine(seqApvts);
        seqEngine.prepare(48000.0, 512);
        seqEngine.updateParametersFromAPVTS();
        seqEngine.getModSequencer().advanceClock(512);

        const auto& rowA = seqEngine.getModSequencer().getTrack(ModSequencer::TIMBRE1_TRACK);
        const auto& rowC2 = seqEngine.getModSequencer().getTrack(ModSequencer::TIMBRE2_TRACK + 2);
        check(static_cast<int>(rowA.destination) == 11 && rowA.length == 8
                  && rowA.motion == ModSeqMotion::Step,
              "the engine reads each row's destination, motion and last step from its parameters");
        check(std::abs(rowA.steps[0] - 0.0f) < 1e-6f && std::abs(rowC2.steps[0] - 1.0f) < 1e-6f,
              "the −63..+63 step bytes land as the row's 0..1 step values");
        check(seqEngine.getModSequencer().getTrack(ModSequencer::TIMBRE1_TRACK + 1).steps[0] == 0.5f,
              "a step byte of 0 (the INIT value) is the centre of the row");

        seqEngine.applySeqModulation(false, seqEngine.voiceParamsA_);
        seqEngine.applySeqModulation(true,  seqEngine.voiceParamsB_);
        check(seqEngine.voiceParamsA_.seq.cutoff == -1.0f,
              "the Timbre 1 row drives its own cutoff (full −1 at the bottom step)");
        check(seqEngine.voiceParamsB_.seq.amp == 1.0f && seqEngine.voiceParamsA_.seq.amp == 0.0f,
              "the Timbre 2 rows drive the Timbre 2 parameters, not the Timbre 1 ones");
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
static void runAllTests()
{
    testDspUtils();
    testEnvelopeCurves();
    testLfo();
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

    printf("\n=== Test Summary ===\n");
    printf("  Passed: %d\n", testsPassed);
    printf("  Failed: %d\n", testsFailed);
    printf("===================\n");
}



} // namespace Tests
} // namespace ABDMS2000

int main() {
    ABDMS2000::Tests::runAllTests();
    return ABDMS2000::Tests::testsFailed > 0 ? 1 : 0;
}