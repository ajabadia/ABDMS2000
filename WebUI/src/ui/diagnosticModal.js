/**
 * Diagnostic Test Mode & DSP Signal Chain Injector Controller.
 * Allows live routing of a 440 Hz test tone at 5 strategic points in the DSP chain
 * and direct DSP note triggering to debug audio silencing.
 */

import { bridge } from '../bridge/bridgeCore.js';
// 2026-09-27: el CONTRATO DE FOCO de la familia (overlayFocus.js, el mismo del
// cajon compartido): al cerrar el modal va `inert` + `aria-hidden`, al abrir
// el foco entra al primer control y al cerrar vuelve al disparador.
import { createOverlayFocus } from '@abdsynths/shared/components';

// Test Mode & DSP Signal Chain Injector is always enabled so users can test
// 440 Hz tone injection and inspect live signal movement across DSP stages.
const IS_DEV = import.meta.env?.DEV === true;
export const SHOW_DEV_TEST_MODE = true;

const POINT_TO_DEFAULT_TAP = {
  1: 'master_out',
  2: 'master_out',
  3: 'pre_fx',
  4: 'post_filter',
  5: 'osc_mix',
};

const TAP_LABELS = {
  master_out: 'MASTER OUTPUT (POST-MASTER)',
  pre_fx: 'PRE-FX BUS (POST-VOICES)',
  post_vca: 'VOICE 1 POST-VCA',
  post_filter: 'VOICE 1 POST-FILTER (VCF)',
  osc_mix: 'VOICE 1 OSC MIXER OUT',
  lfo1: 'VOICE 1 LFO 1',
};

class DiagnosticModalController {
  constructor() {
    this.modal = null;
    this.btnNav = null;
    this.focusContract = null;
    this.isToneActive = false;
    this.currentPoint = 2; // Default to Pre-Master Volume
    this.frequency = 440;
    this.volume = 25; // %
    this.latchMode = false;
    this.activeLatchedNotes = new Set();
    this.selectedTap = 'master_out';
    this.latestTaps = {};
    this._rafId = null;
    this._analyser = null;
    this._analyserBuf = null;
  }

