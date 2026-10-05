#!/usr/bin/env node
/**
 * ABDMS2000 — sincroniza ABDSharedAssets (modelos y marcas) dentro del WebUI.
 *
 * USO
 *   node Scripts/sync_assets.js --check    # NO escribe. Dice que pondria y que
 *                                         # BORRARIA, y sale 1 si hay cambios.
 *   node Scripts/sync_assets.js            # sincroniza de verdad
 *   node Scripts/sync_assets.js --help
 *
 * POR QUE EL FLAG NO ES OPCIONAL. Este script BORRA el destino antes de copiar:
 * `WebUI/images/` y `WebUI/images/brands/` no se mezclan con lo que hay, se
 * sustituyen. Un archivo ahi puede ser una foto anadida a mano, un logo retocado,
 * un placeholder nuevo, y el `rmSync` se lo lleva igual que si fuera una copia
 * vieja. Sin `--check` no hay forma de preguntar, y un pipeline que quiere
 * comprobar acaba de sincronizar otra vez.
 */
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';
import { sincronizarDir, leerFlags } from './syncSeguro.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sharedRoot = path.resolve(rootDir, '..', 'ABDSharedAssets');

const toPosix = (p) => p.split(path.sep).join('/');
const EXCLUDE_DIRS = new Set(['_review']);

function shouldCopy(src) {
  const rel = toPosix(src);
  for (const dir of EXCLUDE_DIRS) {
    if (path.basename(src) === dir) return false;
    if (rel.split('/').includes(dir)) return false;
  }
  return true;
}

function usage() {
  console.log([
    'Sincroniza ABDSharedAssets/models y /brands dentro de WebUI/images/.',
    '',
    'Uso: node Scripts/sync_assets.js [--check] [--help]',
    '',
    '  (sin flag)  Sustituye WebUI/images/ y WebUI/images/brands/ por completo.',
    '  --check     NO escribe. Compara lo que hay con lo que se copiaria y sale 1',
    '              si algo se anadiria, se borraria o cambiaria.',
  ].join('\n'));
}

const flags = leerFlags('ABDSharedAssets -> WebUI/images');
if (flags.ayuda) { usage(); process.exit(0); }
if (flags.desconocido) {
  usage();
  console.error(`Argumento desconocido: ${flags.desconocido}`);
  process.exit(2);
}

// Rutas anidadas absolutas usadas por ABDBankManager: /images/models/thumbs/placeholder-*.svg
// Los placeholders ya estan en images/thumbs/ (espejo de models/thumbs/), se replican
// tambien en la ruta anidada models/thumbs/ que el codigo referencia como fallback.
function placeholdersAnidados(dest) {
  const thumbs = path.join(dest, 'thumbs');
  const nested = path.join(dest, 'models', 'thumbs');
  fs.mkdirSync(nested, { recursive: true });
  for (const f of fs.readdirSync(thumbs).filter((f) => f.startsWith('placeholder-'))) {
    fs.copyFileSync(path.join(thumbs, f), path.join(nested, f));
  }
}

let ok = true;

// 1) models/ -> WebUI/images/
//    Da /images/logos, /images/thumbs y las imagenes de modelo en la raiz.
//
//    `brands/` queda FUERA de este sincronizador a proposito, y no por descuido: es
//    del segundo (abajo), y los dos escriben en el mismo arbol. Sin esta exclusion,
//    el check de este_sync diria que BORRA los doce logos de `WebUI/images/brands/`,
//    porque no estan en `models/` — y despues los volveria a crear. Un check que
//    avisa de una destruccion que no va a ocurrir es peor que no tener check.
const sinBrands = (src) => shouldCopy(src) && path.basename(src) !== 'brands';

ok = sincronizarDir({
  etiqueta: 'ABDSharedAssets/models -> WebUI/images/',
  origen: path.join(sharedRoot, 'models'),
  destino: path.join(rootDir, 'WebUI', 'images'),
  filtro: sinBrands,
  extra: placeholdersAnidados,
  check: flags.check,
}) && ok;

// 2) brands/ -> WebUI/images/brands/
//    Logos de marca (incluye variantes en blanco) disponibles via /images/brands/.
ok = sincronizarDir({
  etiqueta: 'ABDSharedAssets/brands -> WebUI/images/brands',
  origen: path.join(sharedRoot, 'brands'),
  destino: path.join(rootDir, 'WebUI', 'images', 'brands'),
  filtro: shouldCopy,
  check: flags.check,
}) && ok;

if (!ok) {
  process.exit(1);
}

console.log(flags.check ? 'OK - ABDSharedAssets ya esta sincronizado.' : 'OK - ABDSharedAssets sincronizados.');
