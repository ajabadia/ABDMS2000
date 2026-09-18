#pragma once

#include <juce_core/juce_core.h>

#include <functional>
#include <utility>

namespace ABDMS2000 {

/**
 * Transporte MIDI de hardware del anfitrión: el otro extremo del puente MIDI del
 * Bank Manager embebido.
 *
 * Dentro del WebView2 no hay Web MIDI, así que el Bank Manager construye el SysEx
 * del contrato y lo manda por el bridge (`hardware.send`); este transporte lo
 * entrega al dispositivo MIDI y devuelve a la WebUI los bytes que lleguen
 * (`hardware.receive`). `hardware.listen` es el Bank Manager diciendo que ya está
 * suscrito, momento en el que merece la pena abrir la entrada.
 *
 * Los dispositivos los abre el Editor (`bind()`), que sí puede enlazar
 * `juce_audio_devices`; este header solo depende de `juce_core` para poder
 * compilarse también en `ABDMS2000_Tests`, que no enlaza ese módulo.
 *
 * Sin `bind()` (tests, o un host sin hardware disponible) todo falla con un motivo
 * explícito en lugar de tragarse los bytes en silencio: así el Bank Manager puede
 * decir *por qué* no habrá fetch en vez de dejar al usuario esperando un timeout.
 * (Antes de existir este transporte, `hardware.send` / `hardware.listen` no se
 * despachaban en absoluto y el fetch por el puente solo podía morir por timeout.)
 */
class HardwareMidiTransport
{
public:
    /** Resultado de una operación: `detail` es el dispositivo (si ok) o el motivo. */
    struct Outcome
    {
        bool ok = false;
        juce::String detail;
    };

    /** WebUI → hardware. Devuelve ok=false con el motivo cuando no se pudo enviar. */
    using SendFunction = std::function<Outcome(const juce::MemoryBlock&)>;
    /** El Bank Manager ya escucha: abre la entrada. `detail` = nombre del dispositivo. */
    using ListenFunction = std::function<Outcome()>;
    /** Enumeración de puertos para `hardware.listPorts`; devuelve `{ inputs, outputs }`. */
    using PortListFunction = std::function<juce::var()>;
    /** Envoltorio JS ya formado (`{ type, data }`) para los bytes que llegan. */
    using ReceiveSink = std::function<void(const juce::var&)>;

    /** Inyecta el hardware. Sin argumentos válidos, `send`/`listen` fallan con motivo. */
    void bind(SendFunction send, ListenFunction listen, ReceiveSink sink)
    {
        sendFunction = std::move(send);
        listenFunction = std::move(listen);
        receiveSink = std::move(sink);
        listening = false;
        lastDetail.clear();
    }

    /** Instala la enumeración que solo conoce el Editor JUCE. */
    void setPortListFunction(PortListFunction fn) { portListFunction = std::move(fn); }

    /** Lista los puertos actuales; una ausencia de proveedor devuelve un objeto vacío. */
    juce::var listPorts() const
    {
        if (portListFunction)
            return portListFunction();
        return juce::var(new juce::DynamicObject());
    }

    /** Selección explícita del Bank Manager. No se elige ningún primer puerto implícitamente. */
    void selectPorts(const juce::String& outputId, const juce::String& inputId)
    {
        selectedOutputId = outputId;
        selectedInputId = inputId;
        listening = false;
        lastDetail.clear();
    }

    const juce::String& getSelectedOutputId() const noexcept { return selectedOutputId; }
    const juce::String& getSelectedInputId() const noexcept { return selectedInputId; }
    bool hasSelectedOutput() const noexcept { return selectedOutputId.isNotEmpty(); }
    bool hasSelectedInput() const noexcept { return selectedInputId.isNotEmpty(); }

    /** La entrada seleccionada desapareció: no se deben reenviar datos obsoletos. */
    void markInputUnavailable(const juce::String& reason)
    {
        listening = false;
        lastDetail = reason;
    }

    /** El Editor se va (o se quedan sin dispositivo): deja de haber hardware. */
    void unbind()
    {
        listening = false;
        sendFunction = {};
        listenFunction = {};
        portListFunction = {};
        receiveSink = {};
        lastDetail.clear();
    }

    bool isBound() const noexcept { return static_cast<bool>(sendFunction); }
    bool isListening() const noexcept { return listening; }
    const juce::String& getLastDetail() const noexcept { return lastDetail; }

    /** WebUI → hardware. */
    bool send(const juce::MemoryBlock& message)
    {
        if (message.isEmpty())
            return fail("Empty MIDI message");

        if (! sendFunction)
            return fail("No MIDI output device available for the hardware transfer");

        auto outcome = sendFunction(message);
        lastDetail = outcome.detail;
        return outcome.ok;
    }

    /** El Bank Manager ha pedido escuchar: arranca la entrada y reenvía lo que llegue. */
    bool listen()
    {
        if (! listenFunction)
            return fail("No MIDI input device available for the hardware transfer");

        auto outcome = listenFunction();
        listening = outcome.ok;
        lastDetail = outcome.detail;
        return outcome.ok;
    }

    /**
     * Bytes del hardware → WebUI, con la misma forma que el core del Bank Manager
     * standalone: `{ type: 'hardware.receive', data: { payload: <base64>, size } }`.
     */
    void deliverIncoming(const juce::MemoryBlock& message)
    {
        if (! listening || message.isEmpty() || ! receiveSink)
            return;

        juce::DynamicObject::Ptr data = new juce::DynamicObject();
        data->setProperty("payload", juce::Base64::toBase64(message.getData(), message.getSize()));
        data->setProperty("size", static_cast<int>(message.getSize()));

        juce::DynamicObject::Ptr envelope = new juce::DynamicObject();
        envelope->setProperty("type", "hardware.receive");
        envelope->setProperty("data", juce::var(data.get()));

        receiveSink(juce::var(envelope.get()));
    }

private:
    bool fail(const juce::String& reason)
    {
        lastDetail = reason;
        return false;
    }

    SendFunction sendFunction;
    ListenFunction listenFunction;
    PortListFunction portListFunction;
    ReceiveSink receiveSink;
    bool listening = false;
    juce::String lastDetail;
    juce::String selectedOutputId;
    juce::String selectedInputId;
};

} // namespace ABDMS2000
