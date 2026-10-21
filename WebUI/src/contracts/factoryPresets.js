/**
 * Factory Programs (128 Authentic Korg MS2000 ROM Programs A.01..H.16)
 * and SysEx Serializer for Korg MS2000 / microKORG.
 */

import { FACTORY_ROM_128_B64 } from './factoryPreloadRom.gen.js';

export const BANK_LETTERS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
export const PROGRAMS_PER_BANK = 16;
export const TOTAL_FACTORY_PROGRAMS = 128;

function decodeBase64ToBytes(b64) {
  if (typeof Buffer !== 'undefined') {
    return new Uint8Array(Buffer.from(b64, 'base64'));
  }
  const bin = atob(b64);
  const out = new Uint8Array(bin.length);
  for (let i = 0; i < bin.length; i++) {
    out[i] = bin.charCodeAt(i);
  }
  return out;
}

function extractBits(byte, lo, hi) {
  const width = hi - lo + 1;
  const mask = (1 << width) - 1;
  return (byte >> lo) & mask;
}

function inferCategory(index, name, isVocoder, timbreMode, arpOn, modSeqOn) {
  if (isVocoder || (index >= 120 && index <= 125)) return 'Vocoder';
  const n = name.toLowerCase();
  if (n.includes('bass') || n.includes('bs') || n.includes('sub')) return 'Bass';
  if (n.includes('lead') || n.includes('ld') || n.includes('stab')) return 'Lead';
  if (n.includes('pad') || n.includes('str') || n.includes('choir') || n.includes('warm')) return 'Pad';
  if (n.includes('organ') || n.includes('ep') || n.includes('piano') || n.includes('clav') || n.includes('bell')) return 'Keyboard';
  if (n.includes('arp') || n.includes('seq') || n.includes('-ws') || n.includes('motion') || arpOn || modSeqOn) return 'Arp/Seq';
  if (n.includes('sfx') || n.includes('hit') || n.includes('zap') || n.includes('noise') || n.includes('init')) return 'SFX/Init';
  if (timbreMode === 1) return 'Split';
  if (timbreMode === 2) return 'Dual/Layer';
  return 'Synth';
}

function toTimbre2Id(baseId) {
  return 't2' + baseId.charAt(0).toUpperCase() + baseId.slice(1);
}

const ARP_RES_TABLE = [2, 3, 4, 5, 6, 7];
const FORMANT_SHIFT_TABLE = [2, 3, 4, 1, 0];

/**
 * Decodes a 254-byte Korg MS2000 hardware program into the WebUI/APVTS parameter map.
 * Mirrors MS2000HardwareProgram::toNativeProgram() in Source/MIDI/MS2000HardwareProgram.h.
 */
