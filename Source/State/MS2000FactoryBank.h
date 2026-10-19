#pragma once
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>
#include "../MIDI/MS2000HardwareProgram.h"
#include "../MIDI/MS2000ProgramData.h"
#include "MS2000FactoryPreloadData.h"

namespace ABDMS2000
{

struct FactoryPreset
{
    std::string name;
    std::string category;
    MS2000ProgramData programData;
    MS2000HardwareProgram hardwareProgram;
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
        names.reserve(presets_.size());
        for (const auto &preset : presets_)
        {
            names.push_back(preset.name);
        }
        return names;
    }

    const FactoryPreset &getPreset(int index) const noexcept
    {
        int safeIndex = presets_.empty() ? 0 : std::clamp(index, 0, static_cast<int>(presets_.size() - 1));
        return presets_[static_cast<size_t>(safeIndex)];
    }

    const MS2000ProgramData &getPresetProgramData(int index) const noexcept
    {
        return getPreset(index).programData;
    }

    const MS2000HardwareProgram &getPresetHardwareProgram(int index) const noexcept
    {
        return getPreset(index).hardwareProgram;
    }

    int getNumPresets() const noexcept
    {
        return static_cast<int>(presets_.size());
    }

  private:
    std::vector<FactoryPreset> presets_;

    static const char *inferCategory(size_t index, const std::string &name, const MS2000HardwareProgram &hw) noexcept
    {
        if (hw.isVocoderProgram() || (index >= 120 && index <= 125))
            return "Vocoder";
        if (name.find("Bass") != std::string::npos)
            return "Bass";
        if (name.find("Lead") != std::string::npos || name.find("Ld") != std::string::npos)
            return "Lead";
        if (name.find("Pad") != std::string::npos || name.find("Strings") != std::string::npos)
            return "Pad";
        if (name.find("Organ") != std::string::npos || name.find("EP") != std::string::npos
            || name.find("Piano") != std::string::npos || name.find("Clav") != std::string::npos)
            return "Keyboard";
        if (name.find("Arp") != std::string::npos || name.find("Seq") != std::string::npos
            || name.find("-WS") != std::string::npos)
            return "Arp/Seq";
        if (name.find("SFX") != std::string::npos || name.find("Init") != std::string::npos
            || name.find("INIT") != std::string::npos)
            return "SFX/Init";
        return "Synth";
    }

    void initializeBank()
    {
        presets_.clear();
        presets_.reserve(FactoryPreload::kNumPrograms);

        for (size_t i = 0; i < FactoryPreload::kNumPrograms; ++i)
        {
            FactoryPreset p;
            std::memcpy(p.hardwareProgram.raw.data(),
                        FactoryPreload::kFactoryPreloadPrograms[i],
                        FactoryPreload::kProgramSize);
            p.name = p.hardwareProgram.getName();
            p.category = inferCategory(i, p.name, p.hardwareProgram);
            p.programData = p.hardwareProgram.toNativeProgram();
            presets_.push_back(std::move(p));
        }
    }
};

}  // namespace ABDMS2000
