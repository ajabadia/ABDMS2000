/**
 * MS2000 AudioWorkletProcessor
 * High-performance Zero-Copy Web Audio Worklet node running Emscripten WASM Core.
 */

class MS2000WorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this.isReady = false;
    this.outLPtr = 0;
    this.outRPtr = 0;
    this.scopePtr = 0;
    this.vuLPtr = 0;
    this.vuRPtr = 0;
    this.activeVoicesPtr = 0;

    this.port.onmessage = (event) => {
      this.handleMessage(event.data);
    };
  }

  handleMessage(data) {
    if (!data) return;

    if (data.type === 'INIT_WASM') {
      this.initWasmModule(data.wasmModule);
    } else if (this.isReady && this.wasm) {
      switch (data.type) {
        case 'NOTE_ON':
          this.wasm._noteOn(data.note, data.velocity);
          break;
        case 'NOTE_OFF':
          this.wasm._noteOff(data.note);
          break;
        case 'ALL_NOTES_OFF':
          this.wasm._allNotesOff();
          break;
        case 'SET_PARAM':
          if (data.paramId) {
            const lengthBytes = (data.paramId.length + 1);
            const stringPtr = this.wasm._malloc(lengthBytes);
            this.wasm.stringToUTF8(data.paramId, stringPtr, lengthBytes);
            this.wasm._setParamById(stringPtr, data.value);
            this.wasm._free(stringPtr);
          }
          break;
        case 'LOAD_PROGRAM':
          this.wasm._loadProgram(data.programIndex);
          break;
        case 'INIT_PATCH':
          this.wasm._initPatch();
          break;
        case 'RANDOMIZE_PATCH':
          this.wasm._randomizePatch();
          break;
      }
    }
  }

  initWasmModule(wasmModule) {
    this.wasm = wasmModule;
    if (this.wasm && this.wasm._initEngine) {
      this.wasm._initEngine(sampleRate);

      // Allocate output buffer pointers (128 samples per AudioWorklet block)
      this.outLPtr = this.wasm._malloc(128 * 4);
      this.outRPtr = this.wasm._malloc(128 * 4);

      // Telemetry pointers
      this.scopePtr = this.wasm._malloc(512 * 4);
      this.vuLPtr = this.wasm._malloc(4);
      this.vuRPtr = this.wasm._malloc(4);
      this.activeVoicesPtr = this.wasm._malloc(4);

      this.isReady = true;
      this.port.postMessage({ type: 'WASM_READY' });
    }
  }

  process(inputs, outputs, parameters) {
    if (!this.isReady || !this.wasm) return true;

    const output = outputs[0];
    if (!output || output.length < 2) return true;

    const outL = output[0];
    const outR = output[1];
    const numSamples = outL.length; // Standard 128 samples

    // 1. Render Audio Block via C++ WASM
    this.wasm._processAudio(this.outLPtr, this.outRPtr, numSamples);

    // 2. Zero-Copy Subarray Mapping to Web Audio Channel Buffers
    const wasmHeapL = this.wasm.HEAPF32.subarray(this.outLPtr >> 2, (this.outLPtr >> 2) + numSamples);
    const wasmHeapR = this.wasm.HEAPF32.subarray(this.outRPtr >> 2, (this.outRPtr >> 2) + numSamples);

    outL.set(wasmHeapL);
    outR.set(wasmHeapR);

    // 3. Post Telemetry periodically
    if (Math.random() < 0.2) {
      this.wasm._getAudioSnapshot(this.scopePtr, this.vuLPtr, this.vuRPtr, this.activeVoicesPtr);
      const scopeData = new Float32Array(this.wasm.HEAPF32.subarray(this.scopePtr >> 2, (this.scopePtr >> 2) + 512));
      const vuLeft = this.wasm.HEAPF32[this.vuLPtr >> 2];
      const vuRight = this.wasm.HEAPF32[this.vuRPtr >> 2];
      const activeVoices = this.wasm.HEAP32[this.activeVoicesPtr >> 2];

      this.port.postMessage({
        type: 'SNAPSHOT',
        snapshot: {
          scopeBuffer: Array.from(scopeData),
          vuLeft,
          vuRight,
          activeVoices
        }
      });
    }

    return true;
  }
}

registerProcessor('ms2000-worklet-processor', MS2000WorkletProcessor);
