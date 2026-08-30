/**
 * LCD Programmer & Navigation Matrix Controller for ABDMS2000.
 * Replicates the hardware Korg MS2000 16x2 character display navigation:
 * PAGE ◄/►, CURSOR ◄/►, VALUE ▲/▼, EDIT/EXIT, WRITE.
 */

import { paramStore } from '../contracts/paramStore.js';
import { FACTORY_PROGRAMS, generateMS2000SysExHex } from '../contracts/factoryPresets.js';


export class LcdProgrammer {
  constructor(bridge) {
    this.bridge = bridge;

    this.line1Elem = document.getElementById('lcd-line-1');
    this.line2Elem = document.getElementById('lcd-line-2');
    this.sysexHexElem = document.getElementById('sysex-hex-content');

    this.isEditMode = false;
    this.currentPageIndex = 0;
    this.currentCursorIndex = 0;
    this.currentProgramIndex = 1; // 1..8

    this.programs = FACTORY_PROGRAMS;

    // Menu Pages available in EDIT mode
    this.menuPages = [
      {
        title: 'PAGE 01: PROGRAM',
        fields: ['PROGRAM'],
        getValue: () => this.getProgramLabel(),
        onIncrement: () => this.nextProgram(),
        onDecrement: () => this.prevProgram()
      },
      {
        title: 'PAGE 02: SYNTH MODE',
        fields: ['MODE'],
        getValue: () => {
          const m = Math.round(paramStore.values.get('synthMode') || 0);
          return m === 1 ? 'microKORG' : m === 2 ? 'ABD Ultra (32v)' : 'MS2000 (4v)';
        },
        onIncrement: () => this.stepParam('synthMode', 1, 0, 2),
        onDecrement: () => this.stepParam('synthMode', -1, 0, 2)
      },
      {
        title: 'PAGE 03: VOICE ASSIGN',
        fields: ['ASSIGN'],
        getValue: () => {
          const v = Math.round(paramStore.values.get('voiceMode') || 0);
          return ['Mono Normal', 'Mono Legato', 'Poly', 'Unison'][v] || 'Poly';
        },
        onIncrement: () => this.stepParam('voiceMode', 1, 0, 3),
        onDecrement: () => this.stepParam('voiceMode', -1, 0, 3)
      },
      {
        title: 'PAGE 04: FILTER CUTOFF',
        fields: ['CUTOFF'],
        getValue: () => `Cutoff: ${Math.round(paramStore.values.get('filterCutoff') || 127)}`,
        onIncrement: () => this.stepParam('filterCutoff', 2, 0, 127),
        onDecrement: () => this.stepParam('filterCutoff', -2, 0, 127)
      },
      {
        title: 'PAGE 05: FILTER RESO',
        fields: ['RESO'],
        getValue: () => `Resonance: ${Math.round(paramStore.values.get('filterResonance') || 0)}`,
        onIncrement: () => this.stepParam('filterResonance', 2, 0, 127),
        onDecrement: () => this.stepParam('filterResonance', -2, 0, 127)
      },
      {
        title: 'PAGE 06: PORTAMENTO',
        fields: ['TIME'],
        getValue: () => `Time: ${Math.round(paramStore.values.get('portamentoTime') || 0)}`,
        onIncrement: () => this.stepParam('portamentoTime', 2, 0, 127),
        onDecrement: () => this.stepParam('portamentoTime', -2, 0, 127)
      },
      {
        title: 'PAGE 07: MOD EFFECTS',
        fields: ['TYPE'],
        getValue: () => {
          const t = Math.round(paramStore.values.get('modFxType') || 0);
          return ['Chorus/Flanger', 'Ensemble', 'Phaser'][t] || 'Chorus';
        },
        onIncrement: () => this.stepParam('modFxType', 1, 0, 2),
        onDecrement: () => this.stepParam('modFxType', -1, 0, 2)
      },
      {
        title: 'PAGE 08: DELAY FX',
        fields: ['TYPE'],
        getValue: () => {
          const d = Math.round(paramStore.values.get('delayType') || 0);
          return ['Stereo Delay', 'Cross Delay', 'L/R Delay'][d] || 'Stereo';
        },
        onIncrement: () => this.stepParam('delayType', 1, 0, 2),
        onDecrement: () => this.stepParam('delayType', -1, 0, 2)
      },
      {
        title: 'PAGE 09: GLOBAL MIDI',
        fields: ['CHANNEL'],
        getValue: () => `MIDI Ch: ${Math.round(paramStore.values.get('midiChannel') || 1)}`,
        onIncrement: () => this.stepParam('midiChannel', 1, 1, 16),
        onDecrement: () => this.stepParam('midiChannel', -1, 1, 16)
      },
      {
        title: 'PAGE 10: UTILITY',
        fields: ['SYSEX'],
        getValue: () => '[DUMP PROGRAM]',
        onIncrement: () => this.bridge.send('exportSysexProgram', {}),
        onDecrement: () => this.bridge.send('exportSysexProgram', {})
      }
    ];

    this.attachButtonEvents();
    this.loadProgramIndex(0);
  }

