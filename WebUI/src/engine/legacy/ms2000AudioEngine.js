/**
 * Web Audio Real-Time DSP Synthesizer Engine for ABDMS2000.
 * Runs 8-Voice Polyphonic Korg MS2000 Dual-Oscillator + Resonant VCF + ADSR Envelopes + FX.
 * Provides instant, zero-dependency browser playback with full parameter responsiveness.
 */

class SynthVoice {
  constructor(audioCtx, destination) {
    this.ctx = audioCtx;
    this.destination = destination;
    this.activeNote = -1;
    this.isRunning = false;

    // Dual Oscillators
    this.osc1 = null;
    this.osc2 = null;
    this.noiseNode = null;

    // Mixer gains
    this.osc1Gain = this.ctx.createGain();
    this.osc2Gain = this.ctx.createGain();
    this.noiseGain = this.ctx.createGain();

    // VCF (Filter)
    this.filter = this.ctx.createBiquadFilter();
    this.filter.type = 'lowpass';

    // VCA (Amp)
    this.vca = this.ctx.createGain();
    this.vca.gain.value = 0;

    // Stereo Panner (if supported)
    this.panner = (typeof this.ctx.createStereoPanner === 'function') ? this.ctx.createStereoPanner() : null;

    // Wire mixer -> Filter -> VCA -> Panner -> Destination
    this.osc1Gain.connect(this.filter);
    this.osc2Gain.connect(this.filter);
    this.noiseGain.connect(this.filter);
    this.filter.connect(this.vca);

    if (this.panner) {
      this.vca.connect(this.panner);
      this.panner.connect(this.destination);
    } else {
      this.vca.connect(this.destination);
    }

    this.lastFreq = 0;
    this.lastOsc2Freq = 0;
  }

