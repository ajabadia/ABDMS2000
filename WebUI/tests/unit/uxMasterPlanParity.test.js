import { describe, it, expect, beforeEach } from 'vitest';
import { FACTORY_PROGRAMS, generateMS2000SysExBuffer } from '../../src/contracts/factoryPresets.js';
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
    });
  });

  describe('UX2.1: Timbre 1 / Timbre 2 Parameter Symmetry (115 params)', () => {
    it('maps every t2* parameter in PARAM_LOOKUP from its Timbre 1 base ID', () => {
      const t2Ids = Object.keys(PARAM_LOOKUP).filter((k) => k.startsWith('t2') && k.length > 2);
      expect(t2Ids.length).toBe(115);

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
});
