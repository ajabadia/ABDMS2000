#pragma once
#include "../MIDI/MS2000ProgramData.h"
// SysExCodec included transitively via MS2000ProgramData.h -> ABDSharedCode
#include <vector>
#include <string>
#include <algorithm>

namespace ABDMS2000 {

struct FactoryPreset 
{
    std::string name;
    std::string category;
    MS2000ProgramData programData;
};

class MS2000FactoryBank 
{
public:
    MS2000FactoryBank()
    {
        initializeBank();
    }

    ~MS2000FactoryBank() = default;

    std::vector<std::string> getPresetNames() const
    {
        std::vector<std::string> names;
        for (const auto& preset : presets_) {
            names.push_back(preset.name);
        }
        return names;
    }

    const FactoryPreset& getPreset(int index) const noexcept
    {
        int safeIndex = presets_.empty() ? 0 : std::clamp(index, 0, static_cast<int>(presets_.size() - 1));
        return presets_[static_cast<size_t>(safeIndex)];
    }

    const MS2000ProgramData& getPresetProgramData(int index) const noexcept
    {
        return getPreset(index).programData;
    }

    int getNumPresets() const noexcept { return static_cast<int>(presets_.size()); }

private:
    std::vector<FactoryPreset> presets_;

    void initializeBank()
    {
        presets_.clear();
        presets_.reserve(32);

        // Helper to create and push a preset
        auto addPreset = [this](const std::string& name, const std::string& cat, auto setupFn) {
            FactoryPreset p;
            p.name = name;
            p.category = cat;
            p.programData.reset();
            p.programData.setName(name);

            setupFn(p.programData);
            presets_.push_back(p);
        };

        // 1. A.11 Techno Lead
        addPreset("A.11 TechnoLead", "Lead", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Semitone, 0.0f);  // 0 st
            d.setParam(ParamIDs::osc2Tune, 8.0f);      // +8 cents
            d.setParam(ParamIDs::mixOsc1Level, 110.0f);
            d.setParam(ParamIDs::mixOsc2Level, 100.0f);
            d.setParam(ParamIDs::filterType, 0.0f);    // LPF24
            d.setParam(ParamIDs::filterCutoff, 96.0f);
            d.setParam(ParamIDs::filterResonance, 45.0f);
            d.setParam(ParamIDs::filterEg1Int, 20.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::ampPan, 0.0f);
            d.setParam(ParamIDs::eg1Attack, 0.0f);
            d.setParam(ParamIDs::eg1Decay, 55.0f);
            d.setParam(ParamIDs::eg1Sustain, 20.0f);
            d.setParam(ParamIDs::eg1Release, 30.0f);
            d.setParam(ParamIDs::eg2Attack, 0.0f);
            d.setParam(ParamIDs::eg2Decay, 50.0f);
            d.setParam(ParamIDs::eg2Sustain, 120.0f);
            d.setParam(ParamIDs::eg2Release, 25.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 0.0f);     // Chorus
            d.setParam(ParamIDs::modFxSpeed, 35.0f);
            d.setParam(ParamIDs::modFxDepth, 50.0f);
            d.setParam(ParamIDs::delayOn, 1.0f);
            d.setParam(ParamIDs::delayType, 1.0f);     // Cross/Ping-Pong
            d.setParam(ParamIDs::delayTime, 48.0f);
            d.setParam(ParamIDs::delayDepth, 40.0f);
        });

