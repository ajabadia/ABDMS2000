/**
 * OscilloscopeModal.js
 * Real-time waveform & spectrum scope for ABDMS2000 (browser build).
 *
 * Uses ABDScope (single source of truth, copied into WebUI/abdscope by
 * Scripts/sync_scope.js) as a floating multi-lane mount, fed from the real
 * WASM audio telemetry and the native-compatible scope bridge. This replaces
 * the previous synthetic/fake scope.
 */

import { createScope } from '../../abdscope/src/scope.js';

const SCOPE_TAPS = [
  { id: 'master_out',  name: 'Master Out' },
  { id: 'pre_fx',      name: 'Pre FX' },
  { id: 'osc_mix',     name: 'Osc Mix' },
  { id: 'post_filter', name: 'Post Filter' },
  { id: 'post_vca',    name: 'Post VCA' },
  { id: 'lfo1',        name: 'LFO 1' },
];

export class OscilloscopeModal {
  constructor(bridgeOrOpts) {
    this.bridge = bridgeOrOpts?.bridge || bridgeOrOpts;
    this.isOpen = false;
    this.scope = null;
    this.analyser = null;
    this._audioCtx = null;
    this._unsubscribeScopeFrame = null;
    this._wasmPollTimer = null;

    if (this.bridge && typeof this.bridge.on === 'function') {
      this._unsubscribeScopeFrame = this.bridge.on('scopeFrame', (frame) => {
        if (!frame) return;
        const scope = this._ensureScope();
        if (scope && this.isOpen) {
          scope.pushFrame(frame);
        }
      });
    }
  }

  _tryConnectWebAudio() {
    if (this.analyser || !this.scope) return true;
    const wasm = this.bridge?.wasmBridge;
    const ctx = wasm?.audioContext || null;
    const master = wasm?.masterGain || null;

    if (ctx && master) {
      this._audioCtx = ctx;
      this.analyser = ctx.createAnalyser();
      this.analyser.fftSize = 2048;
      master.connect(this.analyser);
      this.scope.connectAnalyser(this.analyser, { sampleRate: ctx.sampleRate });
      if (this._wasmPollTimer) {
        clearInterval(this._wasmPollTimer);
        this._wasmPollTimer = null;
      }
      return true;
    }
    return false;
  }

  _ensureScope() {
    if (!this.scope) {
      this.scope = createScope({
        mountMode: 'floating',
        title: 'OSCILLOSCOPE & SPECTRUM',
        maxLanes: 3,
        layout: 'single',
        enabledModes: ['oscilloscope', 'spectrum', 'lissajous', 'phase', 'spectrogram'],
        availableTaps: SCOPE_TAPS,
        defaultMode: 'oscilloscope',
        showFreeze: true,
        showSnapshot: true,
        showVuMeters: true,
      });
    }

    if (!this.bridge?.isWebView) {
      if (!this._tryConnectWebAudio() && !this._wasmPollTimer) {
        this._wasmPollTimer = setInterval(() => {
          this._tryConnectWebAudio();
        }, 400);
      }
    }

    return this.scope;
  }

  open() {
    const scope = this._ensureScope();
    this.isOpen = true;
    scope.open();
  }

  close() {
    if (!this.scope) return;
    this.isOpen = false;
    this.scope.close();
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }

  destroy() {
    if (this._wasmPollTimer) {
      clearInterval(this._wasmPollTimer);
      this._wasmPollTimer = null;
    }
    if (typeof this._unsubscribeScopeFrame === 'function') {
      this._unsubscribeScopeFrame();
      this._unsubscribeScopeFrame = null;
    }
    if (this.analyser && this._audioCtx && this.bridge?.wasmBridge?.masterGain) {
      try {
        this.bridge.wasmBridge.masterGain.disconnect(this.analyser);
      } catch (_) { /* already disconnected */ }
    }
    if (this.scope) {
      this.scope.destroy();
      this.scope = null;
    }
    this.analyser = null;
    this.isOpen = false;
  }
}
