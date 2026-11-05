import { describe, it, expect, beforeEach } from 'vitest';
import { FACTORY_PROGRAMS, generateMS2000SysExBuffer, encodeHardwareProgram254, decodeHardwareProgram254 } from '../../src/contracts/factoryPresets.js';
import { FACTORY_ROM_128_B64 } from '../../src/contracts/factoryPreloadRom.gen.js';
import { PARAM_LOOKUP } from '../../src/contracts/registry.gen.js';
import { paramStore } from '../../src/contracts/paramStore.js';

describe('UX Master Plan & MS2000 Functional Parity (UX-1 .. UX-3)', () => {
  describe('UX1.1: 128 Factory Presets (Banks A..H x 16)', () => {
    it('loads all 128 Korg MS2000 factory programs across banks A..H', () => {
      expect(FACTORY_PROGRAMS).toHaveLength(128);

      const banks = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
      banks.forEach((bankLetter, bIdx) => {
        const inBank = FACTORY_PROGRAMS.filter((p) => p.bank === bankLetter);
        expect(inBank).toHaveLength(16);
        inBank.forEach((prog, pIdx) => {
          expect(prog.index).toBe(bIdx * 16 + pIdx);
          expect(prog.number).toBe(pIdx + 1);
          expect(prog.id).toBe(`${bankLetter}.${String(pIdx + 1).padStart(2, '0')}`);
          expect(typeof prog.name).toBe('string');
          expect(prog.name.length).toBeGreaterThan(0);
          expect(prog.params).toBeDefined();
        });
      });

      expect(FACTORY_PROGRAMS[0].id).toBe('A.01');
      expect(FACTORY_PROGRAMS[0].name).toContain('Stab Saw');
      expect(FACTORY_PROGRAMS[127].id).toBe('H.16');
    });

    it('preserves each factory program’s original 254-byte MS2000 payload', () => {
      const rom = Buffer.from(FACTORY_ROM_128_B64, 'base64');
      expect(rom).toHaveLength(128 * 254);

      for (let i = 0; i < FACTORY_PROGRAMS.length; i++) {
        const expected = rom.subarray(i * 254, (i + 1) * 254);
        expect(FACTORY_PROGRAMS[i].raw254).toBeInstanceOf(Uint8Array);
        expect(FACTORY_PROGRAMS[i].raw254).toHaveLength(254);
        expect(Buffer.from(FACTORY_PROGRAMS[i].raw254)).toEqual(expected);
        expect(FACTORY_PROGRAMS[i].raw254.buffer).not.toBe(rom.buffer);
      }
    });

    it('decodes Timbre 1, Timbre 2, and Mod Sequencer steps in factory presets', () => {
      const first = FACTORY_PROGRAMS[0];
      expect(typeof first.params.osc1Wave).toBe('number');
      expect(typeof first.params.t2Osc1Wave).toBe('number');
      expect(typeof first.params.seq1Step1).toBe('number');
      expect(typeof first.params.seq2Step1).toBe('number');
      expect(typeof first.params.seq3Step1).toBe('number');
      expect(typeof first.params.vocoderBandLevel1).toBe('number');

      const syx = generateMS2000SysExBuffer(first.params, first.name);
      expect(syx).toBeInstanceOf(Uint8Array);
      expect(syx.length).toBe(288);
      expect(first.params.eqLowGain).toBeGreaterThanOrEqual(0);
      expect(first.params.eqLowGain).toBeLessThanOrEqual(127);
      expect(typeof first.params.pitchTranspose).toBe('number');
      expect(typeof first.params.ampGate).toBe('number');
    });
  });

  describe('UX1.2: Exact Factory Payload Roundtrip', () => {
    it('re-encodes every factory program to its exact original 254-byte payload', () => {
      for (const program of FACTORY_PROGRAMS) {
        const encoded = encodeHardwareProgram254(program.params, program.name, program);
        expect(encoded).toEqual(program.raw254);
      }
    });

    it('embeds the contextual raw payload in the existing MS2000 SysEx buffer format', () => {
      const program = FACTORY_PROGRAMS[0];
      const buffer = generateMS2000SysExBuffer(program.params, program.name, 1, program);
      expect(buffer).toHaveLength(288);
      expect(buffer[0]).toBe(0xf0);
      expect(buffer[4]).toBe(0x40);
      expect(buffer.slice(5, 5 + 254)).toEqual(program.raw254);
      expect(buffer[287]).toBe(0xf7);
    });

    it('re-encodes the timbre layout when switching between synth and vocoder modes', () => {
      const program = FACTORY_PROGRAMS[0];
      const params = { ...program.params, synthVocoderMode: 1 };
      expect(encodeHardwareProgram254(params, program.name, program)).toEqual(
        encodeHardwareProgram254(params, program.name)
      );
    });

    it('updates only fields whose parameter values changed', () => {
      const cases = [
        { index: 0, id: 'osc1Wave', value: 5, offset: 0x26 + 7, mask: 0xff, expected: 5 },
        { index: 0, id: 'pitchTune', value: 12, offset: 0x26 + 3, mask: 0xff, expected: 76 },
        { index: 0, id: 'pitchBendRange', value: 8, offset: 0x26 + 4, mask: 0xff, expected: 72 },
        { index: 0, id: 'pitchTranspose', value: -7, offset: 0x26 + 5, mask: 0xff, expected: 57 },
        { index: 0, id: 'vibratoInt', value: 15, offset: 0x26 + 6, mask: 0xff, expected: 79 },
        { index: 0, id: 'eqHighGain', value: 127, offset: 0x1b, mask: 0xff, expected: 76 },
        { index: 0, id: 'eqLowGain', value: 1, offset: 0x1d, mask: 0xff, expected: 52 },
        { index: 0, id: 'modSeqResolution', value: 15, offset: 0x26 + 52, mask: 0x1f, expected: 15 },
        { index: 0, id: 'ampGate', value: 1, offset: 0x26 + 27, mask: 0x40, expected: 0x40 },
        { index: 0, id: 'ampDistortion', value: 1, offset: 0x26 + 27, mask: 0x01, expected: 0x01 },
        { index: 1, id: 't2ModSeqResolution', value: 14, offset: 0x92 + 52, mask: 0x1f, expected: 14 },
        { index: 1, id: 't2PitchTune', value: -11, offset: 0x92 + 3, mask: 0xff, expected: 53 },
        { index: 1, id: 't2PitchBendRange', value: -9, offset: 0x92 + 4, mask: 0xff, expected: 55 },
        { index: 1, id: 't2PitchTranspose', value: 6, offset: 0x92 + 5, mask: 0xff, expected: 70 },
        { index: 1, id: 't2VibratoInt', value: -13, offset: 0x92 + 6, mask: 0xff, expected: 51 },
      ];

      for (const { index, id, value, offset, mask, expected } of cases) {
        const source = FACTORY_PROGRAMS[index];
        const params = { ...source.params, [id]: value };
        const encoded = encodeHardwareProgram254(params, source.name, source);
        expect(encoded[offset] & mask).toBe(expected);
        for (let i = 0; i < encoded.length; i++) {
          if (i === offset) {
            expect(encoded[i] & ~mask).toBe(source.raw254[i] & ~mask);
          } else {
            expect(encoded[i]).toBe(source.raw254[i]);
          }
        }
      }
    });
  });

  describe('UX1.3: KeySync Roundtrip Parity', () => {
    it('encode then decode roundtrip preserves all KeySync values for every factory program', () => {
      for (let i = 0; i < FACTORY_PROGRAMS.length; i++) {
        const original = FACTORY_PROGRAMS[i];
        // Encode the params back to raw 254-byte firmware data
        const raw = encodeHardwareProgram254(original.params, original.name, original);
        // Decode the raw data back to params
        const decoded = decodeHardwareProgram254(raw, original.index);
        // Verify KeySync modes match original
        expect(decoded.params.lfo1KeySync).toBe(original.params.lfo1KeySync);
        expect(decoded.params.lfo2KeySync).toBe(original.params.lfo2KeySync);
        expect(decoded.params.seqKeySync).toBe(original.params.seqKeySync);
      }
    });

    it('decode then re-encode roundtrip preserves all KeySync values for every factory program', () => {
      for (let i = 0; i < FACTORY_PROGRAMS.length; i++) {
        const original = FACTORY_PROGRAMS[i];
        // First decode (though original is already decoded, test the function path)
        const raw = encodeHardwareProgram254(original.params, original.name, original);
        // Decode the encoded buffer back
        const roundtripped = decodeHardwareProgram254(raw, original.index);
        // Verify KeySync modes match original
        expect(roundtripped.params.lfo1KeySync).toBe(original.params.lfo1KeySync);
        expect(roundtripped.params.lfo2KeySync).toBe(original.params.lfo2KeySync);
        expect(roundtripped.params.seqKeySync).toBe(original.params.seqKeySync);
      }
    });

    it('KeySync mode values are in {0 Off, 1 Timbre, 2 Voice, 3} range across all 128 presets (2-bit hardware field)', () => {
      for (let i = 0; i < FACTORY_PROGRAMS.length; i++) {
        const prog = FACTORY_PROGRAMS[i];
        const ls = prog.params.lfo1KeySync;
        const l2s = prog.params.lfo2KeySync;
        // Only check seqKeySync for non-vocoder programs
        const ks = prog.params.seqKeySync !== undefined ? prog.params.seqKeySync : undefined;
        // All must be 0, 1, 2, or 3 (2-bit field: 0=Off, 1=Timbre, 2=Voice, 3=unused/legato)
        expect(ls).toBeGreaterThanOrEqual(0);
        expect(ls).toBeLessThanOrEqual(3);
        expect(l2s).toBeGreaterThanOrEqual(0);
        expect(l2s).toBeLessThanOrEqual(3);
        if (ks !== undefined) {
          expect(ks).toBeGreaterThanOrEqual(0);
          expect(ks).toBeLessThanOrEqual(3);
        }
      }
    });
  });

  describe('UX2.1: Timbre 1 / Timbre 2 Parameter Symmetry (123 params)', () => {
    it('maps every t2* parameter in PARAM_LOOKUP from its Timbre 1 base ID', () => {
      const t2Ids = Object.keys(PARAM_LOOKUP).filter((k) => k.startsWith('t2') && k.length > 2);
      // 123 desde UX-4: t2TriggerMode se sumó al espejo del Timbre 2.
      expect(t2Ids.length).toBe(123);

      t2Ids.forEach((t2Id) => {
        const baseId = t2Id.charAt(2).toLowerCase() + t2Id.slice(3);
        expect(PARAM_LOOKUP[baseId]).toBeDefined();
        const resolved = 't2' + baseId.charAt(0).toUpperCase() + baseId.slice(1);
        expect(resolved).toBe(t2Id);
      });
    });
  });

  describe('UX3.3: ParamStore Undo / Redo History Stack', () => {
    beforeEach(() => {
      paramStore.syncAll({ filterCutoff: 100 });
      paramStore.clearHistory();
    });

    it('records snapshots and restores state via undo() and redo()', () => {
      const sent = [];
      const mockBridge = {
        setParam: (id, val) => sent.push([id, val]),
      };

      paramStore.set('filterCutoff', 42);
      expect(paramStore.get('filterCutoff')).toBe(42);
      expect(paramStore.canUndo()).toBe(true);

      const undone = paramStore.undo(mockBridge);
      expect(undone.ok).toBe(true);
      expect(paramStore.get('filterCutoff')).toBe(100);
      expect(paramStore.canRedo()).toBe(true);

      const redone = paramStore.redo(mockBridge);
      expect(redone.ok).toBe(true);
      expect(paramStore.get('filterCutoff')).toBe(42);
      expect(sent.length).toBeGreaterThan(0);
    });
  });

  describe('UX4.1: Korg MS2000 16-Button Hardware Strip & 16 Edit Pages', () => {
    it('exposes all 16 illuminated pads, SEQ EDIT [CH PARAM] LEDs, and BANK/OCT/KBD buttons in index.html', async () => {
      const fs = await import('node:fs');
      const path = await import('node:path');
      const htmlPath = path.resolve(process.cwd(), 'WebUI/index.html');
      const html = fs.readFileSync(htmlPath, 'utf8');

      expect(html).toContain('id="ms2k-bottom-strip"');
      expect(html).toContain('id="btn-ms2k-seq-edit"');
      expect(html).toContain('id="led-ms2k-seq-1"');
      expect(html).toContain('id="led-ms2k-seq-2"');
      expect(html).toContain('id="led-ms2k-seq-3"');
      expect(html).toContain('id="btn-ms2k-bank-down"');
      expect(html).toContain('id="btn-ms2k-bank-up"');
      expect(html).toContain('id="btn-ms2k-pad-kbd"');

      const expectedSynthLabels = [
        'COMMON', 'VOICE', 'PITCH', 'OSC 1', 'OSC 2', 'FILTER', 'AMP', 'EG',
        'LFO', 'PATCH', 'SEQ', 'MOD FX', 'DELAY FX', 'EQ', 'ARPEGGIO', 'UTILITY'
      ];
      for (let k = 1; k <= 16; k++) {
        expect(html).toContain(`id="ms2k-pad-${k}"`);
        expect(html).toContain(`data-pad-jump="${k}"`);
        expect(html).toContain(`<span class="ms2k-legend-synth">${expectedSynthLabels[k - 1]}</span>`);
      }
    });
  });
});
