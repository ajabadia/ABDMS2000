#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ABDMS2000 {

class KorgButtonLookAndFeel : public juce::LookAndFeel_V4 
{
public:
    KorgButtonLookAndFeel() 
    {
        // Paleta de colores oficiales de la era analógica-virtual de Korg
        setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGBA(64, 68, 72, 255));
        setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGBA(230, 235, 240, 220));
        setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    ~KorgButtonLookAndFeel() override = default;

    /**
     * Sobrescribimos el método nativo de JUCE encargado de renderizar el fondo de los botones vectoriales.
     */
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, 
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, 
                               bool shouldDrawButtonAsDown) override
    {
        auto bounds = button.getLocalBounds().toFloat();
        auto cornerSize = 2.5f;

        juce::Colour baseColor = backgroundColour;
        if (shouldDrawButtonAsHighlighted) baseColor = baseColor.brighter(0.08f);
        if (shouldDrawButtonAsDown)        baseColor = baseColor.darker(0.15f);

        // 1. Sombra proyectada (Efecto relieve 3D)
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f).reduced(0.5f), cornerSize);

        // 2. Cuerpo del botón (Gradiente vertical)
        juce::ColourGradient bodyGrad(baseColor.brighter(0.12f), bounds.getX(), bounds.getY(),
                                       baseColor.darker(0.22f), bounds.getX(), bounds.getBottom(), false);
        g.setBrush(bodyGrad);
        g.fillRoundedRectangle(bounds.reduced(1.0f), cornerSize);

        // 3. Bisel interior
        g.setColour(baseColor.brighter(0.25f).withAlpha(0.4f));
        g.drawRoundedRectangle(bounds.reduced(1.5f), cornerSize, 1.0f);

        // 4. Borde exterior
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), cornerSize, 1.0f);

        // 5. Diodo LED integrado
        if (button.getToggleState() || shouldDrawButtonAsDown)
        {
            float ledSize = 4.5f;
            float ledX = (bounds.getWidth() - ledSize) * 0.5f;
            float ledY = 5.0f;

            juce::Colour ledRed = juce::Colour::fromRGBA(255, 40, 20, 255);
            g.setColour(ledRed);
            g.fillEllipse(ledX, ledY, ledSize, ledSize);

            // Glow analógico
            g.setColour(ledRed.withAlpha(0.4f));
            g.fillEllipse(ledX - 2.0f, ledY - 2.0f, ledSize + 4.0f, ledSize + 4.0f);
            
            g.setColour(ledRed.withAlpha(0.15f));
            g.fillEllipse(ledX - 4.0f, ledY - 4.0f, ledSize + 8.0f, ledSize + 8.0f);
        }
        else
        {
            float ledSize = 4.5f;
            float ledX = (bounds.getWidth() - ledSize) * 0.5f;
            float ledY = 5.0f;
            
            g.setColour(juce::Colour::fromRGBA(80, 15, 10, 255));
            g.fillEllipse(ledX, ledY, ledSize, ledSize);
            
            g.setColour(juce::Colours::black.withAlpha(0.3f));
            g.drawEllipse(ledX, ledY, ledSize, ledSize, 0.5f);
        }
    }

    /**
     * Formato al texto impreso sobre el botón.
     */
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, 
                         bool /*shouldDrawButtonAsHighlighted*/, 
                         bool /*shouldDrawButtonAsDown*/) override
    {
        auto font = getTextButtonFont(button, button.getHeight());
        g.setFont(font);
        
        auto bounds = button.getLocalBounds().toFloat();
        auto textYOffset = button.getToggleState() ? 4.0f : 2.0f;
        
        g.setColour(button.findColour(button.getToggleState() ? juce::TextButton::textColourOnId 
                                                                : juce::TextButton::textColourOffId));
        
        g.drawText(button.getButtonText(), 
                    static_cast<int>(bounds.getX()), 
                    static_cast<int>(bounds.getY() + textYOffset), 
                    static_cast<int>(bounds.getWidth()), 
                    static_cast<int>(bounds.getHeight()), 
                    juce::Justification::centred);
    }

    juce::Font getTextButtonFont(juce::TextButton&, int /*buttonHeight*/) override
    {
        return juce::Font("Helvetica", 11.0f, juce::Font::bold);
    }
};

} // namespace ABDMS2000