export function decodeHardwareProgram254(raw, index = 0) {
  let name = '';
  for (let i = 0; i < 12; i++) {
    const c = raw[i];
    name += (c >= 32 && c <= 126) ? String.fromCharCode(c) : ' ';
  }
  name = name.trim() || `Init Prog ${index + 1}`;

  const bankIdx = Math.floor(index / PROGRAMS_PER_BANK) % 8;
  const progNum = (index % PROGRAMS_PER_BANK) + 1;
  const bank = BANK_LETTERS[bankIdx];
  const numberStr = String(progNum).padStart(2, '0');
  const id = `${bank}.${numberStr}`;

  const voiceModeBits = extractBits(raw[0x10], 4, 5);
  const isVocoder = voiceModeBits === 3;
  const timbreMode = isVocoder ? 0 : Math.min(2, voiceModeBits);
  const timbreVoices = extractBits(raw[0x10], 6, 7);

  const arpTempo = Math.max(20, Math.min(300, ((raw[0x1E] << 8) | raw[0x1F]) || 120));
  const arpOn = extractBits(raw[0x20], 7, 7);
  const arpLatch = extractBits(raw[0x20], 6, 6);
  const arpType = extractBits(raw[0x21], 0, 3);
  const arpRange = extractBits(raw[0x21], 4, 7) + 1;
  const arpGate = raw[0x22];
  const arpResolution = ARP_RES_TABLE[Math.min(5, raw[0x23])];

  const eqFreqXform = (v) => Math.min(3, Math.floor((v * 4) / 30));
  const eqGainXform = (v) => Math.max(0, Math.min(127, Math.round(64 + Math.max(-12, Math.min(12, v - 64)) * (63 / 12))));

  const params = {
    synthVocoderMode: isVocoder ? 1 : 0,
    timbreMode,
    timbreVoices,
    scaleKey: extractBits(raw[0x11], 4, 7),
    scaleType: extractBits(raw[0x11], 0, 3),
    splitPoint: raw[0x12],
    delayTime: raw[0x14],
    delayDepth: raw[0x15],
    delayFeedback: raw[0x15],
    delayType: raw[0x16],
    delayOn: raw[0x15] > 0 ? 1 : 0,
    modFxSpeed: raw[0x17],
    modFxDepth: raw[0x18],
    modFxType: raw[0x19],
    modFxOn: raw[0x18] > 0 ? 1 : 0,
    eqHighFreq: eqFreqXform(raw[0x1A]),
    eqHighGain: eqGainXform(raw[0x1B]),
    eqLowFreq: eqFreqXform(raw[0x1C]),
    eqLowGain: eqGainXform(raw[0x1D]),
    arpTempo,
    arpOn,
    arpLatch,
    arpType,
    arpRange,
    arpGate,
    arpResolution,
    arpLastStep: 8,
    arpStep1: 1, arpStep2: 1, arpStep3: 1, arpStep4: 1,
    arpStep5: 1, arpStep6: 1, arpStep7: 1, arpStep8: 1,
    vocoderCarrierSrc: 0,
    vocoderFormantShift: 2,
    vocoderGateSense: 50,
    vocoderHpfLevel: 64,
    vocoderDirectLevel: 0,
    vocoderBandLevel1: 127, vocoderBandLevel2: 127, vocoderBandLevel3: 127, vocoderBandLevel4: 127,
    vocoderBandLevel5: 127, vocoderBandLevel6: 127, vocoderBandLevel7: 127, vocoderBandLevel8: 127,
    vocoderBandLevel9: 127, vocoderBandLevel10: 127, vocoderBandLevel11: 127, vocoderBandLevel12: 127,
    vocoderBandLevel13: 127, vocoderBandLevel14: 127, vocoderBandLevel15: 127, vocoderBandLevel16: 127,
    masterVolume: 0.8
  };

  const TIMBRE1_START = 0x26;
  const TIMBRE_SIZE = 108;

  if (isVocoder) {
    const b = TIMBRE1_START;
    params.voiceMode = extractBits(raw[b + 1], 6, 7);
    params.unisonDetune = raw[b + 2];
    params.pitchTune = raw[b + 3] - 64;
    params.pitchBendRange = raw[b + 4] - 64;
    params.pitchTranspose = raw[b + 5] - 64;
    params.vibratoInt = raw[b + 6] - 64;
    params.portamentoTime = extractBits(raw[b + 15], 0, 6);
    params.portamentoOn = params.portamentoTime > 0 ? 1 : 0;
    params.osc1Wave = raw[b + 7];
    params.osc1Ctrl1 = raw[b + 8];
    params.osc1Ctrl2 = raw[b + 9];
    params.osc1DwgsWave = raw[b + 10];
    params.vocoderHpfGate = extractBits(raw[b + 12], 0, 0);
    params.mixOsc1Level = raw[b + 16];
    params.mixOsc2Level = raw[b + 17];
    params.vocoderHpfLevel = raw[b + 18];
    params.filterType = raw[b + 19];
    params.vocoderThreshold = raw[b + 19];
    params.filterCutoff = raw[b + 20];
    params.vocoderCutoff = raw[b + 20] - 64;
    params.filterResonance = raw[b + 21];
    params.vocoderResonance = raw[b + 21];
    params.vocoderFormantShift = FORMANT_SHIFT_TABLE[Math.min(4, raw[b + 22])];
    params.vocoderGateSense = raw[b + 23];
    params.vocoderModSource = extractBits(raw[b + 24], 0, 2);
    params.ampLevel = raw[b + 25];
    params.ampPan = raw[b + 26] - 64;
    params.vocoderEfSense = raw[b + 26];
    params.ampDistortion = extractBits(raw[b + 27], 0, 0);
    params.ampGate = extractBits(raw[b + 27], 6, 6);
    params.vocoderDirectLevel = raw[b + 28];
    params.vocoderModInt = raw[b + 29] - 64;
    params.eg1Attack = raw[b + 30];
    params.eg1Decay = raw[b + 31];
    params.eg1Sustain = raw[b + 32];
    params.eg1Release = raw[b + 33];
    params.eg2Attack = raw[b + 34];
    params.eg2Decay = raw[b + 35];
    params.eg2Sustain = raw[b + 36];
    params.eg2Release = raw[b + 37];
    params.lfo1Wave = extractBits(raw[b + 38], 0, 1);
    params.lfo1KeySync = extractBits(raw[b + 38], 4, 5);
    params.lfo1Freq = raw[b + 39];
    params.lfo2Wave = extractBits(raw[b + 41], 0, 1);
    params.lfo2KeySync = extractBits(raw[b + 41], 4, 5);
    params.lfo2Freq = raw[b + 42];
    for (let i = 0; i < 16; i++) {
      params[`vocoderBandLevel${i + 1}`] = raw[b + 46 + i];
      params[`vocoderBandPan${i + 1}`] = raw[b + 62 + i] - 64;
    }
  } else {
    for (let t = 0; t < 2; t++) {
      const b = TIMBRE1_START + t * TIMBRE_SIZE;
      const pid = (k) => (t === 1 ? toTimbre2Id(k) : k);

      params[pid('voiceMode')] = extractBits(raw[b + 1], 6, 7);
      params[pid('unisonDetune')] = raw[b + 2];
      params[pid('pitchTune')] = raw[b + 3] - 64;
      params[pid('pitchBendRange')] = raw[b + 4] - 64;
      params[pid('pitchTranspose')] = raw[b + 5] - 64;
      params[pid('vibratoInt')] = raw[b + 6] - 64;
      params[pid('portamentoTime')] = extractBits(raw[b + 15], 0, 6);
      params[pid('portamentoOn')] = params[pid('portamentoTime')] > 0 ? 1 : 0;

      params[pid('osc1Wave')] = raw[b + 7];
      params[pid('osc1Ctrl1')] = raw[b + 8];
      params[pid('osc1Ctrl2')] = raw[b + 9];
      params[pid('osc1DwgsWave')] = raw[b + 10];
      params[pid('osc2ModType')] = extractBits(raw[b + 12], 4, 5);
      params[pid('osc2Wave')] = extractBits(raw[b + 12], 0, 1);
      params[pid('osc2Semitone')] = raw[b + 13] - 64;
      params[pid('osc2Tune')] = raw[b + 14] - 64;
      params[pid('mixOsc1Level')] = raw[b + 16];
      params[pid('mixOsc2Level')] = raw[b + 17];
      params[pid('mixNoiseLevel')] = raw[b + 18];
      params[pid('filterType')] = raw[b + 19];
      params[pid('filterCutoff')] = raw[b + 20];
      params[pid('filterResonance')] = raw[b + 21];
      params[pid('filterEg1Int')] = raw[b + 22] - 64;
      params[pid('filterVelo')] = raw[b + 23] - 64;
      params[pid('filterKeyTrack')] = raw[b + 24] - 64;
      params[pid('ampLevel')] = raw[b + 25];
      params[pid('ampPan')] = raw[b + 26] - 64;
      params[pid('ampDistortion')] = extractBits(raw[b + 27], 0, 0);
      params[pid('ampGate')] = extractBits(raw[b + 27], 6, 6);
      params[pid('ampVelo')] = raw[b + 28] - 64;
      params[pid('ampKeyTrack')] = raw[b + 29] - 64;
      params[pid('eg1Attack')] = raw[b + 30];
      params[pid('eg1Decay')] = raw[b + 31];
      params[pid('eg1Sustain')] = raw[b + 32];
      params[pid('eg1Release')] = raw[b + 33];
      params[pid('eg2Attack')] = raw[b + 34];
      params[pid('eg2Decay')] = raw[b + 35];
      params[pid('eg2Sustain')] = raw[b + 36];
      params[pid('eg2Release')] = raw[b + 37];
      params[pid('lfo1Wave')] = extractBits(raw[b + 38], 0, 1);
      params[pid('lfo1KeySync')] = extractBits(raw[b + 38], 4, 5);
      params[pid('lfo1Freq')] = raw[b + 39];
      params[pid('lfo1TempoSync')] = extractBits(raw[b + 40], 7, 7);
      params[pid('lfo1SyncNote')] = extractBits(raw[b + 40], 0, 4);
      params[pid('lfo2Wave')] = extractBits(raw[b + 41], 0, 1);
      params[pid('lfo2KeySync')] = extractBits(raw[b + 41], 4, 5);
      params[pid('lfo2Freq')] = raw[b + 42];
      params[pid('lfo2TempoSync')] = extractBits(raw[b + 43], 7, 7);
      params[pid('lfo2SyncNote')] = extractBits(raw[b + 43], 0, 4);

      params[pid('patch1Source')] = extractBits(raw[b + 44], 0, 3);
      params[pid('patch1Destination')] = extractBits(raw[b + 44], 4, 7);
      params[pid('patch1Intensity')] = raw[b + 45] - 64;
      params[pid('patch2Source')] = extractBits(raw[b + 46], 0, 3);
      params[pid('patch2Destination')] = extractBits(raw[b + 46], 4, 7);
      params[pid('patch2Intensity')] = raw[b + 47] - 64;
      params[pid('patch3Source')] = extractBits(raw[b + 48], 0, 3);
      params[pid('patch3Destination')] = extractBits(raw[b + 48], 4, 7);
      params[pid('patch3Intensity')] = raw[b + 49] - 64;
      params[pid('patch4Source')] = extractBits(raw[b + 50], 0, 3);
      params[pid('patch4Destination')] = extractBits(raw[b + 50], 4, 7);
      params[pid('patch4Intensity')] = raw[b + 51] - 64;

      params[pid('modSeqOn')] = extractBits(raw[b + 52], 7, 7);
      params[pid('modSeqResolution')] = extractBits(raw[b + 52], 0, 4);
      params[pid('modSeqType')] = extractBits(raw[b + 53], 2, 3);
      params[pid('seqRunMode')] = extractBits(raw[b + 52], 6, 6);
      params[pid('seqLastStep')] = extractBits(raw[b + 53], 4, 7) + 1;
      params[pid('seqKeySync')] = extractBits(raw[b + 53], 0, 1);

      const seqRows = [
        { destOff: 54, motionOff: 55, stepsOff: 56, destId: 'seq1Dest', motionId: 'seq1Motion', rowIdx: 1 },
        { destOff: 72, motionOff: 73, stepsOff: 74, destId: 'seq2Dest', motionId: 'seq2Motion', rowIdx: 2 },
        { destOff: 90, motionOff: 91, stepsOff: 92, destId: 'seq3Dest', motionId: 'seq3Motion', rowIdx: 3 },
      ];
      for (const r of seqRows) {
        params[pid(r.destId)] = raw[b + r.destOff];
        // Real hardware byte: 0 = Smooth, 1 = Step; Engine enum: 0 = Step, 1 = Smooth
        params[pid(r.motionId)] = extractBits(raw[b + r.motionOff], 0, 0) === 0 ? 1 : 0;
        for (let s = 1; s <= 16; s++) {
          params[pid(`seq${r.rowIdx}Step${s}`)] = raw[b + r.stepsOff + (s - 1)] - 64;
        }
      }
    }
  }

  const cat = inferCategory(index, name, isVocoder, timbreMode, arpOn === 1, params.modSeqOn === 1);

  return {
    index,
    id,
    bank,
    number: progNum,
    name,
    cat,
    params,
  };
}

