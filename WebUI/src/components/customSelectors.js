/**
 * customSelectors.js — Tactile Hardware Selectors & LCD Dropdowns for ABDMS2000.
 *
 * Provides:
 *   1. SegmentedSelector: Tactile button groups with active LED illumination and SVG icons.
 *   2. LcdDropdown: Retro-futuristic acrylic LCD capsule with glassmorphic dark popover.
 *   3. Predefined SVG Icon sets for Waveforms, Filter Curves, and Modulation modes.
 */

import { paramStore } from '../contracts/paramStore.js';
import { bridge } from '../bridge/bridgeCore.js';

export const WAVE_ICONS = {
  saw: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,14 20,2 20,14" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/></svg>`,
  square: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,14 2,2 12,2 12,14 22,14 22,2" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/></svg>`,
  triangle: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,14 12,2 22,14" fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round"/></svg>`,
  sine: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M2,8 C7,0 7,16 12,8 C17,0 17,16 22,8" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
  vox: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M3,12 C5,5 7,13 12,8 C17,3 19,11 21,5" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
  dwgs: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,12 6,5 10,13 14,3 18,11 22,6" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/></svg>`,
  noise: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,8 5,3 8,13 11,5 14,11 17,4 20,12 22,7" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linejoin="round"/></svg>`,
  audioIn: `<svg viewBox="0 0 24 16" class="sel-icon"><circle cx="6" cy="8" r="3" fill="none" stroke="currentColor" stroke-width="1.8"/><line x1="9" y1="8" x2="19" y2="8" stroke="currentColor" stroke-width="2"/><polyline points="16,5 21,8 16,11" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linejoin="round"/></svg>`,
  sh: `<svg viewBox="0 0 24 16" class="sel-icon"><polyline points="2,10 6,10 6,4 12,4 12,12 18,12 18,6 22,6" fill="none" stroke="currentColor" stroke-width="1.8"/></svg>`,
};

export const FILTER_ICONS = {
  lpf24: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M2,4 L9,4 C14,4 18,8 22,14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
  lpf12: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M2,4 L11,4 L22,13" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
  bpf12: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M2,14 L12,3 L22,14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
  hpf12: `<svg viewBox="0 0 24 16" class="sel-icon"><path d="M2,13 L13,4 L22,4" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round"/></svg>`,
};

/**
 * Segmented tactile button group selector.
 */
export class SegmentedSelector {
  constructor(container, options = {}) {
    this.container = container;
    this.paramId = options.paramId || '';
    this.label = options.label || '';
    this.options = options.options || []; // [{ value, label, iconSvg, badge }]
    this.layout = options.layout || 'row'; // 'row' | 'grid-2' | 'grid-4' | 'wrap'
    this.size = options.size || 'md'; // 'sm' | 'md' | 'lg'
    this.onChange = options.onChange || null;

    const storedVal = this.paramId ? paramStore.get(this.paramId) : undefined;
    this.value = options.value !== undefined ? options.value : (storedVal !== undefined ? storedVal : (this.options[0]?.value ?? 0));

    this.render();
    this.attachEvents();

    if (this.paramId) {
      paramStore.register(this.paramId, this);
    }
  }

  render() {
    this.container.classList.add('segmented-control-wrapper');
    const layoutClass = `segmented-layout-${this.layout}`;
    const sizeClass = `segmented-size-${this.size}`;

    let buttonsHtml = '';
    this.options.forEach(opt => {
      const isSelected = String(opt.value) === String(this.value);
      buttonsHtml += `
        <button type="button" class="segmented-btn ${isSelected ? 'active' : ''}" data-value="${opt.value}">
          ${opt.iconSvg ? `<span class="segmented-btn-icon">${opt.iconSvg}</span>` : ''}
          <span class="segmented-btn-label">${opt.label}</span>
          ${opt.badge ? `<span class="segmented-btn-badge">${opt.badge}</span>` : ''}
          <span class="segmented-btn-led"></span>
        </button>
      `;
    });

    this.container.innerHTML = `
      ${this.label ? `<div class="segmented-label">${this.label}</div>` : ''}
      <div class="segmented-group ${layoutClass} ${sizeClass}">
        ${buttonsHtml}
      </div>
    `;

    this.groupElem = this.container.querySelector('.segmented-group');
  }

  setValue(newVal, notify = true) {
    if (String(this.value) !== String(newVal)) {
      this.value = newVal;
      this.updateVisuals();
      if (this.paramId) {
        paramStore.set(this.paramId, this.value);
      }
      if (notify) {
        if (this.paramId) bridge.setParam(this.paramId, this.value);
        if (this.onChange) this.onChange(this.value, this.paramId);
      }
    }
  }

  updateVisuals() {
    const buttons = this.container.querySelectorAll('.segmented-btn');
    buttons.forEach(btn => {
      btn.classList.toggle('active', String(btn.dataset.value) === String(this.value));
    });
  }

  attachEvents() {
    this.container.addEventListener('click', (e) => {
      const btn = e.target.closest('.segmented-btn');
      if (btn && btn.dataset.value !== undefined) {
        const rawVal = btn.dataset.value;
        const numVal = Number(rawVal);
        const parsedVal = !isNaN(numVal) && rawVal.trim() !== '' ? numVal : rawVal;
        this.setValue(parsedVal, true);
      }
    });
  }
}

/**
 * LCD Acrylic Capsule Dropdown Selector with themed popup list.
 */
