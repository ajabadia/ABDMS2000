/**
 * Anti-drift guard: WebUI/src/components/bank/BankManagerModal.js is a sync
 * artifact of @abdsynths/bank-manager-ui (ABDBankManager/packages/ui/src),
 * NOT a hand-maintained copy. If this test fails, run:
 *
 *   node Scripts/sync_bankmanager_ui.js
 *
 * ...or edit the canonical file in ABDBankManager/packages/ui/src and re-sync.
 * (The native WebView2 build embeds raw WebUI/src and cannot resolve bare
 * imports, so package consumption is materialized as a synced artifact —
 * see ANALISIS_DRY_COMPARTIDO.md §7 option B.)
 *
 * Estado actual: BankManagerModal.css ya no se consume desde WebUI. Se migró a
 * ABDSharedAssets/styles/components/bank.css e importa desde app.js como componente
 * compartido, por lo que ya no forma parte de este artefacto de sincronización.
 *
 * Nota: la versión compartida sigue siendo función del paquete UI, porque el archivo
 * compartido se genera/porta desde ABDBankManager/packages/ui/src/BankManagerModal.css
 * cuando se migra al paquete compartido; si el CSS canonical cambia, también hay que
 * actualizar ABDSharedAssets/styles/components/bank.css.
 */
import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

const here = dirname(fileURLToPath(import.meta.url));
const projectRoot = join(here, '..', '..', '..');
const packageSrc = join(projectRoot, '..', 'ABDBankManager', 'packages', 'ui', 'src');

const FILES = ['BankManagerModal.js', 'mountBankManager.js'];

describe('BankManagerModal vendored from @abdsynths/bank-manager-ui', () => {
  for (const file of FILES) {
    it(`${file} matches packages/ui source byte-for-byte (run Scripts/sync_bankmanager_ui.js if it fails)`, () => {
      const canonical = readFileSync(join(packageSrc, file));
      const vendored = readFileSync(join(projectRoot, 'WebUI', 'src', 'components', 'bank', file));
      expect(vendored.equals(canonical)).toBe(true);
    });
  }
});
