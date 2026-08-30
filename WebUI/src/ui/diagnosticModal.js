/**
 * Diagnostic Test Mode & DSP Signal Chain Injector Controller.
 * Allows live routing of a 440 Hz test tone at 5 strategic points in the DSP chain
 * and direct DSP note triggering to debug audio silencing.
 */

import { bridge } from '../bridge/bridgeCore.js';

// Configuration flag: set to false to completely hide Test Mode in production release builds
export const SHOW_DEV_TEST_MODE = true;

class DiagnosticModalController {
  constructor() {
    this.modal = null;
    this.btnNav = null;
    this.isToneActive = false;
    this.currentPoint = 2; // Default to Pre-Master Volume
    this.frequency = 440;
    this.volume = 25; // %
    this.latchMode = false;
    this.activeLatchedNotes = new Set();
  }

  init() {
    this.modal = document.getElementById('diagnostic-modal');
    this.btnNav = document.getElementById('btn-diagnostic-test');

    if (!SHOW_DEV_TEST_MODE) {
      if (this.btnNav) this.btnNav.style.display = 'none';
      if (this.modal) this.modal.style.display = 'none';
      return;
    }

    if (this.btnNav) {
      this.btnNav.addEventListener('click', () => this.open());
    }

    // Close buttons
    document.getElementById('diag-close-btn')?.addEventListener('click', () => this.close());
    document.getElementById('diag-close-top-btn')?.addEventListener('click', () => this.close());
    this.modal?.addEventListener('click', (e) => {
      if (e.target === this.modal) this.close();
    });

    // Tone Enable Toggle
    const toggle = document.getElementById('diag-tone-enable');
    const statusLabel = document.getElementById('diag-tone-status');
    toggle?.addEventListener('change', (e) => {
      this.isToneActive = e.target.checked;
      if (statusLabel) {
        statusLabel.textContent = this.isToneActive ? 'TONO ACTIVADO (EN TRANSMISIÓN)' : 'TONO DESACTIVADO';
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
        console.log(`[Diagnostic Mode] Bypass ${stage}: ${enabled}`);
      });
    });

    // Reset All Bypasses Button
    document.getElementById('diag-btn-reset-bypasses')?.addEventListener('click', () => {
      bypassMap.forEach(({ id }) => {
        const el = document.getElementById(id);
        if (el) el.checked = false;
      });
      bridge.resetDiagnosticBypasses();
      console.log('[Diagnostic Mode] All bypasses reset to normal operation');
    });
  }

  updateTone() {
    const point = this.isToneActive ? this.currentPoint : 0;
    const level = (this.volume / 100.0) * 0.45;
    bridge.setDiagnosticTone(point, this.frequency, level);
    console.log(`[Diagnostic Mode] Tone Point: ${point}, Freq: ${this.frequency}Hz, Level: ${level.toFixed(3)}`);
  }

  panic() {
    this.activeLatchedNotes.clear();
    const noteButtons = document.querySelectorAll('.diag-note-btn');
    noteButtons.forEach(btn => btn.classList.remove('active'));
    bridge.allNotesOff();
  }

  open() {
    if (!this.modal) return;
    this.modal.style.display = 'flex';
    this.modal.classList.remove('hidden');
  }

  close() {
    if (!this.modal) return;
    this.modal.style.display = 'none';
    this.modal.classList.add('hidden');
    // If latch notes are active, release them upon closing
    if (this.activeLatchedNotes.size > 0) {
      this.panic();
    }
  }
}

export const diagnosticModal = new DiagnosticModalController();
