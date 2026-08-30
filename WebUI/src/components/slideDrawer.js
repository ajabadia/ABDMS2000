/**
 * SlideDrawer Component (CZ-101 / ABDEep style).
 * Controls the sliding drawer panel for deep parameter editing.
 */

export class SlideDrawer {
  constructor(options = {}) {
    this.containerId = options.containerId || 'sliding-drawer';
    this.backdropId = options.backdropId || 'sliding-drawer-backdrop';
    this.activeSection = null;
    this.onCloseCallback = null;
    this.isOpenState = false;

    this.initDOM();
    this.attachEvents();
  }

  initDOM() {
    let backdrop = document.getElementById(this.backdropId);
    if (!backdrop) {
      backdrop = document.createElement('div');
      backdrop.id = this.backdropId;
      backdrop.className = 'drawer-backdrop';
      document.body.appendChild(backdrop);
    }
    this.backdrop = backdrop;

    let drawer = document.getElementById(this.containerId);
    if (!drawer) {
      drawer = document.createElement('aside');
      drawer.id = this.containerId;
      drawer.className = 'slide-drawer';
      drawer.setAttribute('role', 'dialog');
      drawer.setAttribute('aria-modal', 'true');
      drawer.innerHTML = `
        <div class="drawer-header">
          <div class="drawer-title-area">
            <span class="drawer-badge" id="drawer-section-badge">EDIT</span>
            <h2 class="drawer-title" id="drawer-section-title">SECTION</h2>
          </div>
          <div class="drawer-header-actions">
            <button class="drawer-close-btn" id="drawer-close-btn" title="Cerrar (Esc)">✕</button>
          </div>
        </div>
        <div class="drawer-content" id="drawer-content-area">
          <!-- Dynamic control blocks rendered here -->
        </div>
      `;
      document.body.appendChild(drawer);
    }
    this.drawer = drawer;
    this.titleElem = drawer.querySelector('#drawer-section-title');
    this.badgeElem = drawer.querySelector('#drawer-section-badge');
    this.contentElem = drawer.querySelector('#drawer-content-area');
    this.closeBtn = drawer.querySelector('#drawer-close-btn');
  }

  attachEvents() {
    this.closeBtn?.addEventListener('click', () => this.close());
    this.backdrop?.addEventListener('click', () => this.close());

    window.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && this.isOpenState) {
        this.close();
      }
    });
  }

  open({ title = 'DETALLE DE PARÁMETROS', badge = 'EDIT', sectionId = '', render = null, onClose = null } = {}) {
    this.activeSection = sectionId;
    this.onCloseCallback = onClose;

    if (this.titleElem) this.titleElem.textContent = title;
    if (this.badgeElem) this.badgeElem.textContent = badge;

    if (this.contentElem) {
      this.contentElem.innerHTML = '';
      if (typeof render === 'function') {
        render(this.contentElem);
      }
    }

    this.drawer.classList.add('drawer-open');
    this.backdrop.classList.add('backdrop-visible');
    document.body.classList.add('drawer-active');
    this.isOpenState = true;
  }

  close() {
    if (!this.isOpenState) return;

    this.drawer.classList.remove('drawer-open');
    this.backdrop.classList.remove('backdrop-visible');
    document.body.classList.remove('drawer-active');
    this.isOpenState = false;

    if (typeof this.onCloseCallback === 'function') {
      this.onCloseCallback(this.activeSection);
    }
    this.activeSection = null;
    this.onCloseCallback = null;
  }

  isOpen() {
    return this.isOpenState;
  }
}

export const slideDrawer = new SlideDrawer();
