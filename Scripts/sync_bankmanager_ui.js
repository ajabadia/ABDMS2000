import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceDir = path.resolve(rootDir, '..', 'ABDBankManager', 'packages', 'ui', 'src');
const destDir = path.join(rootDir, 'WebUI', 'src', 'components', 'bank');

/**
 * Sincroniza el modal embebible del Bank Manager desde el paquete canonico
 * @abdsynths/bank-manager-ui (ABDBankManager/packages/ui/src) hacia
 * WebUI/src/components/bank/.
 *
 * Esta copia NO se edita a mano: es artefacto de sincronizacion. El test
 * WebUI/tests/unit/bankManagerModalSync.test.js falla si diverge del paquete.
 * (El WebView2 nativo embebe WebUI/src crudo y no resuelve bare imports, por
 * eso el consumo del paquete se materializa como artefacto sincronizado; ver
 * ANALISIS_DRY_COMPARTIDO.md, Opcion B del apartado 7.)
 */

const FILES = ['BankManagerModal.js', 'BankManagerModal.css'];

if (!fs.existsSync(sourceDir)) {
  console.error(`ERROR: No se encontro la fuente de @abdsynths/bank-manager-ui: ${sourceDir}`);
  process.exit(1);
}

fs.mkdirSync(destDir, { recursive: true });

let copied = 0;
for (const file of FILES) {
  const src = path.join(sourceDir, file);
  const dst = path.join(destDir, file);
  if (!fs.existsSync(src)) {
    console.error(`ERROR: Falta ${file} en el paquete packages/ui: ${src}`);
    process.exit(1);
  }
  fs.copyFileSync(src, dst);
  copied += 1;
}

console.log('Sincronizando @abdsynths/bank-manager-ui -> WebUI/src/components/bank/');
console.log(`  Origen:  ${sourceDir}`);
console.log(`  Destino: ${destDir}`);
console.log(`  Ficheros copiados: ${copied}/${FILES.length}`);
console.log('OK - BankManagerModal sincronizado desde packages/ui (no editar a mano).');
