import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceDir = path.resolve(rootDir, '..', 'ABDBankManager', 'WebUI');
const destRoot = path.join(rootDir, 'WebUI', 'abdbank');

const toPosix = (p) => p.split(path.sep).join('/');
const EXCLUDE_FILES = new Set(['vite.config.js', 'vite.config.mjs', 'package.json', 'package-lock.json', 'pnpm-lock.yaml']);

function shouldCopy(src) {
  if (EXCLUDE_FILES.has(path.basename(src))) return false;
  if (path.basename(src) === 'tests') return false;
  if (toPosix(src).includes('/tests/')) return false;
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
      const rel = toPosix(path.relative(sourceDir, full));
      if (rel === 'tests') continue;
      n += countFiles(full);
    } else if (shouldCopy(full)) {
      n += 1;
    }
  }
  return n;
}

console.log('Sincronizando ABDBankManager WebUI -> WebUI/abdbank/');
console.log(`  Origen:  ${sourceDir}`);
console.log(`  Destino: ${destRoot}`);

if (!fs.existsSync(sourceDir)) {
  console.error(`ERROR: No se encontro la fuente de ABDBankManager: ${sourceDir}`);
  process.exit(1);
}

const before = countFiles(sourceDir);
console.log(`  Archivos en origen: ${before}`);

fs.rmSync(destRoot, { recursive: true, force: true });

fs.cpSync(sourceDir, destRoot, {
  recursive: true,
  dereference: true,
  filter: (src) => shouldCopy(src),
});

const after = countFiles(destRoot);
console.log(`  Archivos copiados:  ${after}`);

if (after !== before) {
  console.warn(`  ADVERTENCIA: conteo difiere (origen ${before} vs destino ${after}).`);
}

console.log('OK - ABDBankManager sincronizado.');
