/**
 * ABDMS2000 — SharedAssets Knob wrapper (0..127 MIDI/APVTS <-> 0..1 normalized).
 *
 * Wraps @abdsynths/shared/components Knob with:
 *   - Value conversion: 0..127 (APVTS) <-> 0..1 (SharedAssets)
 *   - paramStore binding (read initial, write on change)
 *   - bridge.setParam on user edits
 *   - skin: 'ms2000' by default
 *
 * Usage:
 *   import { ABDKnob } from './shared/ABDKnob.js';
 *   new ABDKnob(container, { paramId: 'filterCutoff', label: 'CUTOFF', min: 0, max: 127 });
 */
import { Knob } from '@abdsynths/shared/components';
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDKnob {
    constructor(container, options = {}) {
        this.paramId = options.paramId || '';
        this.label = options.label || 'KNOB';
        this.min = options.min !== undefined ? options.min : 0;
        this.max = options.max !== undefined ? options.max : 127;
        this.defaultValue = options.defaultValue !== undefined ? options.defaultValue : 64;
        this.step = options.step !== undefined ? options.step : 1;
        this.isBipolar = options.isBipolar || false;
        this.displayFormatter = options.displayFormatter || ((v) => `${Math.round(v)}`);
        this.onChange = options.onChange || null;

        // Read initial value from paramStore (APVTS) or use default
        let initialValue = this.defaultValue;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        // Convert 0..127 -> 0..1 for SharedAssets Knob
        const normValue = this.toNorm(initialValue);
        this._lastClamped = initialValue;

        this.knob = new Knob(container, {
            size: options.size || 48,
            value: normValue,
            label: this.label,
            skin: options.skin || 'ms2000',
            step: this.step / (this.max - this.min || 1), // normalized step
            format: (v) => this.displayFormatter(this.fromNorm(v)),
            onChange: (normVal) => this.handleChange(normVal),
            onDragStart: options.onDragStart,
            onDragEnd: () => {
                const finalNorm = this.toNorm(this._lastClamped);
                this.knob.setValue(finalNorm);
                options.onDragEnd?.();
            },
        });

        // Para controles booleanos / conmutadores de 2 estados (min: 0, max: 1), permitir click para alternar
        if (this.max - this.min === 1) {
            let downX = 0;
            let downY = 0;
            this.knob.dial.addEventListener('pointerdown', (e) => {
                downX = e.clientX;
                downY = e.clientY;
            });
            this.knob.dial.addEventListener('pointerup', (e) => {
                const dx = Math.abs(e.clientX - downX);
                const dy = Math.abs(e.clientY - downY);
                if (dx < 4 && dy < 4) {
                    const current = this.getValue();
                    const next = current > 0.5 ? 0 : 1;
                    this.setValue(next, true);
                    if (this.paramId) {
                        paramStore.set(this.paramId, next);
                        bridge.setParam(this.paramId, next);
                    }
                }
            });
        }

        // Register with paramStore for external sync (patch load, randomize, etc.)
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

        // Evitar reseteo destructivo de deltas durante el arrastre: solo actualizar el mando
        // y notificar si el valor cuantizado realmente cambió de paso.
        if (clamped !== this._lastClamped) {
            this._lastClamped = clamped;
            const finalNorm = this.toNorm(clamped);
            this.knob.setValue(finalNorm);

            if (this.paramId) {
                paramStore.set(this.paramId, clamped);
                bridge.setParam(this.paramId, clamped);
            }
            if (this.onChange) {
                this.onChange(clamped, this.paramId);
            }
        }
    }

    /** Programmatic update (from paramStore/bridge) — does NOT fire onChange. */
    setValue(value, notify = false) {
        const clamped = Math.max(this.min, Math.min(this.max, value));
        const norm = this.toNorm(clamped);
        this.knob.setValue(norm);
        if (notify && this.onChange) {
            this.onChange(clamped, this.paramId);
        }
    }

    getValue() {
        return this.fromNorm(this.knob.getValue());
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.knob.destroy();
    }
}