import { describe, it, expect } from 'vitest';
import { PARAMETERS_SPEC, PARAM_LOOKUP, PARAM_IDS } from '../src/contracts/registry.gen.js';
import { BUILD_INFO } from '../src/contracts/buildVersion.js';

describe('Parameter Registry Contracts', () => {
  it('should load parameters specification correctly', () => {
    expect(PARAMETERS_SPEC).toBeDefined();
    expect(PARAMETERS_SPEC.parameters.length).toBeGreaterThan(10);
  });

  it('should have valid lookup table matching param IDs', () => {
    expect(PARAM_LOOKUP['masterVolume']).toBeDefined();
    expect(PARAM_LOOKUP['filterCutoff']).toBeDefined();
    expect(PARAM_IDS.filterCutoff).toBe('filterCutoff');
  });

  it('should export valid build version info', () => {
    expect(BUILD_INFO.version).toBe('1.0.0');
    expect(BUILD_INFO.productName).toBe('ABDMS2000');
    expect(BUILD_INFO.buildNumber).toBeDefined();
  });
});
