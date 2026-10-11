// registry_core.js — testable core logic for registry generation.
// Imported by both registry_generator.js (build) and registry_generator.test.js (tests).

export const TYPE_ALIASES = { bool: 'boolean' };
export const VALID_TYPES = ['continuous', 'integer', 'choice', 'boolean'];

/**
 * Normalize a single type string, resolving aliases like "bool" → "boolean".
 */
export function normalizeType(type) {
  return TYPE_ALIASES[type] || type;
}

/**
 * Normalize a parameter object (type alias resolution).
 */
export function normalizeParam(p) {
  return { ...p, type: normalizeType(p.type) };
}

/**
 * Format a number as a C++ float literal (e.g. 1.0 → "1.0f", 42 → "42.0f").
 */
export function formatFloat(val) {
  const str = String(val);
  return str.includes('.') ? `${str}f` : `${str}.0f`;
}

/**
 * Validate an array of parameters and compute sequential sysex offsets.
 *
 * Rules:
 *  - Each param must have id, name, min, max, default, type.
 *  - Type must be one of VALID_TYPES (after alias normalization).
 *  - Choice params need ≥ 2 choices.
 *  - No duplicate param IDs allowed.
 *  - No duplicate MIDI CCs allowed.
 *  - Sysex offsets are auto-computed sequentially (0, 1, 2, ...) for every
 *    param that is NOT marked "sysex: false".
 *  - No duplicate sysex offsets allowed (guaranteed by construction).
 *
 * @param {Array<object>} rawParams  — raw parameter objects from the JSON schema.
 * @returns {{ processed: Array<object>, errors: string[], sysexCount: number }}
 */
export function validateAndProcess(rawParams) {
  const errors = [];
  const seenIds = new Set();
  const seenCcs = new Map();

  // Phase 1: normalize types
  const params = rawParams.map(normalizeParam);

  // Phase 2: validate + compute sysex offsets
  let sysexCounter = 0;
  const seenSysex = new Map();

  const processed = params.map(p => {
    // Required fields
    if (!p.id || !p.name || p.min === undefined || p.max === undefined || p.default === undefined || !p.type) {
      errors.push(`Parameter missing required field (id/name/min/max/default/type): ${JSON.stringify(p)}`);
    }

    // Valid type
    if (!VALID_TYPES.includes(p.type)) {
      errors.push(`[${p.id}] unknown type "${p.type}" (valid: ${VALID_TYPES.join(', ')})`);
    }

    // Validate curve is in valid range (0-3)
    if (p.curve !== undefined && p.curve !== null && (p.curve < 0 || p.curve > 3)) {
      errors.push(`[${p.id}] curve value ${p.curve} out of range (valid: 0=Linear, 1=Exp, 2=Pow, 3=Lut)`);
    }

    // Choice needs ≥ 2 options
    if (p.type === 'choice' && (!p.choices || p.choices.length < 2)) {
      errors.push(`[${p.id}] choice type requires at least 2 choices`);
    }

    // Unique ID
    if (seenIds.has(p.id)) {
      errors.push(`[${p.id}] duplicate parameter id`);
    }
    seenIds.add(p.id);

    // Unique CC
    const cc = (p.cc !== null && p.cc !== undefined) ? p.cc : -1;
    if (cc >= 0) {
      if (seenCcs.has(cc)) {
        errors.push(`CC ${cc} assigned to both "${seenCcs.get(cc)}" and "${p.id}"`);
      }
      seenCcs.set(cc, p.id);
    }

    // Validate wire value
    if (p.wire !== undefined && p.wire !== null) {
      const validWires = ['None', 'CC', 'SysEx', 'Nrpn'];
      if (!validWires.includes(p.wire)) {
        errors.push(`[${p.id}] wire value "${p.wire}" invalid (valid: ${validWires.join(', ')})`);
      }
    }

    // Validate wireCodec value
    if (p.wireCodec !== undefined && p.wireCodec !== null) {
      const validCodecs = ['Value', 'Bipolar', 'Enum'];
      if (!validCodecs.includes(p.wireCodec)) {
        errors.push(`[${p.id}] wireCodec value "${p.wireCodec}" invalid (valid: ${validCodecs.join(', ')})`);
      }
    }

    // Auto-computed sysex offset
    let sysexOffset = -1;
    if (p.sysex !== false) {
      sysexOffset = sysexCounter++;
      if (seenSysex.has(sysexOffset)) {
        errors.push(`[${p.id}] computed duplicate sysexOffset ${sysexOffset}`);
      }
      seenSysex.set(sysexOffset, p.id);
    }

    // Validate domainMin <= domainMax
    if (p.domainMin !== undefined && p.domainMax !== undefined) {
      if (p.domainMin > p.domainMax) {
        errors.push(`[${p.id}] domainMin (${p.domainMin}) > domainMax (${p.domainMax}), must be domainMin <= domainMax`);
      }
    }

    return { ...p, sysexOffset };
  });

  return { processed, errors, sysexCount: sysexCounter };
}