export class LcdDropdown {
  constructor(container, options = {}) {
    this.container = container;
    this.paramId = options.paramId || '';
    this.label = options.label || '';
    this.options = options.options || []; // [{ value, label, category }]
    this.searchable = options.searchable || false;
    this.onChange = options.onChange || null;

    const storedVal = this.paramId ? paramStore.get(this.paramId) : undefined;
    this.value = options.value !== undefined ? options.value : (storedVal !== undefined ? storedVal : (this.options[0]?.value ?? 0));

    this.isOpen = false;
    this.render();
    this.attachEvents();

    if (this.paramId) {
      paramStore.register(this.paramId, this);
    }
  }

  getSelectedLabel() {
    const found = this.options.find(o => String(o.value) === String(this.value));
    return found ? found.label : `Item #${this.value}`;
  }

  render() {
    this.container.classList.add('lcd-dropdown-wrapper');
    this.container.innerHTML = `
      ${this.label ? `<div class="lcd-dropdown-label">${this.label}</div>` : ''}
      <div class="lcd-dropdown-capsule" tabindex="0">
        <span class="lcd-dropdown-text">${this.getSelectedLabel()}</span>
        <span class="lcd-dropdown-arrow">▼</span>
      </div>
      <div class="lcd-dropdown-popover hidden">
        ${this.searchable ? `
          <div class="lcd-dropdown-search-box">
            <input type="text" class="lcd-dropdown-search-input" placeholder="Buscar..." spellcheck="false" autocomplete="off">
          </div>
        ` : ''}
        <div class="lcd-dropdown-options-list"></div>
      </div>
    `;

    this.capsuleElem = this.container.querySelector('.lcd-dropdown-capsule');
    this.textElem = this.container.querySelector('.lcd-dropdown-text');
    this.popoverElem = this.container.querySelector('.lcd-dropdown-popover');
    this.optionsListElem = this.container.querySelector('.lcd-dropdown-options-list');
    this.searchInput = this.container.querySelector('.lcd-dropdown-search-input');

    this.renderOptionsList();
  }

  renderOptionsList(filterQuery = '') {
    if (!this.optionsListElem) return;
    const q = filterQuery.toLowerCase().trim();
    let filtered = this.options;
    if (q) {
      filtered = this.options.filter(o => o.label.toLowerCase().includes(q) || (o.category && o.category.toLowerCase().includes(q)));
    }

    if (filtered.length === 0) {
      this.optionsListElem.innerHTML = `<div class="lcd-opt-empty">No hay coincidencias</div>`;
      return;
    }

    // Group by category if present
    let html = '';
    let currentCat = null;
    filtered.forEach(opt => {
      if (opt.category && opt.category !== currentCat) {
        currentCat = opt.category;
        html += `<div class="lcd-opt-category">${currentCat}</div>`;
      }
      const isSelected = String(opt.value) === String(this.value);
      html += `
        <div class="lcd-opt-item ${isSelected ? 'selected' : ''}" data-value="${opt.value}">
          <span class="lcd-opt-text">${opt.label}</span>
          ${isSelected ? `<span class="lcd-opt-check">✓</span>` : ''}
        </div>
      `;
    });
    this.optionsListElem.innerHTML = html;
  }

  setValue(newVal, notify = true) {
    if (String(this.value) !== String(newVal)) {
      this.value = newVal;
      if (this.textElem) this.textElem.textContent = this.getSelectedLabel();
      this.renderOptionsList();
      if (this.paramId) {
        paramStore.set(this.paramId, this.value);
      }
      if (notify) {
        if (this.paramId) bridge.setParam(this.paramId, this.value);
        if (this.onChange) this.onChange(this.value, this.paramId);
      }
    }
  }

  openPopover() {
    // Close other open popovers
    document.querySelectorAll('.lcd-dropdown-popover:not(.hidden)').forEach(p => p.classList.add('hidden'));
    this.popoverElem.classList.remove('hidden');
    this.capsuleElem.classList.add('active');
    this.isOpen = true;
    if (this.searchInput) {
      this.searchInput.value = '';
      this.renderOptionsList();
      setTimeout(() => this.searchInput.focus(), 50);
    }
  }

  closePopover() {
    if (!this.popoverElem) return;
    this.popoverElem.classList.add('hidden');
    this.capsuleElem.classList.remove('active');
    this.isOpen = false;
  }

  attachEvents() {
    this.capsuleElem.addEventListener('click', (e) => {
      e.stopPropagation();
      if (this.isOpen) this.closePopover();
      else this.openPopover();
    });

    this.optionsListElem.addEventListener('click', (e) => {
      const item = e.target.closest('.lcd-opt-item');
      if (item && item.dataset.value !== undefined) {
        const rawVal = item.dataset.value;
        const numVal = Number(rawVal);
        const parsedVal = !isNaN(numVal) && rawVal.trim() !== '' ? numVal : rawVal;
        this.setValue(parsedVal, true);
        this.closePopover();
      }
    });

    if (this.searchInput) {
      this.searchInput.addEventListener('input', (e) => {
        this.renderOptionsList(e.target.value);
      });
      this.searchInput.addEventListener('click', (e) => e.stopPropagation());
    }

    document.addEventListener('click', (e) => {
      if (this.isOpen && !this.container.contains(e.target)) {
        this.closePopover();
      }
    });

    this.container.addEventListener('keydown', (e) => {
      if (e.key === 'Escape' && this.isOpen) {
        this.closePopover();
        e.stopPropagation();
      }
    });
  }
}