function buildFactoryCatalog() {
  const romBytes = decodeBase64ToBytes(FACTORY_ROM_128_B64);
  const programs = [];
  for (let i = 0; i < TOTAL_FACTORY_PROGRAMS; i++) {
    const slice = romBytes.subarray(i * 254, (i + 1) * 254);
    programs.push(decodeHardwareProgram254(slice, i));
  }
  return programs;
}

export const FACTORY_PROGRAMS = buildFactoryCatalog();

function clampByte(val, lo = 0, hi = 127) {
  const n = Math.round(Number(val) || 0);
  return Math.max(lo, Math.min(hi, n));
}

function setBits(byte, lo, hi, value) {
  const width = hi - lo + 1;
  const mask = ((1 << width) - 1) << lo;
  return (byte & ~mask) | ((value << lo) & mask);
}

/**
 * Encodes a complete 254-byte Korg MS2000 unpacked hardware program from a WebUI/APVTS parameter map.
 * Exact inverse of decodeHardwareProgram254() and mirrors MS2000HardwareProgram::fromNativeProgram().
 */
export function encodeHardwareProgram254(paramsMap = {}, progName = 'Init Prog') {
  const raw = new Uint8Array(254);
  const p = paramsMap || {};
  const val = (k, def = 0) => (p[k] !== undefined ? Number(p[k]) : def);

  // 0x00..0x0B: 12-char ASCII Program Name
  const cleanName = String(progName || 'Init Prog').padEnd(12, ' ').slice(0, 12);
  for (let i = 0; i < 12; i++) {
    const code = cleanName.charCodeAt(i);
    raw[i] = (code >= 32 && code <= 126) ? code : 32;
  }

  const isVocoder = Math.round(val('synthVocoderMode', 0)) === 1;
  const timbreMode = isVocoder ? 3 : clampByte(val('timbreMode', 0), 0, 2);
  const timbreVoices = clampByte(val('timbreVoices', 0), 0, 2);
  raw[0x10] = setBits(setBits(0, 4, 5, timbreMode), 6, 7, timbreVoices);

  const scaleType = clampByte(val('scaleType', 0), 0, 9);
  const scaleKey = clampByte(val('scaleKey', 0), 0, 11);
  raw[0x11] = setBits(setBits(0, 0, 3, scaleType), 4, 7, scaleKey);
  raw[0x12] = clampByte(val('splitPoint', 60), 0, 127);

  // Global FX: Delay, ModFX, EQ
  raw[0x14] = clampByte(val('delayTime', 40), 0, 127);
  const delayOn = Math.round(val('delayOn', 1)) !== 0;
  raw[0x15] = delayOn ? clampByte(val('delayDepth', val('delayFeedback', 50)), 0, 127) : 0;
  raw[0x16] = clampByte(val('delayType', 0), 0, 2);

  raw[0x17] = clampByte(val('modFxSpeed', 40), 0, 127);
  const modFxOn = Math.round(val('modFxOn', 1)) !== 0;
  raw[0x18] = modFxOn ? clampByte(val('modFxDepth', 64), 0, 127) : 0;
  raw[0x19] = clampByte(val('modFxType', 0), 0, 2);

  const eqInvXform = (idx) => clampByte(Math.round(clampByte(idx, 0, 3) * 8), 0, 29);
  raw[0x1A] = eqInvXform(val('eqHighFreq', 2));
  raw[0x1B] = clampByte(val('eqHighGain', 0) + 64, 1, 127);
  raw[0x1C] = eqInvXform(val('eqLowFreq', 1));
  raw[0x1D] = clampByte(val('eqLowGain', 0) + 64, 1, 127);

  // Arpeggiator
  const arpTempo = clampByte(val('arpTempo', 120), 20, 300);
  raw[0x1E] = (arpTempo >> 8) & 0x7F;
  raw[0x1F] = arpTempo & 0xFF;

  const arpOn = clampByte(val('arpOn', 0), 0, 1);
  const arpLatch = clampByte(val('arpLatch', 0), 0, 1);
  raw[0x20] = setBits(setBits(0, 7, 7, arpOn), 6, 6, arpLatch);

  const arpType = clampByte(val('arpType', 0), 0, 5);
  const arpRange = clampByte(val('arpRange', 1) - 1, 0, 3);
  raw[0x21] = setBits(setBits(0, 0, 3, arpType), 4, 7, arpRange);
  raw[0x22] = clampByte(val('arpGate', 64), 0, 127);

  const arpResVal = clampByte(val('arpResolution', 4), 0, 7);
  const arpResIdx = Math.max(0, ARP_RES_TABLE.indexOf(arpResVal));
  raw[0x23] = clampByte(arpResIdx, 0, 5);

  const TIMBRE1_START = 0x26;
  const TIMBRE_SIZE = 108;

  if (isVocoder) {
    const b = TIMBRE1_START;
    raw[b + 1] = setBits(0, 6, 7, clampByte(val('voiceMode', 0), 0, 2));
    raw[b + 2] = clampByte(val('unisonDetune', 0), 0, 99);
    raw[b + 7] = clampByte(val('osc1Wave', 0), 0, 7);
    raw[b + 8] = clampByte(val('osc1Ctrl1', 0), 0, 127);
    raw[b + 9] = clampByte(val('osc1Ctrl2', 0), 0, 127);
    raw[b + 10] = clampByte(val('osc1DwgsWave', 0), 0, 63);
    const portaOn = Math.round(val('portamentoOn', 1)) !== 0;
    raw[b + 15] = portaOn ? clampByte(val('portamentoTime', 0), 0, 127) : 0;
    raw[b + 16] = clampByte(val('mixOsc1Level', 127), 0, 127);
    raw[b + 17] = clampByte(val('mixOsc2Level', 0), 0, 127);
    raw[b + 18] = clampByte(val('vocoderHpfLevel', 64), 0, 127);
    raw[b + 19] = clampByte(val('filterType', 0), 0, 3);
    raw[b + 20] = clampByte(val('filterCutoff', 64), 0, 127);
    raw[b + 21] = clampByte(val('filterResonance', 0), 0, 127);
    const fsVal = clampByte(val('vocoderFormantShift', 2), 0, 4);
    const fsIdx = Math.max(0, FORMANT_SHIFT_TABLE.indexOf(fsVal));
    raw[b + 22] = clampByte(fsIdx, 0, 4);
    raw[b + 23] = clampByte(val('vocoderGateSense', 50), 0, 127);
    raw[b + 25] = clampByte(val('ampLevel', 127), 0, 127);
    raw[b + 26] = clampByte(val('ampPan', 0) + 64, 1, 127);
    raw[b + 27] = clampByte(val('ampDistortion', 0), 0, 1);
    raw[b + 28] = clampByte(val('vocoderDirectLevel', 0), 0, 127);
    raw[b + 30] = clampByte(val('eg1Attack', 0), 0, 127);
    raw[b + 31] = clampByte(val('eg1Decay', 64), 0, 127);
    raw[b + 32] = clampByte(val('eg1Sustain', 100), 0, 127);
    raw[b + 33] = clampByte(val('eg1Release', 30), 0, 127);
    raw[b + 34] = clampByte(val('eg2Attack', 0), 0, 127);
    raw[b + 35] = clampByte(val('eg2Decay', 64), 0, 127);
    raw[b + 36] = clampByte(val('eg2Sustain', 127), 0, 127);
    raw[b + 37] = clampByte(val('eg2Release', 40), 0, 127);
    raw[b + 38] = setBits(setBits(0, 0, 1, clampByte(val('lfo1Wave', 2), 0, 3)), 4, 5, clampByte(val('lfo1KeySync', 0), 0, 2));
    raw[b + 39] = clampByte(val('lfo1Freq', 40), 0, 127);
    raw[b + 41] = setBits(setBits(0, 0, 1, clampByte(val('lfo2Wave', 2), 0, 3)), 4, 5, clampByte(val('lfo2KeySync', 0), 0, 2));
    raw[b + 42] = clampByte(val('lfo2Freq', 40), 0, 127);
    for (let i = 0; i < 16; i++) {
      raw[b + 46 + i] = clampByte(val(`vocoderBandLevel${i + 1}`, 127), 0, 127);
    }
  } else {
    for (let t = 0; t < 2; t++) {
      const b = TIMBRE1_START + t * TIMBRE_SIZE;
      const pid = (k) => (t === 1 ? toTimbre2Id(k) : k);

      const vMode = clampByte(val(pid('voiceMode'), val('voiceMode', 1)), 0, 2);
      raw[b + 1] = setBits(0, 6, 7, vMode);
      raw[b + 2] = clampByte(val(pid('unisonDetune'), val('unisonDetune', 0)), 0, 99);

      raw[b + 7] = clampByte(val(pid('osc1Wave'), 0), 0, 7);
      raw[b + 8] = clampByte(val(pid('osc1Ctrl1'), 0), 0, 127);
      raw[b + 9] = clampByte(val(pid('osc1Ctrl2'), 0), 0, 127);
      raw[b + 10] = clampByte(val(pid('osc1DwgsWave'), 0), 0, 63);

      const osc2Wave = clampByte(val(pid('osc2Wave'), 0), 0, 2);
      const osc2Mod = clampByte(val(pid('osc2ModType'), 0), 0, 3);
      raw[b + 12] = setBits(setBits(0, 0, 1, osc2Wave), 4, 5, osc2Mod);
      raw[b + 13] = clampByte(val(pid('osc2Semitone'), 0) + 64, 40, 88);
      raw[b + 14] = clampByte(val(pid('osc2Tune'), 0) + 64, 1, 127);

      const portaOn = Math.round(val('portamentoOn', 1)) !== 0;
      const portaVal = t === 0 ? val('portamentoTime', 0) : val('t2PortamentoTime', val('portamentoTime', 0));
      raw[b + 15] = portaOn ? clampByte(portaVal, 0, 127) : 0;

      raw[b + 16] = clampByte(val(pid('mixOsc1Level'), 127), 0, 127);
      raw[b + 17] = clampByte(val(pid('mixOsc2Level'), 0), 0, 127);
      raw[b + 18] = clampByte(val(pid('mixNoiseLevel'), 0), 0, 127);

      raw[b + 19] = clampByte(val(pid('filterType'), 0), 0, 3);
      raw[b + 20] = clampByte(val(pid('filterCutoff'), 64), 0, 127);
      raw[b + 21] = clampByte(val(pid('filterResonance'), 0), 0, 127);
      raw[b + 22] = clampByte(val(pid('filterEg1Int'), 0) + 64, 1, 127);
      raw[b + 23] = clampByte(val(pid('filterVelo'), 0) + 64, 1, 127);
      raw[b + 24] = clampByte(val(pid('filterKeyTrack'), 0) + 64, 1, 127);

      raw[b + 25] = clampByte(val(pid('ampLevel'), 127), 0, 127);
      raw[b + 26] = clampByte(val(pid('ampPan'), 0) + 64, 1, 127);
      raw[b + 27] = clampByte(val(pid('ampDistortion'), 0), 0, 1);
      raw[b + 28] = clampByte(val(pid('ampVelo'), 0) + 64, 1, 127);
      raw[b + 29] = clampByte(val(pid('ampKeyTrack'), 0) + 64, 1, 127);

      raw[b + 30] = clampByte(val(pid('eg1Attack'), 0), 0, 127);
      raw[b + 31] = clampByte(val(pid('eg1Decay'), 64), 0, 127);
      raw[b + 32] = clampByte(val(pid('eg1Sustain'), 100), 0, 127);
      raw[b + 33] = clampByte(val(pid('eg1Release'), 30), 0, 127);

      raw[b + 34] = clampByte(val(pid('eg2Attack'), 0), 0, 127);
      raw[b + 35] = clampByte(val(pid('eg2Decay'), 64), 0, 127);
      raw[b + 36] = clampByte(val(pid('eg2Sustain'), 127), 0, 127);
      raw[b + 37] = clampByte(val(pid('eg2Release'), 40), 0, 127);

      raw[b + 38] = setBits(setBits(0, 0, 1, clampByte(val(pid('lfo1Wave'), 2), 0, 3)), 4, 5, clampByte(val(pid('lfo1KeySync'), 0), 0, 2));
      raw[b + 39] = clampByte(val(pid('lfo1Freq'), 40), 0, 127);
      raw[b + 40] = setBits(setBits(0, 7, 7, clampByte(val(pid('lfo1TempoSync'), 0), 0, 1)), 0, 4, clampByte(val(pid('lfo1SyncNote'), 4), 0, 14));

      raw[b + 41] = setBits(setBits(0, 0, 1, clampByte(val(pid('lfo2Wave'), 2), 0, 3)), 4, 5, clampByte(val(pid('lfo2KeySync'), 0), 0, 2));
      raw[b + 42] = clampByte(val(pid('lfo2Freq'), 40), 0, 127);
      raw[b + 43] = setBits(setBits(0, 7, 7, clampByte(val(pid('lfo2TempoSync'), 0), 0, 1)), 0, 4, clampByte(val(pid('lfo2SyncNote'), 4), 0, 14));

      for (let slot = 1; slot <= 4; slot++) {
        const src = clampByte(val(pid(`patch${slot}Source`), 0), 0, 7);
        const dst = clampByte(val(pid(`patch${slot}Destination`), 0), 0, 7);
        const intensity = clampByte(val(pid(`patch${slot}Intensity`), 0) + 64, 1, 127);
        raw[b + 44 + (slot - 1) * 2] = setBits(setBits(0, 0, 3, src), 4, 7, dst);
        raw[b + 45 + (slot - 1) * 2] = intensity;
      }

      const modSeqOn = clampByte(val(pid('modSeqOn'), val('modSeqOn', 0)), 0, 1);
      const seqRunMode = clampByte(val(pid('seqRunMode'), 1), 0, 1);
      const modSeqRes = clampByte(val('modSeqResolution', 3), 0, 5);
      raw[b + 52] = setBits(setBits(setBits(0, 7, 7, modSeqOn), 6, 6, seqRunMode), 0, 4, modSeqRes);

      const seqLastStep = clampByte(val(pid('seqLastStep'), 16) - 1, 0, 15);
      const modSeqType = clampByte(val(pid('modSeqType'), val('modSeqType', 0)), 0, 3);
      const seqKeySync = clampByte(val(pid('seqKeySync'), 0), 0, 2);
      raw[b + 53] = setBits(setBits(setBits(0, 4, 7, seqLastStep), 2, 3, modSeqType), 0, 1, seqKeySync);

      const seqRows = [
        { destOff: 54, motionOff: 55, stepsOff: 56, destId: 'seq1Dest', motionId: 'seq1Motion', rowIdx: 1 },
        { destOff: 72, motionOff: 73, stepsOff: 74, destId: 'seq2Dest', motionId: 'seq2Motion', rowIdx: 2 },
        { destOff: 90, motionOff: 91, stepsOff: 92, destId: 'seq3Dest', motionId: 'seq3Motion', rowIdx: 3 },
      ];
      for (const r of seqRows) {
        raw[b + r.destOff] = clampByte(val(pid(r.destId), 0), 0, 30);
        // Engine enum: 0 = Step, 1 = Smooth -> Real hardware byte: 0 = Smooth, 1 = Step
        const motionEngine = clampByte(val(pid(r.motionId), 0), 0, 1);
        raw[b + r.motionOff] = motionEngine === 1 ? 0 : 1;
        for (let s = 1; s <= 16; s++) {
          raw[b + r.stepsOff + (s - 1)] = clampByte(val(pid(`seq${r.rowIdx}Step${s}`), 0) + 64, 1, 127);
        }
      }
    }
  }

  return raw;
}

