/**
 * BankManagerModal.js
 * Universal Embeddable Modal Hosting the Full Native ABD Bank Manager WebUI
 */

/**
 * Resolve the ABD Bank Manager entry page.
 *
 * The ABDBankManager WebUI is synchronized into this project at WebUI/abdbank and
 * must be loaded from the SAME origin as the host document:
 *  - Native JUCE WebView2 only serves resources under its virtual host
 *    (https://juce.backend/...); the bankwebui:// custom scheme is not routable
 *    inside WebView2, so an iframe pointing at it would stay blank.
 *  - The Vite dev server and file:// previews also resolve this relative URL.
 *
 * Resolving against the current document URL keeps every environment working.
 */
function resolveDefaultIframeSrc() {
  try {
    return new URL('abdbank/index.html', window.location.href).href;
  } catch (e) {
    return 'bankwebui://index.html';
  }
}

/**
 * Motor del CONTRATO DE FOCO de los overlays de la familia ABD (inline).
 *
 * Fuente unica: ABDSharedAssets/components/overlayFocus.js — el mismo contrato
 * que usa el cajon compartido (createDrawer), el slideDrawer de ABDMS2000 y el
 * block-drawer de CZ101. Va copiado aqui porque la WebUI estatica de este repo
 * se sirve con CSP `default-src 'self'` y su importmap no resuelve
 * @abdsynths/shared; el test de drift (packages/ui/tests/drift.test.js)
 * verifica los invariantes del contrato contra este motor. Semantica:
 *
 *   1. CERRADO: `inert` + `aria-hidden="true"` (fuera de tabulacion y del
 *      arbol de accesibilidad, sin sacar nada del DOM). Al abrir se suelta;
 *      al cerrar se restaura DESPUES de devolver el foco.
 *   2. AL ABRIR: el foco entra en el primer control del cuerpo (aqui, el
 *      cierre del encabezado; el iframe no es tabulable por el selector).
 *   3. ABIERTO: Tab/Shift+Tab ciclan por los controles y no se escapan; la
 *      trampa solo actua con el foco DENTRO (varios overlays no se secuestran
 *      el teclado).
 *   4. AL CERRAR: el foco vuelve al DISPARADOR (document.activeElement al
 *      abrir, fuera del overlay; se limpia en cada cierre y un nodo muerto no
 *      se toca).
 */
function createOverlayFocusContract({ root, body = null, isClosed, onEscape = null }) {
  const FOCUSABLE_SELECTOR = [
    'a[href]',
    'button:not([disabled])',
    'input:not([disabled])',
    'select:not([disabled])',
    'textarea:not([disabled])',
    '[tabindex]:not([tabindex="-1"])',
  ].join(', ');

  function focusableWithin(container) {
    return Array.from(container.querySelectorAll(FOCUSABLE_SELECTOR)).filter((node) => {
      if (node.closest('[hidden]')) return false;
      const style = window.getComputedStyle(node);
      return style.display !== 'none' && style.visibility !== 'hidden';
    });
  }

  let lastFocused = null;

  function trapTab(event) {
    if (!root.contains(document.activeElement)) return;

    const controls = focusableWithin(root);
    if (controls.length === 0) {
      event.preventDefault();
      root.focus({ preventScroll: true });
      return;
    }

    const first = controls[0];
    const last = controls[controls.length - 1];
    const active = document.activeElement;

    if (event.shiftKey && active === first) {
      event.preventDefault();
      last.focus({ preventScroll: true });
    } else if (!event.shiftKey && active === last) {
      event.preventDefault();
      first.focus({ preventScroll: true });
    }
  }

  function handleKeydown(event) {
    if (isClosed()) return;
    if (event.key === 'Escape') onEscape?.();
    else if (event.key === 'Tab') trapTab(event);
  }

  return {
    focusFirst() {
      const target =
        (body ? focusableWithin(body)[0] : undefined) ??
        focusableWithin(root)[0] ?? root;
      target.focus({ preventScroll: true });
    },
    restoreFocus() {
      const target = lastFocused;
      lastFocused = null;
      if (target?.isConnected && typeof target.focus === 'function') {
        target.focus({ preventScroll: true });
      }
    },
    rememberTrigger() {
      const active = document.activeElement;
      lastFocused =
        active && active !== document.body && !root.contains(active) ? active : null;
    },
    setInert() {
      root.setAttribute('inert', '');
      root.setAttribute('aria-hidden', 'true');
    },
    releaseInert() {
      root.removeAttribute('inert');
      root.setAttribute('aria-hidden', 'false');
    },
    attach() {
      document.addEventListener('keydown', handleKeydown);
    },
    detach() {
      document.removeEventListener('keydown', handleKeydown);
    },
  };
}

