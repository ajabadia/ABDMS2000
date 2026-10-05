/**
 * SlideDrawer Component (CZ-101 / ABDEep style).
 * Controls the sliding drawer panel for deep parameter editing.
 *
 * 2026-09-27: el CONTRATO DE FOCO de la familia vive en
 * `@abdsynths/shared/components` (overlayFocus.js, el mismo que usa el cajon
 * compartido createDrawer): cerrado el cajon va `inert` + `aria-hidden`
 * (su contenido sigue en el DOM, pero fuera de la tabulacion), al abrir el
 * foco entra en el primer control del CUERPO, con el cajon abierto Tab no se
 * escapa y al cerrar vuelve al DISPARADOR. El contenido aqui se reconstruye
 * por apertura (patron render del slideDrawer), asi que la pieza que importa
 * es el `inert` de cierre: sin el, los controles del render ANTERIOR quedaban
 * muertos pero TABULABLES dentro de un dialogo aria-modal oculto.
 */
import { createOverlayFocus } from '@abdsynths/shared/components';

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
    this.drawer.setAttribute('tabindex', '-1');

    // El contrato de foco lo lleva el modulo compartido; el estado (abierto/
    // cerrado) y el cierre lo sigue llevando esta clase.
    this.focus = createOverlayFocus({
      root: this.drawer,
      body: this.contentElem,
      isClosed: () => !this.isOpenState,
      onEscape: () => this.close(),
    });
    this.focus.setInert();   // nace cerrado: nace inerte
    this.focus.attach();
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

    // El disparador se apunta ANTES de soltar el inert; el foco entra en el
    // primer control del cuerpo DESPUES del render del llamador (que decide
    // cual es el primer control).
    this.focus.rememberTrigger();
    this.focus.releaseInert();
    this.focus.focusFirst();
  }

  close() {
    if (!this.isOpenState) return;

    // El foco sale ANTES del inert (hacer inerte un contenedor con el foco
    // dentro lo tiraria a <body>) y ANTES del aviso: quien recibe onClose ya
    // mira el foco donde estaba antes de abrir.
    this.focus.restoreFocus();
    this.focus.setInert();

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