        // 2. A.12 Space Pad
        addPreset("A.12 Space Pad ", "Pad", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Wave, 2.0f);      // Triangle
            d.setParam(ParamIDs::osc2Semitone, 12.0f); // +1 Octave
            d.setParam(ParamIDs::osc2Tune, 4.0f);      // +4 cents
            d.setParam(ParamIDs::mixOsc1Level, 95.0f);
            d.setParam(ParamIDs::mixOsc2Level, 85.0f);
            d.setParam(ParamIDs::filterType, 0.0f);    // LPF24
            d.setParam(ParamIDs::filterCutoff, 68.0f);
            d.setParam(ParamIDs::filterResonance, 30.0f);
            d.setParam(ParamIDs::filterEg1Int, 14.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::eg1Attack, 65.0f);
            d.setParam(ParamIDs::eg1Decay, 80.0f);
            d.setParam(ParamIDs::eg1Sustain, 80.0f);
            d.setParam(ParamIDs::eg1Release, 75.0f);
            d.setParam(ParamIDs::eg2Attack, 55.0f);
            d.setParam(ParamIDs::eg2Decay, 70.0f);
            d.setParam(ParamIDs::eg2Sustain, 100.0f);
            d.setParam(ParamIDs::eg2Release, 70.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 1.0f);     // Ensemble
            d.setParam(ParamIDs::modFxSpeed, 25.0f);
            d.setParam(ParamIDs::modFxDepth, 80.0f);
            d.setParam(ParamIDs::delayOn, 1.0f);
            d.setParam(ParamIDs::delayType, 0.0f);     // Stereo Delay
            d.setParam(ParamIDs::delayTime, 60.0f);
            d.setParam(ParamIDs::delayDepth, 60.0f);
            d.setParam(ParamIDs::delayFeedback, 50.0f);
        });

        // 3. A.13 Vocoder Chd
        addPreset("A.13 VocoderChd", "Vocoder", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Semitone, 0.0f);
            d.setParam(ParamIDs::osc2Tune, 6.0f);
            d.setParam(ParamIDs::mixOsc1Level, 127.0f);
            d.setParam(ParamIDs::mixOsc2Level, 110.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 127.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::eg2Attack, 0.0f);
            d.setParam(ParamIDs::eg2Decay, 30.0f);
            d.setParam(ParamIDs::eg2Sustain, 127.0f);
            d.setParam(ParamIDs::eg2Release, 15.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 0.0f);
            d.setParam(ParamIDs::modFxDepth, 45.0f);
        });

        // 4. A.14 Acid 303 Bass
        addPreset("A.14 Acid 303  ", "Bass", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::voiceMode, 0.0f);     // Mono
            d.setParam(ParamIDs::portamentoTime, 40.0f);
            d.setParam(ParamIDs::portamentoOn, 1.0f);
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::mixOsc1Level, 127.0f);
            d.setParam(ParamIDs::mixOsc2Level, 0.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 38.0f);
            d.setParam(ParamIDs::filterResonance, 92.0f);
            d.setParam(ParamIDs::filterEg1Int, 40.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::ampDistortion, 1.0f);
            d.setParam(ParamIDs::eg1Attack, 0.0f);
            d.setParam(ParamIDs::eg1Decay, 45.0f);
            d.setParam(ParamIDs::eg1Sustain, 0.0f);
            d.setParam(ParamIDs::eg1Release, 20.0f);
            d.setParam(ParamIDs::eg2Attack, 0.0f);
            d.setParam(ParamIDs::eg2Decay, 50.0f);
            d.setParam(ParamIDs::eg2Sustain, 0.0f);
            d.setParam(ParamIDs::eg2Release, 20.0f);
        });

        // 5. A.15 SuperSawPoly
        addPreset("A.15 SuperSaw  ", "Poly", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::voiceMode, 1.0f);     // Poly
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Semitone, 0.0f);
            d.setParam(ParamIDs::osc2Tune, 12.0f);     // Detune +12
            d.setParam(ParamIDs::mixOsc1Level, 115.0f);
            d.setParam(ParamIDs::mixOsc2Level, 115.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 110.0f);
            d.setParam(ParamIDs::filterResonance, 25.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::eg2Attack, 2.0f);
            d.setParam(ParamIDs::eg2Decay, 60.0f);
            d.setParam(ParamIDs::eg2Sustain, 115.0f);
            d.setParam(ParamIDs::eg2Release, 45.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 0.0f);     // Chorus
            d.setParam(ParamIDs::modFxSpeed, 42.0f);
            d.setParam(ParamIDs::modFxDepth, 70.0f);
        });

        // 6. A.16 DWGS EPiano
        addPreset("A.16 DWGS EPian", "Keyboard", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::osc1Wave, 5.0f);      // DWGS
            d.setParam(ParamIDs::osc1DwgsWave, 2.0f);  // DWGS EPiano
            d.setParam(ParamIDs::osc2Wave, 2.0f);      // Triangle
            d.setParam(ParamIDs::osc2Semitone, 12.0f); // +1 Octave
            d.setParam(ParamIDs::mixOsc1Level, 120.0f);
            d.setParam(ParamIDs::mixOsc2Level, 50.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 85.0f);
            d.setParam(ParamIDs::filterResonance, 15.0f);
            d.setParam(ParamIDs::filterEg1Int, 16.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::eg1Attack, 0.0f);
            d.setParam(ParamIDs::eg1Decay, 65.0f);
            d.setParam(ParamIDs::eg1Sustain, 25.0f);
            d.setParam(ParamIDs::eg2Attack, 0.0f);
            d.setParam(ParamIDs::eg2Decay, 75.0f);
            d.setParam(ParamIDs::eg2Sustain, 30.0f);
            d.setParam(ParamIDs::eg2Release, 35.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 0.0f);     // Chorus
            d.setParam(ParamIDs::modFxSpeed, 30.0f);
            d.setParam(ParamIDs::modFxDepth, 55.0f);
        });

        // 7. A.21 Vox Formant
        addPreset("A.21 VoxFormant", "Lead", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::osc1Wave, 4.0f);      // VoxWave
            d.setParam(ParamIDs::osc1Ctrl1, 64.0f);
            d.setParam(ParamIDs::osc2Wave, 1.0f);      // Pulse
            d.setParam(ParamIDs::osc2Semitone, -12.0f); // Sub octave
            d.setParam(ParamIDs::mixOsc1Level, 110.0f);
            d.setParam(ParamIDs::mixOsc2Level, 70.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 100.0f);
            d.setParam(ParamIDs::filterResonance, 35.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::eg2Attack, 5.0f);
            d.setParam(ParamIDs::eg2Decay, 55.0f);
            d.setParam(ParamIDs::eg2Sustain, 100.0f);
            d.setParam(ParamIDs::eg2Release, 30.0f);
            d.setParam(ParamIDs::modFxOn, 1.0f);
            d.setParam(ParamIDs::modFxType, 2.0f);     // Phaser
            d.setParam(ParamIDs::modFxSpeed, 28.0f);
            d.setParam(ParamIDs::modFxDepth, 60.0f);
        });

        // 8. A.22 HardSyncLead
        addPreset("A.22 HardSyncLd", "Lead", [](MS2000ProgramData& d) {
            d.setParam(ParamIDs::voiceMode, 0.0f);     // Mono
            d.setParam(ParamIDs::portamentoTime, 30.0f);
            d.setParam(ParamIDs::portamentoOn, 1.0f);
            d.setParam(ParamIDs::osc1Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2Wave, 0.0f);      // Saw
            d.setParam(ParamIDs::osc2ModType, 2.0f);   // Hard Sync
            d.setParam(ParamIDs::osc2Semitone, 7.0f);  // +7 Semitones
            d.setParam(ParamIDs::mixOsc1Level, 100.0f);
            d.setParam(ParamIDs::mixOsc2Level, 127.0f);
            d.setParam(ParamIDs::filterType, 0.0f);
            d.setParam(ParamIDs::filterCutoff, 105.0f);
            d.setParam(ParamIDs::filterResonance, 40.0f);
            d.setParam(ParamIDs::ampLevel, 100.0f);
            d.setParam(ParamIDs::ampDistortion, 1.0f);
            d.setParam(ParamIDs::eg2Attack, 0.0f);
            d.setParam(ParamIDs::eg2Decay, 50.0f);
            d.setParam(ParamIDs::eg2Sustain, 120.0f);
            d.setParam(ParamIDs::eg2Release, 20.0f);
            d.setParam(ParamIDs::delayOn, 1.0f);
            d.setParam(ParamIDs::delayType, 1.0f);     // Ping-Pong Delay
            d.setParam(ParamIDs::delayTime, 42.0f);
            d.setParam(ParamIDs::delayDepth, 45.0f);
        });
    }
};

} // namespace ABDMS2000