  init() {
    this.modal = document.getElementById('diagnostic-modal');
    this.btnNav = document.getElementById('btn-diagnostic-test');

    if (!SHOW_DEV_TEST_MODE) {
      if (this.btnNav) this.btnNav.style.display = 'none';
      if (this.modal) this.modal.style.display = 'none';
      return;
    }

    // Cerrado (su estado de arranque): inerte. El Escape propio NO pisa al del
    // host: si el host tambien cierra, este cierre ya esta hecho y es no-op.
    this.focusContract = createOverlayFocus({
      root: this.modal,
      isClosed: () => this.modal.classList.contains('hidden'),
      onEscape: () => this.close(),
    });
    this.focusContract.setInert();
    this.focusContract.attach();

    if (this.btnNav) {
      this.btnNav.addEventListener('click', () => this.open());
    }

    // Close buttons
    document.getElementById('diag-close-btn')?.addEventListener('click', () => this.close());
    document.getElementById('diag-close-top-btn')?.addEventListener('click', () => this.close());
    this.modal?.addEventListener('click', (e) => {
      if (e.target === this.modal) this.close();
    });

    // Scope Tap Selector & clickable stage boxes
    const tapSelect = document.getElementById('diag-scope-tap-select');
    tapSelect?.addEventListener('change', (e) => {
      this.selectedTap = e.target.value || 'master_out';
      const lbl = document.getElementById('diag-scope-tap-label');
      if (lbl) lbl.textContent = `TAP: ${TAP_LABELS[this.selectedTap] || this.selectedTap}`;
    });

    document.querySelectorAll('#diag-stage-meters .diag-stage-box').forEach((box) => {
      box.style.cursor = 'pointer';
      box.title = 'Click to view this DSP stage on the oscilloscope';
      box.addEventListener('click', () => {
        const tap = box.getAttribute('data-tap');
        if (tap) {
          this.selectedTap = tap;
          if (tapSelect) tapSelect.value = tap;
          const lbl = document.getElementById('diag-scope-tap-label');
          if (lbl) lbl.textContent = `TAP: ${TAP_LABELS[tap] || tap}`;
        }
      });
    });

    // Subscribe to C++ multi-tap scope frames
    if (bridge && typeof bridge.on === 'function') {
      bridge.on('scopeFrame', (packet) => {
        if (packet && packet.taps) {
          this.latestTaps = packet.taps;
          if (!this.modal?.classList.contains('hidden')) {
            this._updateStageMeters();
          }
        }
      });
    }

    // Tone Enable Toggle
    const toggle = document.getElementById('diag-tone-enable');
    const statusLabel = document.getElementById('diag-tone-status');
    toggle?.addEventListener('change', (e) => {
      bridge.initWebAudio();
      this.isToneActive = e.target.checked;
      if (statusLabel) {
        statusLabel.textContent = this.isToneActive ? 'TONE ACTIVE (TRANSMITTING)' : 'TONE DISABLED';
        statusLabel.classList.toggle('active', this.isToneActive);
      }
      this.updateTone();
    });

    // Frequency Slider
    const freqSlider = document.getElementById('diag-freq-slider');
    const freqVal = document.getElementById('diag-freq-val');
    freqSlider?.addEventListener('input', (e) => {
      this.frequency = Number(e.target.value);
      if (freqVal) freqVal.textContent = this.frequency;
      if (this.isToneActive) this.updateTone();
    });

    // Volume Slider
    const volSlider = document.getElementById('diag-vol-slider');
    const volVal = document.getElementById('diag-vol-val');
    volSlider?.addEventListener('input', (e) => {
      this.volume = Number(e.target.value);
      if (volVal) volVal.textContent = this.volume;
      if (this.isToneActive) this.updateTone();
    });

    // Injection Point Radio Buttons
    const radioPoints = document.querySelectorAll('input[name="diag-point"]');
    radioPoints.forEach((radio) => {
      radio.addEventListener('change', (e) => {
        if (e.target.checked) {
          this.currentPoint = Number(e.target.value);
          const autoTap = POINT_TO_DEFAULT_TAP[this.currentPoint];
          if (autoTap) {
            this.selectedTap = autoTap;
            if (tapSelect) tapSelect.value = autoTap;
            const lbl = document.getElementById('diag-scope-tap-label');
            if (lbl) lbl.textContent = `TAP: ${TAP_LABELS[autoTap] || autoTap}`;
          }
          if (this.isToneActive) this.updateTone();
        }
      });
    });

    // Note Latch Checkbox
    const latchCheck = document.getElementById('diag-note-latch');
    latchCheck?.addEventListener('change', (e) => {
      this.latchMode = e.target.checked;
      if (!this.latchMode) {
        this.panic();
      }
    });

    // Direct Note Trigger Buttons
    const noteButtons = document.querySelectorAll('.diag-note-btn');
    noteButtons.forEach((btn) => {
      const note = Number(btn.getAttribute('data-note') || 60);

      // Momentary Pointer Events
      const handleNoteStart = (e) => {
        e.preventDefault();
        bridge.initWebAudio();
        if (this.latchMode) {
          if (this.activeLatchedNotes.has(note)) {
            this.activeLatchedNotes.delete(note);
            btn.classList.remove('active');
            bridge.triggerDiagnosticNote(note, 0.0, false);
          } else {
            this.activeLatchedNotes.add(note);
            btn.classList.add('active');
            bridge.triggerDiagnosticNote(note, 0.85, true);
          }
        } else {
          btn.classList.add('active');
          bridge.triggerDiagnosticNote(note, 0.85, true);
        }
      };

      const handleNoteEnd = (e) => {
        if (this.latchMode) return;
        e.preventDefault();
        btn.classList.remove('active');
        bridge.triggerDiagnosticNote(note, 0.0, false);
      };

      btn.addEventListener('pointerdown', handleNoteStart);
      btn.addEventListener('pointerup', handleNoteEnd);
      btn.addEventListener('pointerleave', handleNoteEnd);
      btn.addEventListener('pointercancel', handleNoteEnd);
    });

    // Panic Button
    document.getElementById('diag-btn-panic')?.addEventListener('click', () => {
      this.panic();
    });

    // ── 3. Bypass Checkbox Wiring ──
    const bypassMap = [
      { id: 'diag-bypass-filter', stage: 'filter' },
      { id: 'diag-bypass-vca', stage: 'vca' },
      { id: 'diag-bypass-mixer', stage: 'mixer' },
      { id: 'diag-bypass-distortion', stage: 'distortion' },
      { id: 'diag-bypass-modfx', stage: 'modfx' },
      { id: 'diag-bypass-delayfx', stage: 'delayfx' },
      { id: 'diag-bypass-eq', stage: 'eq' }
    ];

    bypassMap.forEach(({ id, stage }) => {
      const el = document.getElementById(id);
      el?.addEventListener('change', (e) => {
        const enabled = e.target.checked;
        bridge.setDiagnosticBypass(stage, enabled);
        if (IS_DEV) console.log(`[Diagnostic Mode] Bypass ${stage}: ${enabled}`);
      });
    });

    // Reset All Bypasses Button
    document.getElementById('diag-btn-reset-bypasses')?.addEventListener('click', () => {
      bypassMap.forEach(({ id }) => {
        const el = document.getElementById(id);
        if (el) el.checked = false;
      });
      bridge.resetDiagnosticBypasses();
      if (IS_DEV) console.log('[Diagnostic Mode] All bypasses reset to normal operation');
    });
  }

