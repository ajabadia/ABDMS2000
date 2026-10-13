// @vitest-environment jsdom
/**
 * liveSysExInspectorMS2000.test.js
 * Verificación de integración del LiveSysExInspector flotante transversal en ABDMS2000.
 */

import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import {
  generateMS2000SysExBuffer,
  generateMS2000SysExHex,
} from '../../src/contracts/factoryPresets.js';
import {
  createLiveSysExInspector,
  decodeSysExHeader,
} from '@abdsynths/shared/components';

describe('LiveSysExInspector — ABDMS2000 Integration', () => {
  it('generateMS2000SysExBuffer produce un buffer de 288 bytes válido para MS2000', () => {
    const buf = generateMS2000SysExBuffer({ filterCutoff: 100, osc1Wave: 2 }, 'TestPatch');
    expect(buf).toBeInstanceOf(Uint8Array);
    expect(buf.length).toBe(288);
    expect(buf[0]).toBe(0xf0);
    expect(buf[1]).toBe(0x42); // Korg ID
    expect(buf[2]).toBe(0x30); // Global Ch
    expect(buf[3]).toBe(0x58); // MS2000 Model ID
    expect(buf[4]).toBe(0x40); // 1 Program Dump
    expect(buf[287]).toBe(0xf7); // EOX

    // Header decoding
    const decoded = decodeSysExHeader(buf);
    expect(decoded.valid).toBe(true);
    expect(decoded.manufacturer).toContain('Korg');
    expect(decoded.model).toBe('MS2000');
    expect(decoded.hasEox).toBe(true);
  });

  it('generateMS2000SysExHex genera la representación hexadecimal correcta consistente con el buffer', () => {
    const buf = generateMS2000SysExBuffer({}, 'Lead');
    const hex = generateMS2000SysExHex({}, 'Lead');
    const expectedPrefix = 'F0 42 30 58 40';
    expect(hex.startsWith(expectedPrefix)).toBe(true);
    expect(hex.endsWith('F7')).toBe(true);
    expect(hex.split(' ').length).toBe(buf.length);
  });

  it('index.html contiene el elemento de menú Live SysEx Inspector con atajo Ctrl+I en el menú View', () => {
    const htmlPath = resolve(__dirname, '../../index.html');
    const html = readFileSync(htmlPath, 'utf8');

    expect(html).toContain('data-action="toggle-sysex-inspector"');
    expect(html).toContain('Live SysEx Inspector...');
    expect(html).toContain('Ctrl+I');
  });

  it('createLiveSysExInspector se instancia correctamente con los datos SysEx de MS2000', () => {
    const buf = generateMS2000SysExBuffer({ filterCutoff: 120 }, 'My Sound');
    const inspector = createLiveSysExInspector({
      title: 'MS2000 Live SysEx Inspector',
      floating: true,
      initialBuffer: buf,
    });

    expect(inspector).toBeDefined();
    expect(inspector.rootEl).toBeDefined();
    expect(inspector.rootEl.classList.contains('sysex-inspector--floating')).toBe(true);

    const sizeBadge = inspector.rootEl.querySelector('.sysex-inspector__badge--size');
    expect(sizeBadge.textContent).toBe('288 B');

    // Botón de popout (desacoplamiento a ventana externa para multimonitor)
    const popoutBtn = inspector.rootEl.querySelector('.sysex-inspector__btn--popout');
    expect(popoutBtn).not.toBeNull();

    inspector.destroy();
  });
});
