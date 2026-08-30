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

    // Initialize with default values
    for (const p of PARAM_LIST) {
      this.values.set(p.id, p.default !== undefined ? p.default : 0);
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
  }

  /**
   * Synchronize all parameters from a key-value dictionary (from C++ APVTS or SysEx)
   */
  syncAll(paramsMap) {
    if (!paramsMap) return;
    for (const [id, val] of Object.entries(paramsMap)) {
      this.setParamValue(id, Number(val));
    }
  }

  /**
   * Musical Randomizer Algorithm (JS Implementation for WebAudio / Standalone parity)
   */
  generateMusicalRandom(bridge) {
    const randomInt = (min, max) => Math.floor(Math.random() * (max - min + 1)) + min;
    const randomFloat = (min, max) => Math.random() * (max - min) + min;

    const patch = {
      // Oscillators
      osc1Wave: randomInt(0, 5),
      osc1Ctrl1: Math.round(randomFloat(0, 127)),
      osc1DwgsWave: randomInt(0, 63),
      osc2Wave: randomInt(0, 2),
      osc2ModType: randomInt(0, 3),
      osc2Semitone: randomInt(-12, 12),
      osc2Tune: randomInt(-20, 20),

      // Mixer
      mixOsc1Level: Math.round(randomFloat(85, 127)),
      mixOsc2Level: Math.round(randomFloat(0, 127)),
      mixNoiseLevel: Math.round(randomFloat(0, 25)),

      // Filter
      filterType: randomInt(0, 3),
      filterCutoff: Math.round(randomFloat(45, 127)),
      filterResonance: Math.round(randomFloat(0, 80)),
      filterEg1Int: randomInt(-40, 40),
      filterKeyTrack: randomInt(-30, 30),

      // Envelopes
      eg1Attack: Math.round(randomFloat(0, 60)),
      eg1Decay: Math.round(randomFloat(20, 100)),
      eg1Sustain: Math.round(randomFloat(0, 127)),
      eg1Release: Math.round(randomFloat(10, 80)),

      eg2Attack: Math.round(randomFloat(0, 35)),
      eg2Decay: Math.round(randomFloat(20, 90)),
      eg2Sustain: Math.round(randomFloat(30, 127)),
      eg2Release: Math.round(randomFloat(10, 70)),

      // LFOs
      lfo1Wave: randomInt(0, 3),
      lfo1Freq: Math.round(randomFloat(15, 95)),
      lfo2Wave: randomInt(0, 3),
      lfo2Freq: Math.round(randomFloat(15, 95)),

      // Amp
      ampLevel: 100,
      ampPan: 0,
      ampDistortion: Math.random() > 0.85 ? 1 : 0,

      // Effects
      modFxOn: 1,
      modFxType: randomInt(0, 2),
      modFxSpeed: Math.round(randomFloat(20, 90)),
      modFxDepth: Math.round(randomFloat(30, 100)),
      delayOn: 1,
      delayType: randomInt(0, 2),
      delayTime: Math.round(randomFloat(20, 80)),
      delayDepth: Math.round(randomFloat(20, 70)),
      delayFeedback: Math.round(randomFloat(10, 60))
    };

    // Apply to UI and Bridge
    this.syncAll(patch);

    if (bridge) {
      for (const [id, val] of Object.entries(patch)) {
        bridge.setParam(id, val);
      }
    }
  }

  /**
   * Init Patch (Default State)
   */
  generateInit(bridge) {
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
