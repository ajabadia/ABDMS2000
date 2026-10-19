/**
 * Central Parameter Store and UI Control Registry for ABDMS2000.
 * Automatically synchronizes rotary knobs, select dropdowns, and checkboxes when
 * patches are loaded, randomized, initialized, or modified via SysEx / DAW automation.
 */

import { PARAMETERS_SPEC, PARAM_LOOKUP } from './registry.gen.js';

const PARAM_LIST = PARAMETERS_SPEC.parameters || [];

class ParamStore {
  constructor() {
    this.controls = new Map();
    this.values = new Map();
    this.listeners = new Map();
    this.undoStack = [];
    this.redoStack = [];
    this._suppressHistory = false;
    this._lastUndoParamId = null;
    this._lastUndoTime = 0;

    // Initialize with default values
    for (const p of PARAM_LIST) {
      this.values.set(p.id, p.default !== undefined ? p.default : 0);
    }
  }

  /**
   * Capture a full snapshot of current parameter values onto the Undo stack.
   */
  recordSnapshot(label = 'PARAMETER EDIT') {
    if (this._suppressHistory) return;
    const values = Object.fromEntries(this.values);
    this.undoStack.push({ label, values });
    if (this.undoStack.length > 50) {
      this.undoStack.shift();
    }
    this.redoStack.length = 0;
  }

  canUndo() {
    return this.undoStack.length > 0;
  }

  canRedo() {
    return this.redoStack.length > 0;
  }

  clearHistory() {
    this.undoStack.length = 0;
    this.redoStack.length = 0;
    this._lastUndoParamId = null;
    this._lastUndoTime = 0;
  }

  undo(bridge) {
    if (this.undoStack.length === 0) return { ok: false, label: 'EMPTY' };
    const currentValues = Object.fromEntries(this.values);
    const prev = this.undoStack.pop();
    this.redoStack.push({ label: prev.label, values: currentValues });
    this._applyHistorySnapshot(prev.values, bridge);
    return { ok: true, label: prev.label };
  }

  redo(bridge) {
    if (this.redoStack.length === 0) return { ok: false, label: 'EMPTY' };
    const currentValues = Object.fromEntries(this.values);
    const next = this.redoStack.pop();
    this.undoStack.push({ label: next.label, values: currentValues });
    this._applyHistorySnapshot(next.values, bridge);
    return { ok: true, label: next.label };
  }

  _applyHistorySnapshot(targetValues, bridge) {
    this._suppressHistory = true;
    try {
      for (const [id, val] of Object.entries(targetValues)) {
        const prevVal = this.values.get(id);
        const numVal = Number(val);
        if (prevVal !== numVal) {
          this.setParamValue(id, numVal);
          if (bridge) {
            if (typeof bridge.send === 'function') {
              bridge.send('setParam', { paramId: id, value: numVal });
            } else if (typeof bridge.setParam === 'function') {
              bridge.setParam(id, numVal);
            }
          }
        }
      }
    } finally {
      this._suppressHistory = false;
    }
  }

  /**
   * Get the current value of a parameter
   */
  get(paramId) {
    return this.values.get(paramId);
  }

  getValue(paramId) {
    return this.values.get(paramId);
  }

  /**
   * Subscribe to value changes of a specific parameter.
   */
  onChange(paramId, callback) {
    if (!this.listeners.has(paramId)) {
      this.listeners.set(paramId, new Set());
    }
    this.listeners.get(paramId).add(callback);
  }

  offChange(paramId, callback) {
    if (this.listeners.has(paramId)) {
      this.listeners.get(paramId).delete(callback);
    }
  }

  /**
   * Register a UI element (RotaryKnob instance, HTMLSelectElement, HTMLInputElement)
   */
  register(paramId, control) {
    if (!this.controls.has(paramId)) {
      this.controls.set(paramId, new Set());
    }
    this.controls.get(paramId).add(control);

    // Apply current value if available
    if (this.values.has(paramId)) {
      this.applyToControl(control, this.values.get(paramId));
    }
  }

  unregister(paramId, control) {
    if (this.controls.has(paramId)) {
      this.controls.get(paramId).delete(control);
    }
  }

  applyToControl(control, val) {
    if (!control) return;
    if (typeof control.setValue === 'function') {
      // RotaryKnob instance (pass false to avoid re-triggering onChange bridge loop)
      control.setValue(val, false);
    } else if (control.tagName === 'SELECT') {
      control.value = Math.round(val).toString();
    } else if (control.tagName === 'INPUT') {
      if (control.type === 'checkbox') {
        control.checked = val > 0.5;
      } else {
        control.value = val;
      }
    }
  }

  /**
   * Set a parameter value and update any registered active controls
   */
  set(paramId, val) {
    this.setParamValue(paramId, val);
  }

