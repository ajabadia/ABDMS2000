/**
 * MS2000 AudioWorkletProcessor
 * Runs the Emscripten-compiled C++ DSP engine as a raw WebAssembly module
 * inside an AudioWorklet thread.
 *
 * Receives the raw WASM binary from the main thread via FETCH_WASM message,
 * instantiates WebAssembly directly, and renders audio via _processAudio.
 *
 * IMPORTANT: When using raw WebAssembly.instantiate(), we don't get the
 * Emscripten JS runtime (HEAPF32, stringToUTF8, etc.). We create our own
 * typed array views from the WASM memory export.
 */
class MS2000WorkletProcessor extends AudioWorkletProcessor {
  constructor() {
    super();

    this.isReady = false;
    this.wasm = null;

    // WASM memory views (created from exports.memory)
    this.HEAPF32 = null;
    this.HEAP32 = null;

    // Pre-allocated buffer pointers (offsets in WASM linear memory)
    this.outLPtr = 0;
    this.outRPtr = 0;
    this.scopePtr = 0;
    this.vuLPtr = 0;
    this.vuRPtr = 0;
    this.activeVoicesPtr = 0;

    // Bump allocator pointer (grows upward from a base offset)
    this._allocBase = 0;
    this._allocPtr = 0;

    this.port.onmessage = (event) => {
      this.handleMessage(event.data);
    };
  }

  /**
   * Simple bump allocator for WASM linear memory.
   * Allocates 'size' bytes aligned to 'align' bytes.
   * Falls back to _malloc if available.
   */
  _allocate(size, align = 16) {
    // If _malloc is available (rebuild with EXPORTED_FUNCTIONS includes it),
    // use it. Otherwise use a bump allocator.
    if (this.wasm._malloc) {
      return this.wasm._malloc(size);
    }

    // Bump allocator: align the pointer, then advance
    const aligned = (this._allocPtr + align - 1) & ~(align - 1);
    this._allocPtr = aligned + size;
    return aligned;
  }

  handleMessage(data) {
    if (!data) return;

    switch (data.type) {
      case 'FETCH_WASM':
        this.initWasmFromBinary(data.binary, data.sampleRate || sampleRate);
        break;

      case 'INIT_WASM':
        this.initWasmModule(data.wasmModule);
        break;

      default:
        if (this.isReady && this.wasm) {
          this.handleAudioMessage(data);
        }
        break;
    }
  }

  handleAudioMessage(data) {
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
      case 'SET_PARAM': {
        if (data.paramId) {
          const paramId = data.paramId;
          const lengthBytes = paramId.length + 1;
          const stringPtr = this._allocate(lengthBytes);
          // Manual stringToUTF8: write UTF-8 bytes into WASM memory
          const heap = this.HEAPU8;
          for (let i = 0; i < paramId.length; i++) {
            heap[stringPtr + i] = paramId.charCodeAt(i);
          }
          heap[stringPtr + paramId.length] = 0; // null terminator
          this.wasm._setParamById(stringPtr, data.value);
          // No free needed with bump allocator; with _malloc we'd call _free
          if (this.wasm._free) this.wasm._free(stringPtr);
        }
        break;
      }
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

  async initWasmFromBinary(wasmBinary, sr) {
    try {
      const wasmModule = await WebAssembly.instantiate(wasmBinary);
      this.wasm = wasmModule.instance.exports;

      // Create typed array views from WASM memory
      // (Emscripten normally does this in the JS glue via HEAPF32 etc.)
      if (!this.wasm.memory) {
        throw new Error('WASM module does not export memory');
      }

      this._updateMemoryViews();

      // Initialize the DSP engine with the worklet sample rate
      this.wasm._initEngine(sr);

      // Set up bump allocator base: start after 1MB (safe zone above static data)
      this._allocBase = 1024 * 1024;
      this._allocPtr = this._allocBase;

      // Allocate output buffer pointers (128 samples per AudioWorklet block)
      this.outLPtr = this._allocate(128 * 4);
      this.outRPtr = this._allocate(128 * 4);

      // Telemetry buffer pointers
      this.scopePtr = this._allocate(512 * 4);
      this.vuLPtr = this._allocate(4);
      this.vuRPtr = this._allocate(4);
      this.activeVoicesPtr = this._allocate(4);

      this.isReady = true;
      this.port.postMessage({ type: 'WASM_READY' });
      console.log('[MS2000Worklet] WASM engine initialized @', sr, 'Hz');
    } catch (err) {
      console.error('[MS2000Worklet] WASM init error:', err);
      this.port.postMessage({ type: 'WASM_ERROR', error: err.message });
    }
  }

  /**
   * Create/update HEAP views from WASM memory buffer.
   * Re-called if memory grows (buffer is detached and replaced).
   */
  _updateMemoryViews() {
    const buf = this.wasm.memory.buffer;
    this.HEAP8 = new Int8Array(buf);
    this.HEAPU8 = new Uint8Array(buf);
    this.HEAP16 = new Int16Array(buf);
    this.HEAP32 = new Int32Array(buf);
    this.HEAPF32 = new Float32Array(buf);
    this.HEAPF64 = new Float64Array(buf);
  }

  initWasmModule(wasmModule) {
    // Legacy path: accept a pre-built Emscripten Module object
    this.wasm = wasmModule;
    if (this.wasm && this.wasm._initEngine) {
      this.wasm._initEngine(sampleRate);

      // Use the Module's own HEAP views if available
      if (this.wasm.HEAPF32) {
        this.HEAPF32 = this.wasm.HEAPF32;
        this.HEAP32 = this.wasm.HEAP32;
        this.HEAPU8 = this.wasm.HEAPU8 || new Uint8Array(this.wasm.memory.buffer);
      }

      this._allocBase = 1024 * 1024;
      this._allocPtr = this._allocBase;

      this.outLPtr = this._allocate(128 * 4);
      this.outRPtr = this._allocate(128 * 4);
      this.scopePtr = this._allocate(512 * 4);
      this.vuLPtr = this._allocate(4);
      this.vuRPtr = this._allocate(4);
      this.activeVoicesPtr = this._allocate(4);

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

    // Check if WASM memory was detached (growth) and update views
    if (this.HEAPF32.buffer !== this.wasm.memory.buffer) {
      this._updateMemoryViews();
    }

    // 1. Render Audio Block via C++ WASM
    this.wasm._processAudio(this.outLPtr, this.outRPtr, numSamples);

    // 2. Zero-Copy Subarray Mapping to Web Audio Channel Buffers
    const wasmHeapL = this.HEAPF32.subarray(
      this.outLPtr >> 2,
      (this.outLPtr >> 2) + numSamples
    );
    const wasmHeapR = this.HEAPF32.subarray(
      this.outRPtr >> 2,
      (this.outRPtr >> 2) + numSamples
    );

    outL.set(wasmHeapL);
    outR.set(wasmHeapR);

    // 3. Post Telemetry periodically (~20% of blocks)
    if (Math.random() < 0.2) {
      this.wasm._getAudioSnapshot(
        this.scopePtr, this.vuLPtr, this.vuRPtr, this.activeVoicesPtr
      );

      const scopeData = new Float32Array(
        this.HEAPF32.buffer.slice(
          this.scopePtr,
          this.scopePtr + 512 * 4
        )
      );
      const vuLeft = this.HEAPF32[this.vuLPtr >> 2];
      const vuRight = this.HEAPF32[this.vuRPtr >> 2];
      const activeVoices = this.HEAP32[this.activeVoicesPtr >> 2];

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
