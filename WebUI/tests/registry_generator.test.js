import { describe, it, expect } from 'vitest';
import {
  normalizeType,
  normalizeParam,
  formatFloat,
  validateAndProcess,
  TYPE_ALIASES,
  VALID_TYPES,
} from '../../Scripts/registry_core.js';

// ─── normalizeType ──────────────────────────────────────────────────────────

describe('normalizeType', () => {
  it('passes through valid types unchanged', () => {
    expect(normalizeType('continuous')).toBe('continuous');
    expect(normalizeType('integer')).toBe('integer');
    expect(normalizeType('choice')).toBe('choice');
    expect(normalizeType('boolean')).toBe('boolean');
  });

  it('resolves "bool" alias to "boolean"', () => {
    expect(normalizeType('bool')).toBe('boolean');
  });

  it('returns unknown types as-is (validation catches them later)', () => {
    expect(normalizeType('string')).toBe('string');
    expect(normalizeType('')).toBe('');
  });
});

// ─── formatFloat ────────────────────────────────────────────────────────────

describe('formatFloat', () => {
  it('appends "f" to values without decimal point', () => {
    expect(formatFloat(1)).toBe('1.0f');
    expect(formatFloat(0)).toBe('0.0f');
    expect(formatFloat(127)).toBe('127.0f');
  });

  it('appends "f" to values that already have a decimal point', () => {
    expect(formatFloat(1.0)).toBe('1.0f');
    expect(formatFloat(0.45)).toBe('0.45f');
    expect(formatFloat(-63.0)).toBe('-63.0f');
  });

  it('preserves string decimal values', () => {
    expect(formatFloat('0.8')).toBe('0.8f');
    expect(formatFloat('127')).toBe('127.0f');
  });
});

// ─── validateAndProcess: happy path ─────────────────────────────────────────

describe('validateAndProcess — happy path', () => {
  const minimal = [
    { id: 'paramA', name: 'Param A', group: 'G', cc: null, min: 0, max: 127, default: 64, type: 'integer' },
    { id: 'paramB', name: 'Param B', group: 'G', cc: 7, min: 0, max: 1, default: 0, type: 'boolean' },
  ];

  it('returns no errors for valid params', () => {
    const { errors } = validateAndProcess(minimal);
    expect(errors).toEqual([]);
  });

  it('computes sequential sysex offsets', () => {
    const { processed } = validateAndProcess(minimal);
    expect(processed[0].sysexOffset).toBe(0);
    expect(processed[1].sysexOffset).toBe(1);
  });

  it('returns correct sysexCount', () => {
    const { sysexCount } = validateAndProcess(minimal);
    expect(sysexCount).toBe(2);
  });

  it('preserves all original fields on processed params', () => {
    const { processed } = validateAndProcess(minimal);
    expect(processed[0].id).toBe('paramA');
    expect(processed[0].cc).toBe(null);
    expect(processed[0].min).toBe(0);
    expect(processed[0].max).toBe(127);
    expect(processed[0].type).toBe('integer');
  });
});

// ─── validateAndProcess: sysex: false ───────────────────────────────────────