  _updateStageMeters() {
    const stages = ['osc_mix', 'post_filter', 'post_vca', 'pre_fx', 'master_out'];
    stages.forEach((tapKey) => {
      const tapData = this.latestTaps?.[tapKey];
      const peak = tapData ? Math.max(tapData.peakL || 0, tapData.peakR || 0) : 0;
      const pct = Math.min(100, Math.round(peak * 100));
      const bar = document.getElementById(`diag-bar-${tapKey}`);
      const lbl = document.getElementById(`diag-lvl-${tapKey}`);
      if (bar) bar.style.width = `${pct}%`;
      if (lbl) lbl.textContent = `${pct}%`;
    });
  }

  _ensureWebAudioAnalyser() {
    if (this._analyser) return this._analyser;
    const wasm = bridge?.wasmBridge;
    const ctx = wasm?.audioContext;
    const master = wasm?.masterGain;
    if (ctx && master) {
      this._analyser = ctx.createAnalyser();
      this._analyser.fftSize = 1024;
      this._analyserBuf = new Float32Array(512);
      master.connect(this._analyser);
    }
    return this._analyser;
  }

  _startScopeLoop() {
    if (this._rafId) return;
    const render = () => {
      if (!this.modal || this.modal.classList.contains('hidden')) {
        this._rafId = null;
        return;
      }
      this._drawScopeCanvas();
      this._rafId = requestAnimationFrame(render);
    };
    this._rafId = requestAnimationFrame(render);
  }

  _stopScopeLoop() {
    if (this._rafId) {
      cancelAnimationFrame(this._rafId);
      this._rafId = null;
    }
  }

