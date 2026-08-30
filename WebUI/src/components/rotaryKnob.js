/**
 * Vector Rotary Knob Component for Korg MS2000 / microKORG WebUI.
 * Handles mouse drag (vertical/circular), mouse wheel, double click reset, and APVTS bridge binding.
 */

import { paramStore } from '../contracts/paramStore.js';

export class RotaryKnob {
  constructor(container, options = {}) {
    this.container = container;
    this.paramId = options.paramId || '';
    this.label = options.label || 'KNOB';
    this.min = options.min !== undefined ? options.min : 0;
    this.max = options.max !== undefined ? options.max : 127;
    this.defaultValue = options.defaultValue !== undefined ? options.defaultValue : 64;

    const storedVal = this.paramId ? paramStore.get(this.paramId) : undefined;
    this.value = options.value !== undefined ? options.value : (storedVal !== undefined ? storedVal : this.defaultValue);
    this.step = options.step || 1;
    this.isBipolar = options.isBipolar || false;
    this.displayFormatter = options.displayFormatter || ((v) => `${Math.round(v)}`);
    this.onChange = options.onChange || null;

    this.size = options.size || 48;
    this.isDragging = false;
    this.startY = 0;
    this.startVal = 0;

    this.render();
    this.attachEvents();

    if (this.paramId) {
      paramStore.register(this.paramId, this);
    }
  }


  render() {
    this.container.classList.add('rotary-control');
    this.container.innerHTML = `
      <div class="knob-label">${this.label}</div>
      <div class="knob-body" style="width: ${this.size}px; height: ${this.size}px;">
        <svg viewBox="0 0 50 50" class="knob-svg">
          <!-- Outer Shadow & Base Rim -->
          <circle cx="25" cy="25" r="23" class="knob-rim" />
          <!-- Inner Cap with Gradient -->
          <circle cx="25" cy="25" r="19" class="knob-cap" />
          <!-- Indicator Pointer Line -->
          <line x1="25" y1="25" x2="25" y2="8" class="knob-pointer" />
          <!-- Center Inject Dot -->
          <circle cx="25" cy="25" r="2.5" class="knob-center-dot" />
        </svg>
      </div>
      <div class="knob-value-display">${this.formatValue(this.value)}</div>
    `;

    this.bodyElem = this.container.querySelector('.knob-body');
    this.pointerElem = this.container.querySelector('.knob-pointer');
    this.displayElem = this.container.querySelector('.knob-value-display');
    this.updateVisuals();
  }

  formatValue(val) {
    if (this.isBipolar) {
      const center = (this.max + this.min) / 2;
      const offset = Math.round(val - center);
      return offset > 0 ? `+${offset}` : `${offset}`;
    }
    return this.displayFormatter(val);
  }

  setValue(newVal, notify = true) {
    const clamped = Math.max(this.min, Math.min(this.max, newVal));
    if (this.value !== clamped) {
      this.value = clamped;
      this.updateVisuals();
      if (this.paramId) {
        paramStore.values.set(this.paramId, this.value);
      }
      if (notify && this.onChange) {
        this.onChange(this.value, this.paramId);
      }
    }
  }

  updateVisuals() {
    // Standard 270-degree range (-135 deg to +135 deg)
    const norm = (this.value - this.min) / (this.max - this.min || 1);
    const angle = -135 + norm * 270;

    if (this.pointerElem) {
      this.pointerElem.setAttribute('transform', `rotate(${angle} 25 25)`);
    }
    if (this.displayElem) {
      this.displayElem.textContent = this.formatValue(this.value);
    }
  }

  attachEvents() {
    const onMouseDown = (e) => {
      this.isDragging = true;
      this.startY = e.clientY;
      this.startVal = this.value;
      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!this.isDragging) return;
      const deltaY = this.startY - e.clientY;
      const speed = e.shiftKey ? 0.2 : 0.8; // Fine-tune with Shift
      const range = this.max - this.min;
      const change = (deltaY / 150) * range * speed;
      this.setValue(this.startVal + change, true);
    };

    const onMouseUp = () => {
      this.isDragging = false;
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
    };

    const onWheel = (e) => {
      e.preventDefault();
      const step = (e.deltaY < 0 ? 1 : -1) * (e.shiftKey ? 1 : 3);
      this.setValue(this.value + step, true);
    };

    const onDblClick = () => {
      this.setValue(this.defaultValue, true);
    };

    if (this.bodyElem) {
      this.bodyElem.addEventListener('mousedown', onMouseDown);
      this.bodyElem.addEventListener('wheel', onWheel, { passive: false });
      this.bodyElem.addEventListener('dblclick', onDblClick);
    }
  }
}
