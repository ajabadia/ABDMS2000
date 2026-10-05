/**
 * ABDMS2000 — SharedAssets FilmstripFader wrapper (0..127 MIDI/APVTS <-> 0..1 normalized).
 *
 * Wraps @abdsynths/shared/components FilmstripFader with:
 *   - Value conversion: 0..127 (APVTS) <-> 0..1 (SharedAssets)
 *   - paramStore binding
 *   - bridge.setParam on user edits
 *
 * This is the MS2000-style fader: fixed viewport, scrolling filmstrip sprite.
 * Use this for the main mixer faders and other hardware-style faders.
 *
 * Usage:
 *   import { ABDFilmstripFader } from './shared/ABDFilmstripFader.js';
 *   new ABDFilmstripFader(container, {
 *     paramId: 'mixOsc1Level',
 *     label: 'OSC 1',
 *     orientation: 'vertical',
 *     spriteUrl: 'assets/ST_Fader_58x107_128.png',
 *     frameWidth: 58, frameHeight: 107, frames: 128,
 *     viewportWidth: 36, viewportHeight: 80,
 *   });
 */
import { FilmstripFader } from '@abdsynths/shared/components';
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDFilmstripFader {
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

        this.fader = new FilmstripFader(container, {
            orientation: this.orientation,
            spriteUrl: options.spriteUrl,
            frameWidth: options.frameWidth,
            frameHeight: options.frameHeight,
            frames: options.frames,
            viewportWidth: options.viewportWidth,
            viewportHeight: options.viewportHeight,
            value: normValue,
            label: this.label,
            format: (v) => this.displayFormatter(this.fromNorm(v)),
            step: this.step / (this.max - this.min),
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

        this.fader.setValue(finalNorm);

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
        this.fader.setValue(norm);
        if (notify && this.onChange) {
            this.onChange(clamped, this.paramId);
        }
    }

    getValue() {
        return this.fromNorm(this.fader.getValue());
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.fader.destroy();
    }
}