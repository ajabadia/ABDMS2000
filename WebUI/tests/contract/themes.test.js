/**
 * Contrato de los temas del MS2000: datos declarativos que el ThemeSwitcher
 * compartido entiende. Si alguien toca aqui, la ficha de la nav-bar y el
 * bridge nativo cambian de comportamiento — este test lo fija.
 */
import { describe, expect, it } from 'vitest';

import { MS2000_THEMES, synthModeIndexOf } from '../../src/contracts/themes.js';

describe('MS2000_THEMES (datos declarativos)', () => {
  it('declares the three personalities with id, label, skin and synthMode payload', () => {
    expect(MS2000_THEMES.map((t) => t.id)).toEqual(['ms2000', 'microkorg', 'advanced']);

    for (const theme of MS2000_THEMES) {
      expect(typeof theme.label).toBe('string');
      expect(theme.bodyClass).toMatch(/^skin-/);
      expect([0, 1, 2]).toContain(theme.payload);
    }
  });

  it('maps each theme to the synthMode index the native bridge expects', () => {
    expect(synthModeIndexOf('ms2000')).toBe(0);
    expect(synthModeIndexOf('microkorg')).toBe(1);
    expect(synthModeIndexOf('advanced')).toBe(2);
    expect(synthModeIndexOf('desconocido')).toBe(0); // default seguro
  });
});