  noteOn(midiNote, velocity, params, detuneCents = 0, pan = 0) {
    if (this._releaseTimer) {
      clearTimeout(this._releaseTimer);
      this._releaseTimer = null;
    }
    this.activeNote = midiNote;
    this.isRunning = true;
    const now = this.ctx.currentTime;

    const baseFreq = 440 * Math.pow(2, (midiNote - 69) / 12);
    const detuneMul = Math.pow(2, detuneCents / 1200);
    const targetFreq = baseFreq * detuneMul;

    // Stop existing oscs
    if (this.osc1) { try { this.osc1.stop(); this.osc1.disconnect(); } catch(e){} }
    if (this.osc2) { try { this.osc2.stop(); this.osc2.disconnect(); } catch(e){} }
    if (this.noiseNode) { try { this.noiseNode.stop(); this.noiseNode.disconnect(); } catch(e){} }

    // Set stereo pan (combine parameter ampPan + unison spread pan)
    if (this.panner) {
      const globalPan = ((params.ampPan !== undefined ? params.ampPan : 64) - 64) / 64.0;
      const finalPan = Math.max(-1.0, Math.min(1.0, globalPan + pan));
      this.panner.pan.setValueAtTime(finalPan, now);
    }

    // Setup OSC1
    const osc1Type = ['sawtooth', 'square', 'triangle', 'sine', 'sawtooth', 'sawtooth'][params.osc1Wave || 0] || 'sawtooth';
    this.osc1 = this.ctx.createOscillator();
    this.osc1.type = osc1Type;

    // Setup OSC2 (with Semitone + Tune Detune)
    const osc2Type = ['sawtooth', 'square', 'triangle'][params.osc2Wave || 0] || 'sawtooth';
    const semi = params.osc2Semitone || 0;
    const tune = (params.osc2Tune || 0) * 2.0; // cents
    const osc2TargetFreq = targetFreq * Math.pow(2, semi / 12) * Math.pow(2, tune / 1200);

    this.osc2 = this.ctx.createOscillator();
    this.osc2.type = osc2Type;

    // Portamento Glide Logic
    const portaTime = params.portamentoTime || 0;
    const portaOn = (params.portamentoOn !== undefined) ? (params.portamentoOn > 0.5) : (portaTime > 0);
    const shouldGlide = portaOn && (portaTime > 0) && (this.lastFreq > 20);

    if (shouldGlide) {
      const glideSec = 0.005 + 1.6 * Math.pow(portaTime / 127.0, 2.0);
      this.osc1.frequency.setValueAtTime(this.lastFreq, now);
      this.osc1.frequency.exponentialRampToValueAtTime(Math.max(20, targetFreq), now + glideSec);
      this.osc2.frequency.setValueAtTime(this.lastOsc2Freq || this.lastFreq, now);
      this.osc2.frequency.exponentialRampToValueAtTime(Math.max(20, osc2TargetFreq), now + glideSec);
    } else {
      this.osc1.frequency.setValueAtTime(targetFreq, now);
      this.osc2.frequency.setValueAtTime(osc2TargetFreq, now);
    }

    this.lastFreq = targetFreq;
    this.lastOsc2Freq = osc2TargetFreq;

    this.osc1.connect(this.osc1Gain);
    this.osc1Gain.gain.setValueAtTime((params.mixOsc1Level !== undefined ? params.mixOsc1Level : 127) / 127.0, now);
    this.osc1.start(now);

    this.osc2.connect(this.osc2Gain);
    this.osc2Gain.gain.setValueAtTime((params.mixOsc2Level || 0) / 127.0, now);
    this.osc2.start(now);

    // Setup Noise
    if ((params.mixNoiseLevel || 0) > 0) {
      const bufferSize = this.ctx.sampleRate * 2;
      const noiseBuffer = this.ctx.createBuffer(1, bufferSize, this.ctx.sampleRate);
      const output = noiseBuffer.getChannelData(0);
      for (let i = 0; i < bufferSize; i++) {
        output[i] = Math.random() * 2 - 1;
      }
      this.noiseNode = this.ctx.createBufferSource();
      this.noiseNode.buffer = noiseBuffer;
      this.noiseNode.loop = true;
      this.noiseNode.connect(this.noiseGain);
      this.noiseGain.gain.setValueAtTime((params.mixNoiseLevel || 0) / 127.0 * 0.4, now);
      this.noiseNode.start(now);
    }

    // Setup Filter
    const fType = ['lowpass', 'lowpass', 'bandpass', 'highpass'][params.filterType || 0] || 'lowpass';
    this.filter.type = fType;
    const cutoffNorm = (params.filterCutoff !== undefined ? params.filterCutoff : 127) / 127.0;
    this.lastBaseCutoff = cutoffNorm;
    const cutoffHz = 20 * Math.pow(1000, cutoffNorm);
    const reso = (params.filterResonance || 0) / 127.0 * 18;

    this.filter.frequency.cancelScheduledValues(now);
    this.filter.frequency.setValueAtTime(cutoffHz, now);
    this.filter.Q.setValueAtTime(reso, now);

    // Distortion (WaveShaperNode between filter and VCA)
    if (params.ampDistortion > 0.5 && !this._distNode) {
      this._distNode = this.ctx.createWaveShaper();
      const n = 256;
      const curve = new Float32Array(n);
      for (let i = 0; i < n; i++) {
        const x = (i / (n - 1)) * 2 - 1;
        const preGain = 7.8;
        const inp = x * preGain;
        if (inp > 0) {
          curve[i] = Math.tanh(inp * 1.5);
        } else {
          curve[i] = Math.tanh(inp * 2.0) * 0.75;
        }
      }
      this._distNode.curve = curve;
      this._distNode.oversample = '2x';
      // Rewire: filter -> distortion -> VCA
      this.filter.disconnect();
      this.filter.connect(this._distNode);
      this._distNode.connect(this.vca);
    } else if (params.ampDistortion <= 0.5 && this._distNode) {
      this.filter.disconnect();
      try { this._distNode.disconnect(); } catch(e){}
      this._distNode = null;
      this.filter.connect(this.vca);
    }

    // ── EG Time Curves (matching MS2000 power-3 knob curves) ──
    // Attack: 0.5ms to 5s, power 3.0
    const eg1ANorm = (params.eg1Attack || 0) / 127.0;
    const eg1A = 0.0005 + 4.9995 * Math.pow(eg1ANorm, 3.0);
    const eg1DNorm = (params.eg1Decay !== undefined ? params.eg1Decay : 64) / 127.0;
    const eg1D = 0.005 + 9.995 * Math.pow(eg1DNorm, 3.0);
    const eg1S = (params.eg1Sustain !== undefined ? params.eg1Sustain : 0) / 127.0;
    const eg1Int = (params.filterEg1Int || 0) / 64.0; // -1..+1

    // EG1 → Filter: Exponential sweep attack → exponential decay to sustain level
    const peakFilterHz = Math.min(18000, Math.max(20, cutoffHz * Math.pow(2, eg1Int * 4)));
    const susFilterHz = Math.min(18000, Math.max(20, cutoffHz * Math.pow(2, eg1Int * eg1S * 4)));

    this.filter.frequency.setTargetAtTime(peakFilterHz, now, eg1A * 0.33);
    this.filter.frequency.setTargetAtTime(susFilterHz, now + eg1A, eg1D * 0.33);

    // EG2 → Amp: Exponential envelope (capacitor charge/discharge)
    const eg2ANorm = (params.eg2Attack || 0) / 127.0;
    const eg2A = 0.0005 + 4.9995 * Math.pow(eg2ANorm, 3.0);
    const eg2DNorm = (params.eg2Decay !== undefined ? params.eg2Decay : 64) / 127.0;
    const eg2D = 0.005 + 9.995 * Math.pow(eg2DNorm, 3.0);
    const eg2S = (params.eg2Sustain !== undefined ? params.eg2Sustain : 127) / 127.0;
    const peakGain = Math.min(1.0, velocity * 0.85);

    this.vca.gain.cancelScheduledValues(now);
    this.vca.gain.setValueAtTime(0.0001, now);
    // Attack: exponential ramp to peak
    this.vca.gain.setTargetAtTime(peakGain, now, eg2A * 0.33);
    // Decay: exponential settle to sustain
    this.vca.gain.setTargetAtTime(Math.max(0.0001, peakGain * eg2S), now + eg2A, eg2D * 0.33);
  }

