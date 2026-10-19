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
    this.lastOpenOptions = null;
    this.onTimbreChange = null;
    this.activeTimbre = 1;

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
      drawer.setAttribute('aria-modal', 'false');
      drawer.innerHTML = `
        <div class="drawer-header">
          <div class="drawer-title-area">
            <span class="drawer-badge" id="drawer-section-badge">EDIT</span>
            <h2 class="drawer-title" id="drawer-section-title">SECTION</h2>
          </div>
          <div class="drawer-header-actions">
            <div class="drawer-timbre-switch" id="drawer-timbre-switch" title="Switch edited Timbre (Timbre 1 / Timbre 2)">
              <button type="button" class="drawer-timbre-btn active" data-drawer-timbre="1">T1</button>
              <button type="button" class="drawer-timbre-btn" data-drawer-timbre="2">T2</button>
            </div>
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
    this.timbreSwitchElem = drawer.querySelector('#drawer-timbre-switch');
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

    this.timbreSwitchElem?.querySelectorAll('.drawer-timbre-btn').forEach((btn) => {
      btn.addEventListener('click', () => {
        const t = Number(btn.getAttribute('data-drawer-timbre')) || 1;
        if (typeof this.onTimbreChange === 'function') {
          this.onTimbreChange(t);
        }
      });
    });

    window.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && this.isOpenState) {
        // Respect higher-priority modals if open
        const presetPopover = document.getElementById('preset-browser-popover');
        if (presetPopover && !presetPopover.hidden) return;
        const wtModal = document.getElementById('wt-browser-modal');
        if (wtModal && wtModal.style.display !== 'none' && wtModal.classList.contains('wt-modal-open')) return;
        this.close();
      }
    });
  }

  setActiveTimbre(timbre) {
    this.activeTimbre = timbre === 2 ? 2 : 1;
    this.timbreSwitchElem?.querySelectorAll('.drawer-timbre-btn').forEach((btn) => {
      const t = Number(btn.getAttribute('data-drawer-timbre')) || 1;
      btn.classList.toggle('active', t === this.activeTimbre);
    });
    if (this.isOpenState) {
      this.refresh();
    }
  }

  _updateCardHighlight(sectionId) {
    document.querySelectorAll('.dashboard-card.is-editing').forEach(el => el.classList.remove('is-editing'));
    document.querySelectorAll('.card-action-btn.is-editing-btn').forEach(el => el.classList.remove('is-editing-btn'));
    if (!sectionId) return;

    const sectionToBtn = {
      'osc1': 'btn-edit-osc1',
      'osc2': 'btn-edit-osc2',
      'mixer': 'btn-edit-mixer',
      'vocoder-cfg': 'btn-edit-vocoder-cfg',
      'vocoder-bands': 'btn-edit-vocoder-bands',
      'filter-main': 'btn-edit-filter-main',
      'filter-mod': 'btn-edit-filter-mod',
      'amp-main': 'btn-edit-amp-main',
      'eg1': 'btn-edit-eg1',
      'eg2': 'btn-edit-eg2',
      'lfo1': 'btn-edit-lfo1',
      'lfo2': 'btn-edit-lfo2',
      'modmatrix': 'btn-edit-modmatrix',
      'arp': 'btn-edit-arp',
      'modseq': 'btn-edit-modseq',
      'modfx': 'btn-edit-modfx',
      'delayfx': 'btn-edit-delayfx',
      'eq': 'btn-edit-eq',
      'voice': 'btn-edit-voice',
      'pitch': 'btn-edit-pitch',
      'global': 'btn-edit-global'
    };
    const btnId = sectionToBtn[sectionId];
    const btn = btnId ? document.getElementById(btnId) : null;
    if (btn) {
      btn.classList.add('is-editing-btn');
      btn.closest('.dashboard-card')?.classList.add('is-editing');
    }
  }

  refresh() {
    if (!this.isOpenState || !this.lastOpenOptions) return;
    const { title, badge, render } = this.lastOpenOptions;
    const timbreSuffix = ` · TIMBRE ${this.activeTimbre}`;
    if (this.titleElem) this.titleElem.textContent = title;
    if (this.badgeElem) this.badgeElem.textContent = `${badge}${timbreSuffix}`;
    if (this.contentElem && typeof render === 'function') {
      const scrollTop = this.contentElem.scrollTop;
      this.contentElem.innerHTML = '';
      render(this.contentElem);
      this.contentElem.scrollTop = scrollTop;
    }
  }

  open({ title = 'DETALLE DE PARÁMETROS', badge = 'EDIT', sectionId = '', render = null, onClose = null } = {}) {
    this.lastOpenOptions = { title, badge, sectionId, render, onClose };
    this.activeSection = sectionId;
    this.onCloseCallback = onClose;

    const timbreSuffix = ` · TIMBRE ${this.activeTimbre}`;
    if (this.titleElem) this.titleElem.textContent = title;
    if (this.badgeElem) this.badgeElem.textContent = `${badge}${timbreSuffix}`;

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
    this._updateCardHighlight(sectionId);

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
    this._updateCardHighlight(null);

    if (typeof this.onCloseCallback === 'function') {
      this.onCloseCallback(this.activeSection);
    }
    this.activeSection = null;
    this.onCloseCallback = null;
    this.lastOpenOptions = null;
  }

  isOpen() {
    return this.isOpenState;
  }
}

export const slideDrawer = new SlideDrawer();
