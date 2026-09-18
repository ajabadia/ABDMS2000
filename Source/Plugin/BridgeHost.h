#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "HardwareMidiTransport.h"

namespace ABDMS2000 {

class SynthEngine;
class MIDITelemetryManager;
class SysExManager;

/**
 * Puerto mínimo que `BridgeActions` necesita del plugin anfitrión.
 *
 * Existe para que el despacho de acciones (`BridgeActions::handleJsEvent`) sea
 * testeable sin construir el AudioProcessor completo — su `createEditor()`
 * arrastra JUCE GUI, que el ejecutable de tests no enlaza. `ABDMS2000AudioProcessor`
 * lo implementa y los tests aportan un host falso con motores reales.
 */
class BridgeHost
{
public:
    virtual ~BridgeHost() = default;

    virtual juce::AudioProcessorValueTreeState& getAPVTS() = 0;
    virtual SynthEngine& getEngine() = 0;
    virtual MIDITelemetryManager& getMIDITelemetry() = 0;
    virtual SysExManager& getSysExManager() = 0;

    /**
     * Hardware MIDI del anfitrión, para el puente MIDI del Bank Manager embebido
     * (`hardware.send` / `hardware.listen` / `hardware.receive`). Sin dispositivos
     * enlazados el transporte falla con motivo, que es lo que la WebUI necesita
     * para explicar que no habrá fetch en lugar de esperar un timeout.
     */
    virtual HardwareMidiTransport& getHardwareMidiTransport() = 0;

    // Programa activo (los provee juce::AudioProcessor en el plugin real).
    virtual int getCurrentProgram() = 0;
    virtual void setCurrentProgram(int index) = 0;
    virtual void changeProgramName(int index, const juce::String& newName) = 0;
};

} // namespace ABDMS2000
