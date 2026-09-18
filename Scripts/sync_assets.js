import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

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

function countFiles(dir) {
  let n = 0;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    let isDir;
    try {
      isDir = fs.statSync(full).isDirectory();
    } catch {
      continue;
    }
    if (isDir) {
      if (!shouldCopy(full)) continue;
      n += countFiles(full);
    } else if (shouldCopy(full)) {
      n += 1;
    }
  }
  return n;
}

function syncDir(label, sourceDir, destDir, extra = null) {
  console.log(`Sincronizando ${label}`);
  console.log(`  Origen:  ${sourceDir}`);
  console.log(`  Destino: ${destDir}`);

  if (!fs.existsSync(sourceDir)) {
    console.error(`ERROR: No se encontro la fuente: ${sourceDir}`);
    return false;
  }

  const before = countFiles(sourceDir);
  console.log(`  Archivos en origen: ${before}`);

  fs.rmSync(destDir, { recursive: true, force: true });

  fs.cpSync(sourceDir, destDir, {
    recursive: true,
    dereference: true,
    filter: (src) => shouldCopy(src),
  });

  if (extra) extra(destDir);

  const after = countFiles(destDir);
  console.log(`  Archivos copiados:  ${after}`);

  if (after !== before) {
    console.warn(`  ADVERTENCIA: conteo difiere (origen ${before} vs destino ${after}).`);
  }
  return true;
}

let ok = true;

// 1) models/ -> WebUI/images/
//    Da /images/logos, /images/thumbs y las imagenes de modelo en la raiz.
ok &= syncDir(
  'ABDSharedAssets/models -> WebUI/images/',
  path.join(sharedRoot, 'models'),
  path.join(rootDir, 'WebUI', 'images'),
  (dest) => {
    // Rutas anidadas absolutas usadas por ABDBankManager: /images/models/thumbs/placeholder-*.svg
    // Los placeholders ya estan en images/thumbs/ (espejo de models/thumbs/), se replican
    // tambien en la ruta anidada models/thumbs/ que el codigo referencia como fallback.
    const nested = path.join(dest, 'models', 'thumbs');
    fs.mkdirSync(nested, { recursive: true });
    fs.readdirSync(path.join(dest, 'thumbs'))
      .filter((f) => f.startsWith('placeholder-'))
      .forEach((f) => {
        fs.copyFileSync(path.join(dest, 'thumbs', f), path.join(nested, f));
      });
  }
);

// 2) brands/ -> WebUI/images/brands/
//    Logos de marca (incluye variantes en blanco) disponibles via /images/brands/.
ok &= syncDir(
  'ABDSharedAssets/brands -> WebUI/images/brands/',
  path.join(sharedRoot, 'brands'),
  path.join(rootDir, 'WebUI', 'images', 'brands')
);

if (!ok) {
  process.exit(1);
}

console.log('OK - ABDSharedAssets sincronizados.');