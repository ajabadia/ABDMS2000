import { ms2000AudioEngine } from '../engine/ms2000AudioEngine.js';

export class BridgeWasm {
  constructor() {
    this.engine = ms2000AudioEngine;
    this.isInitialized = false;
  }

  async initAudio() {
    this.isInitialized = await this.engine.init();
    return this.isInitialized;
  }

  noteOn(note, velocity = 0.8) {
    this.engine.noteOn(note, velocity);
  }

  noteOff(note) {
    this.engine.noteOff(note);
  }

  allNotesOff() {
    this.engine.allNotesOff();
  }

  midiCC(cc, value) {
    if (this.engine && typeof this.engine.midiCC === 'function') {
      this.engine.midiCC(cc, value);
    }
  }

  pitchBend(value) {
    if (this.engine && typeof this.engine.pitchBend === 'function') {
      this.engine.pitchBend(value);
    }
  }

  modWheel(value) {
    if (this.engine && typeof this.engine.modWheel === 'function') {
      this.engine.modWheel(value);
    }
  }

  setParam(paramId, value) {
    this.engine.setParam(paramId, value);
  }

  setAllParams(paramsObj) {
    this.engine.setAllParams(paramsObj);
  }

  loadProgram(programIndex) {
    // Handled via factoryPresets & paramStore
  }

  initPatch() {
    // Handled via paramStore
  }

  randomizePatch() {
    // Handled via paramStore
  }

  setDiagnosticTone(point, frequency = 440.0, level = 0.25) {
    if (this.engine && typeof this.engine.setDiagnosticTone === 'function') {
      this.engine.setDiagnosticTone(point, frequency, level);
    }
  }

  triggerDiagnosticNote(note, velocity = 0.8, isNoteOn = true) {
    if (isNoteOn) {
      this.noteOn(note, velocity);
    } else {
      this.noteOff(note);
    }
  }

  setDiagnosticBypass(stage, enabled) {
    if (this.engine && typeof this.engine.setDiagnosticBypass === 'function') {
      this.engine.setDiagnosticBypass(stage, enabled);
    }
  }

  resetDiagnosticBypasses() {
    if (this.engine && typeof this.engine.resetDiagnosticBypasses === 'function') {
      this.engine.resetDiagnosticBypasses();
    }
  }
}

export const wasmBridge = new BridgeWasm();

