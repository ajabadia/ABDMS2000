/**
 * Guard de dirección del canal nativo → JS de JUCE WebView2 — pruebas del helper
 * **compartido** por toda la suite.
 *
 * El helper vive en:
 *   ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js
 * y lo consumen todos los hosts con puente WebView2:
 *   - ABDMS2000 y ABDCZ101 por vitest (ver también `hostModelAnnouncement.test.js`
 *     para los checks sobre el código REAL de este proyecto),
 *   - ABDJUNiO601 con un script `node`
 *     (`scripts/check_webview_bridge_direction.js`), al no tener runner JS.
 *
 * Sus pruebas de comportamiento viven aquí porque ABDSharedCode no tiene runner JS
 * propio. Son sintéticas (no dependen de ningún árbol en concreto), así que valen
 * igual desde cualquier consumidor.
 */
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { describe, it, expect } from 'vitest';
import {
  FORBIDDEN_CALL,
  REQUIRED_CALL,
  checkBridgeDirection,
  compactCpp,
  findForbiddenLines,
  formatFindings,
  listCppSources,
  scanForForbiddenCalls,
  stripCppComments
} from '../../../../ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js';

const here = path.dirname(fileURLToPath(import.meta.url));
const workspaceRoot = path.resolve(here, '../../../..');
const sourceRoot = path.resolve(here, '../../../Source');

describe('helper compartido — comentarios C/C++', () => {
  it('ignora el patrón cuando está comentado (`//` y `/* */`)', () => {
    expect(stripCppComments(`// ${FORBIDDEN_CALL}('event', x)`)).not.toContain(FORBIDDEN_CALL);
    expect(stripCppComments(`/* ${FORBIDDEN_CALL}('event', x) */`)).not.toContain(FORBIDDEN_CALL);
  });

  it('no se come los `//` de una URL', () => {
    const source = 'auto u = "https://juce.backend/x"; // nota';
    expect(stripCppComments(source)).toContain('https://juce.backend/x');
  });

  it('sí detecta el patrón en código real', () => {
    const source = `webView_->evaluateJavascript("window.__JUCE__.${FORBIDDEN_CALL}('event', j);");`;
    expect(stripCppComments(source)).toContain(FORBIDDEN_CALL);
    expect(findForbiddenLines(source)).toEqual([1]);
  });

  it('numera las líneas que usan el canal equivocado', () => {
    const source = [`// ${FORBIDDEN_CALL}`, 'ok();', `  x.${FORBIDDEN_CALL}(1);`].join('\n');
    expect(findForbiddenLines(source)).toEqual([3]);
  });

  it('compactCpp colapsa el espaciado (estable ante reindentaciones)', () => {
    expect(compactCpp('void  f()\n{\n    g();\n}')).toBe('void f() { g(); }');
  });
});

describe('helper compartido — recorrido del árbol', () => {
  it('listCppSources cubre el código del host y salta build/ y node_modules', () => {
    const files = listCppSources(sourceRoot);
    const relatives = files.map(file => path.relative(sourceRoot, file).split(path.sep).join('/'));

    expect(files.length).toBeGreaterThan(50);
    expect(relatives).toContain('Plugin/PluginEditor.cpp');
    expect(relatives.some(file => file.startsWith('build/') || file.includes('node_modules'))).toBe(false);
    expect(relatives.every(file => /\.(h|hpp|cpp|cc|cxx|mm|ipp)$/i.test(file))).toBe(true);
  });

  it('scanForForbiddenCalls devuelve rutas relativas + líneas', () => {
    // El árbol real está limpio (lo verifica `hostModelAnnouncement.test.js`);
    // aquí solo se valida la forma del retorno.
    expect(scanForForbiddenCalls(sourceRoot)).toEqual([]);
  });
});

describe('helper compartido — veredicto de checkBridgeDirection', () => {
  it('reporta un emisor ausente (el guard no puede quedar obsoleto en silencio)', () => {
    const result = checkBridgeDirection({
      sourceRoot,
      emitters: ['Plugin/PluginEditor.cpp', 'Plugin/NoExiste.cpp']
    });

    expect(result.absentEmitters).toEqual(['Plugin/NoExiste.cpp']);
    expect(result.ok).toBe(false);
    expect(formatFindings(result)).toContain('NoExiste.cpp');
  });

  it('reporta un emisor que existe pero no emite con la API correcta', () => {
    // HostModelAnnouncement.h es un header real del proyecto que no contiene
    // `emitEventIfBrowserIsVisible`: sirve como "fichero presente pero mudo".
    const result = checkBridgeDirection({
      sourceRoot,
      emitters: ['Plugin/HostModelAnnouncement.h']
    });

    expect(result.missingEmitters).toEqual(['Plugin/HostModelAnnouncement.h']);
    expect(result.ok).toBe(false);
  });

  it('sin sourceRoot avisa en vez de aprobar por defecto', () => {
    expect(() => checkBridgeDirection({})).toThrow(/sourceRoot/);
  });

  it('formatFindings resume el veredicto', () => {
    expect(formatFindings({ ok: true, problems: [] })).toMatch(/^OK/);
    expect(formatFindings({ ok: false, problems: ['a.cpp:1 falla'] })).toContain('a.cpp:1');
  });
});

describe('helper compartido — contrato y ubicación', () => {
  it('vive en el repo compartido de la suite', () => {
    const helperPath = path.join(
      workspaceRoot,
      'ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js'
    );
    expect(fs.existsSync(helperPath)).toBe(true);
  });

  it('no tiene dependencias externas (lo consumen vitest y `node` pelado)', () => {
    const source = fs.readFileSync(
      path.join(workspaceRoot, 'ABDSharedCode/WebView2Bridge/testing/webviewBridgeDirection.js'),
      'utf8'
    );
    const imports = [...source.matchAll(/^import .*from '([^']+)';$/gm)].map(match => match[1]);

    expect(imports).toEqual(['node:fs', 'node:path']);
  });

  it('expone la API que consumen los hosts y las dos constantes del canal', () => {
    expect(typeof checkBridgeDirection).toBe('function');
    expect(typeof formatFindings).toBe('function');
    expect(typeof listCppSources).toBe('function');
    expect(typeof stripCppComments).toBe('function');
    expect(FORBIDDEN_CALL).toBe('backend.emitEvent');
    expect(REQUIRED_CALL).toBe('emitEventIfBrowserIsVisible');
  });
});
