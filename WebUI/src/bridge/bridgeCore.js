import { wasmBridge } from './bridgeWasm.js';
import { paramStore } from '../contracts/paramStore.js';
import { announcesHostModel, hostModelPayload, answersHostInfo, hostInfoPayload } from '../contracts/hostModel.js';

const IS_DEV = import.meta.env?.DEV === true;
const devLog = (...args) => {
  if (IS_DEV) console.log(...args);
};

// IPC Bridge between WebUI and C++ / WASM

export class BridgeCore {
  constructor() {
    this.listeners = new Map();
    this.wasmBridge = wasmBridge;
    this._juceBound = false;
    this._initJuceBackend();
  }

  get isWebView() {
    return typeof window !== 'undefined' && window.__JUCE__ !== undefined && window.__JUCE__.backend !== undefined;
  }

  _initJuceBackend() {
    if (typeof window === 'undefined') return;
    const tryBind = () => {
      if (this.isWebView && !this._juceBound) {
        this._juceBound = true;
        const btn = document.getElementById('btn-audio-init');
        if (btn) btn.style.display = 'none';
        window.__JUCE__.backend.addEventListener('event', (data) => {
          this.emitLocal(data.type, data.data);
        });
        devLog('[BridgeCore]: Connected to JUCE WebView backend');
        this.send('requestFullState', {});
      }
    };
    tryBind();
    if (!this._juceBound) {
      window.addEventListener('DOMContentLoaded', tryBind);
      setTimeout(tryBind, 50);
      setTimeout(tryBind, 200);
      setTimeout(tryBind, 800);
    }
  }

  async initWebAudio() {
    if (!this.isWebView) {
      return await this.wasmBridge.initAudio();
    }
    return true;
  }

  send(action, payload = {}) {
    const msg = { action, ...payload };
    if (this.isWebView) {
      window.__JUCE__.backend.emitEvent('nativeEvent', msg);
    } else if (announcesHostModel(action)) {
      // Dev server / build WASM: no hay C++ que conteste, pero el Bank Manager
      // embebido necesita igualmente la identidad del host. Se responde con el
      // mismo mensaje `hostModel` y el mismo modelId que el lado nativo
      // (fuente única: el ModelContract del host, ver contracts/hostModel.js).
      this.emitLocal('hostModel', hostModelPayload());
    } else if (answersHostInfo(action)) {
      // Ficha del host: identidad + sello + nivel de puente. Misma política que
      // el C++ (`ABDMS2000::actionAnswersHostInfo`), para que el Bank Manager
      // embebido no vea al dev server como un host mudo.
      this.emitLocal('hostInfo', hostInfoPayload());
    } else {
      // Forward to WASM Worklet Bridge
      switch (action) {
        case 'setParam':
          this.wasmBridge.setParam(payload.paramId, payload.value);
          break;
        case 'noteOn':
          this.wasmBridge.noteOn(payload.note, payload.velocity);
          break;
        case 'noteOff':
          this.wasmBridge.noteOff(payload.note);
          break;
        case 'allNotesOff':
          this.wasmBridge.allNotesOff();
          break;
        case 'pitchBend':
          this.wasmBridge.pitchBend(payload.value);
          break;
        case 'modWheel':
          this.wasmBridge.modWheel(payload.value);
          break;
        case 'setDiagnosticTone':
          this.wasmBridge.setDiagnosticTone(payload.point, payload.frequency, payload.level);
          break;
        case 'triggerDiagnosticNote':
          this.wasmBridge.triggerDiagnosticNote(payload.note, payload.velocity, payload.isNoteOn);
          break;
        case 'setDiagnosticBypass':
          this.wasmBridge.setDiagnosticBypass(payload.stage, payload.enabled);
          break;
        case 'resetDiagnosticBypasses':
          this.wasmBridge.resetDiagnosticBypasses();
          break;
        case 'selectProgram':
          // In the browser, LcdProgrammer already sends the complete UI patch
          // through setAllParams/setParam. Do not apply the separate native
          // factory-bank index as well: it is only a partial mapping and would
          // overwrite the patch with a different sound. Native JUCE handles
          // selectProgram through its own host bridge.
          break;
        case 'hardware.listPorts':
          // Physical MIDI is only available through the native JUCE host.
          // Keep the browser Bank Manager deterministic instead of routing this
          // host-only action into the WASM command switch.
          this.emitLocal('hardware.ports', { inputs: [], outputs: [] });
          break;
        case 'hardware.selectPorts':
          // Selection is persisted by the browser UI; there is no hardware host
          // to open while running under Vite.
          break;
        case 'getWavetableCatalog':
          // The browser catalog has a local fallback; native hosts answer this.
          break;
        default:
          // Unknown action — log for debugging
          if (IS_DEV) console.warn('[BridgeCore] Unknown WASM action:', action, payload);
      }
    }
  }

  setParam(paramId, value) {
    paramStore.set(paramId, value);
    this.send('setParam', { paramId, value });
  }

  noteOn(note, velocity = 0.8) {
    this.send('noteOn', { note, velocity });
  }

  noteOff(note, velocity = 0.0) {
    this.send('noteOff', { note, velocity });
  }

  allNotesOff() {
    this.send('allNotesOff');
  }

  setDiagnosticTone(point, frequency = 440.0, level = 0.25) {
    this.send('setDiagnosticTone', { point, frequency, level });
  }

  triggerDiagnosticNote(note, velocity = 0.8, isNoteOn = true) {
    this.send('triggerDiagnosticNote', { note, velocity, isNoteOn });
  }

  setDiagnosticBypass(stage, enabled = false) {
    this.send('setDiagnosticBypass', { stage, enabled });
  }

  resetDiagnosticBypasses() {
    this.send('resetDiagnosticBypasses', {});
  }

  pitchBend(value) {
    this.send('pitchBend', { value });
  }

  modWheel(value) {
    this.send('modWheel', { value });
  }

  sendMidiCC(cc, value) {
    if (this.isWebView) {
      window.__JUCE__.backend.emitEvent('nativeEvent', { action: 'sendMidiCC', cc, value });
    } else {
      this.wasmBridge.midiCC(cc, value);
    }
  }

  // --- Bank Manager & Patch Data Bridge ---

  async getRawProgramData(name = 'Active Patch') {
    if (this.isWebView) {
      return new Promise((resolve) => {
        const handler = (data) => {
          this.off('rawProgramDataResponse', handler);
          if (data && data.dataBase64) {
            const binStr = atob(data.dataBase64);
            const bytes = new Uint8Array(binStr.length);
            for (let i = 0; i < binStr.length; i++) bytes[i] = binStr.charCodeAt(i);
            resolve({ rawData: bytes, name: data.name });
          } else {
            resolve(null);
          }
        };
        this.on('rawProgramDataResponse', handler);
        this.send('getRawProgramData', { name });
      });
    } else {
      // WASM / Local fallback
      return null;
    }
  }

  setRawProgramData(uint8ArrayData) {
    let binary = '';
    for (let i = 0; i < uint8ArrayData.length; i++) {
      binary += String.fromCharCode(uint8ArrayData[i]);
    }
    const dataBase64 = btoa(binary);
    this.send('setRawProgramData', { dataBase64 });
  }

  on(event, callback) {
    if (!this.listeners.has(event)) {
      this.listeners.set(event, new Set());
    }
    this.listeners.get(event).add(callback);
  }

  off(event, callback) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).delete(callback);
    }
  }

  emitLocal(event, data) {
    if (this.listeners.has(event)) {
      this.listeners.get(event).forEach(cb => cb(data));
    }
  }
}

export const bridge = new BridgeCore();
