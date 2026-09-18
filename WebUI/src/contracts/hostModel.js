/**
 * ABDMS2000 — Identidad del anfitrión para el ABD Bank Manager embebido.
 *
 * El host anuncia su `modelId` con la acción unidireccional `hostModel`
 * (contrato completo en ABDBankManager/DOCS/SYNTH_INTEGRATION_GUIDE.md §PASO 5.1).
 * Dos emisores, un solo contrato de mensaje:
 *   - WebView2 / plugin nativo: lo emite el C++
 *     (`BridgeActions` → `HostModelAnnouncement.h`).
 *   - Dev server / build WASM: no hay backend nativo, así que lo responde
 *     `bridge/bridgeCore.js` (ver `announcesHostModel`), incluida la ficha del
 *     host (`requestHostInfo` → `hostInfo`, ver `answersHostInfo`).
 *
 * FUENTE ÚNICA: el contrato canónico `korgAbdSm002Contract`
 * (ABDBankManager/Source/Contracts/Models/korg-ms2000.ts). Este WebUI no lo
 * declara: consume el artefacto generado por ABDBankManager
 * (`Scripts/build_contracts_web.js`, `npm run generate`) y sincronizado aquí por
 * `Scripts/sync_bankmanager.js` → `WebUI/abdbank/`. No repetir el literal
 * `abd-sm002` en este proyecto.
 */
import { korgAbdSm002Contract } from '../../abdbank/src/contracts/gen/modelContracts.gen.js';

/**
 * modelId del ABDMS2000, leído del contrato del host.
 * @type {string}
 */
export const HOST_MODEL_ID = korgAbdSm002Contract.modelId;

/**
 * Acciones que obligan a anunciar el `modelId` del host. Mismo criterio que el
 * C++ (`ABDMS2000::actionAnnouncesHostModel` en `HostModelAnnouncement.h`), para
 * que dev server y plugin no se comporten distinto:
 *  - `requestState`: lo manda el Bank Manager embebido cuando su puente ya está
 *    suscrito a `hostModel` → el momento de entrega fiable.
 *  - `requestFullState`: handshake inicial de la WebUI anfitriona; es idempotente
 *    y cubre el orden inverso de carga.
 * @param {string} action
 * @returns {boolean}
 */
export function announcesHostModel(action) {
  return action === 'requestState' || action === 'requestFullState';
}

/**
 * Payload de la acción `hostModel`: `{ modelId }` (forma anidada aceptada por
 * `ABDBankManager/WebUI/src/core/hostContext.js::readHostModelId`).
 *
 * Incluye el sello de identidad (nombre, fabricante, nivel de puente y build) que
 * el Bank Manager expone cuando diagnostica: en el navegador el sello es
 * `dev-server`, y el nivel de puente replica el del nativo
 * (`kHostBridgeProtocol` en `Source/Plugin/HostModelId.gen.h`).
 * @returns {{ modelId: string, displayName: string, manufacturer: string, protocol: number, buildStamp: string }}
 */
export function hostModelPayload() {
  return {
    modelId: HOST_MODEL_ID,
    displayName: korgAbdSm002Contract.displayName,
    manufacturer: korgAbdSm002Contract.manufacturer,
    protocol: HOST_BRIDGE_PROTOCOL,
    buildStamp: 'dev-server'
  };
}

/**
 * Nivel de puente que habla este emisor (el del navegador). Espejo del nativo:
 *   2 = anuncia el modelId, 3 = responde `requestHostInfo`, 4 = puente MIDI de
 *   hardware (`hardware.listen` / `hardware.send`). En el navegador el MIDI va
 *   por Web MIDI, así que el canal de hardware del bridge no aplica, pero la
 *   paridad de la ficha evita que el Bank Manager se crea ante un host mudo.
 * @type {number}
 */
export const HOST_BRIDGE_PROTOCOL = 4;

/**
 * Acción con la que el Bank Manager pide la ficha del host.
 * @param {string} action
 * @returns {boolean}
 */
export function answersHostInfo(action) {
  return action === 'requestHostInfo';
}

/**
 * Payload de la acción `hostInfo`: misma forma que el anuncio, para que el Bank
 * Manager pueda decir con qué build habla en vez de suponerlo.
 * @returns {{ modelId: string, displayName: string, manufacturer: string, protocol: number, buildStamp: string }}
 */
export function hostInfoPayload() {
  return hostModelPayload();
}
