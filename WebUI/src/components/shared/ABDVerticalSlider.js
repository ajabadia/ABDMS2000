/**
 * ABDMS2000 — Vertical Tactile Slider Component
 *
 * Vector-based vertical slider designed to match the visual language of the
 * horizontal ABD sliders (6px track groove, glowing circular thumb, active level fill,
 * header label, and digital value readout).
 *
 * Features:
 *   - Normalized 0..1 <-> min..max (default 0..127) mapping
 *   - Smooth pointer capture drag (mouse, touch, stylus)
 *   - Mouse wheel increment/decrement
 *   - Keyboard navigation (Arrow keys, PageUp/PageDown, Home, End)
 *   - Double-click to reset to defaultValue
 *   - Automatic two-way binding with paramStore and bridge.setParam
 *   - Full ARIA accessibility (role="slider", aria-orientation="vertical")
 */

import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDVerticalSlider {
  constructor(container, options = {}) {
    this.container = typeof container === 'string'
      ? document.querySelector(container)
      : container;
    if (!this.container) throw new Error('ABDVerticalSlider: container not found');

    this.paramId = options.paramId || '';
    this.label = options.label || '';
    this.min = options.min !== undefined ? options.min : 0;
    this.max = options.max !== undefined ? options.max : 127;
    this.defaultValue = options.defaultValue !== undefined ? options.defaultValue : 127;
    this.step = options.step !== undefined ? options.step : 1;
    this.height = options.height || 110;
    this.trackWidth = options.trackWidth || 6;
    this.thumbSize = options.thumbSize || 16;
    this.showValue = options.showValue !== false;
    this.showLabel = options.showLabel !== false;
    this.unit = options.unit || '';
    this.displayFormatter = options.displayFormatter || ((v) => `${Math.round(v)}${this.unit ? ' ' + this.unit : ''}`);
    this.onChange = options.onChange || null;

    let initialValue = this.defaultValue;
    if (this.paramId) {
      const stored = paramStore.get(this.paramId);
      if (stored !== undefined) initialValue = stored;
    }
    this.value = Math.max(this.min, Math.min(this.max, initialValue));

    this.isDragging = false;

    this.buildDom();
    this.attachEvents();
    this.updateVisuals();

    if (this.paramId) {
      paramStore.register(this.paramId, this);
    }
  }

  buildDom() {
    this.wrapper = document.createElement('div');
    this.wrapper.className = 'abd-vslider';
    this.wrapper.setAttribute('role', 'slider');
    this.wrapper.setAttribute('tabindex', '0');
    this.wrapper.setAttribute('aria-orientation', 'vertical');
    this.wrapper.setAttribute('aria-valuemin', String(this.min));
    this.wrapper.setAttribute('aria-valuemax', String(this.max));
    this.wrapper.setAttribute('aria-valuenow', String(this.value));
    if (this.label) {
      this.wrapper.setAttribute('aria-label', this.label);
    }

    if (this.showLabel && this.label) {
      this.labelEl = document.createElement('span');
      this.labelEl.className = 'abd-vslider__label';
      this.labelEl.textContent = this.label;
      this.wrapper.appendChild(this.labelEl);
    }

    this.trackContainer = document.createElement('div');
    this.trackContainer.className = 'abd-vslider__track-container';
    this.trackContainer.style.height = `${this.height}px`;

    this.trackEl = document.createElement('div');
    this.trackEl.className = 'abd-vslider__track';
    this.trackEl.style.width = `${this.trackWidth}px`;

    this.fillEl = document.createElement('div');
    this.fillEl.className = 'abd-vslider__fill';

    this.thumbEl = document.createElement('div');
    this.thumbEl.className = 'abd-vslider__thumb';
    this.thumbEl.style.width = `${this.thumbSize}px`;
    this.thumbEl.style.height = `${this.thumbSize}px`;

    this.trackEl.appendChild(this.fillEl);
    this.trackEl.appendChild(this.thumbEl);
    this.trackContainer.appendChild(this.trackEl);
    this.wrapper.appendChild(this.trackContainer);

    if (this.showValue) {
      this.valueEl = document.createElement('span');
      this.valueEl.className = 'abd-vslider__value';
      this.wrapper.appendChild(this.valueEl);
    }

    this.container.appendChild(this.wrapper);
  }

  attachEvents() {
    this.handlePointerDown = (e) => {
      if (e.button !== 0) return;
      this.isDragging = true;
      this.wrapper.classList.add('abd-vslider--active');
      try {
        this.trackContainer.setPointerCapture(e.pointerId);
      } catch {}
      this.handleUserEdit(this.calculateValueFromClientY(e.clientY));
    };

    this.handlePointerMove = (e) => {
      if (!this.isDragging) return;
      this.handleUserEdit(this.calculateValueFromClientY(e.clientY));
    };

    this.handlePointerUp = (e) => {
      if (!this.isDragging) return;
      this.isDragging = false;
      this.wrapper.classList.remove('abd-vslider--active');
      try {
        if (this.trackContainer.hasPointerCapture(e.pointerId)) {
          this.trackContainer.releasePointerCapture(e.pointerId);
        }
      } catch {}
    };

    this.trackContainer.addEventListener('pointerdown', this.handlePointerDown);
    this.trackContainer.addEventListener('pointermove', this.handlePointerMove);
    this.trackContainer.addEventListener('pointerup', this.handlePointerUp);
    this.trackContainer.addEventListener('pointercancel', this.handlePointerUp);

    // Double-click resets to default
    this.trackContainer.addEventListener('dblclick', (e) => {
      e.preventDefault();
      this.handleUserEdit(this.defaultValue);
    });

    // Mouse wheel adjustments
    this.trackContainer.addEventListener('wheel', (e) => {
      e.preventDefault();
      const dir = e.deltaY < 0 ? 1 : -1;
      const stepVal = this.step || 1;
      const newVal = Math.max(this.min, Math.min(this.max, this.value + dir * stepVal));
      this.handleUserEdit(newVal);
    }, { passive: false });

    // Keyboard support
    this.wrapper.addEventListener('keydown', (e) => {
      let handled = true;
      const stepVal = this.step || 1;
      if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
        this.handleUserEdit(Math.min(this.max, this.value + stepVal));
      } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
        this.handleUserEdit(Math.max(this.min, this.value - stepVal));
      } else if (e.key === 'PageUp') {
        this.handleUserEdit(Math.min(this.max, this.value + stepVal * 10));
      } else if (e.key === 'PageDown') {
        this.handleUserEdit(Math.max(this.min, this.value - stepVal * 10));
      } else if (e.key === 'Home') {
        this.handleUserEdit(this.min);
      } else if (e.key === 'End') {
        this.handleUserEdit(this.max);
      } else {
        handled = false;
      }
      if (handled) {
        e.preventDefault();
      }
    });
  }

  calculateValueFromClientY(clientY) {
    const rect = this.trackContainer.getBoundingClientRect();
    const halfThumb = this.thumbSize / 2;
    const top = rect.top + halfThumb;
    const bottom = rect.bottom - halfThumb;
    const travel = bottom - top;
    if (travel <= 0) return this.value;

    const clampedY = Math.max(top, Math.min(bottom, clientY));
    const norm = 1 - (clampedY - top) / travel;
    const rawVal = this.min + norm * (this.max - this.min);
    const stepped = Math.round(rawVal / this.step) * this.step;
    return Math.max(this.min, Math.min(this.max, stepped));
  }

  updateVisuals() {
    const norm = (this.value - this.min) / (this.max - this.min || 1);
    const percent = Math.max(0, Math.min(100, norm * 100));
    this.fillEl.style.height = `${percent}%`;
    this.thumbEl.style.bottom = `${percent}%`;

    const formatted = this.displayFormatter(this.value);
    if (this.valueEl) {
      this.valueEl.textContent = formatted;
    }
    this.wrapper.setAttribute('aria-valuenow', String(this.value));
    this.wrapper.setAttribute('aria-valuetext', formatted);
  }

  handleUserEdit(newVal) {
    if (newVal === this.value) return;
    this.value = newVal;
    this.updateVisuals();

    if (this.paramId) {
      paramStore.set(this.paramId, newVal);
      bridge.setParam(this.paramId, newVal);
    }
    if (this.onChange) {
      this.onChange(newVal, this.paramId);
    }
  }

  /**
   * Programmatic update (does NOT fire onChange by default).
   */
  setValue(val, notify = false) {
    const clamped = Math.max(this.min, Math.min(this.max, val));
    if (clamped === this.value) return;
    this.value = clamped;
    this.updateVisuals();

    if (notify) {
      if (this.paramId) {
        paramStore.set(this.paramId, this.value);
        bridge.setParam(this.paramId, this.value);
      }
      if (this.onChange) {
        this.onChange(this.value, this.paramId);
      }
    }
  }

  getValue() {
    return this.value;
  }

  destroy() {
    if (this.paramId) {
      paramStore.unregister(this.paramId, this);
    }
    this.trackContainer.removeEventListener('pointerdown', this.handlePointerDown);
    this.trackContainer.removeEventListener('pointermove', this.handlePointerMove);
    this.trackContainer.removeEventListener('pointerup', this.handlePointerUp);
    this.trackContainer.removeEventListener('pointercancel', this.handlePointerUp);
    this.wrapper.remove();
  }
}
