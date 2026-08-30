/**
 * sliderFilmstrip.js — Photorealistic Hardware Fader Component & Auto-Filmstrip Initializer.
 *
 * Supports:
 *   - Vertical Faders: uses assets/ST_Fader_58x107_128.png (58x107 px per frame, 128 frames)
 *   - Horizontal Faders: uses assets/ST_Fader_230x69_128f.png (230x69 px per frame, 128 frames)
 *   - Shift + drag for fine-grained 0.2x tuning
 *   - Mouse wheel & double-click reset
 *   - Automatic APVTS paramStore binding
 *   - initFilmstrips(root) to auto-skin any input[type="range"]
 */

import { paramStore } from '../contracts/paramStore.js';

export class SliderFilmstrip {
  constructor(container, options = {}) {
    this.container = container;
    this.paramId = options.paramId || '';
    this.label = options.label || '';
    this.min = options.min !== undefined ? options.min : 0;
    this.max = options.max !== undefined ? options.max : 127;
    this.defaultValue = options.defaultValue !== undefined ? options.defaultValue : 64;
    this.value = options.value !== undefined ? options.value : this.defaultValue;
    this.step = options.step || 1;
    this.isBipolar = options.isBipolar || false;
    this.orientation = options.orientation || 'vertical'; // 'vertical' | 'horizontal'
    this.unit = options.unit || '';
    this.width = options.width || (this.orientation === 'vertical' ? 36 : 140);
    this.height = options.height || (this.orientation === 'vertical' ? 80 : 32);
    this.displayFormatter = options.displayFormatter || ((v) => `${Math.round(v)}${this.unit ? ' ' + this.unit : ''}`);
    this.onChange = options.onChange || null;

    this.isDragging = false;
    this.startPos = 0;
    this.startVal = 0;

    this.render();
    this.attachEvents();

    if (this.paramId) {
      paramStore.register(this.paramId, this);
    }
  }

  render() {
    this.container.classList.add('slider-filmstrip-control');
    this.container.classList.toggle('slider-vertical', this.orientation === 'vertical');
    this.container.classList.toggle('slider-horizontal', this.orientation === 'horizontal');

    const spriteImg = this.orientation === 'vertical'
      ? 'assets/ST_Fader_58x107_128.png'
      : 'assets/ST_Fader_230x69_128f.png';

    const aspectRatio = this.orientation === 'vertical' ? '58 / 107' : '230 / 69';

    this.container.innerHTML = `
      ${this.label ? `<div class="slider-label">${this.label}</div>` : ''}
      <div class="filmstrip-fader-wrapper" style="width: ${this.width}px; aspect-ratio: ${aspectRatio};">
        <div class="filmstrip-fader-sprite" style="background-image: url('${spriteImg}');"></div>
      </div>
      <div class="slider-value-display">${this.formatValue(this.value)}</div>
    `;

    this.wrapper = this.container.querySelector('.filmstrip-fader-wrapper');
    this.spriteElem = this.container.querySelector('.filmstrip-fader-sprite');
    this.displayElem = this.container.querySelector('.slider-value-display');

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
    const range = this.max - this.min || 1;
    const norm = Math.max(0, Math.min(1, (this.value - this.min) / range));
    const frame = Math.round(norm * 127); // 128 frames (0..127)
    const bgPosY = (frame / 127) * 100;

    if (this.spriteElem) {
      this.spriteElem.style.backgroundPositionY = `${bgPosY}%`;
    }
    if (this.displayElem) {
      this.displayElem.textContent = this.formatValue(this.value);
    }
  }

