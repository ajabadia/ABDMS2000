/**
 * LCD Programmer, Bank A–H Selector, 128-Preset Browser Popover & Compare/Write Controller
 * for ABDMS2000.
 * Replicates the hardware Korg MS2000 16x2 character display navigation:
 * PAGE ◄/►, CURSOR ◄/►, VALUE ▲/▼, EDIT/EXIT, WRITE, COMPARE, plus Bank A–H & 128-Preset Browser.
 */

import { paramStore } from '../contracts/paramStore.js';
import { PARAM_LOOKUP } from '../contracts/registry.gen.js';
import { FACTORY_PROGRAMS, generateMS2000SysExHex } from '../contracts/factoryPresets.js';

const BANK_LETTERS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
const VOICE_MODE_NAMES = ['SINGLE', 'SPLIT', 'LAYER', 'VOCODER'];

export class LcdProgrammer {
  constructor(bridge, options = {}) {
    this.bridge = bridge;
    this.onTimbreModeMutated = options.onTimbreModeMutated || null;

    this.line1Elem = document.getElementById('lcd-line-1');
    this.line2Elem = document.getElementById('lcd-line-2');
    this.sysexHexElem = document.getElementById('sysex-hex-content');
    this.popoverElem = document.getElementById('preset-browser-popover');
    this.gridElem = document.getElementById('preset-browser-grid');
    this.searchInputElem = document.getElementById('preset-search-input');

    this.isEditMode = false;
    this.currentPageIndex = 0;
    this.currentCursorIndex = 0;
    this.currentProgramIndex = 1; // 1..128 (A.01 .. H.16)
    this.activeTimbre = 1; // 1 or 2

    // Clone factory programs so user edits/writes in session don't mutate frozen static objects
    this.programs = FACTORY_PROGRAMS.map((p, idx) => ({
      ...p,
      index: idx,
      bank: p.bank || BANK_LETTERS[Math.floor(idx / 16)] || 'A',
      number: p.number || ((idx % 16) + 1),
      params: { ...p.params }
    }));

    // Compare A/B state
    this.isComparing = false;
    this.savedSnapshot = null;
    this.editedSnapshot = null;
    this._suppressCompareExit = false;

    // Browser filter state
    this.browserBankFilter = 'all'; // 'all' or '0'..'7'
    this.browserCatFilter = 'all';
    this.browserSearchQuery = '';

    // Temporary LCD readout timer
    this._flashTimer = null;

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
        title: 'PAGE 02: BANK A-H',
        fields: ['BANK'],
        getValue: () => {
          const bankIdx = this.getCurrentBankIndex();
          return `Bank ${BANK_LETTERS[bankIdx]} (${BANK_LETTERS[bankIdx]}.01-${BANK_LETTERS[bankIdx]}.16)`;
        },
        onIncrement: () => this.selectBank((this.getCurrentBankIndex() + 1) % 8),
        onDecrement: () => this.selectBank((this.getCurrentBankIndex() + 7) % 8)
      },
      {
        title: 'PAGE 03: TIMBRE MODE',
        fields: ['TIMBRE'],
        getValue: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) return 'Vocoder (16-Band)';
          const tm = Math.round(paramStore.values.get('timbreMode') || 0);
          return ['Single (1T)', 'Split (2T)', 'Layer (2T)', 'Vocoder'][tm] || 'Single';
        },
        onIncrement: () => this.stepTimbreMode(1),
        onDecrement: () => this.stepTimbreMode(-1)
      },
      {
        title: 'PAGE 04: SYNTH MODE',
        fields: ['MODE'],
        getValue: () => {
          const m = Math.round(paramStore.values.get('synthMode') || 0);
          return m === 1 ? 'microKORG' : m === 2 ? 'ABD Ultra (32v)' : 'MS2000 (4v)';
        },
        onIncrement: () => this.stepParam('synthMode', 1, 0, 2),
        onDecrement: () => this.stepParam('synthMode', -1, 0, 2)
      },
      {
        title: 'PAGE 05: VOICE ASSIGN',
        fields: ['ASSIGN'],
        getValue: () => {
          const pid = this.tParam('voiceMode');
          const v = Math.round(paramStore.values.get(pid) ?? 1);
          return `T${this.activeTimbre}: ${['Mono', 'Poly', 'Unison'][v] || 'Poly'}`;
        },
        onIncrement: () => this.stepParam(this.tParam('voiceMode'), 1, 0, 2),
        onDecrement: () => this.stepParam(this.tParam('voiceMode'), -1, 0, 2)
      },
      {
        title: 'PAGE 06: FILTER CUTOFF',
        fields: ['CUTOFF'],
        getValue: () => {
          const pid = this.tParam('filterCutoff');
          return `T${this.activeTimbre} Cutoff: ${Math.round(paramStore.values.get(pid) ?? 127)}`;
        },
        onIncrement: () => this.stepParam(this.tParam('filterCutoff'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('filterCutoff'), -2, 0, 127)
      },
      {
        title: 'PAGE 07: FILTER RESO',
        fields: ['RESO'],
        getValue: () => {
          const pid = this.tParam('filterResonance');
          return `T${this.activeTimbre} Reso: ${Math.round(paramStore.values.get(pid) ?? 0)}`;
        },
        onIncrement: () => this.stepParam(this.tParam('filterResonance'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('filterResonance'), -2, 0, 127)
      },
      {
        title: 'PAGE 08: PORTAMENTO',
        fields: ['TIME'],
        getValue: () => {
          const pid = this.tParam('portamentoTime');
          return `T${this.activeTimbre} Glide: ${Math.round(paramStore.values.get(pid) ?? 0)}`;
        },
        onIncrement: () => this.stepParam(this.tParam('portamentoTime'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('portamentoTime'), -2, 0, 127)
      },
      {
        title: 'PAGE 09: SCALE TUNING',
        fields: ['SCALE'],
        getValue: () => {
          const st = Math.round(paramStore.values.get('scaleType') || 0);
          const sk = Math.round(paramStore.values.get('scaleKey') || 0);
          const types = ['Equal Temp', 'Pure Major', 'Pure Minor', 'Arabic', 'Pythagorean', 'Werckmeister', 'Kirnberger', 'Slendro', 'Pelog', 'User Scale'];
          const keys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
          return `${types[st] || 'Equal'} (${keys[sk] || 'C'})`;
        },
        onIncrement: () => this.stepParam('scaleType', 1, 0, 9),
        onDecrement: () => this.stepParam('scaleType', -1, 0, 9)
      },
      {
        title: 'PAGE 10: MOD EFFECTS',
        fields: ['TYPE'],
        getValue: () => {
          const t = Math.round(paramStore.values.get('modFxType') || 0);
          return ['Chorus/Flanger', 'Ensemble', 'Phaser'][t] || 'Chorus';
        },
        onIncrement: () => this.stepParam('modFxType', 1, 0, 2),
        onDecrement: () => this.stepParam('modFxType', -1, 0, 2)
      },
      {
        title: 'PAGE 11: DELAY FX',
        fields: ['TYPE'],
        getValue: () => {
          const d = Math.round(paramStore.values.get('delayType') || 0);
          return ['Stereo Delay', 'Cross Delay', 'L/R Delay'][d] || 'Stereo';
        },
        onIncrement: () => this.stepParam('delayType', 1, 0, 2),
        onDecrement: () => this.stepParam('delayType', -1, 0, 2)
      },
      {
        title: 'PAGE 12: GLOBAL MIDI',
        fields: ['CHANNEL'],
        getValue: () => `MIDI Ch: ${Math.round(paramStore.values.get('midiChannel') || 1)}`,
        onIncrement: () => this.stepParam('midiChannel', 1, 1, 16),
        onDecrement: () => this.stepParam('midiChannel', -1, 1, 16)
      },
      {
        title: 'PAGE 13: UTILITY',
        fields: ['SYSEX'],
        getValue: () => '[DUMP PROGRAM]',
        onIncrement: () => this.bridge.send('exportSysexProgram', {}),
        onDecrement: () => this.bridge.send('exportSysexProgram', {})
      }
    ];

    this.attachButtonEvents();
    this.attachBridgeListeners();
    this.loadProgramIndex(0);
    this.bridge.send('getPresetCatalog', {});
  }

  tParam(baseId) {
    if (!baseId || this.activeTimbre !== 2) return baseId;
    const candidate = 't2' + baseId.charAt(0).toUpperCase() + baseId.slice(1);
    return PARAM_LOOKUP[candidate] ? candidate : baseId;
  }

  setActiveTimbre(timbre) {
    this.activeTimbre = timbre === 2 ? 2 : 1;
    if (this.isEditMode) {
      this.updateDisplay();
    } else {
      this.flashMessage(`EDIT TIMBRE ${this.activeTimbre}`, this.activeTimbre === 2 ? 'Split / Layer Sub-Timbre' : 'Main Timbre 1 Active', 900);
    }
  }

  getCurrentBankIndex() {
    const idx = Math.max(0, (this.currentProgramIndex - 1) % this.programs.length);
    return Math.floor(idx / 16);
  }

  getCurrentProgram() {
    return this.programs[(this.currentProgramIndex - 1) % this.programs.length];
  }

  getProgramLabel() {
    const p = this.getCurrentProgram();
    return `${p.id} ${p.name}`;
  }

  attachBridgeListeners() {
    if (!this.bridge || typeof this.bridge.on !== 'function') return;

    this.bridge.on('presetCatalog', (payload) => {
      if (payload && Array.isArray(payload.programs)) {
        for (const item of payload.programs) {
          const idx = Number(item.index);
          if (idx >= 0 && idx < this.programs.length && item.name) {
            this.programs[idx].name = item.name;
            if (item.id) this.programs[idx].id = item.id;
            if (item.bank) this.programs[idx].bank = item.bank;
          }
        }
        if (typeof payload.activeIndex === 'number' && payload.activeIndex >= 0 && payload.activeIndex < this.programs.length) {
          this.currentProgramIndex = payload.activeIndex + 1;
          this.updateBankStripHighlight();
        }
        this.updateDisplay();
        if (this.popoverElem && !this.popoverElem.hidden) {
          this.renderPresetBrowserGrid();
        }
      }
    });

    this.bridge.on('programWritten', (payload) => {
      if (payload && typeof payload.index === 'number') {
        const idx = payload.index;
        if (idx >= 0 && idx < this.programs.length && payload.name) {
          this.programs[idx].name = payload.name;
        }
      }
    });
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

    // WRITE Button (UX1.3)
    document.getElementById('btn-lcd-write')?.addEventListener('click', () => {
      this.writeCurrentProgram();
    });

    // Bank A..H Quick-Select Strip (UX1.2)
    document.querySelectorAll('#bank-selector-strip .bank-quick-btn').forEach((btn) => {
      btn.addEventListener('click', () => {
        const bankIdx = Number(btn.getAttribute('data-bank')) || 0;
        this.selectBank(bankIdx);
      });
    });

    // LCD Display / BROWSE Button -> Toggle 128-Preset Browser Popover (UX1.2)
    document.getElementById('lcd-display')?.addEventListener('click', (e) => {
      e.stopPropagation();
      this.togglePresetBrowser();
    });

    document.getElementById('btn-close-preset-browser')?.addEventListener('click', (e) => {
      e.stopPropagation();
      this.closePresetBrowser();
    });

    // Bank Tabs in Preset Browser
    document.querySelectorAll('#preset-bank-tabs .preset-tab-btn').forEach((btn) => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.browserBankFilter = btn.getAttribute('data-bank-tab') || 'all';
        document.querySelectorAll('#preset-bank-tabs .preset-tab-btn').forEach(b => b.classList.toggle('active', b === btn));
        this.renderPresetBrowserGrid();
      });
    });

    // Category Tabs in Preset Browser
    document.querySelectorAll('#preset-cat-tabs .preset-cat-btn').forEach((btn) => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.browserCatFilter = btn.getAttribute('data-cat') || 'all';
        document.querySelectorAll('#preset-cat-tabs .preset-cat-btn').forEach(b => b.classList.toggle('active', b === btn));
        this.renderPresetBrowserGrid();
      });
    });

    // Search Input in Preset Browser
    this.searchInputElem?.addEventListener('input', (e) => {
      this.browserSearchQuery = (e.target.value || '').trim().toLowerCase();
      this.renderPresetBrowserGrid();
    });
    this.searchInputElem?.addEventListener('click', (e) => e.stopPropagation());

    // Close Preset Browser on outside click or Escape
    document.addEventListener('click', (e) => {
      if (this.popoverElem && !this.popoverElem.hidden) {
        if (!this.popoverElem.contains(e.target) && !document.getElementById('lcd-display')?.contains(e.target)) {
          this.closePresetBrowser();
        }
      }
    });

    window.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && this.popoverElem && !this.popoverElem.hidden) {
        e.stopPropagation();
        this.closePresetBrowser();
      }
    });
  }

  togglePresetBrowser() {
    if (!this.popoverElem) return;
    if (this.popoverElem.hidden) {
      this.openPresetBrowser();
    } else {
      this.closePresetBrowser();
    }
  }

  openPresetBrowser() {
    if (!this.popoverElem) return;
    this.popoverElem.hidden = false;
    this.renderPresetBrowserGrid();
    // Scroll active preset into view
    const activeSlot = this.gridElem?.querySelector('.preset-slot-btn.active');
    if (activeSlot && typeof activeSlot.scrollIntoView === 'function') {
      activeSlot.scrollIntoView({ block: 'nearest' });
    }
  }

  closePresetBrowser() {
    if (!this.popoverElem) return;
    this.popoverElem.hidden = true;
  }

  renderPresetBrowserGrid() {
    if (!this.gridElem) return;
    const activeIdx = (this.currentProgramIndex - 1 + this.programs.length) % this.programs.length;
    const bankFilter = this.browserBankFilter;
    const catFilter = this.browserCatFilter.toLowerCase();
    const q = this.browserSearchQuery;

    const fragment = document.createDocumentFragment();
    let matchCount = 0;

    for (let i = 0; i < this.programs.length; i++) {
      const p = this.programs[i];
      const bankIdx = Math.floor(i / 16);
      if (bankFilter !== 'all' && Number(bankFilter) !== bankIdx) continue;
      if (catFilter !== 'all' && !(p.cat || '').toLowerCase().includes(catFilter)) continue;
      if (q && !`${p.id} ${p.name} ${p.cat}`.toLowerCase().includes(q)) continue;

      matchCount++;
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'preset-slot-btn' + (i === activeIdx ? ' active' : '');
      const tm = Math.round(p.params?.synthVocoderMode ? 3 : (p.params?.timbreMode ?? 0));
      const modeLabel = VOICE_MODE_NAMES[tm] || 'SINGLE';
      const modeClass = tm === 3 ? 'mode-vocoder' : (tm === 2 ? 'mode-layer' : (tm === 1 ? 'mode-split' : 'mode-single'));

      btn.innerHTML = `
        <div class="preset-slot-main">
          <span class="preset-slot-id">${p.id}</span>
          <span class="preset-slot-name">${p.name}</span>
        </div>
        <div class="preset-slot-meta">
          <span class="preset-slot-cat">${(p.cat || 'Synth').split(' ')[0]}</span>
          <span class="preset-slot-mode ${modeClass}">${modeLabel}</span>
        </div>
      `;
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        this.loadProgramIndex(i);
        this.closePresetBrowser();
      });
      fragment.appendChild(btn);
    }

    this.gridElem.innerHTML = '';
    if (matchCount === 0) {
      const empty = document.createElement('div');
      empty.style.cssText = 'grid-column: 1 / -1; text-align: center; padding: 28px; color: #94a3b8; font-size: 0.85rem;';
      empty.textContent = 'No presets match the current filter.';
      this.gridElem.appendChild(empty);
    } else {
      this.gridElem.appendChild(fragment);
    }
  }

  selectBank(bankIdx) {
    const safeBank = ((bankIdx % 8) + 8) % 8;
    const currentOffsetInBank = (this.currentProgramIndex - 1) % 16;
    const targetIdx = safeBank * 16 + currentOffsetInBank;
    this.loadProgramIndex(targetIdx);
  }

  updateBankStripHighlight() {
    const bankIdx = this.getCurrentBankIndex();
    document.querySelectorAll('#bank-selector-strip .bank-quick-btn').forEach((btn) => {
      const b = Number(btn.getAttribute('data-bank'));
      btn.classList.toggle('active', b === bankIdx);
    });
  }

  loadProgramIndex(idx) {
    const safeIdx = (idx + this.programs.length) % this.programs.length;
    this.currentProgramIndex = safeIdx + 1;
    const p = this.programs[safeIdx];

    this.exitCompareMode(false);
    this.updateBankStripHighlight();

    if (p && p.params) {
      this.savedSnapshot = { ...p.params };

      // 1. Sync all UI controls & ParamStore
      paramStore.syncAll(p.params);

      // 2. Forward parameters to audio engine
      if (!this.bridge.isWebView && this.bridge.wasmBridge && this.bridge.wasmBridge.setAllParams) {
        this.bridge.wasmBridge.setAllParams(p.params);
      } else {
        for (const [k, v] of Object.entries(p.params)) {
          this.bridge.setParam(k, v);
        }
      }

      // 3. Update SysEx stream preview on the 2nd LCD
      this.updateSysExDisplay(p);

      // 4. Trigger LED cascade sweep across the keyboard
      if (window.__kbd) window.__kbd.sweep('right', 10);

      if (typeof this.onTimbreModeMutated === 'function') {
        this.onTimbreModeMutated();
      }
    }

    this.bridge.send('selectProgram', { index: safeIdx });
    this.updateDisplay();
    if (this.popoverElem && !this.popoverElem.hidden) {
      this.renderPresetBrowserGrid();
    }
  }

  /**
   * Toggle COMPARE A/B snapshot (saved original preset vs edited buffer) (UX1.3)
   */
  toggleCompare() {
    const compareBtn = document.getElementById('btn-compare');
    if (!this.savedSnapshot) {
      const p = this.getCurrentProgram();
      this.savedSnapshot = p?.params ? { ...p.params } : Object.fromEntries(paramStore.values);
    }

    this._suppressCompareExit = true;
    try {
      if (!this.isComparing) {
        this.editedSnapshot = Object.fromEntries(paramStore.values);
        this.isComparing = true;
        compareBtn?.classList.add('active');
        this._applySnapshotToEngine(this.savedSnapshot);
        const p = this.getCurrentProgram();
        this.flashMessage(`[COMPARE: ORIG]`, `${p.id} ${p.name}`, 2200);
      } else {
        this.isComparing = false;
        compareBtn?.classList.remove('active');
        if (this.editedSnapshot) {
          this._applySnapshotToEngine(this.editedSnapshot);
        }
        this.flashMessage(`[COMPARE: EXIT]`, `Restored Edited Patch`, 1200);
      }
    } finally {
      this._suppressCompareExit = false;
    }
  }

  exitCompareMode(restoreEdited = false) {
    if (!this.isComparing || this._suppressCompareExit) return;
    this.isComparing = false;
    document.getElementById('btn-compare')?.classList.remove('active');
    if (restoreEdited && this.editedSnapshot) {
      this._suppressCompareExit = true;
      try {
        this._applySnapshotToEngine(this.editedSnapshot);
      } finally {
        this._suppressCompareExit = false;
      }
    }
  }

  _applySnapshotToEngine(snapshot) {
    if (!snapshot) return;
    paramStore.syncAll(snapshot);
    if (!this.bridge.isWebView && this.bridge.wasmBridge && this.bridge.wasmBridge.setAllParams) {
      this.bridge.wasmBridge.setAllParams(snapshot);
    } else {
      for (const [k, v] of Object.entries(snapshot)) {
        this.bridge.send('setParam', { paramId: k, value: v });
      }
    }
    this.updateSysExDisplay();
    if (typeof this.onTimbreModeMutated === 'function') {
      this.onTimbreModeMutated();
    }
  }

  /**
   * Write / Store edited parameters into current program slot & C++ host bank (UX1.3)
   */
  writeCurrentProgram() {
    const safeIdx = (this.currentProgramIndex - 1 + this.programs.length) % this.programs.length;
    const p = this.programs[safeIdx];
    const currentParams = Object.fromEntries(paramStore.values);

    p.params = { ...currentParams };
    this.savedSnapshot = { ...currentParams };
    this.editedSnapshot = null;
    this.exitCompareMode(false);

    if (window.__kbd) window.__kbd.sweep('left', 8);
    this.flashMessage('WRITE COMPLETED', `SAVED TO ${p.id} (${p.name.trim()})`, 1600);
    this.bridge.send('writeProgramToSlot', { index: safeIdx, name: p.name });
    this.bridge.send('exportSysexProgram', {});
    this.updateSysExDisplay(p);
  }

  nextProgram() {
    this.loadProgramIndex(this.currentProgramIndex % this.programs.length);
  }

  prevProgram() {
    this.loadProgramIndex((this.currentProgramIndex - 2 + this.programs.length) % this.programs.length);
  }

  stepTimbreMode(delta) {
    const curVoc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
    const curMode = curVoc === 1 ? 3 : Math.round(paramStore.values.get('timbreMode') || 0);
    const nextMode = (curMode + delta + 4) % 4;
    if (nextMode === 3) {
      paramStore.setParamValue('synthVocoderMode', 1);
      this.bridge.setParam('synthVocoderMode', 1);
      paramStore.setParamValue('timbreMode', 3);
      this.bridge.setParam('timbreMode', 3);
    } else {
      paramStore.setParamValue('synthVocoderMode', 0);
      this.bridge.setParam('synthVocoderMode', 0);
      paramStore.setParamValue('timbreMode', nextMode);
      this.bridge.setParam('timbreMode', nextMode);
    }
    if (typeof this.onTimbreModeMutated === 'function') {
      this.onTimbreModeMutated();
    }
    this.updateSysExDisplay();
  }

  stepParam(paramId, delta, min, max) {
    const current = paramStore.values.get(paramId) !== undefined ? paramStore.values.get(paramId) : min;
    const nextVal = Math.max(min, Math.min(max, current + delta));
    paramStore.setParamValue(paramId, nextVal);
    this.bridge.setParam(paramId, nextVal);
    this.updateSysExDisplay();
  }

  /**
   * Real-time LCD readout when any UI control (knob/slider/select/switch) is moved (UX2.3)
   */
  showParameterFeedback(paramId, value, customDisplayValue = null) {
    if (this._suppressCompareExit) return;
    if (this.isComparing) {
      this.isComparing = false;
      document.getElementById('btn-compare')?.classList.remove('active');
    }

    const spec = PARAM_LOOKUP[paramId];
    if (!spec) return;

    const isT2 = paramId.startsWith('t2');
    const prefix = isT2 ? 'T2' : (PARAM_LOOKUP['t2' + paramId.charAt(0).toUpperCase() + paramId.slice(1)] ? 'T1' : 'GLB');
    const cleanName = (spec.name || paramId).replace(/^T2\s+/i, '').toUpperCase().slice(0, 12);

    let valStr = customDisplayValue;
    if (!valStr) {
      if (Array.isArray(spec.choices) && spec.choices.length > 0) {
        const idx = Math.max(0, Math.min(spec.choices.length - 1, Math.round(value)));
        valStr = String(spec.choices[idx]);
      } else if (spec.type === 'boolean') {
        valStr = value > 0.5 ? 'ON' : 'OFF';
      } else {
        const rounded = Math.round(value);
        valStr = (spec.min < 0 && rounded > 0) ? `+${rounded}` : `${rounded}`;
        if (spec.unit) valStr += ` ${spec.unit}`;
      }
    }

    this.flashMessage(`${prefix}: ${cleanName}`, `Value: ${valStr}`, 1400);
  }

  updateSysExDisplay(prog) {
    if (!this.sysexHexElem) this.sysexHexElem = document.getElementById('sysex-hex-content');
    if (this.sysexHexElem) {
      const p = prog || this.programs[(this.currentProgramIndex - 1) % this.programs.length];
      const hex = generateMS2000SysExHex(paramStore.values ? Object.fromEntries(paramStore.values) : p.params, p.name);
      this.sysexHexElem.textContent = hex;
    }
    if (typeof window !== 'undefined' && typeof window.__onMS2000ParamMutated === 'function') {
      window.__onMS2000ParamMutated();
    }
  }

  flashMessage(line1, line2, duration = 1200) {
    if (this.line1Elem) this.line1Elem.textContent = line1;
    if (this.line2Elem) this.line2Elem.textContent = line2;
    if (this._flashTimer) clearTimeout(this._flashTimer);
    this._flashTimer = setTimeout(() => {
      this._flashTimer = null;
      this.updateDisplay();
    }, duration);
  }

  updateDisplay() {
    if (!this.line1Elem || !this.line2Elem) return;

    if (!this.isEditMode) {
      // Program Play Mode
      const p = this.programs[(this.currentProgramIndex - 1) % this.programs.length];
      const cmpTag = this.isComparing ? '[CMP] ' : '';
      this.line1Elem.textContent = `${cmpTag}${p.id} ${p.name}`;
      const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
      const tm = voc === 1 ? 3 : Math.round(paramStore.values.get('timbreMode') || 0);
      const modeBadge = VOICE_MODE_NAMES[tm] || 'SINGLE';
      this.line2Elem.textContent = `${p.cat} · ${modeBadge} [T${this.activeTimbre}]`;
    } else {
      // Edit Parameter Menu Mode
      const page = this.menuPages[this.currentPageIndex];
      this.line1Elem.textContent = page.title;
      this.line2Elem.textContent = `> ${page.getValue()}`;
    }
  }
}
