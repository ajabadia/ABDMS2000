/**
 * LCD Programmer, Bank A–H Selector, 128-Preset Browser Popover & Compare/Write Controller
 * for ABDMS2000.
 * Replicates the hardware Korg MS2000 16x2 character display navigation:
 * PAGE ◄/►, CURSOR ◄/►, VALUE ▲/▼, EDIT/EXIT, WRITE, COMPARE, plus Bank A–H & 128-Preset Browser.
 */

import { paramStore } from '../contracts/paramStore.js';
import { PARAM_LOOKUP } from '../contracts/registry.gen.js';
import { FACTORY_PROGRAMS, generateMS2000SysExHex, encodeHardwareProgram254 } from '../contracts/factoryPresets.js';
import { buildEditTimbreNotice } from '../contracts/lcdNotices.js';
import { buildWriteCompletedNotice } from '@abdsynths/shared/components';

const BANK_LETTERS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
const VOICE_MODE_NAMES = ['SINGLE', 'SPLIT', 'LAYER', 'VOCODER'];

export class LcdProgrammer {
  constructor(bridge, options = {}) {
    this.bridge = bridge;
    this.onTimbreModeMutated = options.onTimbreModeMutated || null;

    this.line1Elem = document.getElementById('lcd-line-1');
    this.line2Elem = document.getElementById('lcd-line-2');
    this.origLedElem = document.getElementById('led-original-value');
    this.origIndicatorElem = document.getElementById('orig-val-indicator');
    this.sysexHexElem = document.getElementById('sysex-hex-content');
    this.popoverElem = document.getElementById('preset-browser-popover');
    this.gridElem = document.getElementById('preset-browser-grid');
    this.searchInputElem = document.getElementById('preset-search-input');

    this.isEditMode = false;
    this.isGlobalMode = false;
    this.currentPageIndex = 0;
    this.currentGlobalPageIndex = 0;
    this.currentCursorIndex = 0;
    this.currentProgramIndex = 1; // 1..128 (A.01 .. H.16)
    this.activeTimbre = 1; // 1 or 2
    this.lastEditedParamId = null;

    // Hardware MS2000 Global Mode State
    this.globalState = {
      masterTune: 440.0,
      transpose: 0,
      position: 'PostKBD',
      midiCh: 1,
      clockMode: 'Auto',
      memoryProtect: false
    };

    // Built-in MS2000 Hardware Demo Riff Sequencer State (EDIT + GLOBAL = DEMO)
    this.isDemoPlaying = false;
    this._demoTimer = null;
    this._demoStep = 0;
    this._demoActiveNotes = [];

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
    this._noticeLockUntil = 0;

    // 16 Hardware-Mapped Edit Pages (1: COMMON .. 16: UTILITY)
    this.menuPages = [
      {
        id: 'common',
        pad: 1,
        title: 'P01: COMMON MODE',
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
        id: 'voice',
        pad: 2,
        title: 'P02: VOICE ASSIGN',
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
        id: 'pitch',
        pad: 3,
        title: 'P03: PITCH & GLIDE',
        fields: ['PORTA'],
        getValue: () => {
          const pid = this.tParam('portamentoTime');
          const tr = Math.round(paramStore.values.get(this.tParam('pitchTranspose')) ?? 0);
          return `T${this.activeTimbre} Tr:${tr >= 0 ? '+' : ''}${tr} Porta:${Math.round(paramStore.values.get(pid) ?? 0)}`;
        },
        onIncrement: () => this.stepParam(this.tParam('portamentoTime'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('portamentoTime'), -2, 0, 127)
      },
      {
        id: 'osc1',
        pad: 4,
        title: 'P04: OSCILLATOR 1',
        fields: ['WAVE'],
        getValue: () => {
          const pid = this.tParam('osc1Wave');
          const w = Math.round(paramStore.values.get(pid) ?? 0);
          const names = ['Saw', 'Pulse', 'Tri', 'Sine', 'Vox', 'DWGS', 'Noise', 'AudioIn'];
          return `T${this.activeTimbre} Wave: ${names[w] || 'Saw'}`;
        },
        onIncrement: () => this.stepParam(this.tParam('osc1Wave'), 1, 0, 7),
        onDecrement: () => this.stepParam(this.tParam('osc1Wave'), -1, 0, 7)
      },
      {
        id: 'osc2',
        pad: 5,
        title: 'P05: OSC 2 / IN 2',
        fields: ['WAVE'],
        getValue: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) {
            const g = Math.round(paramStore.values.get('vocoderGateSense') ?? 50);
            const hpf = Math.round(paramStore.values.get('vocoderHpfLevel') ?? 64);
            return `Gate:${g} HPF:${hpf}`;
          }
          const w = Math.round(paramStore.values.get(this.tParam('osc2Wave')) ?? 0);
          const m = Math.round(paramStore.values.get(this.tParam('osc2ModType')) ?? 0);
          return `T${this.activeTimbre} ${['Saw', 'Sqr', 'Tri'][w] || 'Saw'} (${['Off', 'Ring', 'Sync', 'X-Mod'][m] || 'Off'})`;
        },
        onIncrement: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) this.stepParam('vocoderGateSense', 2, 0, 127);
          else this.stepParam(this.tParam('osc2Wave'), 1, 0, 2);
        },
        onDecrement: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) this.stepParam('vocoderGateSense', -2, 0, 127);
          else this.stepParam(this.tParam('osc2Wave'), -1, 0, 2);
        }
      },
      {
        id: 'filter',
        pad: 6,
        title: 'P06: FILTER VCF',
        fields: ['CUTOFF'],
        getValue: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) {
            const sh = ['-2', '-1', '0', '+1', '+2'][Math.round(paramStore.values.get('vocoderFormantShift') ?? 2)] || '0';
            const fc = Math.round(paramStore.values.get('vocoderCutoff') ?? 0);
            const res = Math.round(paramStore.values.get('vocoderResonance') ?? 64);
            return `Sh:${sh} Fc:${fc >= 0 ? '+' : ''}${fc} R:${res}`;
          }
          const pid = this.tParam('filterCutoff');
          const rPid = this.tParam('filterResonance');
          return `T${this.activeTimbre} Cut:${Math.round(paramStore.values.get(pid) ?? 127)} Res:${Math.round(paramStore.values.get(rPid) ?? 0)}`;
        },
        onIncrement: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) this.stepParam('vocoderCutoff', 2, -63, 63);
          else this.stepParam(this.tParam('filterCutoff'), 2, 0, 127);
        },
        onDecrement: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) this.stepParam('vocoderCutoff', -2, -63, 63);
          else this.stepParam(this.tParam('filterCutoff'), -2, 0, 127);
        }
      },
      {
        id: 'amp',
        pad: 7,
        title: 'P07: AMPLIFIER VCA',
        fields: ['LEVEL'],
        getValue: () => {
          const pid = this.tParam('ampLevel');
          const pan = Math.round(paramStore.values.get(this.tParam('ampPan')) ?? 0);
          return `T${this.activeTimbre} Vol:${Math.round(paramStore.values.get(pid) ?? 100)} Pan:${pan === 0 ? 'CNT' : (pan > 0 ? 'R' + pan : 'L' + Math.abs(pan))}`;
        },
        onIncrement: () => this.stepParam(this.tParam('ampLevel'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('ampLevel'), -2, 0, 127)
      },
      {
        id: 'eg',
        pad: 8,
        title: 'P08: ENVELOPES EG',
        fields: ['DECAY'],
        getValue: () => {
          const a = Math.round(paramStore.values.get(this.tParam('eg2Attack')) ?? 0);
          const d = Math.round(paramStore.values.get(this.tParam('eg2Decay')) ?? 64);
          const s = Math.round(paramStore.values.get(this.tParam('eg2Sustain')) ?? 127);
          const r = Math.round(paramStore.values.get(this.tParam('eg2Release')) ?? 20);
          return `T${this.activeTimbre} EG2 ${a}/${d}/${s}/${r}`;
        },
        onIncrement: () => this.stepParam(this.tParam('eg2Release'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('eg2Release'), -2, 0, 127)
      },
      {
        id: 'lfo',
        pad: 9,
        title: 'P09: LFO 1 & 2',
        fields: ['FREQ'],
        getValue: () => {
          const pid = this.tParam('lfo1Freq');
          return `T${this.activeTimbre} LFO1 Spd: ${Math.round(paramStore.values.get(pid) ?? 30)}`;
        },
        onIncrement: () => this.stepParam(this.tParam('lfo1Freq'), 2, 0, 127),
        onDecrement: () => this.stepParam(this.tParam('lfo1Freq'), -2, 0, 127)
      },
      {
        id: 'patch',
        pad: 10,
        title: 'P10: PATCH / CH LVL',
        fields: ['INT'],
        getValue: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) {
            return `Vocoder 16-Ch Levels`;
          }
          const pid = this.tParam('patch1Intensity');
          const v = Math.round(paramStore.values.get(pid) ?? 0);
          return `T${this.activeTimbre} P1 Int: ${v >= 0 ? '+' : ''}${v}`;
        },
        onIncrement: () => this.stepParam(this.tParam('patch1Intensity'), 1, -63, 63),
        onDecrement: () => this.stepParam(this.tParam('patch1Intensity'), -1, -63, 63)
      },
      {
        id: 'seq',
        pad: 11,
        title: 'P11: MOD SEQ / PAN',
        fields: ['SEQ'],
        getValue: () => {
          const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
          if (voc === 1) {
            return `Vocoder 16-Ch Panpot`;
          }
          const on = Math.round(paramStore.values.get('modSeqOn') || 0);
          const len = Math.round(paramStore.values.get(this.tParam('seqLastStep')) || 16);
          return `ModSeq: ${on ? 'ON' : 'OFF'} (Len:${len})`;
        },
        onIncrement: () => this.stepParam('modSeqOn', 1, 0, 1),
        onDecrement: () => this.stepParam('modSeqOn', -1, 0, 1)
      },
      {
        id: 'modfx',
        pad: 12,
        title: 'P12: MOD EFFECTS',
        fields: ['TYPE'],
        getValue: () => {
          const t = Math.round(paramStore.values.get('modFxType') || 0);
          return ['Chorus/Flanger', 'Ensemble', 'Phaser'][t] || 'Chorus';
        },
        onIncrement: () => this.stepParam('modFxType', 1, 0, 2),
        onDecrement: () => this.stepParam('modFxType', -1, 0, 2)
      },
      {
        id: 'delayfx',
        pad: 13,
        title: 'P13: DELAY FX',
        fields: ['TYPE'],
        getValue: () => {
          const d = Math.round(paramStore.values.get('delayType') || 0);
          return ['Stereo Delay', 'Cross Delay', 'L/R Delay'][d] || 'Stereo';
        },
        onIncrement: () => this.stepParam('delayType', 1, 0, 2),
        onDecrement: () => this.stepParam('delayType', -1, 0, 2)
      },
      {
        id: 'eq',
        pad: 14,
        title: 'P14: EQUALIZER',
        fields: ['GAIN'],
        getValue: () => {
          const lg = Math.round(paramStore.values.get('eqLowGain') ?? 64) - 64;
          const hg = Math.round(paramStore.values.get('eqHighGain') ?? 64) - 64;
          return `Lo:${lg >= 0 ? '+' : ''}${lg} Hi:${hg >= 0 ? '+' : ''}${hg}`;
        },
        onIncrement: () => this.stepParam('eqHighGain', 2, 0, 127),
        onDecrement: () => this.stepParam('eqHighGain', -2, 0, 127)
      },
      {
        id: 'arp',
        pad: 15,
        title: 'P15: ARPEGGIATOR',
        fields: ['ARP'],
        getValue: () => {
          const on = Math.round(paramStore.values.get('arpOn') || 0);
          const t = Math.round(paramStore.values.get('arpType') || 0);
          const types = ['Up', 'Down', 'Alt1', 'Alt2', 'Random', 'Trigger'];
          return `${on ? 'ON' : 'OFF'} (${types[t] || 'Up'})`;
        },
        onIncrement: () => this.stepParam('arpOn', 1, 0, 1),
        onDecrement: () => this.stepParam('arpOn', -1, 0, 1)
      },
      {
        id: 'utility',
        pad: 16,
        title: 'P16: UTILITY',
        fields: ['SYSEX'],
        getValue: () => '[COPY / WRITE / SYSEX]',
        onIncrement: () => this.bridge.send('exportSysexProgram', {}),
        onDecrement: () => this.bridge.send('exportSysexProgram', {})
      }
    ];

    // 6 Hardware Global Mode Pages (GLOBAL Button)
    this.globalPages = [
      {
        id: 'g_tune',
        title: 'G01: MASTER TUNE',
        getParamId: () => 'masterVolume',
        getValue: () => {
          const tune = Number(this.globalState.masterTune || 440.0).toFixed(1);
          const vol = Math.round((paramStore.values.get('masterVolume') ?? 0.8) * 100);
          return `A4=${tune}Hz  Master:${vol}%`;
        },
        onIncrement: () => {
          this.globalState.masterTune = Math.min(450.0, Math.round((this.globalState.masterTune + 0.5) * 10) / 10);
        },
        onDecrement: () => {
          this.globalState.masterTune = Math.max(430.0, Math.round((this.globalState.masterTune - 0.5) * 10) / 10);
        }
      },
      {
        id: 'g_transpose',
        title: 'G02: KEY TRANSPOSE',
        getParamId: () => 'pitchTranspose',
        getValue: () => {
          const tr = Math.round(paramStore.values.get('pitchTranspose') ?? 0);
          return `Global Transpose: ${tr >= 0 ? '+' : ''}${tr} st`;
        },
        onIncrement: () => this.stepParam('pitchTranspose', 1, -24, 24),
        onDecrement: () => this.stepParam('pitchTranspose', -1, -24, 24)
      },
      {
        id: 'g_position',
        title: 'G03: POSITION / LOCAL',
        getValue: () => `Pos: ${this.globalState.position} (Local:ON)`,
        onIncrement: () => {
          this.globalState.position = this.globalState.position === 'PostKBD' ? 'PreTG' : 'PostKBD';
        },
        onDecrement: () => {
          this.globalState.position = this.globalState.position === 'PostKBD' ? 'PreTG' : 'PostKBD';
        }
      },
      {
        id: 'g_midich',
        title: 'G04: MIDI CHANNEL',
        getValue: () => `Global MIDI Ch: ${String(this.globalState.midiCh).padStart(2, '0')}`,
        onIncrement: () => {
          this.globalState.midiCh = Math.min(16, (this.globalState.midiCh || 1) + 1);
          if (typeof this.onGlobalMidiChannelChanged === 'function') {
            this.onGlobalMidiChannelChanged(this.globalState.midiCh);
          }
        },
        onDecrement: () => {
          this.globalState.midiCh = Math.max(1, (this.globalState.midiCh || 1) - 1);
          if (typeof this.onGlobalMidiChannelChanged === 'function') {
            this.onGlobalMidiChannelChanged(this.globalState.midiCh);
          }
        }
      },
      {
        id: 'g_clock',
        title: 'G05: MIDI CLOCK',
        getValue: () => {
          const bpm = Math.round(paramStore.values.get('arpTempo') ?? 120);
          return `Clock: ${this.globalState.clockMode} (♩=${bpm})`;
        },
        onIncrement: () => {
          const modes = ['Auto', 'Internal', 'External'];
          const idx = (modes.indexOf(this.globalState.clockMode) + 1) % modes.length;
          this.globalState.clockMode = modes[idx];
        },
        onDecrement: () => {
          const modes = ['Auto', 'Internal', 'External'];
          const idx = (modes.indexOf(this.globalState.clockMode) - 1 + modes.length) % modes.length;
          this.globalState.clockMode = modes[idx];
        }
      },
      {
        id: 'g_dump',
        title: 'G06: SYSEX DATA DUMP',
        getValue: () => 'Press +/YES to Dump Patch',
        onIncrement: () => {
          this.bridge.send('exportSysexProgram', {});
          this.flashMessage('SYSEX DUMP', '1 PROGRAM TRANSMITTED', 1400);
        },
        onDecrement: () => {
          this.bridge.send('exportSysexBank', {});
          this.flashMessage('SYSEX DUMP', 'ALL DATA TRANSMITTED', 1400);
        }
      }
    ];

    this.onStateSync = options.onStateSync || null;
    this.onExitButton = options.onExitButton || null;
    this.onGlobalMidiChannelChanged = options.onGlobalMidiChannelChanged || null;

    this.attachButtonEvents();
    this.attachBridgeListeners();
    this.loadProgramIndex(0);
    this.bridge.send('getPresetCatalog', {});
  }

  jumpToEditPage(padNum) {
    const idx = Math.max(0, Math.min(this.menuPages.length - 1, (Number(padNum) || 1) - 1));
    this.isGlobalMode = false;
    this.isEditMode = true;
    this.currentPageIndex = idx;
    this.updateDisplay();
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
      const timbreNotice = buildEditTimbreNotice({ timbre: this.activeTimbre });
      this.flashNotice(timbreNotice.line1, timbreNotice.line2, 900);
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
    // PAGE Buttons (PAGE - / PAGE +)
    document.getElementById('btn-lcd-page-next')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        this.currentGlobalPageIndex = (this.currentGlobalPageIndex + 1) % this.globalPages.length;
      } else {
        this.isEditMode = true;
        this.currentPageIndex = (this.currentPageIndex + 1) % this.menuPages.length;
      }
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-page-prev')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        this.currentGlobalPageIndex = (this.currentGlobalPageIndex - 1 + this.globalPages.length) % this.globalPages.length;
      } else {
        this.isEditMode = true;
        this.currentPageIndex = (this.currentPageIndex - 1 + this.menuPages.length) % this.menuPages.length;
      }
      this.updateDisplay();
    });

    // CURSOR Buttons (CURSOR ◄ / CURSOR ►)
    document.getElementById('btn-lcd-cursor-next')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        this.currentGlobalPageIndex = (this.currentGlobalPageIndex + 1) % this.globalPages.length;
      } else {
        this.isEditMode = true;
        this.currentCursorIndex = (this.currentCursorIndex + 1) % 2;
      }
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-cursor-prev')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        this.currentGlobalPageIndex = (this.currentGlobalPageIndex - 1 + this.globalPages.length) % this.globalPages.length;
      } else {
        this.isEditMode = true;
        this.currentCursorIndex = (this.currentCursorIndex - 1 + 2) % 2;
      }
      this.updateDisplay();
    });

    // VALUE Buttons (- / NO and + / YES)
    document.getElementById('btn-lcd-val-up')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        const gPage = this.globalPages[this.currentGlobalPageIndex];
        if (gPage && gPage.onIncrement) gPage.onIncrement();
      } else if (!this.isEditMode) {
        this.nextProgram();
      } else {
        const page = this.menuPages[this.currentPageIndex];
        if (page && page.onIncrement) page.onIncrement();
      }
      this.updateDisplay();
    });

    document.getElementById('btn-lcd-val-down')?.addEventListener('click', () => {
      if (this.isGlobalMode) {
        const gPage = this.globalPages[this.currentGlobalPageIndex];
        if (gPage && gPage.onDecrement) gPage.onDecrement();
      } else if (!this.isEditMode) {
        this.prevProgram();
      } else {
        const page = this.menuPages[this.currentPageIndex];
        if (page && page.onDecrement) page.onDecrement();
      }
      this.updateDisplay();
    });

    // EDIT Button
    document.getElementById('btn-lcd-edit')?.addEventListener('click', () => {
      this.isGlobalMode = false;
      this.isEditMode = !this.isEditMode;
      this.updateDisplay();
    });

    // GLOBAL Button (Hardware MS2000 Global Mode)
    document.getElementById('btn-lcd-global')?.addEventListener('click', () => {
      if (!this.isGlobalMode) {
        this.isGlobalMode = true;
        this.isEditMode = false;
      } else {
        this.currentGlobalPageIndex = (this.currentGlobalPageIndex + 1) % this.globalPages.length;
      }
      this.updateDisplay();
    });

    // EXIT Button (Hardware MS2000 Exit to Program Play Mode)
    document.getElementById('btn-lcd-exit')?.addEventListener('click', () => {
      this.exitToPlayMode();
    });

    // DEMO Button (Hardware EDIT + GLOBAL = DEMO Riff Audition)
    document.getElementById('btn-lcd-demo')?.addEventListener('click', () => {
      this.toggleDemoSequence();
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

    p.raw254 = encodeHardwareProgram254(currentParams, p.name, p);
    p.params = { ...currentParams };
    this.savedSnapshot = { ...currentParams };
    this.editedSnapshot = null;
    this.exitCompareMode(false);

    if (window.__kbd) window.__kbd.sweep('left', 8);
    const writeNotice = buildWriteCompletedNotice({ programId: p.id, programName: p.name });
    this.flashNotice(writeNotice.line1, writeNotice.line2, 1600);
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
    // A notice owns the LCD for its duration: bulk operations echo many params.
    if (Date.now() < this._noticeLockUntil) return;
    if (this.isComparing) {
      this.isComparing = false;
      document.getElementById('btn-compare')?.classList.remove('active');
    }

    const spec = PARAM_LOOKUP[paramId];
    if (!spec) return;

    this.lastEditedParamId = paramId;
    this.updateOriginalValueLed(paramId, value);

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
    const p = prog || this.programs[(this.currentProgramIndex - 1) % this.programs.length];
    if (this.sysexHexElem) {
      const hex = generateMS2000SysExHex(paramStore.values ? Object.fromEntries(paramStore.values) : p.params, p.name, 1, p);
      this.sysexHexElem.textContent = hex;
    }
    if (typeof window !== 'undefined' && typeof window.__onMS2000ParamMutated === 'function') {
      window.__onMS2000ParamMutated();
    }
  }

  /**
   * Notice that wins over parameter echoes until it expires (bulk ops, mode switches).
   */
  flashNotice(line1, line2, duration = 1200) {
    this._noticeLockUntil = Date.now() + duration;
    this.flashMessage(line1, line2, duration);
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

  exitToPlayMode() {
    if (this.isDemoPlaying) {
      this.stopDemoSequence();
    }
    this.isEditMode = false;
    this.isGlobalMode = false;
    if (this.isComparing) {
      this.exitCompareMode(true);
    }
    this.closePresetBrowser();
    if (typeof this.onExitButton === 'function') {
      this.onExitButton();
    }
    this.updateDisplay();
  }

  toggleDemoSequence() {
    if (this.isDemoPlaying) {
      this.stopDemoSequence();
      this.flashMessage('DEMO STOPPED', this.getProgramLabel(), 1000);
    } else {
      this.startDemoSequence();
    }
  }

  startDemoSequence() {
    this.stopDemoSequence();
    this.isDemoPlaying = true;
    this._demoStep = 0;
    document.getElementById('btn-lcd-demo')?.classList.add('active');

    // Classic Korg MS2000 Trance / Electro Demo Pattern (C minor / Eb / F / G)
    const demoPattern = [
      [48, 60, 67], [63], [60], [70],
      [48, 60, 68], [67], [63], [60],
      [44, 56, 63], [67], [68], [72],
      [43, 55, 62], [67], [71], [74]
    ];

    const playStep = () => {
      if (!this.isDemoPlaying) return;
      for (const n of this._demoActiveNotes) {
        this.bridge.noteOff(n);
      }
      this._demoActiveNotes = [];

      const notes = demoPattern[this._demoStep % demoPattern.length] || [60];
      for (const n of notes) {
        this.bridge.noteOn(n, 0.85);
        this._demoActiveNotes.push(n);
      }
      this._demoStep = (this._demoStep + 1) % demoPattern.length;
    };

    const bpm = Math.max(40, Math.min(240, Math.round(paramStore.values.get('arpTempo') ?? 125)));
    const stepMs = Math.round((60000 / bpm) / 2); // 8th-note groove
    this.flashMessage('♪ MS2000 DEMO ♪', `${this.getProgramLabel()} (♩=${bpm})`, 1600);
    playStep();
    this._demoTimer = setInterval(playStep, stepMs);
  }

  stopDemoSequence() {
    if (this._demoTimer) {
      clearInterval(this._demoTimer);
      this._demoTimer = null;
    }
    if (this._demoActiveNotes.length > 0) {
      for (const n of this._demoActiveNotes) {
        this.bridge.noteOff(n);
      }
      this._demoActiveNotes = [];
    }
    this.isDemoPlaying = false;
    document.getElementById('btn-lcd-demo')?.classList.remove('active');
  }

  /**
   * Hardware ORIGINAL VALUE LED: lights up when the active parameter matches the saved program's original value
   */
  updateOriginalValueLed(paramId = null, currentValOverride = undefined) {
    if (!this.origLedElem) this.origLedElem = document.getElementById('led-original-value');
    if (!this.origIndicatorElem) this.origIndicatorElem = document.getElementById('orig-val-indicator');
    if (!this.origLedElem) return;

    let isOriginal = false;
    if (this.isComparing) {
      isOriginal = true;
    } else if (this.savedSnapshot) {
      const targetId = paramId || this.lastEditedParamId;
      if (targetId && this.savedSnapshot[targetId] !== undefined) {
        const cur = currentValOverride !== undefined ? currentValOverride : paramStore.values.get(targetId);
        const orig = this.savedSnapshot[targetId];
        if (cur !== undefined) {
          isOriginal = Math.abs(Number(cur) - Number(orig)) < 0.01;
        }
      } else {
        // In Play mode with no specific param selected, check if current program is unmodified
        isOriginal = true;
        for (const [k, origVal] of Object.entries(this.savedSnapshot)) {
          const curVal = paramStore.values.get(k);
          if (curVal !== undefined && Math.abs(Number(curVal) - Number(origVal)) >= 0.05) {
            isOriginal = false;
            break;
          }
        }
      }
    }

    this.origLedElem.classList.toggle('active', isOriginal);
    this.origIndicatorElem?.classList.toggle('active', isOriginal);
  }

  updateDisplay() {
    document.getElementById('btn-lcd-edit')?.classList.toggle('active', this.isEditMode);
    document.getElementById('btn-lcd-global')?.classList.toggle('active', this.isGlobalMode);
    this.updateOriginalValueLed();

    if (typeof this.onStateSync === 'function') {
      this.onStateSync();
    }
    if (!this.line1Elem || !this.line2Elem) return;
    // A transient notice owns the LCD text until its timer expires, so engine echoes
    // (param refresh, program reload) cannot clobber it before the user reads it.
    if (this._flashTimer) return;

    if (this.isGlobalMode) {
      // Hardware Global Mode
      const gPage = this.globalPages[this.currentGlobalPageIndex] || this.globalPages[0];
      this.line1Elem.textContent = gPage.title;
      this.line2Elem.textContent = `> ${gPage.getValue()}`;
    } else if (!this.isEditMode) {
      // Program Play Mode (Matches ms2000r-emu-main: Line 1 = Program ID & Name, Line 2 = Category · Mode [Timbre] ♩=BPM)
      const p = this.programs[(this.currentProgramIndex - 1) % this.programs.length];
      const cmpTag = this.isComparing ? '[CMP] ' : '';
      this.line1Elem.textContent = `${cmpTag}${p.id} ${p.name}`;
      const voc = Math.round(paramStore.values.get('synthVocoderMode') || 0);
      const tm = voc === 1 ? 3 : Math.round(paramStore.values.get('timbreMode') || 0);
      const modeBadge = VOICE_MODE_NAMES[tm] || 'SINGLE';
      const bpm = Math.round(paramStore.values.get('arpTempo') ?? 120);
      this.line2Elem.textContent = `${p.cat} · ${modeBadge} [T${this.activeTimbre}]  ♩=${bpm}`;
    } else {
      // Edit Parameter Menu Mode
      const page = this.menuPages[this.currentPageIndex];
      this.line1Elem.textContent = page.title;
      this.line2Elem.textContent = `> ${page.getValue()}`;
    }
  }
}