  _drawScopeCanvas() {
    const canvas = document.getElementById('diag-scope-canvas');
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const w = canvas.width;
    const h = canvas.height;
    ctx.fillStyle = '#050811';
    ctx.fillRect(0, 0, w, h);

    // Center line & grid
    ctx.strokeStyle = 'rgba(148, 163, 184, 0.14)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, h / 2);
    ctx.lineTo(w, h / 2);
    ctx.moveTo(0, h * 0.25);
    ctx.lineTo(w, h * 0.25);
    ctx.moveTo(0, h * 0.75);
    ctx.lineTo(w, h * 0.75);
    ctx.stroke();

    let samples = null;
    let peak = 0;
    let rms = 0;
    let freqHz = 0;

    const tapPacket = this.latestTaps?.[this.selectedTap] || this.latestTaps?.master_out;
    if (tapPacket && Array.isArray(tapPacket.timeDataL) && tapPacket.timeDataL.length > 0) {
      samples = tapPacket.timeDataL;
      peak = Math.max(tapPacket.peakL || 0, tapPacket.peakR || 0);
      rms = Math.max(tapPacket.rmsL || 0, tapPacket.rmsR || 0);
      freqHz = tapPacket.estimatedFrequencyHz || 0;
    } else if (!bridge?.isWebView) {
      const analyser = this._ensureWebAudioAnalyser();
      if (analyser && this._analyserBuf) {
        analyser.getFloatTimeDomainData(this._analyserBuf);
        samples = this._analyserBuf;
        let sumSq = 0;
        for (let i = 0; i < samples.length; i++) {
          const a = Math.abs(samples[i]);
          if (a > peak) peak = a;
          sumSq += samples[i] * samples[i];
        }
        rms = Math.sqrt(sumSq / samples.length);
        const pct = Math.min(100, Math.round(peak * 100));
        const bar = document.getElementById('diag-bar-master_out');
        const lbl = document.getElementById('diag-lvl-master_out');
        if (bar) bar.style.width = `${pct}%`;
        if (lbl) lbl.textContent = `${pct}%`;
      }
    }

    const metricsEl = document.getElementById('diag-scope-metrics');
    if (metricsEl) {
      const freqStr = freqHz > 1 ? `${freqHz.toFixed(1)} Hz` : (this.isToneActive ? `${this.frequency} Hz` : '--- Hz');
      metricsEl.textContent = `PEAK: ${peak.toFixed(3)} | RMS: ${rms.toFixed(3)} | FREQ: ${freqStr}`;
    }

    if (samples && samples.length > 1) {
      // Find rising zero-crossing trigger for stable display
      let trig = 0;
      const maxSearch = Math.floor(samples.length / 2);
      for (let i = 1; i < maxSearch; i++) {
        if (samples[i - 1] <= 0 && samples[i] > 0) {
          trig = i;
          break;
        }
      }
      const drawLen = Math.min(256, samples.length - trig);
      ctx.strokeStyle = peak > 0.002 ? '#38bdf8' : '#475569';
      ctx.lineWidth = 2;
      ctx.beginPath();
      for (let i = 0; i < drawLen; i++) {
        const x = (i / (drawLen - 1)) * w;
        const s = Math.max(-1, Math.min(1, samples[trig + i]));
        const y = (h / 2) - s * (h * 0.42);
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      }
      ctx.stroke();
    }
  }

  updateTone() {
    const point = this.isToneActive ? this.currentPoint : 0;
    const level = (this.volume / 100.0) * 0.45;
    bridge.setDiagnosticTone(point, this.frequency, level);
    if (IS_DEV) {
      console.log(`[Diagnostic Mode] Tone Point: ${point}, Freq: ${this.frequency}Hz, Level: ${level.toFixed(3)}`);
    }
  }

  panic() {
    this.activeLatchedNotes.clear();
    const noteButtons = document.querySelectorAll('.diag-note-btn');
    noteButtons.forEach(btn => btn.classList.remove('active'));
    bridge.allNotesOff();
  }

  open() {
    if (!this.modal) return;
    this.focusContract?.rememberTrigger();
    this.modal.style.display = 'flex';
    this.modal.classList.remove('hidden');
    this.focusContract?.releaseInert();
    this.focusContract?.focusFirst();
    this._startScopeLoop();
  }

  close() {
    if (!this.modal) return;
    this._stopScopeLoop();
    // El foco sale ANTES del inert (hacer inerte un contenedor con el foco
    // dentro lo tiraria a <body>) y vuelve al disparador.
    this.focusContract?.restoreFocus();
    this.focusContract?.setInert();
    this.modal.style.display = 'none';
    this.modal.classList.add('hidden');
    // If latch notes are active, release them upon closing
    if (this.activeLatchedNotes.size > 0) {
      this.panic();
    }
  }
}

export const diagnosticModal = new DiagnosticModalController();