  noteOff(params) {
    const now = this.ctx.currentTime;
    const eg2RNorm = (params.eg2Release !== undefined ? params.eg2Release : 40) / 127.0;
    const eg2R = 0.005 + 9.995 * Math.pow(eg2RNorm, 3.0);

    this.vca.gain.cancelScheduledValues(now);
    const currentGain = Math.max(0.0001, this.vca.gain.value || 0.5);
    this.vca.gain.setValueAtTime(currentGain, now);
    // Exponential release to silence (3 time constants ≈ 95% decay)
    this.vca.gain.setTargetAtTime(0.0001, now, eg2R * 0.33);

    // Also release the filter EG
    const eg1RNorm = (params.eg1Release !== undefined ? params.eg1Release : 40) / 127.0;
    const eg1R = 0.005 + 9.995 * Math.pow(eg1RNorm, 3.0);
    const cutoffNorm = (params.filterCutoff !== undefined ? params.filterCutoff : 127) / 127.0;
    const restHz = 20 * Math.pow(1000, cutoffNorm);
    this.filter.frequency.cancelScheduledValues(now);
    this.filter.frequency.setValueAtTime(this.filter.frequency.value, now);
    this.filter.frequency.setTargetAtTime(restHz, now, eg1R * 0.33);

    // Cleanup after release completes (3 time constants)
    const cleanupMs = Math.max(100, eg2R * 3 * 1000 + 50);
    if (this._releaseTimer) clearTimeout(this._releaseTimer);
    this._releaseTimer = setTimeout(() => {
      this.activeNote = -1;
      this.isRunning = false;
      this._releaseTimer = null;
      if (this.osc1) { try { this.osc1.stop(); this.osc1.disconnect(); } catch(e){} this.osc1 = null; }
      if (this.osc2) { try { this.osc2.stop(); this.osc2.disconnect(); } catch(e){} this.osc2 = null; }
      if (this.noiseNode) { try { this.noiseNode.stop(); this.noiseNode.disconnect(); } catch(e){} this.noiseNode = null; }
      if (this._distNode) { try { this._distNode.disconnect(); } catch(e){} this._distNode = null; this.filter.connect(this.vca); }
    }, cleanupMs);
  }

  setModCutoff(normVal, isSmooth, stepDurationMs) {
    if (!this.filter || !this.ctx || !this.isRunning) return;
    const now = this.ctx.currentTime;
    const baseNorm = (this.lastBaseCutoff !== undefined ? this.lastBaseCutoff : 1.0);
    const effNorm = Math.max(0.01, Math.min(1.0, baseNorm + (normVal - 0.5) * 0.9));
    const targetHz = 20 * Math.pow(1000, effNorm);

    if (isSmooth) {
      this.filter.frequency.setTargetAtTime(targetHz, now, (stepDurationMs / 1000) * 0.4);
    } else {
      this.filter.frequency.cancelScheduledValues(now);
      this.filter.frequency.setValueAtTime(targetHz, now);
    }
  }

  kill() {
    const now = this.ctx.currentTime;
    this.vca.gain.cancelScheduledValues(now);
    this.vca.gain.setValueAtTime(0, now);
    this.activeNote = -1;
    this.isRunning = false;
    if (this.osc1) { try { this.osc1.stop(); } catch(e){} }
    if (this.osc2) { try { this.osc2.stop(); } catch(e){} }
    if (this.noiseNode) { try { this.noiseNode.stop(); } catch(e){} }
  }
}

export class MS2000AudioEngine {
  constructor() {
    this.audioCtx = null;
    this.voiceBus = null;
    this.masterGain = null;
    this.voices = [];
    this.maxVoices = 16;
    this.heldNotes = [];
    this.bypassFlags = {};
    this.params = {
      osc1Wave: 0,
      mixOsc1Level: 127,
      osc2Wave: 1,
      mixOsc2Level: 0,
      mixNoiseLevel: 0,
      filterType: 0,
      filterCutoff: 127,
      filterResonance: 0,
      filterEg1Int: 0,
      eg1Attack: 0,
      eg1Decay: 40,
      eg1Sustain: 64,
      eg1Release: 40,
      eg2Attack: 0,
      eg2Decay: 40,
      eg2Sustain: 127,
      eg2Release: 40,
      ampLevel: 100,
      voiceMode: 1,
      unisonDetune: 10,
      unisonSpread: 0.5,
      portamentoTime: 0,
      portamentoOn: 0,
      modFxOn: 1,
      modFxType: 0,
      modFxSpeed: 40,
      modFxDepth: 64,
      modFxFeedback: 0,
      delayOn: 1,
      delayType: 0,
      delayTime: 40,
      delayDepth: 50,
      delayFeedback: 40,
      eqLowFreq: 1,
      eqLowGain: 64,
      eqHighFreq: 2,
      eqHighGain: 64,
      masterVolume: 0.8
    };
    this.isReady = false;
  }