describe('validateAndProcess — sysex: false excludes from offset map', () => {
  it('skips params with sysex:false, assigns -1', () => {
    const params = [
      { id: 'master', name: 'Master', group: 'G', min: 0, max: 1, default: 0, type: 'continuous', sysex: false },
      { id: 'voice', name: 'Voice', group: 'G', min: 0, max: 3, default: 0, type: 'choice', choices: ['A', 'B'] },
      { id: 'cutoff', name: 'Cutoff', group: 'G', min: 0, max: 127, default: 100, type: 'integer' },
    ];
    const { processed, sysexCount } = validateAndProcess(params);

    expect(processed[0].sysexOffset).toBe(-1); // master: sysex:false
    expect(processed[1].sysexOffset).toBe(0);  // voice: first with offset
    expect(processed[2].sysexOffset).toBe(1);  // cutoff: second
    expect(sysexCount).toBe(2);
  });

  it('handles all params sysex:false', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
      { id: 'b', name: 'B', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
    ];
    const { processed, sysexCount } = validateAndProcess(params);
    expect(processed[0].sysexOffset).toBe(-1);
    expect(processed[1].sysexOffset).toBe(-1);
    expect(sysexCount).toBe(0);
  });

  it('fills gaps from sysex:false params without skipping numbers', () => {
    const params = [
      { id: 'skip', name: 'Skip', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
      { id: 'first', name: 'First', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'skip2', name: 'Skip2', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
      { id: 'second', name: 'Second', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { processed } = validateAndProcess(params);
    // Offsets should be contiguous: skip→-1, first→0, skip2→-1, second→1
    expect(processed[0].sysexOffset).toBe(-1);
    expect(processed[1].sysexOffset).toBe(0);
    expect(processed[2].sysexOffset).toBe(-1);
    expect(processed[3].sysexOffset).toBe(1);
  });
});

// ─── validateAndProcess: type normalization ─────────────────────────────────

describe('validateAndProcess — type normalization', () => {
  it('normalizes "bool" type to "boolean"', () => {
    const params = [
      { id: 'flag', name: 'Flag', group: 'G', min: 0, max: 1, default: 0, type: 'bool' },
    ];
    const { processed, errors } = validateAndProcess(params);
    expect(errors).toEqual([]);
    expect(processed[0].type).toBe('boolean');
  });

  it('normalizes "bool" mixed with "boolean" correctly', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'bool' },
      { id: 'b', name: 'B', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { processed, errors } = validateAndProcess(params);
    expect(errors).toEqual([]);
    expect(processed[0].type).toBe('boolean');
    expect(processed[1].type).toBe('boolean');
  });

  it('rejects unknown types', () => {
    const params = [
      { id: 'x', name: 'X', group: 'G', min: 0, max: 1, default: 0, type: 'string' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('unknown type');
    expect(errors[0]).toContain('string');
  });
});

// ─── validateAndProcess: duplicate ID detection ─────────────────────────────

describe('validateAndProcess — duplicate ID detection', () => {
  it('detects duplicate IDs', () => {
    const params = [
      { id: 'cutoff', name: 'Cutoff 1', group: 'G', min: 0, max: 127, default: 100, type: 'integer' },
      { id: 'cutoff', name: 'Cutoff 2', group: 'G', min: 0, max: 127, default: 100, type: 'integer' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('duplicate parameter id');
    expect(errors[0]).toContain('cutoff');
  });

  it('reports all duplicate IDs when multiple pairs exist', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'b', name: 'B', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'a', name: 'A2', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'b', name: 'B2', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { errors } = validateAndProcess(params);
    const dupErrors = errors.filter(e => e.includes('duplicate parameter id'));
    expect(dupErrors.length).toBe(2);
  });
});

// ─── validateAndProcess: duplicate CC detection ─────────────────────────────

describe('validateAndProcess — duplicate CC detection', () => {
  it('detects duplicate CCs', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', cc: 74, min: 0, max: 127, default: 100, type: 'integer' },
      { id: 'b', name: 'B', group: 'G', cc: 74, min: 0, max: 127, default: 0, type: 'integer' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('CC 74');
    expect(errors[0]).toContain('"a"');
    expect(errors[0]).toContain('"b"');
  });

  it('allows null/undefined CCs without conflict', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', cc: null, min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'b', name: 'B', group: 'G', cc: undefined, min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'c', name: 'C', group: 'G', cc: -1, min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors).toEqual([]);
  });

  it('allows the same CC on different numeric values', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', cc: 74, min: 0, max: 127, default: 100, type: 'integer' },
      { id: 'b', name: 'B', group: 'G', cc: 75, min: 0, max: 127, default: 0, type: 'integer' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors).toEqual([]);
  });
});

// ─── validateAndProcess: choice validation ──────────────────────────────────

describe('validateAndProcess — choice validation', () => {
  it('rejects choice with only 1 option', () => {
    const params = [
      { id: 'x', name: 'X', group: 'G', min: 0, max: 0, default: 0, type: 'choice', choices: ['Only'] },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('at least 2 choices');
  });

  it('rejects choice with no choices array', () => {
    const params = [
      { id: 'x', name: 'X', group: 'G', min: 0, max: 1, default: 0, type: 'choice' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('at least 2 choices');
  });

  it('accepts choice with 2+ options', () => {
    const params = [
      { id: 'x', name: 'X', group: 'G', min: 0, max: 1, default: 0, type: 'choice', choices: ['A', 'B'] },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors).toEqual([]);
  });
});

// ─── validateAndProcess: required field validation ──────────────────────────

describe('validateAndProcess — required fields', () => {
  it('rejects param missing id', () => {
    const params = [
      { name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
    expect(errors[0]).toContain('missing required field');
  });

  it('rejects param missing name', () => {
    const params = [
      { id: 'a', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
  });

  it('rejects param missing type', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', min: 0, max: 1, default: 0 },
    ];
    const { errors } = validateAndProcess(params);
    // Missing type triggers both 'required field' and 'unknown type' errors
    expect(errors.length).toBe(2);
    expect(errors.some(e => e.includes('missing required field'))).toBe(true);
    expect(errors.some(e => e.includes('unknown type'))).toBe(true);
  });

  it('rejects param missing min', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', max: 1, default: 0, type: 'boolean' },
    ];
    const { errors } = validateAndProcess(params);
    expect(errors.length).toBe(1);
  });
});

// ─── validateAndProcess: combined error scenarios ───────────────────────────

describe('validateAndProcess — multiple errors collected', () => {
  it('collects all errors without short-circuiting', () => {
    const params = [
      { id: 'dup', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'dup', name: 'B', group: 'G', cc: 50, min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'bad', name: 'C', group: 'G', cc: 50, min: 0, max: 1, default: 0, type: 'choice', choices: ['X'] },
    ];
    const { errors } = validateAndProcess(params);
    // Should have: dup id, dup CC, bad choice
    expect(errors.length).toBeGreaterThanOrEqual(3);
    expect(errors.some(e => e.includes('duplicate parameter id'))).toBe(true);
    expect(errors.some(e => e.includes('CC 50'))).toBe(true);
    expect(errors.some(e => e.includes('at least 2 choices'))).toBe(true);
  });
});

// ─── validateAndProcess: sequential offset ordering ─────────────────────────

describe('validateAndProcess — sysex offset ordering matches schema order', () => {
  it('assigns offsets strictly by position in the array', () => {
    const params = [
      { id: 'z_last', name: 'Z', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'a_first', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'm_middle', name: 'M', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { processed } = validateAndProcess(params);
    // z_last is first in array → offset 0
    expect(processed[0].sysexOffset).toBe(0);
    expect(processed[0].id).toBe('z_last');
    // a_first is second → offset 1
    expect(processed[1].sysexOffset).toBe(1);
    expect(processed[1].id).toBe('a_first');
    // m_middle is third → offset 2
    expect(processed[2].sysexOffset).toBe(2);
    expect(processed[2].id).toBe('m_middle');
  });

  it('skips sysex:false params and continues numbering', () => {
    const params = [
      { id: 'p1', name: 'P1', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
      { id: 'p2', name: 'P2', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
      { id: 'p3', name: 'P3', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysex: false },
      { id: 'p4', name: 'P4', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { processed } = validateAndProcess(params);
    expect(processed[0].sysexOffset).toBe(-1);  // p1: false
    expect(processed[1].sysexOffset).toBe(0);   // p2: first
    expect(processed[2].sysexOffset).toBe(-1);  // p3: false
    expect(processed[3].sysexOffset).toBe(1);   // p4: second
  });
});

// ─── validateAndProcess: no manual sysexOffset in input ─────────────────────

describe('validateAndProcess — ignores any sysexOffset in input', () => {
  it('does not read input sysexOffset field', () => {
    const params = [
      { id: 'a', name: 'A', group: 'G', min: 0, max: 1, default: 0, type: 'boolean', sysexOffset: 99 },
      { id: 'b', name: 'B', group: 'G', min: 0, max: 1, default: 0, type: 'boolean' },
    ];
    const { processed } = validateAndProcess(params);
    // Should be auto-computed, ignoring the 99 in input
    expect(processed[0].sysexOffset).toBe(0);
    expect(processed[1].sysexOffset).toBe(1);
  });
});

// ─── validateAndProcess: empty input ────────────────────────────────────────

describe('validateAndProcess — edge cases', () => {
  it('handles empty parameter array', () => {
    const { processed, errors, sysexCount } = validateAndProcess([]);
    expect(processed).toEqual([]);
    expect(errors).toEqual([]);
    expect(sysexCount).toBe(0);
  });

  it('handles single parameter', () => {
    const params = [
      { id: 'solo', name: 'Solo', group: 'G', min: 0, max: 100, default: 50, type: 'integer' },
    ];
    const { processed, errors, sysexCount } = validateAndProcess(params);
    expect(errors).toEqual([]);
    expect(processed[0].sysexOffset).toBe(0);
    expect(sysexCount).toBe(1);
  });
});
