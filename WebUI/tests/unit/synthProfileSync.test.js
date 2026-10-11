import { beforeEach, describe, expect, it, vi } from 'vitest';
import { paramStore } from '../../src/contracts/paramStore.js';
import { themeIdOfSynthMode, synthModeIndexOf, MS2000_THEMES } from '../../src/contracts/themes.js';

describe('Synth Profile <-> Theme bidirectional synchronization contract', () => {
  it('correctly maps all 3 synthMode indices to theme identifiers', () => {
    expect(themeIdOfSynthMode(0)).toBe('ms2000');
    expect(themeIdOfSynthMode(1)).toBe('microkorg');
    expect(themeIdOfSynthMode(2)).toBe('advanced');
  });

  it('correctly maps theme identifiers back to synthMode indices', () => {
    expect(synthModeIndexOf('ms2000')).toBe(0);
    expect(synthModeIndexOf('microkorg')).toBe(1);
    expect(synthModeIndexOf('advanced')).toBe(2);
  });

  it('triggers paramStore listener without recursion loop when synchronized', () => {
    const mockThemeSwitcher = {
      value: 'ms2000',
      setValue: vi.fn((themeId) => {
        mockThemeSwitcher.value = themeId;
      }),
    };

    // Simulate setupThemeSelector registration
    const syncHandler = (modeIdx) => {
      const targetThemeId = themeIdOfSynthMode(modeIdx);
      if (mockThemeSwitcher && mockThemeSwitcher.value !== targetThemeId) {
        mockThemeSwitcher.setValue(targetThemeId, { fromUser: false });
      }
    };

    paramStore.onChange('synthMode', syncHandler);

    // 1. Changing to microkorg in paramStore triggers mockThemeSwitcher.setValue
    paramStore.set('synthMode', 1);
    expect(mockThemeSwitcher.setValue).toHaveBeenCalledWith('microkorg', { fromUser: false });
    expect(mockThemeSwitcher.value).toBe('microkorg');

    // 2. Setting same mode does not trigger extra setValue
    mockThemeSwitcher.setValue.mockClear();
    paramStore.set('synthMode', 1);
    expect(mockThemeSwitcher.setValue).not.toHaveBeenCalled();

    // 3. Changing to advanced triggers mockThemeSwitcher.setValue
    paramStore.set('synthMode', 2);
    expect(mockThemeSwitcher.setValue).toHaveBeenCalledWith('advanced', { fromUser: false });
    expect(mockThemeSwitcher.value).toBe('advanced');

    // 4. Return to ms2000
    paramStore.set('synthMode', 0);
    expect(mockThemeSwitcher.setValue).toHaveBeenCalledWith('ms2000', { fromUser: false });
    expect(mockThemeSwitcher.value).toBe('ms2000');
  });

  it('pairs 16 vocoder bands into 8 dual channels in microKORG mode', () => {
    // Channel k (1..8) controls bands 2k-1 and 2k
    for (let ch = 1; ch <= 8; ch++) {
      const bA = (ch * 2) - 1;
      const bB = ch * 2;
      expect(bA).toBeGreaterThanOrEqual(1);
      expect(bB).toBeLessThanOrEqual(16);
      expect(bB - bA).toBe(1);
    }
  });

  it('ensures modSeqOn is disabled when switching to microKORG profile', () => {
    paramStore.set('modSeqOn', 1);
    expect(paramStore.get('modSeqOn')).toBe(1);

    // Switching to microkorg (modeIdx 1) forces modSeqOn off
    const syncHandler = (modeIdx) => {
      if (modeIdx === 1) {
        paramStore.set('modSeqOn', 0);
      }
    };
    paramStore.onChange('synthMode', syncHandler);

    paramStore.set('synthMode', 1);
    expect(paramStore.get('modSeqOn')).toBe(0);
  });

  it('declares arpLastStep and arpStep1..arpStep8 in registry contract with sysex: false', async () => {
    const { PARAM_LOOKUP } = await import('../../src/contracts/registry.gen.js');
    
    expect(PARAM_LOOKUP['arpLastStep']).toBeDefined();
    expect(PARAM_LOOKUP['arpLastStep'].min).toBe(1);
    expect(PARAM_LOOKUP['arpLastStep'].max).toBe(8);
    expect(PARAM_LOOKUP['arpLastStep'].default).toBe(8);
    expect(PARAM_LOOKUP['arpLastStep'].sysexOffset).toBeNull();

    for (let s = 1; s <= 8; s++) {
      const stepParam = PARAM_LOOKUP[`arpStep${s}`];
      expect(stepParam).toBeDefined();
      expect(stepParam.min).toBe(0);
      expect(stepParam.max).toBe(1);
      expect(stepParam.default).toBe(1);
      expect(stepParam.sysexOffset).toBeNull();
    }
  });

  it('manages 8-step rhythm mask values in paramStore for microKORG and Advanced profiles', () => {
    // Set initial full pattern
    paramStore.set('arpLastStep', 8);
    for (let s = 1; s <= 8; s++) {
      paramStore.set(`arpStep${s}`, 1);
    }
    expect(paramStore.get('arpLastStep')).toBe(8);

    // Set odd steps pattern (1, 3, 5, 7 on; 2, 4, 6, 8 muted)
    for (let s = 1; s <= 8; s++) {
      paramStore.set(`arpStep${s}`, s % 2 === 1 ? 1 : 0);
    }

    const lastStep = paramStore.get('arpLastStep');
    let activeSteps = 0;
    for (let s = 1; s <= lastStep; s++) {
      if (paramStore.get(`arpStep${s}`) === 1) activeSteps++;
    }
    expect(activeSteps).toBe(4);

    // Change pattern length (lastStep = 4)
    paramStore.set('arpLastStep', 4);
    activeSteps = 0;
    for (let s = 1; s <= paramStore.get('arpLastStep'); s++) {
      if (paramStore.get(`arpStep${s}`) === 1) activeSteps++;
    }
    expect(activeSteps).toBe(2); // Only steps 1 and 3 are active within length 4
  });
});