  /**
   * Update a single parameter visually without re-emitting bridge event
   */
  setParamValue(paramId, val) {
    const prevVal = this.values.get(paramId);
    if (!this._suppressHistory && prevVal !== undefined && prevVal !== val) {
      const now = Date.now();
      if (this._lastUndoParamId !== paramId || (now - this._lastUndoTime) > 450) {
        const meta = PARAM_LOOKUP[paramId];
        this.recordSnapshot(meta ? meta.name.toUpperCase() : paramId);
      }
      this._lastUndoParamId = paramId;
      this._lastUndoTime = now;
    }

    this.values.set(paramId, val);
    const set = this.controls.get(paramId);
    if (set) {
      for (const ctrl of Array.from(set)) {
        const elem = ctrl.container || ctrl;
        if (elem && elem.nodeType && !document.body.contains(elem)) {
          set.delete(ctrl);
          continue;
        }
        this.applyToControl(ctrl, val);
      }
    }
    const cbs = this.listeners.get(paramId);
    if (cbs) {
      for (const cb of cbs) {
        try { cb(val); } catch (e) { console.error('[ParamStore Listener Error]:', e); }
      }
    }
    if (typeof window !== 'undefined' && typeof window.__onMS2000ParamMutated === 'function') {
      window.__onMS2000ParamMutated(paramId, val);
    }
  }

  /**
   * Synchronize all parameters from a key-value dictionary (from C++ APVTS or SysEx)
   */
  syncAll(paramsMap) {
    if (!paramsMap) return;
    const prevSuppress = this._suppressHistory;
    this._suppressHistory = true;
    try {
      for (const [id, val] of Object.entries(paramsMap)) {
        this.setParamValue(id, Number(val));
      }
    } finally {
      this._suppressHistory = prevSuppress;
    }
  }

