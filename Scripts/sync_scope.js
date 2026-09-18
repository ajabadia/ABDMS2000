import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

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
      if (EXCLUDE_DIRS.has(rel)) continue;
      n += countFiles(full);
    } else if (shouldCopy(full)) {
      n += 1;
    }
  }
  return n;
}

console.log('Sincronizando ABDScope WebUI -> WebUI/abdscope/');
console.log(`  Origen:  ${sourceDir}`);
console.log(`  Destino: ${destRoot}`);

if (!fs.existsSync(sourceDir)) {
  console.error(`ERROR: No se encontro la fuente de ABDScope: ${sourceDir}`);
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

console.log('OK - ABDScope sincronizado.');