  attachEvents() {
    const onMouseDown = (e) => {
      this.isDragging = true;
      this.startPos = this.orientation === 'vertical' ? e.clientY : e.clientX;
      this.startVal = this.value;
      this.container.classList.add('slider-active');
      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
      e.preventDefault();
    };

    const onMouseMove = (e) => {
      if (!this.isDragging) return;
      const currentPos = this.orientation === 'vertical' ? e.clientY : e.clientX;
      // For vertical: up is positive. For horizontal: right is positive.
      const deltaPx = this.orientation === 'vertical'
        ? (this.startPos - currentPos)
        : (currentPos - this.startPos);

      const travelPx = this.orientation === 'vertical' ? this.height : this.width;
      const speed = e.shiftKey ? 0.2 : 1.0;
      const deltaVal = (deltaPx / (travelPx || 80)) * (this.max - this.min) * speed;
      this.setValue(this.startVal + deltaVal);
    };

    const onMouseUp = () => {
      this.isDragging = false;
      this.container.classList.remove('slider-active');
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
    };

    const onWheel = (e) => {
      e.preventDefault();
      const dir = (e.deltaY < 0 || e.deltaX > 0) ? 1 : -1;
      const step = (this.step || 1) * dir;
      const mult = e.shiftKey ? 0.2 : 1.0;
      this.setValue(this.value + step * mult);
    };

    const onDblClick = () => {
      this.setValue(this.defaultValue);
    };

    this.wrapper?.addEventListener('mousedown', onMouseDown);
    this.wrapper?.addEventListener('wheel', onWheel, { passive: false });
    this.wrapper?.addEventListener('dblclick', onDblClick);
  }
}

/**
 * Auto-enhances all <input type="range"> within root into photorealistic filmstrip faders.
 * @param {HTMLElement} root Container to search for range inputs.
 */
export function initFilmstrips(root = document) {
  const sliders = root.querySelectorAll('input[type="range"]');
  sliders.forEach(slider => {
    if (slider._filmstripPatched || slider.parentElement?.classList.contains('filmstrip-fader-wrapper')) return;

    const isVertical = slider.classList.contains('vocoder-v-slider') ||
                       slider.dataset.orientation === 'vertical' ||
                       slider.getAttribute('orient') === 'vertical';

    const spriteImg = isVertical
      ? 'assets/ST_Fader_58x107_128.png'
      : 'assets/ST_Fader_230x69_128f.png';

    const aspectRatio = isVertical ? '58 / 107' : '230 / 69';

    // Create wrapper
    const wrapper = document.createElement('div');
    wrapper.className = 'filmstrip-fader-wrapper' + (isVertical ? ' filmstrip-vertical' : ' filmstrip-horizontal');
    wrapper.style.position = 'relative';
    wrapper.style.aspectRatio = aspectRatio;
    wrapper.style.width = isVertical ? '32px' : '100%';
    wrapper.style.maxWidth = isVertical ? '36px' : '150px';
    wrapper.style.display = 'inline-block';
    wrapper.style.margin = '2px auto';

    // Sprite div
    const sprite = document.createElement('div');
    sprite.className = 'filmstrip-fader-sprite';
    sprite.style.position = 'absolute';
    sprite.style.top = '0';
    sprite.style.left = '0';
    sprite.style.width = '100%';
    sprite.style.height = '100%';
    sprite.style.backgroundImage = `url("${spriteImg}")`;
    sprite.style.backgroundSize = '100% 12800%';
    sprite.style.backgroundPositionX = 'center';
    sprite.style.backgroundRepeat = 'no-repeat';
    sprite.style.pointerEvents = 'none';

    // Insert wrapper before slider and place slider inside
    slider.parentNode.insertBefore(wrapper, slider);
    wrapper.appendChild(sprite);
    wrapper.appendChild(slider);

    // Make original input invisible overlay
    slider.style.position = 'absolute';
    slider.style.top = '0';
    slider.style.left = '0';
    slider.style.width = '100%';
    slider.style.height = '100%';
    slider.style.opacity = '0';
    slider.style.cursor = isVertical ? 'ns-resize' : 'ew-resize';
    slider.style.margin = '0';
    slider.style.zIndex = '5';

    slider._filmstripPatched = true;

    const updateSprite = () => {
      const min = parseFloat(slider.min) || 0;
      const max = parseFloat(slider.max) || 127;
      const val = parseFloat(slider.value) || 0;
      let percent = (val - min) / (max - min);
      percent = Math.max(0, Math.min(1, percent));
      const frame = Math.round(percent * 127);
      const bgPosY = (frame / 127) * 100;
      sprite.style.backgroundPositionY = `${bgPosY}%`;
    };

    slider.addEventListener('input', updateSprite);
    slider.addEventListener('change', updateSprite);
    requestAnimationFrame(updateSprite);
  });
}
