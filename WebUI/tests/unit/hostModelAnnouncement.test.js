/**
 * ABDMS2000 — Anuncio de `hostModel` desde el dev server / build WASM.
 *
 * En WebView2 el `hostModel` lo emite el C++ (BridgeActions → HostModelAnnouncement.h)
 * y el path nativo ya está cubierto por los tests C++ (DSPCoreTests, Test 16/17).
 * En el dev server (Vite, puerto 8384) y en el build WASM no existe backend
 * nativo: sin este anuncio el Bank Manager embebido se queda sin contexto y, al
 * ser `plugin-host`, muestra catálogo vacío + aviso en vez de filtrar.
 *
 * Este test cubre el camino JS equivalente y la FUENTE ÚNICA del modelId
 * (el contrato canónico del host, no un literal duplicado en este proyecto).
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, it, expect, vi, afterEach } from 'vitest';
// Guard de dirección del canal nativo → JS: lógica compartida por toda la suite.
import {
  checkBridgeDirection,
  compactCpp,
  listCppSources,
  stripCppComments
} from '../../../../ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js';
import { BridgeCore } from '../../src/bridge/bridgeCore.js';
import {
  HOST_MODEL_ID,
  HOST_BRIDGE_PROTOCOL,
  announcesHostModel,
  hostModelPayload,
  answersHostInfo,
  hostInfoPayload
} from '../../src/contracts/hostModel.js';
import {
  allModelContracts,
  korgAbdSm002Contract,
} from '../../abdbank/src/contracts/gen/modelContracts.gen.js';

/** Escucha los `hostModel` que el bridge emite en modo dev server / WASM. */
function collectHostModel(bridge) {
  const received = [];
  bridge.on('hostModel', (payload) => received.push(payload));
  return received;
}

afterEach(() => {
  vi.restoreAllMocks();
});

describe('fuente única del modelId del host', () => {
  it('HOST_MODEL_ID sale del contrato canónico, no de un literal del proyecto', () => {
    expect(HOST_MODEL_ID).toBe(korgAbdSm002Contract.modelId);
    expect(korgAbdSm002Contract.modelId).toBe('abd-sm002');
  });

  it('el contrato del host está en el registro generado y es el softsynth (no hardware)', () => {
    expect(allModelContracts.map(c => c.modelId)).toContain(HOST_MODEL_ID);
    expect(korgAbdSm002Contract.isSoftsynth).toBe(true);
    expect(korgAbdSm002Contract.manufacturer).toBe('ABDSynths');
  });
});

describe('announcesHostModel() — qué acciones disparan el anuncio', () => {
  it('requestState y requestFullState anuncian', () => {
    expect(announcesHostModel('requestState')).toBe(true);
    expect(announcesHostModel('requestFullState')).toBe(true);
  });

  it('las acciones de audio/params NO anuncian', () => {
    for (const action of ['setParam', 'noteOn', 'noteOff', 'allNotesOff', 'setDiagnosticTone', '']) {
      expect(announcesHostModel(action)).toBe(false);
    }
  });
});

