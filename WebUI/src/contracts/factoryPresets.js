/**
 * Factory Programs and SysEx Serializer for Korg MS2000 / microKORG.
 */

export const FACTORY_PROGRAMS = [
  {
    id: 'A.01',
    name: 'Init Program',
    cat: 'VA Saw + MultiFilter',
    params: {
      osc1Wave: 0, osc1Ctrl1: 0, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 0, osc2Semitone: 0, osc2Tune: 0,
      mixOsc1Level: 127, mixOsc2Level: 0, mixNoiseLevel: 0,
      filterType: 0, filterCutoff: 127, filterResonance: 0, filterEg1Int: 0, filterKeyTrack: 0,
      ampLevel: 100, ampPan: 64, ampDistortion: 0,
      eg1Attack: 0, eg1Decay: 64, eg1Sustain: 0, eg1Release: 32,
      eg2Attack: 0, eg2Decay: 40, eg2Sustain: 127, eg2Release: 25,
      lfo1Wave: 2, lfo1Freq: 45, lfo2Wave: 2, lfo2Freq: 64,
      modFxOn: 1, modFxType: 0, modFxSpeed: 30, modFxDepth: 50,
      delayOn: 1, delayType: 0, delayTime: 40, delayDepth: 40,
      portamentoTime: 0, voiceMode: 2, synthMode: 0
    }
  },
  {
    id: 'A.02',
    name: 'MS2000 Poly Lead',
    cat: 'Sync 4-Pole LPF',
    params: {
      osc1Wave: 0, osc1Ctrl1: 30, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 1, osc2Semitone: 7, osc2Tune: 5,
      mixOsc1Level: 110, mixOsc2Level: 95, mixNoiseLevel: 0,
      filterType: 0, filterCutoff: 85, filterResonance: 45, filterEg1Int: 35, filterKeyTrack: 20,
      ampLevel: 105, ampPan: 64, ampDistortion: 1,
      eg1Attack: 5, eg1Decay: 70, eg1Sustain: 40, eg1Release: 45,
      eg2Attack: 2, eg2Decay: 60, eg2Sustain: 110, eg2Release: 35,
      lfo1Wave: 0, lfo1Freq: 55, lfo2Wave: 2, lfo2Freq: 70,
      modFxOn: 1, modFxType: 0, modFxSpeed: 45, modFxDepth: 65,
      delayOn: 1, delayType: 1, delayTime: 55, delayDepth: 50,
      portamentoTime: 20, voiceMode: 2, synthMode: 0
    }
  },
  {
    id: 'A.03',
    name: 'Cyberpunk Bass',
    cat: 'Overdrive Acid Bass',
    params: {
      osc1Wave: 1, osc1Ctrl1: 64, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 2, osc2Semitone: -12, osc2Tune: -4,
      mixOsc1Level: 127, mixOsc2Level: 80, mixNoiseLevel: 10,
      filterType: 0, filterCutoff: 50, filterResonance: 78, filterEg1Int: 55, filterKeyTrack: -10,
      ampLevel: 115, ampPan: 64, ampDistortion: 1,
      eg1Attack: 0, eg1Decay: 55, eg1Sustain: 10, eg1Release: 20,
      eg2Attack: 0, eg2Decay: 50, eg2Sustain: 90, eg2Release: 15,
      lfo1Wave: 1, lfo1Freq: 30, lfo2Wave: 2, lfo2Freq: 40,
      modFxOn: 0, modFxType: 0, modFxSpeed: 20, modFxDepth: 0,
      delayOn: 1, delayType: 0, delayTime: 35, delayDepth: 30,
      portamentoTime: 15, voiceMode: 0, synthMode: 2
    }
  },
  {
    id: 'A.04',
    name: 'Ambient DWGS Pad',
    cat: 'Bell Wave Chorus',
    params: {
      osc1Wave: 5, osc1Ctrl1: 0, osc1DwgsWave: 14,
      osc2Wave: 2, osc2ModType: 0, osc2Semitone: 12, osc2Tune: 8,
      mixOsc1Level: 100, mixOsc2Level: 85, mixNoiseLevel: 5,
      filterType: 1, filterCutoff: 75, filterResonance: 20, filterEg1Int: 25, filterKeyTrack: 15,
      ampLevel: 95, ampPan: 64, ampDistortion: 0,
      eg1Attack: 65, eg1Decay: 90, eg1Sustain: 80, eg1Release: 85,
      eg2Attack: 50, eg2Decay: 85, eg2Sustain: 105, eg2Release: 80,
      lfo1Wave: 2, lfo1Freq: 25, lfo2Wave: 2, lfo2Freq: 35,
      modFxOn: 1, modFxType: 1, modFxSpeed: 35, modFxDepth: 80,
      delayOn: 1, delayType: 2, delayTime: 70, delayDepth: 65,
      portamentoTime: 0, voiceMode: 2, synthMode: 0
    }
  },
  {
    id: 'A.05',
    name: '16-Band Vocoder',
    cat: 'Formant Shift Vox',
    params: {
      osc1Wave: 0, osc1Ctrl1: 0, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 0, osc2Semitone: 0, osc2Tune: 0,
      mixOsc1Level: 120, mixOsc2Level: 60, mixNoiseLevel: 25,
      filterType: 0, filterCutoff: 100, filterResonance: 30, filterEg1Int: 0, filterKeyTrack: 0,
      ampLevel: 105, ampPan: 64, ampDistortion: 0,
      eg1Attack: 5, eg1Decay: 60, eg1Sustain: 50, eg1Release: 40,
      eg2Attack: 5, eg2Decay: 60, eg2Sustain: 120, eg2Release: 35,
      lfo1Wave: 2, lfo1Freq: 40, lfo2Wave: 2, lfo2Freq: 50,
      modFxOn: 1, modFxType: 0, modFxSpeed: 40, modFxDepth: 55,
      delayOn: 1, delayType: 0, delayTime: 45, delayDepth: 45,
      portamentoTime: 0, voiceMode: 2, synthMode: 0, synthVocoderMode: 1
    }
  },
  {
    id: 'A.06',
    name: 'Trance Pluck Arp',
    cat: '16th Arp Delay',
    params: {
      osc1Wave: 0, osc1Ctrl1: 0, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 0, osc2Semitone: 0, osc2Tune: 10,
      mixOsc1Level: 115, mixOsc2Level: 110, mixNoiseLevel: 0,
      filterType: 0, filterCutoff: 60, filterResonance: 50, filterEg1Int: 60, filterKeyTrack: 15,
      ampLevel: 100, ampPan: 64, ampDistortion: 0,
      eg1Attack: 0, eg1Decay: 45, eg1Sustain: 0, eg1Release: 30,
      eg2Attack: 0, eg2Decay: 45, eg2Sustain: 0, eg2Release: 25,
      lfo1Wave: 2, lfo1Freq: 60, lfo2Wave: 2, lfo2Freq: 70,
      modFxOn: 0, modFxType: 0, modFxSpeed: 30, modFxDepth: 0,
      delayOn: 1, delayType: 1, delayTime: 50, delayDepth: 60,
      arpOn: 1, portamentoTime: 0, voiceMode: 2, synthMode: 0
    }
  },
  {
    id: 'A.07',
    name: 'Vintage Brass',
    cat: 'Dual Layer Detune',
    params: {
      osc1Wave: 0, osc1Ctrl1: 15, osc1DwgsWave: 0,
      osc2Wave: 0, osc2ModType: 0, osc2Semitone: 0, osc2Tune: -8,
      mixOsc1Level: 115, mixOsc2Level: 115, mixNoiseLevel: 0,
      filterType: 0, filterCutoff: 70, filterResonance: 15, filterEg1Int: 40, filterKeyTrack: 25,
      ampLevel: 105, ampPan: 64, ampDistortion: 0,
      eg1Attack: 25, eg1Decay: 75, eg1Sustain: 65, eg1Release: 45,
      eg2Attack: 20, eg2Decay: 75, eg2Sustain: 115, eg2Release: 40,
      lfo1Wave: 2, lfo1Freq: 35, lfo2Wave: 2, lfo2Freq: 45,
      modFxOn: 1, modFxType: 1, modFxSpeed: 25, modFxDepth: 60,
      delayOn: 1, delayType: 0, delayTime: 35, delayDepth: 30,
      portamentoTime: 0, voiceMode: 2, synthMode: 0
    }
  },
  {
    id: 'A.08',
    name: 'Sub Octave Bass',
    cat: 'Ring Mod Sub',
    params: {
      osc1Wave: 1, osc1Ctrl1: 50, osc1DwgsWave: 0,
      osc2Wave: 2, osc2ModType: 2, osc2Semitone: -12, osc2Tune: 0,
      mixOsc1Level: 127, mixOsc2Level: 95, mixNoiseLevel: 0,
      filterType: 0, filterCutoff: 45, filterResonance: 35, filterEg1Int: 30, filterKeyTrack: 0,
      ampLevel: 115, ampPan: 64, ampDistortion: 1,
      eg1Attack: 0, eg1Decay: 60, eg1Sustain: 20, eg1Release: 25,
      eg2Attack: 0, eg2Decay: 55, eg2Sustain: 100, eg2Release: 20,
      lfo1Wave: 2, lfo1Freq: 25, lfo2Wave: 2, lfo2Freq: 30,
      modFxOn: 0, modFxType: 0, modFxSpeed: 0, modFxDepth: 0,
      delayOn: 0, delayType: 0, delayTime: 0, delayDepth: 0,
      portamentoTime: 10, voiceMode: 0, synthMode: 0
    }
  }
];

