/**
 * mountBankManager.js
 * Función de montaje directo en DOM para ABD Bank Manager (Zero-Iframe).
 *
 * Permite que los sintetizadores host (ABDMS2000, ABDEep, ABDCZ101) o cualquier
 * contenedor DOM alojen la interfaz completa del Bank Manager directamente en su
 * propio árbol de documentos, compartiendo variables CSS, temas y bridge sin
 * barreras entre ventanas.
 */

export const BANK_MANAGER_HTML = `
  <div class="abdbank-shell">
    <header class="header">
        <h1>ABD Bank Manager</h1>
        <div class="header-actions">
            <div class="midi-status" id="midi-status" title="Connect MIDI">
                <span class="midi-dot"></span>
                <span class="midi-label" id="midi-label">MIDI</span>
            </div>
            <div class="host-diag" id="host-diag" hidden data-host-diag-state="silent" title="Host diagnostics">
                <span class="host-diag-dot"></span>
                <span id="host-diag-label">Host</span>
            </div>
            <button class="btn btn-sm" id="btn-help" title="Keyboard shortcuts (?)">?</button>
            <span id="version" class="version-badge">v0.1.0</span>
        </div>
    </header>

    <div class="main-grid">
        <aside class="sidebar">
            <div class="sidebar-header">
                <div class="sidebar-search">
                    <input type="search" id="global-search" placeholder="Search...">
                </div>
                <button class="btn btn-sm btn-ghost" id="tree-view-toggle" title="Tree View" style="margin:0 0.5rem 0.5rem;">
                    <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="vertical-align:-2px; margin-right:4px;"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"></path></svg> Tree View
                </button>
            </div>
            <ul class="sidebar-list" id="nav-list"></ul>
        </aside>

        <main class="main-content" id="main-content">
            <div class="welcome" id="panel-welcome">
                <h2>ABD Universal Bank Manager</h2>
                <p>Select a manufacturer from the sidebar to get started, or connect MIDI hardware.</p>
            </div>
            <div id="panel-content" style="display:none;"></div>
            <div class="drop-zone" id="drop-zone" style="display:none;">
                <div class="drop-zone-icon">📁</div>
                <div class="drop-zone-text">Drop .syx, .abdbank, .abdlibrary, or .json files here</div>
                <div class="drop-zone-hint">or paste hex with Ctrl+V</div>
            </div>
        </main>
    </div>

    <div id="toast-container"></div>

    <div class="modal-overlay" id="modal-overlay">
        <div class="modal" id="modal-content"></div>
    </div>
  </div>
`;

/**
 * Monta la interfaz del Bank Manager en un elemento contenedor del DOM.
 *
 * @param {HTMLElement} containerElement  Contenedor donde se montará la interfaz.
 * @param {object} [options]             Opciones de inicialización (bridge, theme, etc.).
 * @returns {Promise<{ unmount: () => void }>}
 */
export async function mountBankManager(containerElement, options = {}) {
  if (!containerElement) {
    throw new Error('[mountBankManager] Se requiere un containerElement válido.');
  }

  // Establecer puente global si se proporciona
  if (options.bridge && typeof window !== 'undefined') {
    window.__synthBridge = options.bridge;
  }

  // Inyectar esqueleto HTML
  containerElement.innerHTML = BANK_MANAGER_HTML;

  // Cargar dinámicamente la lógica de la app
  let appModule = null;
  if (typeof options.appInit === 'function') {
    await options.appInit();
  } else {
    try {
      // Import dinámico en monorepo / paquete @abdsynths/bank-manager-ui
      appModule = await import('../../../WebUI/src/app.js');
    } catch (err1) {
      try {
        // Fallback para ejecución desde WebUI/src/components/
        appModule = await import('../app.js');
      } catch (err2) {
        console.warn('[mountBankManager] No se pudo cargar automáticamente app.js:', err1, err2);
      }
    }

    if (typeof appModule?.init === 'function') {
      await appModule.init();
    }
    if (typeof appModule?.setupDragDrop === 'function') {
      appModule.setupDragDrop();
    }
  }

  return {
    unmount() {
      if (typeof appModule?.cleanup === 'function') {
        appModule.cleanup();
      }
      containerElement.innerHTML = '';
    }
  };
}