describe('BridgeCore (dev server / WASM) despacha hostModel', () => {
  it('requestState emite hostModel con el modelId del host', () => {
    const bridge = new BridgeCore();
    const received = collectHostModel(bridge);

    bridge.send('requestState', {});

    expect(received).toHaveLength(1);
    expect(received[0]).toEqual(hostModelPayload());
    expect(received[0].modelId).toBe('abd-sm002');
  });

  it('requestFullState también anuncia (handshake inicial del host)', () => {
    const bridge = new BridgeCore();
    const received = collectHostModel(bridge);

    bridge.send('requestFullState', {});

    expect(received).toHaveLength(1);
    expect(received[0]).toEqual(hostModelPayload());
  });

  it('la ficha del host (requestHostInfo → hostInfo) lleva identidad y sello', () => {
    const bridge = new BridgeCore();
    const received = [];
    bridge.on('hostInfo', data => received.push(data));

    bridge.send('requestHostInfo', {});

    expect(received).toHaveLength(1);
    expect(received[0]).toEqual(hostInfoPayload());
    expect(received[0].modelId).toBe(HOST_MODEL_ID);
    expect(received[0].protocol).toBe(HOST_BRIDGE_PROTOCOL);
    expect(received[0].buildStamp).toBe('dev-server');
  });

  it('answersHostInfo solo responde a requestHostInfo (no al anuncio)', () => {
    expect(answersHostInfo('requestHostInfo')).toBe(true);
    for (const action of ['requestState', 'requestFullState', 'setParam', 'hostModel', '']) {
      expect(answersHostInfo(action)).toBe(false);
    }
    expect(announcesHostModel('requestHostInfo')).toBe(false);
  });

  it('el nivel de puente del navegador no se queda por detrás del nativo', () => {
    const header = fs.readFileSync(
      path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../../Source/Plugin/HostModelId.gen.h'),
      'utf8'
    );
    const declared = Number((header.match(/kHostBridgeProtocol\s*=\s*(\d+)/) || [])[1]);

    expect(Number.isFinite(declared)).toBe(true);
    // Un Bank Manager puede exigir el nivel para el fetch por el puente MIDI: si el
    // C++ sube el nivel, el emisor JS tiene que subirlo con él.
    expect(HOST_BRIDGE_PROTOCOL).toBe(declared);
    expect(HOST_BRIDGE_PROTOCOL).toBeGreaterThanOrEqual(4);
  });

  it('el anuncio se repite si el Bank Manager vuelve a pedir estado (idempotente)', () => {
    const bridge = new BridgeCore();
    const received = collectHostModel(bridge);

    bridge.send('requestState', {});
    bridge.send('requestState', {});
    bridge.send('requestFullState', {});

    expect(received).toHaveLength(3);
    expect(received.every(payload => payload.modelId === HOST_MODEL_ID)).toBe(true);
  });

  it('las acciones no relacionadas no anuncian nada', () => {
    const bridge = new BridgeCore();
    const received = collectHostModel(bridge);
    vi.spyOn(console, 'warn').mockImplementation(() => {});

    bridge.send('setParam', { paramId: 'cutoff', value: 0.5 });
    bridge.send('noteOn', { note: 60, velocity: 1 });
    bridge.send('notAnAction', {});

    expect(received).toHaveLength(0);
  });

  it('una acción desconocida sigue avisando por consola (no rompe el bridge)', () => {
    const bridge = new BridgeCore();
    const warn = vi.spyOn(console, 'warn').mockImplementation(() => {});

    expect(() => bridge.send('notAnAction', {})).not.toThrow();
    expect(warn).toHaveBeenCalledWith('[BridgeCore] Unknown WASM action:', 'notAnAction', {});
  });

  it('con backend JUCE (WebView) el mensaje va al nativo y no se emite en local', () => {
    const emitEvent = vi.fn();
    // Entorno node: el bridge nativo necesita un `document` mínimo para su
    // handshake (oculta el botón de audio) antes de poder emitir al nativo.
    vi.stubGlobal('window', { __JUCE__: { backend: { emitEvent, addEventListener: vi.fn() } } });
    vi.stubGlobal('document', { getElementById: () => null });
    try {
      const bridge = new BridgeCore();
      const received = collectHostModel(bridge);

      bridge.send('requestState', {});

      expect(received).toHaveLength(0);
      expect(emitEvent).toHaveBeenCalledWith('nativeEvent', { action: 'requestState' });
    } finally {
      vi.unstubAllGlobals();
    }
  });
});

// ─── C++ side: el id sale del contrato, no de un literal propio ───────────────
// El C++ anuncia `kHostModelId` (Source/Plugin/HostModelId.gen.h). Estos tests
// unen los dos extremos: contrato canonico -> header generado -> C++ sin repeticiones.
const here = path.dirname(fileURLToPath(import.meta.url));
// here = <proyecto>/WebUI/tests/unit → la raíz del proyecto son tres niveles arriba.
const headerPath = path.resolve(here, '../../../Source/Plugin/HostModelId.gen.h');
const announcementPath = path.resolve(here, '../../../Source/Plugin/HostModelAnnouncement.h');

/** Raíz del código C++ del proyecto (la comparten los dos guards de abajo). */
const sourceRoot = path.resolve(here, '../../../Source');