/**
 * Generate a 288-byte Korg MS2000 formatted SysEx Hex string preview for any parameter map.
 */
export function generateMS2000SysExHex(paramsMap, progName = 'Init Prog') {
  const bytes = new Uint8Array(288);
  // Header: F0 42 30 58 40 (Korg ID: 42, Global Ch: 30, Model MS2000: 58, 1 Program Dump: 40)
  bytes[0] = 0xF0;
  bytes[1] = 0x42;
  bytes[2] = 0x30;
  bytes[3] = 0x58;
  bytes[4] = 0x40;

  // Program Name ASCII
  const cleanName = (progName || 'Init').padEnd(12, ' ').slice(0, 12);
  for (let i = 0; i < 12; i++) {
    bytes[5 + i] = cleanName.charCodeAt(i) & 0x7F;
  }

  // Pack key parameter values
  if (paramsMap) {
    bytes[17] = (paramsMap.osc1Wave || 0) & 0x07;
    bytes[18] = (paramsMap.osc1Ctrl1 || 0) & 0x7F;
    bytes[19] = (paramsMap.osc1DwgsWave || 0) & 0x7F;
    bytes[20] = (paramsMap.osc2Wave || 0) & 0x03;
    bytes[21] = (paramsMap.osc2ModType || 0) & 0x03;
    bytes[22] = ((paramsMap.osc2Semitone || 0) + 12) & 0x1F;
    bytes[23] = ((paramsMap.osc2Tune || 0) + 64) & 0x7F;
    bytes[24] = (paramsMap.mixOsc1Level || 127) & 0x7F;
    bytes[25] = (paramsMap.mixOsc2Level || 0) & 0x7F;
    bytes[26] = (paramsMap.mixNoiseLevel || 0) & 0x7F;
    bytes[27] = (paramsMap.filterType || 0) & 0x03;
    bytes[28] = (paramsMap.filterCutoff || 127) & 0x7F;
    bytes[29] = (paramsMap.filterResonance || 0) & 0x7F;
    bytes[30] = ((paramsMap.filterEg1Int || 0) + 64) & 0x7F;
    bytes[31] = (paramsMap.eg1Attack || 0) & 0x7F;
    bytes[32] = (paramsMap.eg1Decay || 64) & 0x7F;
    bytes[33] = (paramsMap.eg1Sustain || 0) & 0x7F;
    bytes[34] = (paramsMap.eg1Release || 32) & 0x7F;
    bytes[35] = (paramsMap.eg2Attack || 0) & 0x7F;
    bytes[36] = (paramsMap.eg2Decay || 40) & 0x7F;
    bytes[37] = (paramsMap.eg2Sustain || 127) & 0x7F;
    bytes[38] = (paramsMap.eg2Release || 25) & 0x7F;
  }

  // End of SysEx
  bytes[287] = 0xF7;

  // Format to hex string
  let hex = '';
  for (let i = 0; i < bytes.length; i++) {
    hex += bytes[i].toString(16).toUpperCase().padStart(2, '0') + ' ';
  }
  return hex.trim();
}
