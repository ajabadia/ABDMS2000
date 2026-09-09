/**
 * BridgeWasm — Connects the WebUI to the real C++ DSP engine compiled to WASM.
 *
 * Architecture:
 *   Main thread:  AudioContext + AudioWorkletNode (message passing)
 *   Worklet:      WebAssembly instance rendering audio via _processAudio()
 *
 * The WASM binary is embedded in ms2000_dsp.js (Emscripten SINGLE_FILE build).
 * We extract it on the main thread and transfer it to the worklet as an ArrayBuffer.
 */

export class BridgeWasm {
  constructor() {
    this.audioCtx = null;
    this.workletNode = null;
    this.masterGain = null;
    this.isInitialized = false;
    this._snapshotCallback = null;
  }

  /** Alias for backward compatibility (OscilloscopeModal uses wasmBridge.audioContext) */
  get audioContext() {
    return this.audioCtx;
  }

  get masterGainNode() {
    return this.masterGain;
  }

  async initAudio() {
    if (this.isInitialized) return true;

    try {
      // 1. Create AudioContext
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      this.audioCtx = new AudioCtx({ latencyHint: 'interactive' });
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }

      // 2. Register AudioWorklet processor
      await this.audioCtx.audioWorklet.addModule('src/wasm/ms2000Worklet.js');

      // 3. Extract WASM binary from the Emscripten module file
      const wasmBinary = await this._extractWasmBinary();

      // 4. Create worklet node
      this.workletNode = new AudioWorkletNode(
        this.audioCtx,
        'ms2000-worklet-processor'
      );

      // 5. Set up message handler (telemetry from worklet)
      this.workletNode.port.onmessage = (event) => {
        const data = event.data;
        if (data.type === 'WASM_READY') {
          console.log('[BridgeWasm] WASM engine ready');
        } else if (data.type === 'SNAPSHOT' && this._snapshotCallback) {
          this._snapshotCallback(data.snapshot);
        } else if (data.type === 'WASM_ERROR') {
          console.error('[BridgeWasm] WASM error in worklet:', data.error);
        }
      };

      // 6. Send WASM binary to worklet for instantiation
      this.workletNode.port.postMessage({
        type: 'FETCH_WASM',
        binary: wasmBinary,
        sampleRate: this.audioCtx.sampleRate
      });

      // 7. Master gain → destination
      this.masterGain = this.audioCtx.createGain();
      this.masterGain.gain.value = 0.85;
      this.workletNode.connect(this.masterGain);
      this.masterGain.connect(this.audioCtx.destination);

      this.isInitialized = true;
      console.log(
        '[BridgeWasm] WASM AudioWorklet pipeline ready @',
        this.audioCtx.sampleRate,
        'Hz'
      );
      return true;
    } catch (err) {
      console.error('[BridgeWasm] Init failed:', err);
      return false;
    }
  }

  /**
   * Extract the raw WASM binary from ms2000_dsp.js.
   * The Emscripten SINGLE_FILE build embeds the WASM binary as a
   * binary-decoded string inside the findWasmBinary() function.
   *
   * NOTE: We use index-based extraction instead of regex because the
   * binary data contains 115+ embedded single-quote characters that
   * break [^'] regex patterns. The sequence ')} does NOT appear in
   * the binary data, making it a reliable end marker.
   */
  async _extractWasmBinary() {
    // Fetch the Emscripten JS file as text
    const response = await fetch('src/wasm/ms2000_dsp.js');
    const jsSource = await response.text();

    // Find the binary string boundaries using indexOf
    const startMarker = "findWasmBinary(){return binaryDecode('";
    const startIdx = jsSource.indexOf(startMarker);
    if (startIdx < 0) {
      throw new Error('Could not find findWasmBinary() in ms2000_dsp.js');
    }
    const binStart = startIdx + startMarker.length;

    // The binary string is terminated by ')} — this sequence does not
    // appear inside the encoded binary data (embedded quotes are never
    // followed by a closing paren).
    const binEnd = jsSource.indexOf("')}", binStart);
    if (binEnd < 0) {
      throw new Error('Could not find end of binary string in ms2000_dsp.js');
    }

    const binStr = jsSource.substring(binStart, binEnd);
    console.log(
      '[BridgeWasm] Binary string extracted:',
      binStr.length,
      'chars'
    );

    // Decode the binary string to Uint8Array (same as Emscripten's binaryDecode)
    const bytes = new Uint8Array(binStr.length);
    for (let i = 0; i < binStr.length; i++) {
      const c = binStr.charCodeAt(i);
      bytes[i] = (~c >> 8) & c;
    }

    // Verify WASM magic bytes (\0asm)
    if (bytes[0] !== 0x00 || bytes[1] !== 0x61 || bytes[2] !== 0x73 || bytes[3] !== 0x6d) {
      throw new Error('Extracted data is not a valid WASM binary (bad magic bytes)');
    }

    console.log(
      '[BridgeWasm] Extracted WASM binary:',
      (bytes.length / 1024).toFixed(1),
      'KB'
    );
    return bytes.buffer;
  }

  // ─── Command forwarding to WASM worklet ───

  _post(msg) {
    if (this.workletNode) {
      this.workletNode.port.postMessage(msg);
    }
  }

  noteOn(note, velocity = 0.8) {
    this._post({ type: 'NOTE_ON', note, velocity });
  }

  noteOff(note) {
    this._post({ type: 'NOTE_OFF', note });
  }

  allNotesOff() {
    this._post({ type: 'ALL_NOTES_OFF' });
  }

  midiCC(cc, value) {
    // WASM engine handles MIDI CC via setParamById
    // Map common CCs to parameter IDs
    if (cc === 120 || cc === 123) {
      this.allNotesOff();
    }
  }

  pitchBend(value) {
    // value is -1..+1, map to pitch bend parameter
    // The WASM engine may handle this via a dedicated function if exported
  }

  modWheel(value) {
    // value is 0..1
  }

  setParam(paramId, value) {
    this._post({ type: 'SET_PARAM', paramId, value });
  }

  setAllParams(paramsObj) {
    if (!paramsObj) return;
    for (const [key, value] of Object.entries(paramsObj)) {
      this._post({ type: 'SET_PARAM', paramId: key, value });
    }
  }

  loadProgram(programIndex) {
    this._post({ type: 'LOAD_PROGRAM', programIndex });
  }

  initPatch() {
    this._post({ type: 'INIT_PATCH' });
  }

  randomizePatch() {
    this._post({ type: 'RANDOMIZE_PATCH' });
  }

  // ─── Telemetry ───

  onSnapshot(callback) {
    this._snapshotCallback = callback;
  }

  // ─── Diagnostic (stubs — can be wired to WASM if exported) ───

  setDiagnosticTone(point, frequency = 440.0, level = 0.25) {
    // Not implemented in WASM build yet
  }

  triggerDiagnosticNote(note, velocity = 0.8, isNoteOn = true) {
    if (isNoteOn) {
      this.noteOn(note, velocity);
    } else {
      this.noteOff(note);
    }
  }

  setDiagnosticBypass(stage, enabled) {
    // Not implemented in WASM build yet
  }

  resetDiagnosticBypasses() {
    // Not implemented in WASM build yet
  }
}

export const wasmBridge = new BridgeWasm();
