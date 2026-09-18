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
  { id: 'master',   name: 'Master Out' },
  { id: 'pre_fx',   name: 'Pre FX' },
  { id: 'osc_mix',  name: 'Osc Mix' },
  { id: 'post_filter', name: 'Post Filter' },
  { id: 'post_vca', name: 'Post VCA' },
  { id: 'lfo1',     name: 'LFO 1' },
];

export class OscilloscopeModal {
  constructor(bridge) {
    this.bridge = bridge;
    this.isOpen = false;
    this.scope = null;
    this.analyser = null;
    this._audioCtx = null;
  }

  _ensureScope() {
    if (this.scope) return this.scope;

    const wasm = this.bridge?.wasmBridge;
    this._audioCtx = wasm?.audioContext || null;
    const master = wasm?.masterGain || null;

    this.scope = createScope({
      mountMode: 'floating',
      title: 'OSCILLOSCOPE & SPECTRUM',
      maxLanes: 2,
      layout: 'single',
      enabledModes: ['oscilloscope', 'spectrum', 'lissajous', 'phase', 'spectrogram'],
      availableTaps: SCOPE_TAPS,
      defaultMode: 'oscilloscope',
      showFreeze: true,
      showSnapshot: true,
    });

    // Tap the real master bus so the scope shows live audio.
    if (this._audioCtx && master) {
      this.analyser = this._audioCtx.createAnalyser();
      this.analyser.fftSize = 2048;
      master.connect(this.analyser);
      this.scope.connectAnalyser(this.analyser, { sampleRate: this._audioCtx.sampleRate });
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
