#!/usr/bin/env node
/**
 * ABDMS2000 — sincroniza el WebUI del ABDScope dentro del WebUI del host.
 *
 * USO
 *   node Scripts/sync_scope.js --check    # NO escribe. Dice que pondria y que
 *                                         # BORRARIA, y sale 1 si hay cambios.
 *   node Scripts/sync_scope.js            # sincroniza de verdad
 *   node Scripts/sync_scope.js --help
 *
 * POR QUE EL FLAG NO ES OPCIONAL. `WebUI/abdscope/` se SUSTITUYE, no se copia
 * encima: el script borra el directorio entero antes de copiar el de al lado. Un
 * fichero adaptationado a mano dentro del destino —un ajuste de este host que
 * todavia no esta en ABDScope— se va con el mismo silencio que una copia vieja.
 * Con `--check` esa lista se ve antes, y el check nombra aparte lo que se borraria,
 * que es justo lo que despues no se puede recuperar.
 */
import path from 'path';
import { fileURLToPath } from 'url';
import { sincronizarDir, leerFlags } from './syncSeguro.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceDir = path.resolve(rootDir, '..', 'ABDScope', 'WebUI');
const destRoot = path.join(rootDir, 'WebUI', 'abdscope');

const toPosix = (p) => p.split(path.sep).join('/');
const EXCLUDE_FILES = new Set(['vite.config.js', 'vite.config.mjs', 'vitest.config.js', 'package.json', 'package-lock.json', 'pnpm-lock.yaml', 'index.html']);
const EXCLUDE_DIRS = new Set(['node_modules', 'tests', 'demo']);

function shouldCopy(src) {
  if (EXCLUDE_FILES.has(path.basename(src))) return false;
  const rel = toPosix(src);
  for (const dir of EXCLUDE_DIRS) {
    if (path.basename(src) === dir) return false;
    if (rel.split('/').includes(dir)) return false;
  }
  return true;
}

function usage() {
  console.log([
    'Sincroniza ABDScope/WebUI dentro de WebUI/abdscope/.',
    '',
    'Uso: node Scripts/sync_scope.js [--check] [--help]',
    '',
    '  (sin flag)  Borra WebUI/abdscope/ entero y lo vuelve a copiar del origen.',
    '  --check     NO escribe. Compara lo que hay con lo que se copiaria y sale 1',
    '              si algo se anadiria, se borraria o cambiaria.',
  ].join('\n'));
}

const flags = leerFlags('ABDScope -> WebUI/abdscope');
if (flags.ayuda) { usage(); process.exit(0); }
if (flags.desconocido) {
  usage();
  console.error(`Argumento desconocido: ${flags.desconocido}`);
  process.exit(2);
}

const ok = sincronizarDir({
  etiqueta: 'ABDScope WebUI -> WebUI/abdscope/',
  origen: sourceDir,
  destino: destRoot,
  filtro: shouldCopy,
  check: flags.check,
});

if (!ok) {
  process.exit(1);
}

console.log(flags.check ? 'OK - ABDScope ya esta sincronizado.' : 'OK - ABDScope sincronizado.');
