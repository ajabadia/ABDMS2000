import { wasmBridge } from './bridgeWasm.js';
import { paramStore } from '../contracts/paramStore.js';
import { announcesHostModel, hostModelPayload, answersHostInfo, hostInfoPayload } from '../contracts/hostModel.js';
import { FACTORY_PROGRAMS } from '../contracts/factoryPresets.js';

const IS_DEV = import.meta.env?.DEV === true;
export const devLog = (...args) => {
  if (IS_DEV) console.log(...args);
};

function encodeAsciiName384(name = 'Init Synth') {
  const bytes = new Uint8Array(384);
  const clean = String(name || 'Init Synth').padEnd(12, ' ').slice(0, 12);
  for (let i = 0; i < 12; i++) {
    bytes[i] = clean.charCodeAt(i) & 0x7F;
  }
  return bytes;
}

function uint8ToBase64(bytes) {
  let binary = '';
  for (let i = 0; i < bytes.length; i++) {
    binary += String.fromCharCode(bytes[i] & 0xFF);
  }
  return btoa(binary);
}

function base64ToUint8(b64) {
  try {
    const bin = atob(String(b64 || ''));
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i++) out[i] = bin.charCodeAt(i) & 0xFF;
    return out;
  } catch {
    return new Uint8Array(0);
  }
}

function extractNameFromBytes(bytes, fallback = 'Init Synth') {
  if (!bytes || bytes.length < 12) return fallback;
  let s = '';
  for (let i = 0; i < 12; i++) {
    const c = bytes[i];
    s += (c >= 32 && c <= 126) ? String.fromCharCode(c) : ' ';
  }
  return s.trim() || fallback;
}

// IPC Bridge between WebUI and C++ / WASM

export class BridgeCore {
  constructor() {
    this.listeners = new Map();
    this.wasmBridge = wasmBridge;
    this._juceBound = false;
    this._activeSlot = 0;
    this._browserBank = null;
    this._initJuceBackend();
  }

  _ensureBrowserBank() {
    if (this._browserBank) return this._browserBank;
    this._browserBank = Array.from({ length: 128 }, (_, idx) => {
      const fp = FACTORY_PROGRAMS[idx];
      const name = fp?.name || `Init ${idx + 1}`;
      return {
        slot: idx,
        name,
        rawData: encodeAsciiName384(name),
      };
    });
    return this._browserBank;
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
      const ready = await this.wasmBridge.initAudio();
      if (ready && !this._hasSyncedInitialState) {
        this._hasSyncedInitialState = true;
        if (paramStore?.values) {
          for (const [paramId, val] of paramStore.values.entries()) {
            this.wasmBridge.setParam(paramId, val);
          }
        }
      }
      return ready;
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
        case 'setTempoBPM':
          this.wasmBridge.setTempoBPM(payload.bpm);
          break;
        case 'selectProgram':
          // Browser mode applies WebUI factoryPresets parameter maps directly in app.js.
          // Do not apply the separate native C++ factory index here.
          this._activeSlot = Math.max(0, Math.min(127, Number(payload.index) || 0));
          break;
        case 'hardware.listPorts':
          this.emitLocal('hardware.ports', { inputs: [], outputs: [] });
          break;
        case 'hardware.selectPorts':
          break;
        case 'getWavetableCatalog':
        case 'getPresetCatalog':
          break;
        case 'preset.read': {
          const bank = this._ensureBrowserBank();
          const slot = Math.max(0, Math.min(127, Number(payload.slot) || 0));
          const entry = bank[slot];
          this.emitLocal('preset.data', {
            requestId: payload.requestId,
            slot,
            name: entry.name,
            payload: uint8ToBase64(entry.rawData),
          });
          break;
        }
        case 'bank.read': {
          const bank = this._ensureBrowserBank();
          const slots = bank.map((entry, idx) => ({
            slot: idx,
            name: entry.name,
            payload: uint8ToBase64(entry.rawData),
          }));
          this.emitLocal('bank.data', {
            requestId: payload.requestId,
            slots,
          });
          break;
        }
        case 'preset.write': {
          const bank = this._ensureBrowserBank();
          const slot = Math.max(0, Math.min(127, Number(payload.slot) || 0));
          const raw = base64ToUint8(payload.payload);
          const name = (payload.name && String(payload.name).trim()) || extractNameFromBytes(raw, bank[slot]?.name || `Prog ${slot + 1}`);
          const padded = new Uint8Array(384);
          padded.set(raw.subarray(0, 384));
          const nameBytes = encodeAsciiName384(name);
          padded.set(nameBytes.subarray(0, 12), 0);
          if (!payload.audition) {
            bank[slot] = { slot, name, rawData: padded };
            this._activeSlot = slot;
            this.emitLocal('presetCatalog', {
              activeIndex: this._activeSlot,
              programs: bank.map((b, i) => ({ index: i, name: b.name })),
            });
          }
          this.emitLocal('preset.written', {
            requestId: payload.requestId,
            slot,
            name,
            audition: !!payload.audition,
          });
          break;
        }
        case 'bank.write': {
          const bank = this._ensureBrowserBank();
          const incoming = Array.isArray(payload.slots) ? payload.slots : [];
          let count = 0;
          for (const item of incoming) {
            const slot = Number(item?.slot ?? -1);
            if (slot < 0 || slot >= 128) continue;
            const raw = base64ToUint8(item?.payload);
            const name = (item?.name && String(item.name).trim()) || extractNameFromBytes(raw, bank[slot]?.name || `Prog ${slot + 1}`);
            const padded = new Uint8Array(384);
            padded.set(raw.subarray(0, 384));
            const nameBytes = encodeAsciiName384(name);
            padded.set(nameBytes.subarray(0, 12), 0);
            bank[slot] = { slot, name, rawData: padded };
            count++;
          }
          this.emitLocal('presetCatalog', {
            activeIndex: this._activeSlot,
            programs: bank.map((b, i) => ({ index: i, name: b.name })),
          });
          this.emitLocal('bank.written', {
            requestId: payload.requestId,
            count,
          });
          break;
        }
        case 'preset.capture': {
          const bank = this._ensureBrowserBank();
          const slot = payload.slot !== undefined && payload.slot !== null
            ? Math.max(0, Math.min(127, Number(payload.slot) || 0))
            : this._activeSlot;
          const entry = bank[slot] || { slot, name: 'Captured', rawData: encodeAsciiName384('Captured') };
          this.emitLocal('preset.captured', {
            requestId: payload.requestId,
            slot,
            name: entry.name,
            payload: uint8ToBase64(entry.rawData),
          });
          break;
        }
        case 'writeProgramToSlot':
          this.emitLocal('programWritten', { index: payload.index ?? 0, name: payload.name ?? '' });
          break;
        case 'exportSysexProgram':
        case 'beginGesture':
        case 'endGesture':
        case 'checkForUpdates':
        case 'getUpdateStatus':
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

  beginGesture(paramId) {
    if (!paramId) return;
    this.send('beginGesture', { paramId });
  }

  endGesture(paramId) {
    if (!paramId) return;
    this.send('endGesture', { paramId });
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

  setTempoBPM(bpm) {
    this.send('setTempoBPM', { bpm });
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
        const timeoutId = setTimeout(() => {
          this.off('rawProgramDataResponse', handler);
          resolve(null);
        }, 3000);
        const handler = (data) => {
          clearTimeout(timeoutId);
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
    return () => this.off(event, callback);
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
if (typeof window !== 'undefined' && !window.__synthBridge) {
  window.__synthBridge = bridge;
}