  /**
   * Musical Randomizer Algorithm (Adapted from ReMS2000 + musical sweet spots)
   */
  generateMusicalRandom(bridge) {
    this.recordSnapshot('RANDOMIZE PATCH');
    const randomInt = (min, max) => Math.floor(Math.random() * (max - min + 1)) + min;
    const musicalIntervals = [-12, -7, -5, 0, 0, 7, 12];
    const stepLengths = [4, 8, 12, 16];

    const patch = {};

    const randomizeTimbre = (prefix) => {
      const pid = (base) =>
        prefix ? prefix + base.charAt(0).toUpperCase() + base.slice(1) : base;

      // 1. Voice Assign, Detune & Portamento (ReMS2000 bounds)
      patch[pid('voiceMode')] = randomInt(0, 2);
      patch[pid('unisonDetune')] = randomInt(5, 45);
      patch[pid('portamentoTime')] = Math.random() < 0.5 ? randomInt(0, 45) : 0;

      // 2. Oscillators: skip AudioIn (7) per ReMS2000 rndValueMan(0, 6)
      patch[pid('osc1Wave')] = randomInt(0, 6);
      patch[pid('osc1Ctrl1')] = randomInt(0, 127);
      patch[pid('osc1Ctrl2')] = randomInt(0, 127);
      patch[pid('osc1DwgsWave')] = randomInt(0, 63);

      patch[pid('osc2Wave')] = randomInt(0, 2);
      const osc2Mod = Math.random() < 0.65 ? 0 : randomInt(1, 3);
      patch[pid('osc2ModType')] = osc2Mod;
      patch[pid('osc2Semitone')] =
        osc2Mod === 0
          ? musicalIntervals[randomInt(0, musicalIntervals.length - 1)]
          : randomInt(-12, 12);
      patch[pid('osc2Tune')] = randomInt(-15, 15);

      // 3. Mixer: ReMS2000 guarantees audible OSC1/OSC2 (45..127) and bounded Noise (* 0.5)
      patch[pid('mixOsc1Level')] = randomInt(65, 127);
      patch[pid('mixOsc2Level')] = randomInt(45, 127);
      patch[pid('mixNoiseLevel')] = randomInt(0, 45);

      // 4. Filter: ReMS2000 HPF rule (if filterType === 3 HPF12, clamp cutoff to 0..40)
      const fType = randomInt(0, 3);
      patch[pid('filterType')] = fType;
      patch[pid('filterCutoff')] = fType === 3 ? randomInt(0, 40) : randomInt(35, 127);
      patch[pid('filterResonance')] = randomInt(0, 38); // ReMS2000: * 0.3
      patch[pid('filterEg1Int')] = randomInt(-25, 25);  // ReMS2000: * 0.4
      patch[pid('filterKeyTrack')] = randomInt(-30, 30);
      patch[pid('filterVelo')] = randomInt(-30, 30);

      // 5. Amp: ReMS2000 level 110..127, pan * 0.4 (-25..+25)
      patch[pid('ampLevel')] = randomInt(110, 127);
      patch[pid('ampPan')] = randomInt(-25, 25);
      patch[pid('ampDistortion')] = Math.random() > 0.8 ? 1 : 0;
      patch[pid('ampKeyTrack')] = randomInt(-25, 25);
      patch[pid('ampVelo')] = randomInt(0, 45);

      // 6. Envelopes: ReMS2000 Attack * 0.3 (0..38) + coupled ADSR sustain/decay
      patch[pid('eg1Attack')] = randomInt(0, 38);
      patch[pid('eg1Decay')] = randomInt(20, 110);
      patch[pid('eg1Sustain')] = randomInt(0, 127);
      patch[pid('eg1Release')] = randomInt(10, 90);

      patch[pid('eg2Attack')] = randomInt(0, 38);
      const eg2Sus = randomInt(0, 127);
      patch[pid('eg2Sustain')] = eg2Sus;
      patch[pid('eg2Decay')] = eg2Sus < 35 ? randomInt(55, 115) : randomInt(20, 105);
      patch[pid('eg2Release')] = randomInt(12, 85);

      // 7. LFOs (ReMS2000)
      patch[pid('lfo1Wave')] = randomInt(0, 3);
      patch[pid('lfo1KeySync')] = randomInt(0, 2);
      patch[pid('lfo1TempoSync')] = Math.random() < 0.5 ? 1 : 0;
      patch[pid('lfo1SyncNote')] = randomInt(0, 14);
      patch[pid('lfo1Freq')] = randomInt(10, 100);

      patch[pid('lfo2Wave')] = randomInt(0, 3);
      patch[pid('lfo2KeySync')] = randomInt(0, 2);
      patch[pid('lfo2TempoSync')] = Math.random() < 0.5 ? 1 : 0;
      patch[pid('lfo2SyncNote')] = randomInt(0, 14);
      patch[pid('lfo2Freq')] = randomInt(10, 100);

      // 8. Virtual Patch 1-4 (ReMS2000: intensity * 0.5 => -31..+31)
      for (let p = 1; p <= 4; p++) {
        patch[pid(`patch${p}Source`)] = randomInt(0, 7);
        patch[pid(`patch${p}Destination`)] = randomInt(0, 7);
        patch[pid(`patch${p}Intensity`)] = Math.random() < 0.5 ? randomInt(-31, 31) : 0;
      }

      // 9. Mod Sequencer (ReMS2000 destination-dependent step bounds)
      patch[pid('modSeqOn')] = Math.random() < 0.35 ? 1 : 0;
      patch[pid('modSeqType')] = randomInt(0, 3);
      patch[pid('modSeqResolution')] = randomInt(2, 8);
      patch[pid('seqLastStep')] = stepLengths[randomInt(0, stepLengths.length - 1)];
      patch[pid('seqRunMode')] = randomInt(0, 1);
      patch[pid('seqKeySync')] = randomInt(0, 2);

      for (let lane = 1; lane <= 3; lane++) {
        const dest = randomInt(0, 30);
        patch[pid(`seq${lane}Dest`)] = dest;
        patch[pid(`seq${lane}Motion`)] = randomInt(0, 1);
        const maxStepVal = dest === 2 ? 6 : (dest === 1 || dest === 6 ? 24 : 63);
        for (let step = 1; step <= 16; step++) {
          patch[pid(`seq${lane}Step${step}`)] = randomInt(-maxStepVal, maxStepVal);
        }
      }
    };

    randomizeTimbre('');
    randomizeTimbre('t2');

    // 10. Global Effects & EQ (ReMS2000: EQ gain -4..+12 dB => 60..76 in 0..127 scale)
    patch.eqLowFreq = randomInt(0, 3);
    patch.eqLowGain = randomInt(60, 76);
    patch.eqHighFreq = randomInt(0, 3);
    patch.eqHighGain = randomInt(60, 76);

    patch.modFxOn = Math.random() < 0.75 ? 1 : 0;
    patch.modFxType = randomInt(0, 2);
    patch.modFxSpeed = randomInt(15, 90);
    patch.modFxDepth = randomInt(25, 95);

    patch.delayOn = Math.random() < 0.70 ? 1 : 0;
    patch.delayType = randomInt(0, 2);
    patch.delayTime = randomInt(20, 95);
    patch.delayDepth = randomInt(20, 75);
    patch.delayFeedback = randomInt(15, 75);

    // 11. Arpeggiator (ReMS2000: Gate 33..127)
    patch.arpOn = Math.random() < 0.30 ? 1 : 0;
    patch.arpLatch = Math.random() < 0.5 ? 1 : 0;
    patch.arpType = randomInt(0, 5);
    patch.arpRange = randomInt(1, 4);
    patch.arpGate = randomInt(33, 127);
    patch.arpResolution = randomInt(2, 7);

    // Apply to UI and Bridge
    this.syncAll(patch);

    if (bridge && typeof bridge.setParam === 'function') {
      for (const [id, val] of Object.entries(patch)) {
        bridge.setParam(id, val);
      }
    }
  }

  /**
   * Init Patch (Default State)
   */
  generateInit(bridge) {
    this.recordSnapshot('INIT PATCH');
    const patch = {};
    for (const p of PARAM_LIST) {
      patch[p.id] = p.default !== undefined ? p.default : 0;
    }
    this.syncAll(patch);


    if (bridge) {
      for (const [id, val] of Object.entries(patch)) {
        bridge.setParam(id, val);
      }
    }
  }
}

export const paramStore = new ParamStore();
export { PARAM_LIST };