  getProgramLabel() {
    const p = this.programs[(this.currentProgramIndex - 1) % this.programs.length];
    return `${p.id} ${p.name}`;
  }

  attachButtonEvents() {
    // PAGE Buttons
    document.getElementById('btn-lcd-page-next')?.addEventListener('click', () => {
      this.isEditMode = true;
      this.currentPageIndex = (this.currentPageIndex + 1) % this.menuPages.length;
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-page-prev')?.addEventListener('click', () => {
      this.isEditMode = true;
      this.currentPageIndex = (this.currentPageIndex - 1 + this.menuPages.length) % this.menuPages.length;
      this.updateDisplay();
    });

    // CURSOR Buttons
    document.getElementById('btn-lcd-cursor-next')?.addEventListener('click', () => {
      this.isEditMode = true;
      this.currentCursorIndex = (this.currentCursorIndex + 1) % 2;
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-cursor-prev')?.addEventListener('click', () => {
      this.isEditMode = true;
      this.currentCursorIndex = (this.currentCursorIndex - 1 + 2) % 2;
      this.updateDisplay();
    });

    // VALUE Buttons
    document.getElementById('btn-lcd-val-up')?.addEventListener('click', () => {
      if (!this.isEditMode) {
        this.nextProgram();
      } else {
        const page = this.menuPages[this.currentPageIndex];
        if (page && page.onIncrement) page.onIncrement();
      }
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-val-down')?.addEventListener('click', () => {
      if (!this.isEditMode) {
        this.prevProgram();
      } else {
        const page = this.menuPages[this.currentPageIndex];
        if (page && page.onDecrement) page.onDecrement();
      }
      this.updateDisplay();
    });

    // EDIT / EXIT Button
    document.getElementById('btn-lcd-edit')?.addEventListener('click', () => {
      this.isEditMode = !this.isEditMode;
      this.updateDisplay();
    });

    // WRITE Button
    document.getElementById('btn-lcd-write')?.addEventListener('click', () => {
      if (window.__kbd) window.__kbd.sweep('left', 8);
      this.flashMessage('WRITE COMPLETED', `SAVED TO ${this.programs[(this.currentProgramIndex - 1) % this.programs.length].id}`);
      this.bridge.send('exportSysexProgram', {});
    });
  }

  loadProgramIndex(idx) {
    const safeIdx = (idx + this.programs.length) % this.programs.length;
    this.currentProgramIndex = safeIdx + 1;
    const p = this.programs[safeIdx];

    if (p && p.params) {
      // 1. Sync all UI controls & ParamStore
      paramStore.syncAll(p.params);

      // 2. Forward parameters to audio engine
      if (this.bridge.wasmBridge && this.bridge.wasmBridge.setAllParams) {
        this.bridge.wasmBridge.setAllParams(p.params);
      }
      for (const [k, v] of Object.entries(p.params)) {
        this.bridge.setParam(k, v);
      }

      // 3. Update SysEx stream preview on the 2nd LCD
      this.updateSysExDisplay(p);

      // 4. Trigger LED cascade sweep across the keyboard
      if (window.__kbd) window.__kbd.sweep('right', 10);
    }

    this.bridge.send('selectProgram', { index: safeIdx });
    this.updateDisplay();
  }

  nextProgram() {
    this.loadProgramIndex(this.currentProgramIndex % this.programs.length);
  }

  prevProgram() {
    this.loadProgramIndex((this.currentProgramIndex - 2 + this.programs.length) % this.programs.length);
  }

  stepParam(paramId, delta, min, max) {
    const current = paramStore.values.get(paramId) !== undefined ? paramStore.values.get(paramId) : min;
    const nextVal = Math.max(min, Math.min(max, current + delta));
    paramStore.setParamValue(paramId, nextVal);
    this.bridge.setParam(paramId, nextVal);
    this.updateSysExDisplay();
  }

  updateSysExDisplay(prog) {
    if (!this.sysexHexElem) this.sysexHexElem = document.getElementById('sysex-hex-content');
    if (this.sysexHexElem) {
      const p = prog || this.programs[(this.currentProgramIndex - 1) % this.programs.length];
      const hex = generateMS2000SysExHex(paramStore.values ? Object.fromEntries(paramStore.values) : p.params, p.name);
      this.sysexHexElem.textContent = hex;
    }
  }

  flashMessage(line1, line2, duration = 1200) {
    if (this.line1Elem) this.line1Elem.textContent = line1;
    if (this.line2Elem) this.line2Elem.textContent = line2;
    setTimeout(() => this.updateDisplay(), duration);
  }

  updateDisplay() {
    if (!this.line1Elem || !this.line2Elem) return;

    if (!this.isEditMode) {
      // Program Play Mode
      const p = this.programs[(this.currentProgramIndex - 1) % this.programs.length];
      this.line1Elem.textContent = `${p.id} ${p.name}`;
      this.line2Elem.textContent = p.cat;
    } else {
      // Edit Parameter Menu Mode
      const page = this.menuPages[this.currentPageIndex];
      this.line1Elem.textContent = page.title;
      this.line2Elem.textContent = `> ${page.getValue()}`;
    }
  }
}

