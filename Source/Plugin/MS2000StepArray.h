#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "KorgLedButton.h"
#include <array>
#include <memory>

namespace ABDMS2000 {

class MS2000StepArray : public juce::Component 
{
public:
    MS2000StepArray()
    {
        for (int i = 0; i < 16; ++i)
        {
            juce::String stepNumber = juce::String(i + 1);
            stepButtons_[i] = std::make_unique<KorgLedButton>(stepNumber);
            addAndMakeVisible(stepButtons_[i].get());
        }
    }

    ~MS2000StepArray() override = default;

    /**
     * Actualiza la luz del cabezal en base al paso actual.
     * @param activeStepIndex El índice del paso que está sonando (0 a 15), o -1 si está parado.
     */
    void updatePlayheadPosition(int activeStepIndex)
    {
        for (int i = 0; i < 16; ++i)
        {
            stepButtons_[i]->setPlayheadActive(i == activeStepIndex);
        }
    }

    void resized() override
    {
        constexpr int buttonW = 24;
        constexpr int buttonH = 36;
        constexpr int gap = 4;

        for (int i = 0; i < 16; ++i)
        {
            stepButtons_[i]->setBounds(i * (buttonW + gap), 0, buttonW, buttonH);
        }
    }

    KorgLedButton* getButtonPointer(int index) noexcept 
    { 
        return stepButtons_[juce::jlimit(0, 15, index)].get(); 
    }

private:
    std::array<std::unique_ptr<KorgLedButton>, 16> stepButtons_;
};

} // namespace ABDMS2000
