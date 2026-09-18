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
    this.isOpen = true;
    this.overlay.hidden = false;
    this.overlay.setAttribute('aria-hidden', 'false');
    this.overlay.classList.add('is-active');
    const currentTheme = document.documentElement.getAttribute('data-theme') || 'ms2000';
    this.setTheme(currentTheme);
  }

  close() {
    this.isOpen = false;
    this.overlay.classList.remove('is-active');
    this.overlay.hidden = true;
    this.overlay.setAttribute('aria-hidden', 'true');
  }

  toggle() {
    if (this.isOpen) this.close();
    else this.open();
  }
}