  async init() {
    if (this.isReady && this.audioCtx) {
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }
      return true;
    }

    try {
      const AudioContextClass = window.AudioContext || window.webkitAudioContext;
      if (!AudioContextClass) return false;
      this.audioCtx = new AudioContextClass({ latencyHint: 'interactive' });
      if (this.audioCtx.state === 'suspended') {
        await this.audioCtx.resume();
      }

      // Voice mix bus (collects all voices)
      this.voiceBus = this.audioCtx.createGain();
      this.voiceBus.gain.value = 1.0;

      // ─── 1. MOD FX (Chorus / Ensemble / Flanger) ───
      this.modFxInput = this.audioCtx.createGain();
      this.modFxDry = this.audioCtx.createGain();
      this.modFxWet = this.audioCtx.createGain();
      this.modFxOutput = this.audioCtx.createGain();

      this.modFxDelayL = this.audioCtx.createDelay(0.1);
      this.modFxDelayR = this.audioCtx.createDelay(0.1);
      this.modFxFbL = this.audioCtx.createGain();
      this.modFxFbR = this.audioCtx.createGain();

      this.modFxLfo = this.audioCtx.createOscillator();
      this.modFxLfo.type = 'sine';
      this.modFxLfoDepthL = this.audioCtx.createGain();
      this.modFxLfoDepthR = this.audioCtx.createGain();

      this.voiceBus.connect(this.modFxInput);

      // Dry path
      this.modFxInput.connect(this.modFxDry);
      this.modFxDry.connect(this.modFxOutput);

      // Wet path with feedback
      this.modFxInput.connect(this.modFxDelayL);
      this.modFxInput.connect(this.modFxDelayR);

      this.modFxDelayL.connect(this.modFxFbL);
      this.modFxFbL.connect(this.modFxDelayL);
      this.modFxDelayR.connect(this.modFxFbR);
      this.modFxFbR.connect(this.modFxDelayR);

      this.modFxDelayL.connect(this.modFxWet);
      this.modFxDelayR.connect(this.modFxWet);
      this.modFxWet.connect(this.modFxOutput);

      // LFO modulation of delay time (stereo inverted for rich stereo spread)
      this.modFxLfo.connect(this.modFxLfoDepthL);
      this.modFxLfo.connect(this.modFxLfoDepthR);
      this.modFxLfoDepthL.connect(this.modFxDelayL.delayTime);
      this.modFxLfoDepthR.connect(this.modFxDelayR.delayTime);
      try { this.modFxLfo.start(); } catch(e){}

      // ─── 2. DELAY FX (Stereo / Cross Ping-Pong) ───
      this.delayInput = this.audioCtx.createGain();
      this.delayDry = this.audioCtx.createGain();
      this.delayWet = this.audioCtx.createGain();
      this.delayOutput = this.audioCtx.createGain();

      this.delayNodeL = this.audioCtx.createDelay(2.0);
      this.delayNodeR = this.audioCtx.createDelay(2.0);
      this.delayFbL = this.audioCtx.createGain();
      this.delayFbR = this.audioCtx.createGain();

      // MS2000 analog damping lowpass filters in feedback loop
      this.delayDampL = this.audioCtx.createBiquadFilter();
      this.delayDampR = this.audioCtx.createBiquadFilter();
      this.delayDampL.type = 'lowpass';
      this.delayDampL.frequency.value = 7500;
      this.delayDampR.type = 'lowpass';
      this.delayDampR.frequency.value = 7500;

      this.modFxOutput.connect(this.delayInput);

      // Dry path
      this.delayInput.connect(this.delayDry);
      this.delayDry.connect(this.delayOutput);

      // Wet delay lines
      this.delayInput.connect(this.delayNodeL);
      this.delayInput.connect(this.delayNodeR);

      this.delayNodeL.connect(this.delayDampL);
      this.delayNodeR.connect(this.delayDampR);

      this.delayDampL.connect(this.delayFbL);
      this.delayDampR.connect(this.delayFbR);
      this.delayFbL.connect(this.delayNodeL);
      this.delayFbR.connect(this.delayNodeR);

      this.delayDampL.connect(this.delayWet);
      this.delayDampR.connect(this.delayWet);
      this.delayWet.connect(this.delayOutput);

      // ─── 3. MASTER EQ (Low Shelf + High Shelf) ───
      this.eqLow = this.audioCtx.createBiquadFilter();
      this.eqLow.type = 'lowshelf';
      this.eqLow.frequency.value = 80;
      this.eqLow.gain.value = 0;

      this.eqHigh = this.audioCtx.createBiquadFilter();
      this.eqHigh.type = 'highshelf';
      this.eqHigh.frequency.value = 2000;
      this.eqHigh.gain.value = 0;

      this.delayOutput.connect(this.eqLow);
      this.eqLow.connect(this.eqHigh);

      // ─── 4. MASTER GAIN & DESTINATION ───
      this.masterGain = this.audioCtx.createGain();
      this.masterGain.gain.value = 0.85;

      this.eqHigh.connect(this.masterGain);
      this.masterGain.connect(this.audioCtx.destination);

      // Create polyphonic voice pool connected to voiceBus
      this.voices = [];
      for (let i = 0; i < this.maxVoices; i++) {
        this.voices.push(new SynthVoice(this.audioCtx, this.voiceBus));
      }

      this.isReady = true;
      this.updateModFx();
      this.updateDelay();
      this.updateEQ();

      console.log('[MS2000AudioEngine] Web Audio DSP Ready with ModFX, DelayFX & EQ @', this.audioCtx.sampleRate, 'Hz');
      return true;
    } catch(err) {
      console.warn('[MS2000AudioEngine] Init error:', err);
      return false;
    }
  }

  updateDelay() {
    if (!this.audioCtx || !this.delayNodeL) return;
    const now = this.audioCtx.currentTime;
    const on = (this.params.delayOn !== undefined ? this.params.delayOn : 1) > 0.5;
    const bypass = !!this.bypassFlags?.delay;
    const active = on && !bypass;

    const dNorm = (this.params.delayTime !== undefined ? this.params.delayTime : 40) / 127.0;
    const timeSec = 0.020 + (0.980 * dNorm);
    const depth = (this.params.delayDepth !== undefined ? this.params.delayDepth : 50) / 127.0;
    const fb = Math.min(0.92, ((this.params.delayFeedback !== undefined ? this.params.delayFeedback : 40) / 127.0) * 0.95);
    const type = this.params.delayType || 0; // 0 = Stereo, 1 = Cross, 2 = L/R

    let timeL = timeSec;
    let timeR = (type === 2) ? timeSec * 0.75 : timeSec;

    this.delayNodeL.delayTime.setTargetAtTime(Math.max(0.001, timeL), now, 0.03);
    this.delayNodeR.delayTime.setTargetAtTime(Math.max(0.001, timeR), now, 0.03);

    this.delayFbL.gain.setTargetAtTime(fb, now, 0.02);
    this.delayFbR.gain.setTargetAtTime(fb, now, 0.02);

    this.delayWet.gain.setTargetAtTime(active ? depth * 0.88 : 0, now, 0.02);
    this.delayDry.gain.setTargetAtTime(1.0, now, 0.02);
  }

  updateModFx() {
    if (!this.audioCtx || !this.modFxDelayL) return;
    const now = this.audioCtx.currentTime;
    const on = (this.params.modFxOn !== undefined ? this.params.modFxOn : 1) > 0.5;
    const bypass = !!this.bypassFlags?.modfx;
    const active = on && !bypass;

    const type = Math.min(2, Math.max(0, Math.round(this.params.modFxType || 0))); // 0 = Chorus/Flanger, 1 = Ensemble, 2 = Phaser
    const speedNorm = (this.params.modFxSpeed !== undefined ? this.params.modFxSpeed : 40) / 127.0;
    const depthNorm = (this.params.modFxDepth !== undefined ? this.params.modFxDepth : 64) / 127.0;
    const fbNorm = (this.params.modFxFeedback !== undefined ? this.params.modFxFeedback : 0) / 127.0;

    let lfoFreq = 0.2 + (speedNorm * speedNorm * 7.5);
    let baseDelay = 0.0075 - (0.0057 * fbNorm); // Transitions 7.5ms to 1.8ms
    let modAmplitude = (0.0035 - 0.0020 * fbNorm) * depthNorm;
    let feedback = fbNorm * 0.94;
    let wetMix = depthNorm * 0.85;
    let dryMix = Math.max(0.35, 1.0 - depthNorm * 0.45);

    if (type === 1) {
      // ── ENSEMBLE: Multi-voice slow, rich orchestral chorus ──
      lfoFreq = 0.4 + (speedNorm * 3.5);
      baseDelay = 0.028; // 28ms deep delay
      modAmplitude = depthNorm * 0.014;
      feedback = fbNorm * 0.35;
      wetMix = depthNorm * 0.88;
      dryMix = 0.65;
    } else if (type === 2) {
      // ── PHASER: Ultra-short delay with phase cancellation notches ──
      lfoFreq = 0.15 + (speedNorm * speedNorm * 5.0);
      baseDelay = 0.0022; // 2.2ms notch center
      modAmplitude = depthNorm * 0.0018;
      feedback = Math.min(0.92, 0.25 + fbNorm * 0.67); // Resonant sweep
      wetMix = depthNorm * 0.95;
      dryMix = 0.75;
    }

    this.modFxLfo.frequency.setTargetAtTime(lfoFreq, now, 0.03);
    this.modFxDelayL.delayTime.setTargetAtTime(baseDelay, now, 0.03);
    this.modFxDelayR.delayTime.setTargetAtTime(baseDelay * 1.15, now, 0.03);

    this.modFxLfoDepthL.gain.setTargetAtTime(modAmplitude, now, 0.03);
    this.modFxLfoDepthR.gain.setTargetAtTime(-modAmplitude, now, 0.03);

    this.modFxFbL.gain.setTargetAtTime(feedback, now, 0.02);
    this.modFxFbR.gain.setTargetAtTime(feedback, now, 0.02);

    this.modFxWet.gain.setTargetAtTime(active ? wetMix : 0, now, 0.02);
    this.modFxDry.gain.setTargetAtTime(active ? dryMix : 1.0, now, 0.02);
  }

  updateArpeggiator() {
    if (this.params.arpOn > 0.5) {
      this.startArp();
    } else {
      this.stopArp();
    }
  }

  startArp() {
    this.stopArp();
    if (!this.params.arpOn) return;

    this.arpStepIndex = 0;
    const bpm = this.params.arpTempo || 120;
    const res = this.params.arpResolution || 3;
    const stepsPerBeat = [12, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.33, 0.25, 0.17, 0.125, 0.08, 0.06][res] || 4;
    const stepIntervalMs = Math.max(30, (60000 / bpm) / stepsPerBeat);

    this.arpTimer = setInterval(() => {
      this.stepArp();
    }, stepIntervalMs);
  }

  stopArp() {
    if (this.arpTimer) {
      clearInterval(this.arpTimer);
      this.arpTimer = null;
    }
    if (this._arpGateTimer) {
      clearTimeout(this._arpGateTimer);
      this._arpGateTimer = null;
    }
    if (this.currentArpVoice) {
      this.currentArpVoice.kill();
      this.currentArpVoice = null;
    }
  }

  stepArp() {
    if (this.heldNotes.length === 0) return;

    const baseNotes = [...this.heldNotes].sort((a, b) => a - b);
    const range = Math.min(4, Math.max(1, this.params.arpRange || 1));
    const fullNotes = [];
    for (let o = 0; o < range; o++) {
      for (const n of baseNotes) {
        fullNotes.push(n + o * 12);
      }
    }

    const type = this.params.arpType || 0;
    let sequence = fullNotes;
    if (type === 1) {
      sequence = [...fullNotes].reverse();
    } else if (type === 2) {
      sequence = [...fullNotes, ...[...fullNotes].reverse().slice(1, -1)];
    } else if (type === 4) {
      sequence = [fullNotes[Math.floor(Math.random() * fullNotes.length)]];
    }

    const noteToPlay = sequence[this.arpStepIndex % sequence.length];
    this.arpStepIndex++;

    if (this.currentArpVoice) {
      this.currentArpVoice.kill();
      this.currentArpVoice = null;
    }

    const v = this.voices[0];
    this.currentArpVoice = v;
    v.noteOn(noteToPlay, 0.8, this.params, 0, 0);

    const gate = (this.params.arpGate !== undefined ? this.params.arpGate : 80) / 127.0;
    const bpm = this.params.arpTempo || 120;
    const res = this.params.arpResolution || 3;
    const stepsPerBeat = [12, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.33, 0.25, 0.17, 0.125, 0.08, 0.06][res] || 4;
    const stepMs = (60000 / bpm) / stepsPerBeat;
    const gateMs = Math.max(20, Math.min(stepMs - 10, stepMs * gate));

    if (this._arpGateTimer) clearTimeout(this._arpGateTimer);
    this._arpGateTimer = setTimeout(() => {
      if (this.currentArpVoice === v) {
        v.noteOff(this.params);
        this.currentArpVoice = null;
      }
    }, gateMs);
  }

  updateModSeq() {
    if (this.params.modSeqOn > 0.5) {
      this.startModSeq();
    } else {
      this.stopModSeq();
    }
  }

  startModSeq() {
    this.stopModSeq();
    if (!this.params.modSeqOn) return;

    this.modSeqStep = 0;
    this.modSeqDir = 1;
    this.modSeqSteps = [
      0.15, 0.40, 0.75, 0.30, 0.90, 0.55, 0.20, 0.85,
      0.35, 0.65, 0.45, 0.95, 0.25, 0.70, 0.50, 0.10
    ];

    const bpm = this.params.arpTempo || 120;
    const res = this.params.modSeqResolution || 3;
    const stepsPerBeat = [12, 8, 6, 4, 3, 2, 1.5, 1, 0.75, 0.5, 0.33, 0.25, 0.17, 0.125, 0.08, 0.06][res] || 4;
    const stepIntervalMs = Math.max(30, (60000 / bpm) / stepsPerBeat);

    this.modSeqTimer = setInterval(() => {
      this.stepModSeq(stepIntervalMs);
    }, stepIntervalMs);
  }

  stopModSeq() {
    if (this.modSeqTimer) {
      clearInterval(this.modSeqTimer);
      this.modSeqTimer = null;
    }
  }

  stepModSeq(stepDurationMs) {
    if (!this.params.modSeqOn || !this.modSeqSteps) return;

    const len = 16;
    const mode = this.params.modSeqType || 0; // 0=Forward, 1=Reverse, 2=Bounce, 3=Random
    const isSmooth = (this.params.modSeqSmooth || 0) > 0.5;

    let nextStep = this.modSeqStep;
    if (mode === 0) {
      nextStep = (this.modSeqStep + 1) % len;
    } else if (mode === 1) {
      nextStep = (this.modSeqStep - 1 + len) % len;
    } else if (mode === 2) {
      if (this.modSeqDir === 1) {
        nextStep = this.modSeqStep + 1;
        if (nextStep >= len - 1) { nextStep = len - 1; this.modSeqDir = -1; }
      } else {
        nextStep = this.modSeqStep - 1;
        if (nextStep <= 0) { nextStep = 0; this.modSeqDir = 1; }
      }
    } else if (mode === 3) {
      nextStep = Math.floor(Math.random() * len);
    }
    this.modSeqStep = nextStep;

    const stepVal = this.modSeqSteps[this.modSeqStep];
    this.voices.forEach(v => {
      if (v.isRunning) {
        v.setModCutoff(stepVal, isSmooth, stepDurationMs);
      }
    });
  }

  updateEQ() {
    if (!this.audioCtx || !this.eqLow) return;
    const now = this.audioCtx.currentTime;
    const bypass = !!this.bypassFlags?.eq;

    const lowFreqs = [160, 250, 400, 600];
    const highFreqs = [4000, 6000, 8000, 12000];

    const lIdx = Math.min(3, Math.max(0, Math.floor(this.params.eqLowFreq || 1)));
    const hIdx = Math.min(3, Math.max(0, Math.floor(this.params.eqHighFreq || 2)));

    this.eqLow.frequency.setTargetAtTime(lowFreqs[lIdx], now, 0.03);
    this.eqHigh.frequency.setTargetAtTime(highFreqs[hIdx], now, 0.03);

    const lGain = bypass ? 0 : (((this.params.eqLowGain !== undefined ? this.params.eqLowGain : 64) - 64) / 64) * 12;
    const hGain = bypass ? 0 : (((this.params.eqHighGain !== undefined ? this.params.eqHighGain : 64) - 64) / 64) * 12;

    this.eqLow.gain.setTargetAtTime(lGain, now, 0.03);
    this.eqHigh.gain.setTargetAtTime(hGain, now, 0.03);
  }

  setParam(id, val) {
    this.params[id] = val;
    if (id.startsWith('delay')) {
      this.updateDelay();
    } else if (id.startsWith('modFx')) {
      this.updateModFx();
    } else if (id.startsWith('eq')) {
      this.updateEQ();
    } else if (id === 'arpLatch') {
      if (!val) {
        this.heldNotes = [];
        this.stopArp();
      }
    } else if (id.startsWith('arp')) {
      this.updateArpeggiator();
    } else if (id.startsWith('modSeq')) {
      this.updateModSeq();
    } else if (id === 'masterVolume') {
      if (this.masterGain && this.audioCtx) {
        const vol = (val > 1.0) ? (val / 127.0) : val;
        this.masterGain.gain.setTargetAtTime(Math.max(0.0001, Math.min(1.0, vol)), this.audioCtx.currentTime, 0.02);
      }
    }
  }

  setAllParams(paramsObj) {
    if (!paramsObj) return;
    this.params = { ...this.params, ...paramsObj };
    this.updateDelay();
    this.updateModFx();
    this.updateEQ();
    this.updateArpeggiator();
    this.updateModSeq();
    if (this.masterGain && this.audioCtx && this.params.masterVolume !== undefined) {
      const vol = (this.params.masterVolume > 1.0) ? (this.params.masterVolume / 127.0) : this.params.masterVolume;
      this.masterGain.gain.setTargetAtTime(Math.max(0.0001, Math.min(1.0, vol)), this.audioCtx.currentTime, 0.02);
    }
  }

  noteOn(midiNote, velocity = 0.8) {
    if (!this.heldNotes.includes(midiNote)) {
      this.heldNotes.push(midiNote);
    }
    if (!this.isReady) {
      this.init().then(() => {
        if (this.params.arpOn > 0.5) this.startArp();
        else this._playNote(midiNote, velocity);
      });
    } else {
      if (this.audioCtx && this.audioCtx.state === 'suspended') {
        this.audioCtx.resume();
      }
      if (this.params.arpOn > 0.5) {
        if (!this.arpTimer) this.startArp();
      } else {
        this._playNote(midiNote, velocity);
      }
    }
  }

  _playNote(midiNote, velocity) {
    const vMode = Math.min(2, Math.max(0, Math.round(this.params.voiceMode !== undefined ? this.params.voiceMode : 1)));

    if (vMode === 0) {
      // ── Mono Mode: Legato retrigger / glide on single voice ──
      const v = this.voices[0];
      v.noteOn(midiNote, velocity, this.params, 0, 0);
    } else if (vMode === 2) {
      // ── Unison Mode: 4 Stacked Detuned Voices on a monophonic note ──
      const detuneCents = (this.params.unisonDetune !== undefined ? this.params.unisonDetune : 10);
      const spread = (this.params.unisonSpread !== undefined ? this.params.unisonSpread : 0.5);

      const offsets = [
        { detune: -1.0 * detuneCents, pan: -1.0 * spread },
        { detune: -0.33 * detuneCents, pan: -0.5 * spread },
        { detune: +0.33 * detuneCents, pan: +0.5 * spread },
        { detune: +1.0 * detuneCents, pan: +1.0 * spread }
      ];

      for (let i = 0; i < 4; ++i) {
        if (this.voices[i]) {
          this.voices[i].kill();
          this.voices[i].noteOn(midiNote, velocity * 0.55, this.params, offsets[i].detune, offsets[i].pan);
        }
      }
    } else {
      // ── Poly Mode: Standard voice allocation ──
      let freeVoice = this.voices.find(v => !v.isRunning);
      if (!freeVoice) {
        freeVoice = this.voices.shift();
        this.voices.push(freeVoice);
      }
      freeVoice.noteOn(midiNote, velocity, this.params, 0, 0);
    }
  }

  noteOff(midiNote) {
    const idx = this.heldNotes.indexOf(midiNote);
    if (idx !== -1) {
      this.heldNotes.splice(idx, 1);
    }

    if (this.params.arpOn > 0.5) {
      if (this.heldNotes.length === 0 && !this.params.arpLatch) {
        this.stopArp();
        this.allNotesOff();
      }
      return;
    }

    const vMode = Math.min(2, Math.max(0, Math.round(this.params.voiceMode !== undefined ? this.params.voiceMode : 1)));

    if (vMode === 0 || vMode === 2) {
      if (this.heldNotes.length > 0) {
        // Fall back to previous held note in stack
        const prevNote = this.heldNotes[this.heldNotes.length - 1];
        this._playNote(prevNote, 0.8);
      } else {
        if (vMode === 0) {
          this.voices[0].noteOff(this.params);
        } else {
          for (let i = 0; i < 4; ++i) {
            if (this.voices[i]) this.voices[i].noteOff(this.params);
          }
        }
      }
    } else {
      this.voices.filter(v => v.activeNote === midiNote).forEach(v => v.noteOff(this.params));
    }
  }

  allNotesOff() {
    this.stopArp();
    this.heldNotes = [];
    this.voices.forEach(v => v.kill());
  }

  midiCC(cc, value) {
    if (cc === 120 || cc === 123) {
      this.allNotesOff();
    }
  }

  async setDiagnosticTone(point, freq = 440, level = 0.25) {
    if (!this.audioCtx) {
      if (point > 0) await this.init();
      else return;
    }
    if (this.audioCtx && this.audioCtx.state === 'suspended') {
      try { await this.audioCtx.resume(); } catch (e) {}
    }
    if (this.diagOsc) {
      try { this.diagOsc.stop(); this.diagOsc.disconnect(); } catch(e){}
      this.diagOsc = null;
    }
    if (this.diagGain) {
      try { this.diagGain.disconnect(); } catch(e){}
      this.diagGain = null;
    }
    if (!point || point === 0) return;

    this.diagOsc = this.audioCtx.createOscillator();
    this.diagOsc.type = 'sine';
    this.diagOsc.frequency.setValueAtTime(freq, this.audioCtx.currentTime);
    this.diagGain = this.audioCtx.createGain();
    this.diagGain.gain.setValueAtTime(level, this.audioCtx.currentTime);

    this.diagOsc.connect(this.diagGain);

    if (point === 1) { // Post Master
      this.diagGain.connect(this.audioCtx.destination);
    } else if (point === 2) { // Pre Master
      this.diagGain.connect(this.masterGain);
    } else if (point === 3) { // Pre Effects
      if (this.modFxInput) this.diagGain.connect(this.modFxInput);
      else this.diagGain.connect(this.masterGain);
    } else { // Pre Filter / OSC1 (Voice level)
      if (this.voiceBus) this.diagGain.connect(this.voiceBus);
      else this.diagGain.connect(this.masterGain);
    }
    this.diagOsc.start();
  }

  setDiagnosticBypass(stage, enabled) {
    if (!this.bypassFlags) this.bypassFlags = {};
    this.bypassFlags[stage] = !!enabled;
    console.log(`[ms2000AudioEngine] setDiagnosticBypass: ${stage} = ${enabled}`);
    if (stage === 'modfx') this.updateModFx();
    if (stage === 'delay') this.updateDelay();
    if (stage === 'eq') this.updateEQ();
  }

  resetDiagnosticBypasses() {
    this.bypassFlags = {};
    console.log(`[ms2000AudioEngine] resetDiagnosticBypasses`);
    this.updateModFx();
    this.updateDelay();
    this.updateEQ();
  }
}

export const ms2000AudioEngine = new MS2000AudioEngine();
