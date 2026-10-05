#!/usr/bin/env node
/**
 * ABDMS2000 — sincroniza el modal embebible del Bank Manager desde el paquete
 * canonico @abdsynths/bank-manager-ui (ABDBankManager/packages/ui/src) hacia
 * WebUI/src/components/bank/.
 *
 * Esta copia NO se edita a mano: es artefacto de sincronizacion. El test
 * WebUI/tests/unit/bankManagerModalSync.test.js falla si diverge del paquete.
 * (El WebView2 nativo embebe WebUI/src crudo y no resuelve bare imports, por
 * eso el consumo del paquete se materializa como artefacto sincronizado; ver
 * ANALISIS_DRY_COMPARTIDO.md, Opcion B del apartado 7.)
 *
 * USO
 *   node Scripts/sync_bankmanager_ui.js --check    # NO escribe. Sale 1 si alguno
 *                                                 # de los dos ficheros esta stale.
 *   node Scripts/sync_bankmanager_ui.js            # sincroniza de verdad
 *   node Scripts/sync_bankmanager_ui.js --help
 *
 * Aqui el riesgo es menor que en los otros `sync_*`: no se borra nada, se
 * sobrescriben dos ficheros concretos. Pero se sobrescriben ENCIMA, y si alguien
 * los ha tocado a mano el ajuste desaparece igual. El test de sincronizacion ya
 * avisa; el `--check` avisa ANTES y sin tener que lanzar la suite.
 */
import path from 'path';
import { fileURLToPath } from 'url';
import { sincronizarFicheros, leerFlags } from './syncSeguro.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceDir = path.resolve(rootDir, '..', 'ABDBankManager', 'packages', 'ui', 'src');
const destDir = path.join(rootDir, 'WebUI', 'src', 'components', 'bank');

const FILES = ['BankManagerModal.js', 'BankManagerModal.css'];

function usage() {
  console.log([
    'Sincroniza @abdsynths/bank-manager-ui -> WebUI/src/components/bank/.',
    'No editar a mano: es artefacto de sincronizacion.',
    '',
    'Uso: node Scripts/sync_bankmanager_ui.js [--check] [--help]',
    '',
    '  (sin flag)  Copia los dos ficheros encima.',
    '  --check     NO escribe. Sale 1 si alguno esta stale.',
  ].join('\n'));
}

const flags = leerFlags('bank-manager-ui -> components/bank');
if (flags.ayuda) { usage(); process.exit(0); }
if (flags.desconocido) {
  usage();
  console.error(`Argumento desconocido: ${flags.desconocido}`);
  process.exit(2);
}

const ok = sincronizarFicheros({
  etiqueta: '@abdsynths/bank-manager-ui -> WebUI/src/components/bank/',
  origen: sourceDir,
  destino: destDir,
  ficheros: FILES,
  check: flags.check,
});

if (!ok) {
  process.exit(1);
}

console.log(flags.check
  ? 'OK - BankManagerModal ya esta sincronizado.'
  : 'OK - BankManagerModal sincronizado desde packages/ui (no editar a mano).');
