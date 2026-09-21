/**
 * Temas del MS2000 — datos, no código.
 * =====================================
 *
 * La UI tiene tres personalidades (ms2000, microkorg, advanced/cyberpunk).
 * Cada entrada declara TODO lo que la selección del tema implica:
 *
 *   - id / label          → los que el ThemeSwitcher compartido ya entiende
 *                           (data-theme + etiqueta del selector).
 *   - bodyClass           → la clase de skin que viaja en <body>. El switcher
 *                           compartido la aplica con política de dueño único.
 *   - payload             → el índice de `synthMode` que el bridge nativo
 *                           espera (ms2000=0, microkorg=1, advanced=2). Es el
 *                           dato del synth, no del switcher: viaja como
 *                           segundo argumento del onChange.
 *   - modeIdx (helper)    → lectura cómoda del payload del tema activo.
 */

export const MS2000_THEMES = [
  { id: 'ms2000', label: 'MS2000', bodyClass: 'skin-ms2000', payload: 0 },
  { id: 'microkorg', label: 'microKORG', bodyClass: 'skin-microkorg', payload: 1 },
  { id: 'advanced', label: 'ADVANCED', bodyClass: 'skin-cyberpunk', payload: 2 },
];

/** El índice de synthMode del tema activo (default: el primero). */
export function synthModeIndexOf(themeId) {
  const theme = MS2000_THEMES.find((t) => t.id === themeId);
  return theme ? theme.payload : MS2000_THEMES[0].payload;
}
