#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <string>
#include <vector>

namespace ABDMS2000 {

enum class ParamType {
    Continuous,
    Integer,
    Choice,
    Boolean
};

struct MockParameter {
    juce::String id;
    float value;
};

/**
 * @brief Minimal mock APVTS for unit testing DSP modules in isolation.
 * Only supports getRawParameterValue and getParameter.
 */
class MockAPVTS {
public:
    void addParameter(const juce::String& id, float defaultValue = 0.0f) {
        params_.push_back({ id, defaultValue });
    }

    float* getRawParameterValue(const juce::String& id) {
        for (auto& p : params_) {
            if (p.id == id) return &p.value;
        }
        return &dummy_;
    }

    juce::RangedAudioParameter* getParameter(const juce::String& id) {
        // Return nullptr by default — tests should mock this
        juce::ignoreUnused(id);
        return nullptr;
    }

    void setValue(const juce::String& id, float v) {
        for (auto& p : params_) {
            if (p.id == id) { p.value = v; return; }
        }
    }

private:
    std::vector<MockParameter> params_;
    float dummy_{ 0.0f };
};

} // namespace ABDMS2000