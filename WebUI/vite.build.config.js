import { defineConfig } from 'vite';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const webUiRoot = __dirname; // .../ABDMS2000/WebUI

/**
 * Build config de produccion para el WebUI nativo (Fase 3 del plan DRY).
 *
 * `vite build` resuelve los bare imports (@abdsynths/shared — cascade de tokens
 * compartidos) y empaqueta el grafo completo (JS + CSS) en WebUI/dist/.
 * La cascada funciona en ambos modos: dev (Vite dev server) y build (producción). El build nativo embebe dist/ (CMakeLists) y el
 * resource provider sirve esos ficheros por coincidencia de sufijo.
 *
 * Nombres estables SIN hash (assets/app.js, assets/index.css): el provider
 * nativo resuelve por ruta, no hay mapping de hashes embebido.
 */

/** Inserta la cascada compartida (como el dev server) y limpia el cache-busting ?v=. */
const ms2000HtmlPreprocess = () => ({
  name: 'ms2000-html-preprocess',
  transformIndexHtml: {
    order: 'pre',
    handler(html) {
      return html
        // Cache-busting del standalone (?v=20260907): en dist el bundler ya
        // gestiona el versionado; una query residual romperia la resolucion.
        .replace(/\?v=\d{8}/g, '')
        // El modo de diagnóstico es exclusivamente una herramienta de desarrollo;
        // no se incluye ni siquiera en el HTML que se embebe en el EXE.
        .replace(/\s*<!-- Diagnostic Test Mode Button -->[\s\S]*?<\/button>\s*/i, '\n')
        .replace(/\s*<!-- Diagnostic Test Modal Dialog -->[\s\S]*?<!-- About Modal Dialog -->/i, '\n<!-- About Modal Dialog -->')
        // Misma insercion que el plugin de dev (vite.config.js): tokens
        // compartidos primero, themes.css del host como capa de overrides.
        .replace(
          '<link rel="stylesheet" href="src/styles/themes.css">',
          '<link rel="stylesheet" href="/src/styles/shared-cascade.css">\n  ' +
          '<link rel="stylesheet" href="src/styles/themes.css">',
        );
    },
  },
});

/**
 * Copia los estaticos que Vite no puede rastrear:
 *  - assets/: imagenes referenciadas por strings en JS ('assets/bender.png',
 *    filmstrips de sliders) y fondos (los url() de CSS si se empaquetan, pero
 *    la copia cubre los que quedan fuera del grafo). assets/images se excluye
 *    (igual que hacia el glob de CMake: se sirven via bankwebui:// en nativo).
 *  - src/wasm/: Motor Emscripten (ms2000_dsp.js + worklet) cargados via
 *    fetch() en runtime; Vite no los rastrea porque no son imports estaticos.
 *  - abdscope/: WebUI del scope sincronizada por sync_scope.js, servida tal cual.
 *  - icons/: si existe (manifest/PWA).
 */
const ms2000StaticCopy = () => ({
  name: 'ms2000-static-copy',
  apply: 'build',
  closeBundle() {
    // assets/images es una junction (bankwebui:// la sirve en nativo) y se
    // excluye igual que hacia el glob de CMake. cpSync con filter evita el
    // EPERM de copyFileSync sobre la junction.
    const copyDir = (src, dest, excludedTop = new Set()) => {
      fs.cpSync(src, dest, {
        recursive: true,
        verbatimSymlinks: true,
        filter: (candidate) => {
          const rel = path.relative(src, candidate);
          if (!rel) return true;
          const top = rel.split(path.sep)[0];
          return !excludedTop.has(top);
        },
      });
    };

    copyDir(path.join(webUiRoot, 'assets'), path.join(webUiRoot, 'dist', 'assets'), new Set(['images']));
    copyDir(path.join(webUiRoot, 'src', 'wasm'), path.join(webUiRoot, 'dist', 'src', 'wasm'));
    copyDir(path.join(webUiRoot, 'abdscope'), path.join(webUiRoot, 'dist', 'abdscope'));
    if (fs.existsSync(path.join(webUiRoot, 'icons'))) {
      copyDir(path.join(webUiRoot, 'icons'), path.join(webUiRoot, 'dist', 'icons'));
    }
    console.log('[ms2000-static-copy] assets/ + src/wasm/ + abdscope/ copiados a dist/');
  },
});

export default defineConfig({
  root: webUiRoot,
  base: './',
  plugins: [ms2000HtmlPreprocess(), ms2000StaticCopy()],
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    // Sin inlining base64: el provider nativo sirve cada fichero por ruta
    assetsInlineLimit: 0,
    rollupOptions: {
      output: {
        // Nombres estables (sin hash) para el resource provider del WebView2
        entryFileNames: 'assets/[name].js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name][extname]',
      },
    },
  },
});