describe('HostModelId.gen.h — el C++ deriva el modelId del contrato canónico', () => {
  it('el header generado declara el modelId del contrato del host', () => {
    expect(fs.existsSync(headerPath)).toBe(true);
    const header = fs.readFileSync(headerPath, 'utf8');
    expect(header).toContain('AUTO-GENERATED');
    expect(header).toContain(`kHostModelId = "${korgAbdSm002Contract.modelId}"`);
    expect(header).toContain(`kHostModelDisplayName = "${korgAbdSm002Contract.displayName}"`);
    expect(header).toContain(`kHostModelManufacturer = "${korgAbdSm002Contract.manufacturer}"`);
  });

  it('el C++ del anuncio usa el header en vez del literal', () => {
    const announcement = fs.readFileSync(announcementPath, 'utf8');
    expect(announcement).toContain('#include "HostModelId.gen.h"');
    expect(announcement).toContain('juce::String(kHostModelId)');
    expect(announcement).not.toContain(korgAbdSm002Contract.modelId);
  });

  it('ningún otro fichero C++ del proyecto repite el modelId', () => {
    const offenders = listCppSources(sourceRoot)
      .filter(file => file !== headerPath)
      .filter(file => fs.readFileSync(file, 'utf8').includes(korgAbdSm002Contract.modelId))
      .map(file => path.relative(sourceRoot, file));

    expect(offenders).toEqual([]);
  });
});

// ─── C++ side: la dirección del canal nativo → JS ────────────────────────────
// La *lógica* (qué cuenta como violación, cómo se ignoran los comentarios, cómo se
// recorren los fuentes) vive en el helper compartido de la suite:
//   ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js
// Sus pruebas de comportamiento están en `webviewBridgeDirection.test.js`.
//
// Regresión real (handoff §17.1): `PluginEditor::emitEventToWebView()` emitía con
// `evaluateJavascript("window.__JUCE__.backend.emitEvent('event', …)")`, que es el
// canal *JS → nativo*: **todo** el `cppToWebui` se descartaba en silencio y el Bank
// Manager embebido abría con el catálogo vacío dentro del plugin real.
const editorPath = path.resolve(sourceRoot, 'Plugin/PluginEditor.cpp');

describe('canal nativo → JS del WebView2 — guard de dirección (JUCE 8)', () => {
  it('el emisor del editor usa emitEventIfBrowserIsVisible y sigue enchufado al bridge', () => {
    const editor = compactCpp(fs.readFileSync(editorPath, 'utf8'));

    expect(editor).toContain('webView_->emitEventIfBrowserIsVisible("event", message)');
    // El sumidero tiene que seguir cableado al emisor real: si se rompe, no sale
    // ningún mensaje cppToWebui del plugin.
    expect(editor).toContain(
      'bridge_->setJsMessageSink([this](const juce::var& message) { emitEventToWebView(message); })'
    );
  });

  it('ningún fichero C++ del proyecto llama a backend.emitEvent (iría a contramano)', () => {
    // Un `backend.emitEvent` escrito desde C++ no llega a ningún listener del
    // WebUI (JUCE lo enruta al nativo). Si algún host necesita simular un evento
    // de la página, el camino es `emitEventIfBrowserIsVisible` + un listener en
    // el JS, no este. La comprobación entera la hace el helper compartido.
    const result = checkBridgeDirection({
      sourceRoot,
      emitters: ['Plugin/PluginEditor.cpp']
    });

    expect(result.scanned).toBeGreaterThan(0);
    expect(result.problems).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('la dirección contraria (JS → nativo) sigue registrada y despachada', () => {
    const editor = compactCpp(fs.readFileSync(editorPath, 'utf8'));

    expect(editor).toContain('withEventListener("nativeEvent"');
    expect(editor).toContain('bridge_->handleJsEvent(msg)');
  });

  it('el WebUI sí usa backend.emitEvent: es SU dirección', () => {
    const bridgeCore = fs.readFileSync(path.resolve(here, '../../src/bridge/bridgeCore.js'), 'utf8');

    // El guard no es una prohibición indiscriminada: el JS emite a nativo con
    // `backend.emitEvent` y escucha al nativo con `backend.addEventListener`.
    expect(bridgeCore).toContain("window.__JUCE__.backend.emitEvent('nativeEvent'");
    expect(bridgeCore).toContain("window.__JUCE__.backend.addEventListener('event'");
  });

  it('el stripper de comentarios funciona (si no, el guard se dispararía solo)', () => {
    const raw = fs.readFileSync(editorPath, 'utf8');

    // El fichero documenta el fallo en un comentario; el guard debe ignorarlo…
    expect(raw).toContain('backend.emitEvent');
    expect(stripCppComments(raw)).not.toContain('backend.emitEvent');
  });
});
