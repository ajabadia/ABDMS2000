#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace ABDMS2000 {

class KorgLedButton : public juce::Button 
{
public:
    explicit KorgLedButton(const juce::String& buttonName = "")
        : juce::Button(buttonName)
    {
        setClickingTogglesState(true);
        setSize(24, 36); // Proporción rectangular vertical clásica de Korg
    }

    ~KorgLedButton() override = default;

    /**
     * Estado del secuenciador: Define si el cabezal de reproducción del DAW 
     * está pisando este paso concreto en la muestra actual (Efecto luz flotante).
     */
    void setPlayheadActive(bool isActive)
    {
        if (isPlayheadActive_ != isActive)
        {
            isPlayheadActive_ = isActive;
            repaint(); // Forzar a JUCE a redibujar el LED solo cuando cambia de estado
        }
    }

    bool isPlayheadActive() const noexcept { return isPlayheadActive_; }

protected:
    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. DIBUJAR EL CUERPO DEL BOTÓN (Plástico translúcido Korg gris/marrón texturizado)
        juce::Colour baseColor = juce::Colour::fromRGBA(74, 78, 82, 255);
        if (shouldDrawButtonAsHighlighted) baseColor = baseColor.brighter(0.08f);
        if (shouldDrawButtonAsDown)        baseColor = baseColor.darker(0.15f);

        // Bisel exterior con un gradiente sutil para simular el relieve 3D
        juce::ColourGradient bodyGrad(baseColor.brighter(0.1f), bounds.getX(), bounds.getY(),
                                       baseColor.darker(0.2f), bounds.getX(), bounds.getBottom(), false);
        g.setBrush(bodyGrad);
        g.fillRoundedRectangle(bounds.reduced(1.0f), 2.0f);

        // Borde exterior negro fino
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawRoundedRectangle(bounds.reduced(1.0f), 2.0f, 1.0f);

        // 2. DIBUJAR LA RANURA GRÁFICA DEL LED CENTRAL
        float ledW = 6.0f;
        float ledH = 6.0f;
        float ledX = (bounds.getWidth() - ledW) * 0.5f;
        float ledY = 6.0f;

        // Fondo de la cavidad plástica del LED
        g.setColour(juce::Colours::black.withAlpha(0.7f));
        g.fillRoundedRectangle(ledX, ledY, ledW, ledH, 1.0f);

        // 3. LÓGICA DE ILUMINACIÓN DEL DIODO LED
        bool isLedOn = getToggleState() || isPlayheadActive_;

        if (isLedOn)
        {
            juce::Colour ledRed = juce::Colour::fromRGBA(255, 30, 20, 255);
            if (isPlayheadActive_) ledRed = juce::Colours::orangeBright;

            // Núcleo del diodo
            g.setColour(ledRed);
            g.fillRoundedRectangle(ledX + 0.5f, ledY + 0.5f, ledW - 1.0f, ledH - 1.0f, 1.0f);

            // SIMULACIÓN DE RESPLANDOR (Glow analógico)
            g.setColour(ledRed.withAlpha(0.35f));
            g.fillEllipse(ledX - 3.0f, ledY - 3.0f, ledW + 6.0f, ledH + 6.0f);
            
            g.setColour(ledRed.withAlpha(0.15f));
            g.fillEllipse(ledX - 6.0f, ledY - 6.0f, ledW + 12.0f, ledH + 12.0f);
        }
        else
        {
            // LED apagado
            g.setColour(juce::Colour::fromRGBA(90, 10, 10, 255));
            g.fillRoundedRectangle(ledX + 0.5f, ledY + 0.5f, ledW - 1.0f, ledH - 1.0f, 1.0f);
        }

        // 4. NÚMERO DE PASO / BANDA
        g.setColour(juce::Colours::white.withAlpha(0.7f));
        g.setFont(juce::Font("Courier New", 12.0f, juce::Font::bold));
        g.drawText(getButtonText(), 0, static_cast<int>(bounds.getHeight() - 16.0f),
                   static_cast<int>(bounds.getWidth()), 12, juce::Justification::centred);
    }

private:
    bool isPlayheadActive_{ false };
};

} // namespace ABDMS2000
