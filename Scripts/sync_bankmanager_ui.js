#!/usr/bin/env node
/**
 * ABDMS2000 — sincroniza el modal embebible del Bank Manager desde el paquete
 * canonico @abdsynths/bank-manager-ui (ABDBankManager/packages/ui/src) hacia
 * WebUI/src/components/bank/.
 *
 * Estado actual: BankManagerModal.css ya no se consume desde WebUI; se migró a
 * ABDSharedAssets/styles/components/bank.css e importa desde app.js como componente
 * compartido. Por eso ya no forma parte de la sincronización activa desde este script.
 *
 * Nota de soporte: este script está en fase de desuso gradual. Su razón de ser hoy es
 * el build nativo, porque WebUI/src crudo (consumo antes de la Fase 3) aún necesita
 * BankManagerModal.js sincronizado. El CSS ya no pertenece a este pipeline.
 *
 * Esta copia NO se edita a mano: es artefacto de sincronizacion. El test
 * WebUI/tests/unit/bankManagerModalSync.test.js falla si diverge del paquete.
 * (El WebView2 nativo embebe WebUI/src crudo y no resuelve bare imports, por
 * eso el consumo del paquete se materializa como artefacto sincronizado; ver
 * ANALISIS_DRY_COMPARTIDO.md, Opcion B del apartado 7.)
 *
 * Vista de pipeline actual:
 * - Vite dev (puerto 8384): ya no necesita la copia para el CSS. El CSS del modal
 *   vive en ABDSharedAssets/styles/components/bank.css y se importa desde app.js
 *   como componente compartido.
 * - Build nativo: este script permanece como puente. WebUI/src crudo (consumo
 *   anterior a la Fase 3) sigue necesitando BankManagerModal.js sincronizado.
 *   El CSS ya no tiene razón de ser en esta trayectoria.
 *
 * Cuando la Fase 3 (Vite bundle + embeber dist, ver
 * ANALISIS_DRY_BANKMANAGER_CZ101_ABDEEP.md §10.1) esté operativa, este script dejará
 * de ser necesario para el modal, porque la WebUI nativa consumirá el bundle de
 * dist en vez de WebUI/src sincronizado.
 *
 * USO
 *   node Scripts/sync_bankmanager_ui.js --check    # NO escribe. Sale 1 si alguno
 *                                                 # de los dos ficheros esta stale.
 *   node Scripts/sync_bankmanager_ui.js            # sincroniza de verdad
 *   node Scripts/sync_bankmanager_ui.js --help
 *
 * Aquí el riesgo es menor que en los otros `sync_*`: no se borra nada, se
 * sobrescribe un fichero concreto (BankManagerModal.js). Pero se sobrescribe ENCIMA,
 * y si alguien lo ha tocado a mano el ajuste desaparece igual. El test de
 * sincronización ya avisa; el `--check` avisa ANTES y sin tener que lanzar la suite.
 */
import path from 'path';
import { fileURLToPath } from 'url';
import { sincronizarFicheros, leerFlags } from './syncSeguro.mjs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const rootDir = path.resolve(__dirname, '..');
const sourceDir = path.resolve(rootDir, '..', 'ABDBankManager', 'packages', 'ui', 'src');
const destDir = path.join(rootDir, 'WebUI', 'src', 'components', 'bank');

const FILES = ['BankManagerModal.js', 'mountBankManager.js'];

function usage() {
  console.log([
    'Sincroniza @abdsynths/bank-manager-ui -> WebUI/src/components/bank/.',
    'No editar a mano: es artefacto de sincronizacion.',
    '',
    'Uso: node Scripts/sync_bankmanager_ui.js [--check] [--help]',
    '',
    '  (sin flag)  Copia BankManagerModal.js encima.',
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
