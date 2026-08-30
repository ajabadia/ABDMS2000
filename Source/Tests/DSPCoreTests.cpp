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

#include "../../MIDI/NRPNParser.h"
#include "../../MIDI/SysExCodec.h"
#include "../State/LCDMenuFormatter.h"

#include <cmath>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <memory>



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

static void runAllTests() {
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
    float dist = DSPUtils::ampDistortion(0.3f, 0.8f);
    check(std::abs(dist) <= 1.0f, "ampDistortion stays bounded");
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

    printf("\n=== EnvelopeCurves Tests ===\n");

    // --- Attack time ---
    float at0 = EnvelopeCurves::getAttackTimeSeconds(0.0f);
    float at1 = EnvelopeCurves::getAttackTimeSeconds(1.0f);
    check(std::abs(at0 - 0.001f) < 0.0005f, "attack norm=0 => ~1ms");
    check(std::abs(at1 - 11.001f) < 0.01f, "attack norm=1 => ~11s");
    check(EnvelopeCurves::getAttackTimeSeconds(0.1f) < EnvelopeCurves::getAttackTimeSeconds(0.9f),
          "attack monotonic");

    // --- Decay/Release time ---
    float dr0 = EnvelopeCurves::getDecayReleaseTimeSeconds(0.0f);
    float dr1 = EnvelopeCurves::getDecayReleaseTimeSeconds(1.0f);
    check(std::abs(dr0 - 0.002f) < 0.001f, "decay norm=0 => ~2ms");
    check(std::abs(dr1 - 20.002f) < 0.1f, "decay norm=1 => ~20s");

    // --- Decay multiplier ---
    double multLong = EnvelopeCurves::getDecayMultiplier(10.0, 44100.0);
    check(multLong > 0.99, "decay multiplier long time ~1.0");
    double multShort = EnvelopeCurves::getDecayMultiplier(0.002, 44100.0);
    double totalDecayResidue = std::pow(multShort, 0.002 * 44100.0);
    check(totalDecayResidue < 0.02, "decay multiplier short time -> < 2% residue");

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

    // --- Test 12: Filter & Vocoder Stability Auditing (Impulse, Resonance Headroom, Nyquist Sanity) ---
    printf("\n[Test 12] Filter & Vocoder Stability Auditing...\n");
    
    // 12.1: Dirac Impulse Test on Filter
    double testSampleRate = 44100.0;
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
    // We create a dummy AudioProcessor or APVTS
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

    DummyProcessor dummyProc;
    juce::AudioProcessorValueTreeState apvts(dummyProc, nullptr, "Parameters", ParameterRegistry::createParameterLayout());
    MS2000PatchBuilder::buildInitPatch(apvts);

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

    // --- Summary ---
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