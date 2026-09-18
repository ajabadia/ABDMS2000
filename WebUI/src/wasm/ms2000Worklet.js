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
    this.debug = false;
    this._renderNoticeSent = false;

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

    // Browser-only diagnostic injection. The native engine owns the complete
    // diagnostic chain; the raw WASM bridge has no exported diagnostic API, so
    // the worklet provides the same audible end-of-chain probe.
    this.diagnosticPoint = 0;
    this.diagnosticFrequency = 440;
    this.diagnosticLevel = 0;
    this.diagnosticPhase = 0;
    this.diagnosticBypasses = new Map();

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
    if (this.wasm.malloc) {
      return this.wasm.malloc(size);
    }

    // Bump allocator: align the pointer, then advance
    const aligned = (this._allocPtr + align - 1) & ~(align - 1);
    this._allocPtr = aligned + size;
    return aligned;
  }

  handleMessage(data) {
    if (!data) return;

    switch (data.type) {
      case 'INIT_WASM':
        this.debug = data.debug === true;
        this.initWasmFromBinary(data.binary, data.sampleRate || sampleRate);
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
        this.wasm.noteOn(data.note, data.velocity);
        break;
      case 'NOTE_OFF':
        this.wasm.noteOff(data.note);
        break;
      case 'ALL_NOTES_OFF':
        this.wasm.allNotesOff();
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
          this.wasm.setParamById(stringPtr, data.value);
          // No free needed with bump allocator; with _malloc we'd call _free
          if (this.wasm.free) this.wasm.free(stringPtr);
        }
        break;
      }
      case 'LOAD_PROGRAM':
        this.wasm.loadProgram(data.programIndex);
        break;
      case 'INIT_PATCH':
        this.wasm.initPatch();
        break;
      case 'RANDOMIZE_PATCH':
        this.wasm.randomizePatch();
        break;
      case 'DIAGNOSTIC_TONE':
        this.diagnosticPoint = data.point > 0 ? data.point : 0;
        this.diagnosticFrequency = Math.max(20, Math.min(10000, data.frequency || 440));
        this.diagnosticLevel = Math.max(0, Math.min(1, data.level || 0));
        if (this.diagnosticPoint === 0) this.diagnosticPhase = 0;
        break;
      case 'DIAGNOSTIC_BYPASS':
        this.diagnosticBypasses.set(data.stage, !!data.enabled);
        break;
      case 'RESET_DIAGNOSTIC_BYPASSES':
        this.diagnosticBypasses.clear();
        break;
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

  /**
   * Initialize from raw WASM binary with Emscripten-required imports.
   * The Emscripten SINGLE_FILE build needs these imports to function:
   *   a: ___cxa_throw  (exception handling)
   *   b: _emscripten_resize_heap (memory growth)
   *   c: __abort_js    (abort handler)
   */
  async initWasmFromBinary(wasmBinary, sr) {
    try {
      if (this.debug) {
        console.log('[MS2000Worklet] Received WASM binary:', wasmBinary.byteLength, 'bytes');
      }

      // Emscripten WASM expects these imports in BOTH 'env' and
      // 'wasi_snapshot_preview1' namespaces (see getWasmImports() in the glue code).
      const envImports = {
        __cxa_throw: (ptr, type, destructor) => {
          console.error('[MS2000Worklet] C++ exception (ptr=' + ptr + ')');
        },
        _abort_js: () => {
          console.error('[MS2000Worklet] WASM abort');
        },
        emscripten_resize_heap: (requestedSize) => {
          const oldSize = this.HEAPU8 ? this.HEAPU8.length : 0;
          const pages = ((requestedSize - oldSize + 65535) / 65536) | 0;
          try {
            this.wasm.memory.grow(pages);
            this._updateMemoryViews();
            return 1;
          } catch (e) {
            return 0;
          }
        },
      };
      const wasmImports = {
        env: envImports,
        wasi_snapshot_preview1: envImports,
      };

      if (this.debug) console.log('[MS2000Worklet] Instantiating WASM...');
      const result = await WebAssembly.instantiate(wasmBinary, wasmImports);
      if (this.debug) {
        console.log('[MS2000Worklet] Instantiation result:', Object.keys(result));
        console.log('[MS2000Worklet] Has instance:', !!result.instance);
        console.log('[MS2000Worklet] Has exports:', result.instance ? Object.keys(result.instance.exports).join(', ') : 'NONE');
      }
      this.wasm = result.instance.exports;

      if (!this.wasm || !this.wasm.memory) {
        throw new Error('WASM module does not export memory. Exports: ' + (this.wasm ? Object.keys(this.wasm).join(', ') : 'undefined'));
      }

      // Emscripten normally invokes global C++ constructors from its JS glue
      // before exposing the module as ready. This worklet instantiates the raw
      // WASM binary, so it must perform that step explicitly.
      if (typeof this.wasm.__wasm_call_ctors === 'function') {
        this.wasm.__wasm_call_ctors();
      }

      this._updateMemoryViews();
      this.wasm.initEngine(sr);

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
      if (this.debug) console.log('[MS2000Worklet] WASM engine initialized @', sr, 'Hz');
    } catch (err) {
      console.error('[MS2000Worklet] WASM init error:', err);
      this.port.postMessage({ type: 'WASM_ERROR', error: err.message });
    }
  }

  process(inputs, outputs, parameters) {
    if (!this.isReady || !this.wasm) return true;

    const output = outputs[0];
    if (!output || output.length === 0 || !output[0]) return true;

    const outL = output[0];
    // AudioWorklet normally receives the explicitly requested stereo output,
    // but mirror the left channel if a browser supplies a mono destination.
    const outR = output[1] || output[0];
    const numSamples = outL.length; // Standard 128 samples

    // Check if WASM memory was detached (growth) and update views
    if (this.HEAPF32.buffer !== this.wasm.memory.buffer) {
      this._updateMemoryViews();
    }

    // 1. Render Audio Block via the raw C++ WASM export. This notice is
    // intentionally emitted from process(), not from initialization, so the
    // UI can distinguish an actually-rendering WASM engine from a merely
    // ready AudioWorklet or a browser oscillator.
    this.wasm.processAudio(this.outLPtr, this.outRPtr, numSamples);
    if (this.debug && !this._renderNoticeSent) {
      this._renderNoticeSent = true;
      this.port.postMessage({
        type: 'WASM_RENDERING',
        renderer: 'raw-wasm-cpp',
        blockSize: numSamples,
      });
    }

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

    // Diagnostic point 1 is the final output probe. Since the raw WASM
    // interface does not export SynthEngine::setDiagnosticTone(), inject the
    // tone after the WASM render for every selected point. This deliberately
    // bypasses the entire chain and makes the browser output path testable.
    if (this.diagnosticPoint > 0 && this.diagnosticLevel > 0) {
      const phaseStep = this.diagnosticFrequency / sampleRate;
      for (let i = 0; i < numSamples; i++) {
        const sample = Math.sin(2 * Math.PI * this.diagnosticPhase) * this.diagnosticLevel;
        outL[i] += sample;
        outR[i] += sample;
        this.diagnosticPhase += phaseStep;
        if (this.diagnosticPhase >= 1) this.diagnosticPhase -= 1;
      }
    }

    // 3. Post Telemetry periodically (~20% of blocks)
    if (Math.random() < 0.2) {
      this.wasm.getAudioSnapshot(
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
