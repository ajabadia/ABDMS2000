// Test compatibility layer — minimal essential shims for ABDMS2000 tests
#pragma once

#include <cmath>
#include "../Core/SynthEngine.h"
#include "../Core/VoiceManager.h"
#include "../DSP/Common/DSPUtils.h"
#include "../DSP/Modulation/VirtualPatchMatrix.h"
#include "JuceHeader.h"
#include "MS2000SysExExporter.h"
#include "SysExManager.h"

// ── M_PI compatibility ──
#ifndef M_PI
#define M_PI juce::MathConstants<double>::pi
#endif

// ── filterKeyTrackRatio (ABDEep compat) ──
namespace ABDMS2000
{
namespace DSPUtils
{
float filterKeyTrackRatio(float freqHz, float referenceHz, float keyTrackAmount);
}
}  // namespace ABDMS2000

// ── MS2000SysExExporter shim (actual API: exportSingleProgram) ──
namespace ABDMS2000
{
std::vector<uint8_t> exportProgramShim(MS2000SysExExporter &exporter,
                                       juce::AudioProcessorValueTreeState &apvts,
                                       int programIndex,
                                       const juce::String &name);
}

// ── SysExManager shim (actual API: parseSysEx) ──
namespace ABDMS2000
{
SysExParseResult handleIncomingSysExShim(SysExManager &manager,
                                         const uint8_t *data,
                                         size_t size,
                                         juce::AudioProcessorValueTreeState &apvts);
}

// ── SynthEngine API mapping ──
namespace ABDMS2000
{
void panicShim(SynthEngine &engine);
std::vector<uint8_t> exportProgramShim(MS2000SysExExporter &exporter,
                                       juce::AudioProcessorValueTreeState &apvts,
                                       int programIndex,
                                       const juce::String &name);
SysExParseResult handleIncomingSysExShim(SysExManager &manager,
                                         const uint8_t *data,
                                         size_t size,
                                         juce::AudioProcessorValueTreeState &apvts);
void processBlockShim(SynthEngine &engine, juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midi);
}  // namespace ABDMS2000

// ── El recuento de la suite y su `check`, para los ficheros de test que no son
//    `DSPCoreTests.cpp` (la paridad del ecualizador, por ejemplo). Los dos estan
//    definidos ahi SIN `static` por esto mismo. ──
namespace ABDMS2000
{
namespace Tests
{
extern int testsPassed;
extern int testsFailed;
void check(bool condition, const char *testName);
}  // namespace Tests
}  // namespace ABDMS2000

// ── expectWithinAbsoluteError with explicit cast ──
#define EXPECT_WITHIN_ABS_ERROR_FLOAT(actual, expected, tolerance, msg)                                                \
    expectWithinAbsoluteError(                                                                                         \
        static_cast<float>(actual), static_cast<float>(expected), static_cast<float>(tolerance), msg)

#define EXPECT_WITHIN_ABS_ERROR_DOUBLE(actual, expected, tolerance, msg)                                               \
    expectWithinAbsoluteError(                                                                                         \
        static_cast<double>(actual), static_cast<double>(expected), static_cast<double>(tolerance), msg)

// ── Panic shim ──
#define ENGINE_PANIC(engine)                                                                                           \
    engine.allNotesOff();                                                                                              \
    engine.setPitchBend(0.0f);                                                                                         \
    engine.setModWheel(0.0f)

// ── Process block shim ──
#define ENGINE_PROCESS_BLOCK(engine, buffer, midi) ABDMS2000::processBlockShim(engine, buffer, midi)

// ── SysEx shims ──
#define EXPORT_PROGRAM_SHIM(exporter, apvts, idx, name) ABDMS2000::exportProgramShim(exporter, apvts, idx, name)
#define HANDLE_SYSEX_SHIM(manager, data, size, apvts) ABDMS2000::handleIncomingSysExShim(manager, data, size, apvts)
#define EXPORT_PROGRAM_SHIM2(exporter, apvts, name) ABDMS2000::exportProgramShim(exporter, apvts, 0, name)

// ── Process block with 3 args ──
#define PROCESS_BLOCK(engine, buffer, midi) engine.processBlock(buffer, midi, nullptr)