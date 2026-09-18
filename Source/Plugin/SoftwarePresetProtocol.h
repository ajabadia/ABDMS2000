#pragma once

#include <juce_core/juce_core.h>

#include <cstddef>

namespace ABDMS2000 {

/**
 * Protocolo del transporte de software: los verbos con los que el ABD Bank Manager
 * embebido lee y escribe los presets del synth que lo hospeda.
 *
 * Son los **mismos cuatro verbos** que con un equipo físico (`createMidiTransport`)
 * más `preset.capture`, y los nombres son compartidos con el Bank Manager
 * (`WebUI/src/core/transportCapabilities.js` → `SOFTWARE_ACTIONS` / `SOFTWARE_REPLIES`).
 * La decisión y el porqué están en
 * `ABDBankManager/DOCS/BANK_MANAGER_TRANSPORT_MODEL.md` §6.2/§6.4/§7.
 *
 * | Verbo   | Acción            | Petición                                  | Respuesta        |
 * |---------|-------------------|-------------------------------------------|------------------|
 * | leer 1  | `preset.read`     | `{ system, slot, requestId }`             | `preset.data`    |
 * | leer N  | `bank.read`       | `{ system, requestId }`                   | `bank.data`      |
 * | escribir| `preset.write`    | `{ system, slot, payload, name, audition? }` | `preset.written` |
 * | lote    | `bank.write`      | `{ system, slots: [...] }`                | `bank.written`   |
 * | capturar| `preset.capture`  | `{ system, slot?, requestId }`            | `preset.captured`|
 *
 * Reglas del cable (verificadas por `Source/Tests/DSPCoreTests.cpp` `[Test 19]`):
 *
 *  - `payload` es **base64 estándar** (el `atob`/`btoa` del WebUI). No es
 *    `juce::MemoryBlock::toBase64Encoding()`, que codifica `<tamaño>.<datos>`:
 *    mezclarlos no falla al compilar, falla en silencio al decodificar.
 *  - El host devuelve el `requestId` que reciba. Si no lo devuelve, el transporte
 *    del Bank Manager resuelve la petición más antigua de su tipo.
 *  - Una acción que este host no conozca se contesta `preset.error` con
 *    `code: 'unsupported-action'`, y el Bank Manager cae al verbo por slot en vez
 *    de fallar (`bank.read` → `preset.read` × N).
 *
 * El **sistema de presets** de este host es `native` (decisión §7.1): el bloque de
 * programa propio del plugin, opaco para el Bank Manager. No se declara `sysex`
 * porque su memoria modela Timbre 1 + voz en 128 B con su propio layout, mientras el
 * patch MS2000 del contrato (288 B) es otro formato.
 */
namespace SoftwarePresetProtocol
{
    // ─── Acciones (WebUI → host) ─────────────────────────────────────────────
    inline constexpr const char* kReadPreset    = "preset.read";
    inline constexpr const char* kWritePreset   = "preset.write";
    inline constexpr const char* kReadBank      = "bank.read";
    inline constexpr const char* kWriteBank     = "bank.write";
    inline constexpr const char* kCapturePreset = "preset.capture";

    // ─── Respuestas (host → WebUI) ───────────────────────────────────────────
    inline constexpr const char* kPresetData     = "preset.data";
    inline constexpr const char* kPresetWritten  = "preset.written";
    inline constexpr const char* kBankData       = "bank.data";
    inline constexpr const char* kBankWritten    = "bank.written";
    inline constexpr const char* kPresetCaptured = "preset.captured";
    inline constexpr const char* kError          = "preset.error";

    // ─── Sistema de presets declarado por el contrato del host (HostModelId.gen.h) ──
    inline constexpr const char* kPresetSystem = "native";

    // ─── Códigos de error ────────────────────────────────────────────────────
    /** Acción o sistema que este host no habla → el Bank Manager cae al verbo por slot. */
    inline constexpr const char* kUnsupportedAction = "unsupported-action";
    inline constexpr const char* kInvalidSlot       = "invalid-slot";
    inline constexpr const char* kInvalidPayload    = "invalid-payload";

    /** ¿El `system` que pide el Bank Manager es el que este host expone? */
    inline bool isSupportedSystem(const juce::String& system)
    {
        return system.isEmpty() || system == kPresetSystem;
    }

    /**
     * Bloque de programa en base64 estándar (el que entiende `atob` en el WebUI).
     * `juce::Base64::toBase64` produce exactamente eso; `MemoryBlock` no.
     */
    inline juce::String encodePayload(const void* data, size_t size)
    {
        if (data == nullptr || size == 0)
            return {};

        return juce::Base64::toBase64(data, size);
    }

    /** Decodifica el `payload` del Bank Manager. Devuelve false si no hay bytes. */
    inline bool decodePayload(const juce::String& base64, juce::MemoryBlock& out)
    {
        out.reset();
        if (base64.isEmpty())
            return false;

        juce::MemoryOutputStream stream;
        if (! juce::Base64::convertFromBase64(stream, base64) || stream.getDataSize() == 0)
            return false;

        out = juce::MemoryBlock(stream.getData(), stream.getDataSize());
        return true;
    }
}

} // namespace ABDMS2000
