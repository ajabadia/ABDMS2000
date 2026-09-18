import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { afterEach, describe, expect, it, vi } from 'vitest';
import { BridgeWasm } from '../../src/bridge/bridgeWasm.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const webUiRoot = path.resolve(here, '../..');
const bridgeSource = fs.readFileSync(path.join(webUiRoot, 'src/bridge/bridgeWasm.js'), 'utf8');
const workletSource = fs.readFileSync(path.join(webUiRoot, 'src/wasm/ms2000Worklet.js'), 'utf8');
const diagnosticSource = fs.readFileSync(path.join(webUiRoot, 'src/ui/diagnosticModal.js'), 'utf8');

function installAudioFakes(messages) {
  const postedMessages = [];

  class FakePort {
    onmessage = null;

    postMessage(message) {
      postedMessages.push(message);
      if (message.type === 'INIT_WASM') {
        queueMicrotask(() => {
          this.onmessage?.({ data: messages.shift() || { type: 'WASM_READY' } });
        });
      }
    }
  }

  class FakeAudioContext {
    state = 'running';
    sampleRate = 48000;
    destination = {};
    audioWorklet = { addModule: vi.fn(async () => {}) };

    async resume() {}
    async close() {}

    createGain() {
      return {
        gain: { value: 0 },
        connect: vi.fn(),
        disconnect: vi.fn(),
      };
    }
  }

  class FakeAudioWorkletNode {
    constructor() {
      this.port = new FakePort();
      this.disconnect = vi.fn();
    }

    connect() {}
  }

  vi.stubGlobal('window', { AudioContext: FakeAudioContext });
  vi.stubGlobal('AudioWorkletNode', FakeAudioWorkletNode);
  vi.stubGlobal('fetch', vi.fn(async () => ({
    ok: true,
    arrayBuffer: async () => new ArrayBuffer(8),
  })));

  return { postedMessages };
}

afterEach(() => {
  vi.unstubAllGlobals();
  vi.restoreAllMocks();
});

describe('WASM raw AudioWorklet bootstrap', () => {
  it('ejecuta los constructores globales de Emscripten antes de initEngine', () => {
    expect(workletSource).toContain('this.wasm.__wasm_call_ctors');
    expect(workletSource.indexOf('this.wasm.__wasm_call_ctors'))
      .toBeLessThan(workletSource.indexOf('this.wasm.initEngine(sr)'));
  });

  it('solo marca el bridge como inicializado después de WASM_READY', async () => {
    const fakes = installAudioFakes([{ type: 'WASM_READY' }]);
    const bridge = new BridgeWasm();

    const result = await bridge.initAudio();

    expect(result).toBe(true);
    expect(bridge.isInitialized).toBe(true);
    expect(bridge.audioContext).not.toBeNull();
  });

  it('conserva una nota enviada durante el arranque y la entrega tras WASM_READY', async () => {
    const fakes = installAudioFakes([{ type: 'WASM_READY' }]);
    const bridge = new BridgeWasm();

    const init = bridge.initAudio();
    bridge.noteOn(60, 0.75);
    bridge.noteOff(60);
    await expect(init).resolves.toBe(true);

    expect(fakes.postedMessages.map((message) => message.type)).toEqual([
      'INIT_WASM',
      'NOTE_ON',
      'NOTE_OFF',
    ]);
  });

  it('inicia el audio y conserva el tono diagnóstico si se activa antes de WASM_READY', async () => {
    const fakes = installAudioFakes([{ type: 'WASM_READY' }]);
    const bridge = new BridgeWasm();

    bridge.setDiagnosticTone(1, 440, 0.2);
    await expect(bridge._initPromise).resolves.toBe(true);

    expect(fakes.postedMessages.map((message) => message.type)).toEqual([
      'INIT_WASM',
      'DIAGNOSTIC_TONE',
    ]);
    expect(fakes.postedMessages[1]).toMatchObject({
      point: 1,
      frequency: 440,
      level: 0.2,
    });
  });

  it('limpia el pipeline tras WASM_ERROR y permite reintentar', async () => {
    installAudioFakes([{ type: 'WASM_ERROR', error: 'constructor failure' }]);
    const bridge = new BridgeWasm();

    await expect(bridge.initAudio()).resolves.toBe(false);
    expect(bridge.isInitialized).toBe(false);
    expect(bridge.audioContext).toBeNull();

    // El siguiente intento simula que el worklet ya arranca correctamente.
    installAudioFakes([{ type: 'WASM_READY' }]);
    await expect(bridge.initAudio()).resolves.toBe(true);
    expect(bridge.isInitialized).toBe(true);
  });

  it('evita dos inicializaciones concurrentes del AudioContext', async () => {
    installAudioFakes([{ type: 'WASM_READY' }]);
    const bridge = new BridgeWasm();

    const first = bridge.initAudio();
    const second = bridge.initAudio();
    expect(await first).toBe(true);
    expect(await second).toBe(true);
  });

  it('no mezcla la carga de patches web con el índice de fábrica nativo', async () => {
    const coreSource = await fs.promises.readFile(
      path.resolve(process.cwd(), 'WebUI/src/bridge/bridgeCore.js'),
      'utf8'
    );
    expect(coreSource).toContain('Do not apply the separate native');
    expect(coreSource).not.toContain('this.wasmBridge.loadProgram(payload.index');
  });

  it('confirma que el primer bloque de audio procede del WASM', () => {
    expect(workletSource).toContain("type: 'WASM_RENDERING'");
    expect(workletSource).toContain("renderer: 'raw-wasm-cpp'");
    expect(bridgeSource).toContain("data.type === 'WASM_RENDERING'");
  });

  it('no expone el diagnóstico WASM en el bundle de producción', () => {
    expect(diagnosticSource).toContain('const IS_DEV = import.meta.env?.DEV === true');
    expect(diagnosticSource).toContain('export const SHOW_DEV_TEST_MODE = IS_DEV');

    const bundlePath = path.join(webUiRoot, 'dist/assets/index.js');
    const htmlPath = path.join(webUiRoot, 'dist/index.html');
    expect(fs.existsSync(bundlePath)).toBe(true);
    expect(fs.existsSync(htmlPath)).toBe(true);
    const productionBundle = fs.readFileSync(bundlePath, 'utf8');
    const productionHtml = fs.readFileSync(htmlPath, 'utf8');

    // Production must neither expose the control in HTML nor include dev
    // diagnostic text in the executable's JavaScript bundle.
    expect(productionHtml).not.toContain('btn-diagnostic-test');
    expect(productionHtml).not.toContain('diagnostic-modal');
    expect(productionBundle).toContain('const tt=!1');
    expect(productionBundle).not.toContain('Diagnostic Mode');
  });

  it('conserva el contrato de mensajes INIT_WASM y WASM_ERROR', () => {
    expect(bridgeSource).toContain("type: 'INIT_WASM'");
    expect(bridgeSource).toContain("type: 'DIAGNOSTIC_TONE'");
    expect(bridgeSource).toContain("data.type === 'WASM_READY'");
    expect(bridgeSource).toContain("data.type === 'WASM_ERROR'");
    expect(workletSource).toContain("case 'DIAGNOSTIC_TONE'");
    expect(workletSource).toContain('diagnosticLevel');
    expect(workletSource).toContain('outL[i] += sample');
    expect(bridgeSource).toContain('outputChannelCount: [2]');
    expect(workletSource).toContain('const outR = output[1] || output[0]');
  });
});