export class BankManagerModal {
  constructor(options = {}) {
    this.container = options.container || document.body;
    this.iframeSrc = options.iframeSrc || resolveDefaultIframeSrc();
    this.synthBridge = options.synthBridge || null;
    this.isOpen = false;

    // Expose bridge globally to child iframe
    if (this.synthBridge && !window.__synthBridge) {
      window.__synthBridge = this.synthBridge;
    }

    this._createDOM();

    // 2026-09-27: el contrato de foco de la familia, atado al overlay (que
    // nace cerrado, o sea, inerte). El Escape propio NO pisa al del host: si
    // el host tambien cierra, este cierre ya esta hecho y es un no-op.
    this._focusContract = createOverlayFocusContract({
      root: this.overlay,
      body: this.modal,
      isClosed: () => !this.isOpen,
      onEscape: () => this.close(),
    });
    this._focusContract.setInert();
    this._focusContract.attach();
  }

  _createDOM() {
    // Avoid duplicate modals
    const existing = document.getElementById('abdbank-modal-overlay');
    if (existing) existing.remove();

    this.overlay = document.createElement('div');
    this.overlay.className = 'abdbank-modal-overlay';
    this.overlay.id = 'abdbank-modal-overlay';
    this.overlay.hidden = true;
    this.overlay.setAttribute('aria-hidden', 'true');

    this.modal = document.createElement('div');
    this.modal.className = 'abdbank-modal-container';

    this.modal.innerHTML = `
      <header class="abdbank-modal-header">
        <div class="abdbank-modal-heading">
          <span class="abdbank-modal-title">ABD BANK MANAGER</span>
          <span class="abdbank-modal-subtitle">Biblioteca universal de bancos y parches</span>
        </div>
        <button class="abdbank-close-floating-btn" id="abdbank-close-floating-btn" title="Cerrar (Esc)">&times;</button>
      </header>
      <iframe class="abdbank-iframe" id="abdbank-iframe" src="${this.iframeSrc}"></iframe>
    `;

    this.overlay.appendChild(this.modal);
    this.container.appendChild(this.overlay);

    this.modal.querySelector('#abdbank-close-floating-btn').addEventListener('click', () => this.close());
    this.overlay.addEventListener('click', (e) => {
      if (e.target === this.overlay) this.close();
    });

    const iframe = this.modal.querySelector('#abdbank-iframe');
    iframe.addEventListener('load', () => {
      const currentTheme = document.documentElement.getAttribute('data-theme') || 'ms2000';
      this.setTheme(currentTheme);
    });
  }

  setTheme(theme) {
    this.currentTheme = theme;
    const iframe = this.modal?.querySelector('#abdbank-iframe');
    if (iframe && iframe.contentDocument) {
      try {
        iframe.contentDocument.documentElement.setAttribute('data-theme', theme);
        iframe.contentDocument.body.className = 'skin-' + theme;
      } catch (e) {
        // Cross-origin fallback (if any)
      }
    }
  }

  open() {
    // El disparador se apunta ANTES de mostrar el overlay (el activeElement de
    // fuera aun es el boton que abre el banco).
    this._focusContract.rememberTrigger();

    this.isOpen = true;
    this.overlay.hidden = false;
    this.overlay.classList.add('is-active');
    // El contrato suelta inert + aria-hidden; un contenedor inert es sordo y
    // no dejaria entrar el foco de aqui abajo.
    this._focusContract.releaseInert();
    const currentTheme = document.documentElement.getAttribute('data-theme') || 'ms2000';
    this.setTheme(currentTheme);

    // El foco entra al primer control del dialogo (el cierre del encabezado).
    this._focusContract.focusFirst();
  }

  close() {
    // El foco sale ANTES del inert y de ocultar (hacer inerte un contenedor
    // con el foco dentro lo tiraria a <body>) y vuelve al disparador. Doble
    // cierre (el host tambien llama a close en su Escape): no-op seguro.
    this._focusContract.restoreFocus();
    this._focusContract.setInert();

    this.isOpen = false;
    this.overlay.classList.remove('is-active');
    this.overlay.hidden = true;
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }
}
