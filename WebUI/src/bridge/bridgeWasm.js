/**
 * BridgeWasm — Connects the WebUI to the real C++ DSP engine compiled to WASM.
 *
 * Architecture:
 *   Main thread:  AudioContext + AudioWorkletNode (message passing)
 *   Worklet:      WebAssembly instance rendering audio via _processAudio()
 *
 * The WASM binary is a separate .wasm file (not SINGLE_FILE).
 * The main thread fetches it and transfers the ArrayBuffer to the worklet,
 * which instantiates WebAssembly with the required Emscripten imports.
 */

const IS_DEV = import.meta.env?.DEV === true;

export class BridgeWasm {
  constructor() {
    this.audioCtx = null;
    this.workletNode = null;
    this.masterGain = null;
    this.isInitialized = false;
    this._snapshotCallback = null;
    this._initPromise = null;
    this._readyTimeoutMs = 10000;
    this._pendingMessages = [];
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
    if (this._initPromise) return this._initPromise;

    this._initPromise = this._initAudioPipeline();
    try {
      return await this._initPromise;
    } finally {
      this._initPromise = null;
    }
  }

  async _initAudioPipeline() {
    try {
      // 1. Create AudioContext
      const AudioCtx = window.AudioContext || window.webkitAudioContext;
      this.audioCtx = new AudioCtx({ latencyHint: 'interactive' });
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }

      // 2. Register AudioWorklet processor
      await this.audioCtx.audioWorklet.addModule('src/wasm/ms2000Worklet.js');

      // 3. Fetch the WASM binary
      const response = await fetch('src/wasm/ms2000_dsp.wasm');
      if (!response.ok) {
        throw new Error(`Failed to fetch WASM: ${response.status}`);
      }
      const wasmBinary = await response.arrayBuffer();
      if (IS_DEV) {
        console.log(
          '[BridgeWasm] Fetched WASM binary:',
          (wasmBinary.byteLength / 1024).toFixed(1),
          'KB'
        );
      }

      // 4. Create worklet node
      this.workletNode = new AudioWorkletNode(
        this.audioCtx,
        'ms2000-worklet-processor',
        {
          numberOfInputs: 0,
          numberOfOutputs: 1,
          outputChannelCount: [2],
        }
      );

      // WASM_READY is emitted only after the module, constructors and engine
      // have initialized. Do not advertise an initialized audio pipeline before it.
      const readyPromise = new Promise((resolve, reject) => {
        const timeout = setTimeout(
          () => reject(new Error('Timed out waiting for WASM_READY')),
          this._readyTimeoutMs
        );
        this._resolveWasmReady = () => {
          clearTimeout(timeout);
          resolve();
        };
        this._rejectWasmReady = (error) => {
          clearTimeout(timeout);
          reject(error);
        };
      });

      // 5. Set up message handler (telemetry and initialization status)
      this.workletNode.port.onmessage = (event) => {
        const data = event.data;
        if (data.type === 'WASM_READY') {
          if (IS_DEV) console.log('[BridgeWasm] WASM engine ready (renderer: raw-wasm-cpp)');
          this._resolveWasmReady?.();
          this._resolveWasmReady = null;
          this._rejectWasmReady = null;
        } else if (data.type === 'WASM_RENDERING') {
          if (IS_DEV) {
            console.log('[BridgeWasm] Audio rendering confirmed by raw C++ WASM:', data.blockSize, 'samples');
          }
        } else if (data.type === 'SNAPSHOT' && this._snapshotCallback) {
          this._snapshotCallback(data.snapshot);
        } else if (data.type === 'WASM_ERROR') {
          const error = new Error(data.error || 'WASM initialization failed');
          console.error('[BridgeWasm] WASM error in worklet:', error.message);
          this._rejectWasmReady?.(error);
          this._resolveWasmReady = null;
          this._rejectWasmReady = null;
        }
      };

      // 6. Send WASM binary to worklet for instantiation
      this.workletNode.port.postMessage({
        type: 'INIT_WASM',
        binary: wasmBinary,
        sampleRate: this.audioCtx.sampleRate,
        debug: IS_DEV
      });

      // 7. Master gain → destination
      this.masterGain = this.audioCtx.createGain();
      this.masterGain.gain.value = 0.85;
      this.workletNode.connect(this.masterGain);
      this.masterGain.connect(this.audioCtx.destination);

      await readyPromise;
      this.isInitialized = true;
      this._flushPendingMessages();
      if (IS_DEV) {
        console.log(
          '[BridgeWasm] WASM AudioWorklet pipeline ready @',
          this.audioCtx.sampleRate,
          'Hz'
        );
      }
      return true;
    } catch (err) {
      console.error('[BridgeWasm] Init failed:', err);
      this.isInitialized = false;
      this._resolveWasmReady = null;
      this._rejectWasmReady = null;
      this._pendingMessages = [];
      try {
        this.workletNode?.disconnect();
        this.masterGain?.disconnect();
        await this.audioCtx?.close();
      } catch (_) {
        // Cleanup is best effort; the next user gesture can retry initialization.
      }
      this.workletNode = null;
      this.masterGain = null;
      this.audioCtx = null;
      return false;
    }
  }

  // ─── Command forwarding to WASM worklet ───

  _post(msg) {
    if (this.isInitialized && this.workletNode) {
      this.workletNode.port.postMessage(msg);
      return;
    }

    // Keyboard input can arrive from the same pointer gesture that starts
    // AudioContext/WASM initialization. Preserve command order instead of
    // silently dropping the first note while the worklet is still booting.
    if (this._initPromise || this.workletNode) {
      this._pendingMessages.push(msg);
    }
  }

  _flushPendingMessages() {
    if (!this.isInitialized || !this.workletNode || this._pendingMessages.length === 0) {
      return;
    }
    const pending = this._pendingMessages;
    this._pendingMessages = [];
    for (const message of pending) {
      this.workletNode.port.postMessage(message);
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
    if (cc === 120 || cc === 123) {
      this.allNotesOff();
    }
  }

  pitchBend(value) {
    // value is -1..+1
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

  // ─── Diagnostic ───

  setDiagnosticTone(point, frequency = 440.0, level = 0.25) {
    // Diagnostic controls can be used before the first keyboard note. Start
    // the pipeline here as well, then queue the command until WASM is ready.
    if (point > 0 && !this.isInitialized && !this._initPromise) {
      this.initAudio();
    }
    this._post({
      type: 'DIAGNOSTIC_TONE',
      point: Math.max(0, Math.min(5, Number(point) || 0)),
      frequency: Math.max(20, Math.min(10000, Number(frequency) || 440)),
      level: Math.max(0, Math.min(1, Number(level) || 0)),
    });
  }

  triggerDiagnosticNote(note, velocity = 0.8, isNoteOn = true) {
    if (isNoteOn) {
      this.noteOn(note, velocity);
    } else {
      this.noteOff(note);
    }
  }

  setDiagnosticBypass(stage, enabled) {
    this._post({ type: 'DIAGNOSTIC_BYPASS', stage, enabled: !!enabled });
  }

  resetDiagnosticBypasses() {
    this._post({ type: 'RESET_DIAGNOSTIC_BYPASSES' });
  }
}

export const wasmBridge = new BridgeWasm();
