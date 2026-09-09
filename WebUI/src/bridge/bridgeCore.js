import { wasmBridge } from './bridgeWasm.js';
import { paramStore } from '../contracts/paramStore.js';

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
        console.log('[BridgeCore]: Connected to JUCE WebView backend');
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
        default:
          // Unknown action — log for debugging
          console.warn('[BridgeCore] Unknown WASM action:', action, payload);
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