/**
 * Generate a 288-byte Korg MS2000 Current Program Dump SysEx Uint8Array buffer
 * (F0 42 3g 58 40 + 254-byte complete hardware program + padding + F7) for any parameter map.
 */
export function generateMS2000SysExBuffer(paramsMap, progName = 'Init Prog', midiChannel = 1) {
  const raw254 = encodeHardwareProgram254(paramsMap, progName);
  const bytes = new Uint8Array(288);
  const chNibble = Math.max(0, Math.min(15, (Math.round(Number(midiChannel) || 1) - 1)));
  // Header: F0 42 3g 58 40 (Korg ID: 42, Global Ch: 30+g, Model MS2000: 58, Current Program Dump: 40)
  bytes[0] = 0xF0;
  bytes[1] = 0x42;
  bytes[2] = 0x30 | chNibble;
  bytes[3] = 0x58;
  bytes[4] = 0x40;
  bytes.set(raw254, 5);
  bytes[287] = 0xF7;
  return bytes;
}

/**
 * Generate a Korg MS2000 formatted SysEx Hex string preview for any parameter map.
 */
export function generateMS2000SysExHex(paramsMap, progName = 'Init Prog', midiChannel = 1) {
  const bytes = generateMS2000SysExBuffer(paramsMap, progName, midiChannel);
  let hex = '';
  for (let i = 0; i < bytes.length; i++) {
    hex += bytes[i].toString(16).toUpperCase().padStart(2, '0') + ' ';
  }
  return hex.trim();
}

