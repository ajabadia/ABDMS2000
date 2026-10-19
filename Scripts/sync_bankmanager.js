#!/usr/bin/env node
/**
 * ABDMS2000 — sincroniza el WebUI del ABDBankManager dentro del WebUI del host.
 *
 * USO
 *   node Scripts/sync_bankmanager.js --check    # NO escribe. Dice que pondria y
 *                                              # que BORRARIA, y sale 1 si hay cambios.
 *   node Scripts/sync_bankmanager.js            # sincroniza de verdad
 *   node Scripts/sync_bankmanager.js --help
 *
 * POR QUE EL FLAG NO ES OPCIONAL. `WebUI/abdbank/` no se copia encima: se SUSTITUYE.
 * El script borra el directorio entero antes de copiar el de al lado. Un fichero
 * anadido ahi dentro —un ajuste del host que nadie sincronizo, un test escrito a
 * mano— desaparece con el mismo silencio que una copia vieja. Con `--check` se
 * puede ver esa lista antes de que sea tarde, y el check dice explicitamente lo que
 * se BORRARIA, que es la parte que no se ve en un `git status` despues.
 */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { sincronizarDir, leerFlags, contarArchivos } from './syncSeguro.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceCandidates = [
  path.resolve(rootDir, '..', 'ABDBankManager', 'WebUI'),
  path.resolve(rootDir, '..', 'ABDShared', 'Code', 'BankManager', 'WebUI'),
];
const destRoot = path.join(rootDir, 'WebUI', 'abdbank');

const toPosix = (p) => p.split(path.sep).join('/');
const EXCLUDE_FILES = new Set(['vite.config.js', 'vite.config.mjs', 'package.json', 'package-lock.json', 'pnpm-lock.yaml']);

function shouldCopy(src) {
  if (EXCLUDE_FILES.has(path.basename(src))) return false;
  if (path.basename(src) === 'tests') return false;
  if (toPosix(src).includes('/tests/')) return false;
  return true;
}

const sourceDir =
  sourceCandidates.find((dir) => fs.existsSync(dir) && contarArchivos(dir, shouldCopy) > 0) ||
  sourceCandidates[0];

function usage() {
  console.log([
    'Sincroniza ABDBankManager/WebUI dentro de WebUI/abdbank/.',
    '',
    'Uso: node Scripts/sync_bankmanager.js [--check] [--help]',
    '',
    '  (sin flag)  Borra WebUI/abdbank/ entero y lo vuelve a copiar del origen.',
    '  --check     NO escribe. Compara lo que hay con lo que se copiaria y sale 1',
    '              si algo se anadiria, se borraria o cambiaria.',
  ].join('\n'));
}

const flags = leerFlags('ABDBankManager -> WebUI/abdbank');
if (flags.ayuda) { usage(); process.exit(0); }
if (flags.desconocido) {
  usage();
  console.error(`Argumento desconocido: ${flags.desconocido}`);
  process.exit(2);
}

if ((!fs.existsSync(sourceDir) || contarArchivos(sourceDir, shouldCopy) === 0) &&
    fs.existsSync(path.join(destRoot, 'index.html'))) {
  console.log(`OK - Origen ABDBankManager/WebUI vacio; se conserva WebUI/abdbank existente (${contarArchivos(destRoot)} archivos).`);
  process.exit(0);
}

const ok = sincronizarDir({
  etiqueta: 'ABDBankManager WebUI -> WebUI/abdbank/',
  origen: sourceDir,
  destino: destRoot,
  filtro: shouldCopy,
  check: flags.check,
});

if (!ok) {
  process.exit(1);
}

console.log(flags.check ? 'OK - ABDBankManager ya esta sincronizado.' : 'OK - ABDBankManager sincronizado.');
