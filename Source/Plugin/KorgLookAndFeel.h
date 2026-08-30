#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

namespace ABDMS2000 {

class KorgLookAndFeel : public juce::LookAndFeel_V4 
{
public:
    KorgLookAndFeel() 
    {
        // Paleta de colores oficial de Korg de la era MS2000 / Electribe
        setColour(juce::Slider::thumbColourId, juce::Colour::fromRGBA(245, 245, 245, 255)); // Línea indicadora (Off-white)
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromRGBA(35, 38, 42, 255)); // Cuerpo del Knob (Negro mate)
    }

    ~KorgLookAndFeel() override = default;

    /**
     * Sobrescribimos el método nativo de JUCE encargado de renderizar potenciómetros circulares vectoriales.
     */
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        // 1. Calcular el área y radio del potenciómetro de forma proporcional
        auto radius = static_cast<float>(juce::jmin(width / 2, height / 2)) - 4.0f;
        auto centreX = static_cast<float>(x) + static_cast<float>(width)  * 0.5f;
        auto centreY = static_cast<float>(y) + static_cast<float>(height) * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;

        // 2. Dibujar la sombra exterior proyectada sobre el chasis (Look analógico)
        g.setColour(juce::Colours::black.withAlpha(0.25f));
        g.fillEllipse(rx + 1.0f, ry + 2.0f, rw, rw);

        // 3. Dibujar la falda/base del potenciómetro (Plástico estriado exterior)
        juce::Colour bodyColor = slider.findColour(juce::Slider::rotarySliderFillColourId);
        g.setColour(bodyColor.darker(0.15f));
        g.fillEllipse(rx, ry, rw, rw);

        // 4. Dibujar la tapa superior (Metal/Plástico cóncavo con sutil gradiente)
        float topRadiusOffset = radius * 0.12f;
        auto trx = rx + topRadiusOffset;
        auto tryY = ry + topRadiusOffset;
        auto trw = rw - (topRadiusOffset * 2.0f);

        juce::ColourGradient capGradient(bodyColor.brighter(0.08f), centreX, centreY - radius,
                                          bodyColor.darker(0.2f), centreX, centreY + radius, false);
        g.setBrush(capGradient);
        g.fillEllipse(trx, tryY, trw, trw);

        // 5. Dibujar el borde metálico circular exterior
        g.setColour(juce::Colours::black.withAlpha(0.4f));
        g.drawEllipse(trx, tryY, trw, trw, 1.0f);

        // 6. Cálculo de la posición y dibujo de la línea indicadora de Korg
        auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        
        float pointerLength = trw * 0.42f; 
        float pointerThickness = 2.0f;

        auto pointerStartX = centreX + std::sin(angle) * (trw * 0.1f);
        auto pointerStartY = centreY - std::cos(angle) * (trw * 0.1f);
        auto pointerEndX = centreX + std::sin(angle) * pointerLength;
        auto pointerEndY = centreY - std::cos(angle) * pointerLength;

        g.setColour(slider.findColour(juce::Slider::thumbColourId));
        g.drawLine(pointerStartX, pointerStartY, pointerEndX, pointerEndY, pointerThickness);

        // Sutil punto central
        g.setColour(bodyColor.darker(0.5f));
        g.fillEllipse(centreX - 1.5f, centreY - 1.5f, 3.0f, 3.0f);
    }

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

        // 1. Sombra proyectada
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(bounds.translated(0.0f, 1.5f).reduced(0.5f), cornerSize);

        // 2. Cuerpo del botón
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
