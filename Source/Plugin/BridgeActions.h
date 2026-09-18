#pragma once

#include "BridgeHost.h"
#include "HostModelAnnouncement.h"
#include "SoftwarePresetProtocol.h"

#include <functional>

namespace ABDMS2000 {

/**
 * Puente de acciones WebUI ↔ plugin.
 *
 * Sus dos dependencias se inyectan, y ambas existen para poder testear el
 * despacho sin un WebView ni el AudioProcessor completo:
 *   - `BridgeHost&`   : puerto con lo que el bridge necesita del plugin
 *                       (APVTS, motor, telemetría MIDI, SysEx, programas).
 *   - `JsMessageSink` : sumidero de mensajes JS `{ type, data }`. En producción
 *                       el Editor lo conecta al WebBrowserComponent; en los
 *                       tests, a un capturador.
 */
class BridgeActions {
public:
    explicit BridgeActions(BridgeHost& host);
    ~BridgeActions() = default;

    /** Conecta el sumidero de mensajes JS. Sin sumidero no se emite nada. */
    void setJsMessageSink(JsMessageSink sink) { jsEventSink = std::move(sink); }

    void handleJsEvent(const juce::var& message);
    void sendEventToJs(const juce::String& eventType, const juce::var& payload);
    void sendFullParamSync();

    /**
     * Anuncia el modelId de este plugin al ABD Bank Manager embebido
     * (acción cppToWebui `hostModel`; política, disparadores y payload en
     * HostModelAnnouncement.h).
     */
    void sendHostModel();

    void setOnToggleScope(std::function<void()> cb) { onToggleScope_ = std::move(cb); }

private:
    /** Envía `{ type, data }` al sumidero inyectado. */
    void emitJsMessage(const juce::var& message);

    // ─── Transporte de software (SoftwarePresetProtocol.h) ────────────────────
    // El ABD Bank Manager embebido lee y escribe los presets de este synth con los
    // mismos verbos que con un equipo físico. Ver
    // ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md §6.2/§6.4 y §7.
    void handlePresetRead(const juce::var& message);
    void handleBankRead(const juce::var& message);
    void handlePresetWrite(const juce::var& message);
    void handleBankWrite(const juce::var& message);
    void handlePresetCapture(const juce::var& message);

    /** Bloque de programa del slot de la memoria del synth, con su nombre. */
    juce::MemoryBlock readProgramBlob(int slot, juce::String& name) const;

    /** Qué toca un bloque que llega del Bank Manager (§7.2 del modelo de transporte). */
    enum class PresetWriteMode
    {
        Audition,      // solo el motor: buffer de edición, la memoria del synth no se toca
        StoreAndLoad,  // guarda el slot y lo carga en el motor (pasa a ser el patch activo)
        StoreOnly,     // solo guarda el slot (un lote no cambia lo que suena)
    };

    /**
     * Escribe un bloque del Bank Manager según `mode`. `syncParams` permite al lote
     * sincronizar la WebUI una sola vez al final.
     */
    bool writeProgramBlob(int slot, const juce::MemoryBlock& blob, const juce::String& name,
                          PresetWriteMode mode, juce::String& error, bool syncParams = true);

    /** `preset.error` con el `requestId` de la petición (si lo traía). */
    void sendPresetError(const juce::var& message, const juce::String& code, const juce::String& reason);

    /** Slot dentro de la memoria del synth (0..127). */
    static bool isValidProgramSlot(int slot);

    BridgeHost& host_;
    JsMessageSink jsEventSink;
    std::function<void()> onToggleScope_;
};

} // namespace ABDMS2000
