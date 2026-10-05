// Test compatibility layer — implementation
#include "TestCompat.h"
#include "MS2000SysExExporter.h"
#include "SysExManager.h"
#include <cmath>

namespace ABDMS2000 {
namespace DSPUtils {
float filterKeyTrackRatio(float freqHz, float referenceHz, float keyTrackAmount) {
    if (keyTrackAmount <= 0.0f || freqHz <= 0.0f || referenceHz <= 0.0f)
        return 1.0f;
    return std::pow(freqHz / referenceHz, keyTrackAmount);
}
}

std::vector<uint8_t> exportProgramShim(MS2000SysExExporter& exporter, 
                                       juce::AudioProcessorValueTreeState& apvts, 
                                       int programIndex, 
                                       const juce::String& name) {
    juce::MemoryBlock dump;
    exporter.exportSingleProgram(apvts, name, dump, 1);
    return std::vector<uint8_t>(static_cast<const uint8_t*>(dump.getData()), 
                                 static_cast<const uint8_t*>(dump.getData()) + dump.getSize());
}

SysExParseResult handleIncomingSysExShim(SysExManager& manager, 
                                                               const uint8_t* data, 
                                                               size_t size,
                                                               juce::AudioProcessorValueTreeState& apvts) {
    return manager.parseSysEx(data, size, apvts);
}

void panicShim(SynthEngine& engine) {
    engine.allNotesOff();
    engine.setPitchBend(0.0f);
    engine.setModWheel(0.0f);
}

void processBlockShim(SynthEngine& engine, juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    engine.processBlock(buffer, midi, nullptr);
}
}