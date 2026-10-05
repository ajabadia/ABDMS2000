/**
 * ABDMS2000 — SharedAssets Slider wrapper (0..127 MIDI/APVTS <-> 0..1 normalized).
 *
 * Wraps @abdsynths/shared/components Slider with:
 *   - Value conversion: 0..127 (APVTS) <-> 0..1 (SharedAssets)
 *   - paramStore binding
 *   - bridge.setParam on user edits
 *   - skin: 'ms2000' by default (filmstrip fader support via spriteUrl)
 *
 * Usage:
 *   import { ABDSlider } from './shared/ABDSlider.js';
 *   new ABDSlider(container, {
 *     paramId: 'filterCutoff',
 *     label: 'CUTOFF',
 *     orientation: 'vertical',
 *     spriteUrl: 'assets/ST_Fader_58x107_128.png',
 *     frameWidth: 58, frameHeight: 107, frames: 128,
 *   });
 */
import { Slider } from '@abdsynths/shared/components';
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDSlider {
    constructor(container, options = {}) {
        this.paramId = options.paramId || '';
        this.label = options.label || '';
        this.min = options.min !== undefined ? options.min : 0;
        this.max = options.max !== undefined ? options.max : 127;
        this.defaultValue = options.defaultValue !== undefined ? options.defaultValue : 64;
        this.step = options.step !== undefined ? options.step : 1;
        this.isBipolar = options.isBipolar || false;
        this.orientation = options.orientation || 'vertical';
        this.unit = options.unit || '';
        this.displayFormatter = options.displayFormatter || ((v) => `${Math.round(v)}${this.unit ? ' ' + this.unit : ''}`);
        this.onChange = options.onChange || null;

        // Read initial value from paramStore
        let initialValue = this.defaultValue;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        const normValue = this.toNorm(initialValue);

        this.slider = new Slider(container, {
            orientation: this.orientation,
            length: options.length || (this.orientation === 'vertical' ? 80 : 160),
            value: normValue,
            label: this.label,
            skin: options.skin || 'ms2000',
            step: this.step / (this.max - this.min),
            format: (v) => this.displayFormatter(this.fromNorm(v)),
            spriteUrl: options.spriteUrl,
            frameWidth: options.frameWidth,
            frameHeight: options.frameHeight,
            frames: options.frames,
            onChange: (normVal) => this.handleChange(normVal),
            onDragStart: options.onDragStart,
            onDragEnd: options.onDragEnd,
        });

        if (this.paramId) {
            paramStore.register(this.paramId, this);
        }
    }

    toNorm(value) {
        return (value - this.min) / (this.max - this.min || 1);
    }

    fromNorm(norm) {
        return this.min + norm * (this.max - this.min);
    }

    handleChange(normVal) {
        const realVal = this.fromNorm(normVal);
        const stepped = Math.round(realVal / this.step) * this.step;
        const clamped = Math.max(this.min, Math.min(this.max, stepped));
        const finalNorm = this.toNorm(clamped);

        this.slider.setValue(finalNorm);

        if (this.paramId) {
            paramStore.set(this.paramId, clamped);
            bridge.setParam(this.paramId, clamped);
        }
        if (this.onChange) {
            this.onChange(clamped, this.paramId);
        }
    }

    /** Programmatic update — does NOT fire onChange. */
    setValue(value, notify = false) {
        const clamped = Math.max(this.min, Math.min(this.max, value));
        const norm = this.toNorm(clamped);
        this.slider.setValue(norm);
        if (notify && this.onChange) {
            this.onChange(clamped, this.paramId);
        }
    }

    getValue() {
        return this.fromNorm(this.slider.getValue());
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.slider.destroy();
    }
}