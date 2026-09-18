#pragma once

#include <juce_core/juce_core.h>
#include <functional>

// Identidad del host generada desde el contrato canonico: no repetir el modelId
// como literal (Scripts/generate_host_model_id.js).
#include "HostModelId.gen.h"

/**
 * Anuncio del modelId del anfitrión al ABD Bank Manager embebido.
 *
 * El plugin anfitrión publica su propio `modelId` por el bridge C++ → JS con la
 * acción `hostModel`, de modo que el selector de hardware del Bank Manager solo
 * ofrezca modelos compatibles con él (contrato completo en
 * `ABDBankManager/DOCS/SYNTH_INTEGRATION_GUIDE.md` §PASO 5.1).
 *
 * Este header es deliberadamente libre de JUCE GUI: `BridgeActions` lo usa desde
 * `handleJsEvent()` y así el despacho es compilable y testeable sin WebView ni el
 * resto del plugin (el ejecutable de tests no enlaza juce_gui_extra).
 */

namespace ABDMS2000 {

/** Sumidero de mensajes JS: recibe `{ type, data }` listo para el WebUI. */
using JsMessageSink = std::function<void(const juce::var& message)>;

/**
 * modelId canónico de este plugin, según el contrato `korgAbdSm002Contract`
 * (`Source/Contracts/Models/korg-ms2000.ts`, fabricante ABDSynths).
 *
 * El valor vive en `HostModelId.gen.h`, generado desde ese contrato por
 * `Scripts/generate_host_model_id.js`, para que el C++ no sea un segundo
 * propietario del literal.
 */
inline juce::String hostModelId()
{
    return juce::String(kHostModelId);
}

/**
 * Acciones `webuiToCpp` que obligan a anunciar el modelId del host:
 *  - `requestState`: lo envía el ABD Bank Manager embebido cuando su bridge ya
 *    está suscrito a `hostModel` (ABDBankManager/WebUI/src/app.js hace
 *    `initHostContext(bridge)` antes de `bridge.send('requestState')`), por lo
 *    que es el momento fiable de entrega.
 *  - `requestFullState`: handshake inicial de la WebUI anfitriona. Es
 *    idempotente y cubre el orden inverso de carga.
 */
inline bool actionAnnouncesHostModel(const juce::String& action)
{
    return action == "requestState" || action == "requestFullState";
}

/**
 * Sello de build del binario (`HostModelId.gen.h`: fecha/hora de compilación de
 * esta unidad) y revisión del código que generó el header. Se publican tanto en
 * el anuncio como en la ficha.
 */
inline juce::String hostBuildStamp()
{
    return juce::String(ABD_HOST_BUILD_STAMP);
}

inline juce::String hostBuildRevision()
{
    return juce::String(kHostBuildRevision);
}

/**
 * Mensaje `cppToWebui` de la acción `hostModel`:
 * `{ type, data: { modelId, displayName, manufacturer, protocol, buildStamp, buildRevision } }`.
 *
 * El `modelId` es lo que consume el filtrado; los cuatro campos de sello viajan
 * para que el Bank Manager pueda registrar/diagnosticar contra qué build habla
 * (y un build antiguo del Bank Manager los ignora sin romperse).
 */
inline juce::var hostModelMessage()
{
    juce::DynamicObject::Ptr data = new juce::DynamicObject();
    data->setProperty("modelId", hostModelId());
    data->setProperty("displayName", juce::String(kHostModelDisplayName));
    data->setProperty("manufacturer", juce::String(kHostModelManufacturer));
    data->setProperty("protocol", kHostBridgeProtocol);
    data->setProperty("buildStamp", hostBuildStamp());
    data->setProperty("buildRevision", hostBuildRevision());

    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty("type", "hostModel");
    message->setProperty("data", juce::var(data.get()));
    return juce::var(message.get());
}

/**
 * Acción `webuiToCpp` con la que el Bank Manager pide la *ficha* del host
 * (identidad + sello de build + nivel de puente). A diferencia del anuncio, esto
 * responde a una pregunta: es lo que permite decir si el binario de al lado es
 * antiguo en lugar de insinuarlo cuando el catálogo queda bloqueado.
 */
inline bool actionAnswersHostInfo(const juce::String& action)
{
    return action == "requestHostInfo";
}

/**
 * Mensaje `cppToWebui` de la acción `hostInfo`: la ficha del host, con la misma
 * forma que el anuncio. Responde a `requestHostInfo`.
 */
inline juce::var hostInfoMessage()
{
    juce::DynamicObject::Ptr message = new juce::DynamicObject();
    message->setProperty("type", "hostInfo");
    message->setProperty("data", hostModelMessage().getProperty("data", juce::var()));
    return juce::var(message.get());
}

/**
 * Despacho de la ficha del host, invocado junto al anuncio en
 * `BridgeActions::handleJsEvent()`. Responde a `requestHostInfo`.
 */
inline bool answerHostInfoForAction(const juce::String& action, const JsMessageSink& sink)
{
    if (! actionAnswersHostInfo(action))
        return false;

    if (sink != nullptr)
        sink(hostInfoMessage());

    return true;
}

/**
 * Despacho del anuncio, invocado por `BridgeActions::handleJsEvent()` para cada
 * acción recibida del WebUI. Si la acción lo requiere, emite `hostModel` por el
 * sumidero y devuelve true.
 *
 * @param action  acción `webuiToCpp` recibida
 * @param sink    sumidero de mensajes JS (puede estar vacío: no emite nada)
 * @returns true si la acción debía anunciar el modelId del host
 */
inline bool announceHostModelForAction(const juce::String& action, const JsMessageSink& sink)
{
    if (! actionAnnouncesHostModel(action))
        return false;

    if (sink != nullptr)
        sink(hostModelMessage());

    return true;
}

} // namespace ABDMS2000
