/**
 * ABDMS2000 — SharedAssets Select wrapper (index-based, 0..127 MIDI/APVTS <-> index).
 *
 * Wraps @abdsynths/shared/components Select with:
 *   - Value conversion: APVTS value (0..N) <-> Select index
 *   - paramStore binding
 *   - bridge.setParam on user edits
 *   - Dynamic disabled options support (setDisabled)
 *   - Divergent value handling (host state on disabled option)
 *
 * Usage:
 *   import { ABDSelect } from './shared/ABDSelect.js';
 *   new ABDSelect(container, {
 *     paramId: 'osc1Wave',
 *     label: 'WAVE',
 *     options: ['SAW', 'SQUARE', 'TRIANGLE', 'SINE', 'VOX', 'DWGS', 'NOISE', 'AUDIO IN'],
 *   });
 */
import { Select } from '@abdsynths/shared/components';
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDSelect {
    constructor(container, options = {}) {
        this.paramId = options.paramId || '';
        this.label = options.label || '';
        this.rawOptions = options.options || []; // array of strings or {value?, label, disabled?, note?}
        this.onChange = options.onChange || null;

        // Support explicit value mapping ({ value, label })
        this.valueToIndex = new Map();
        this.indexToValue = new Map();
        this.hasExplicitValues = false;

        const entries = this.rawOptions.map((opt, idx) => {
            if (typeof opt === 'object' && opt !== null) {
                if (opt.value !== undefined) {
                    this.hasExplicitValues = true;
                    this.valueToIndex.set(opt.value, idx);
                    this.indexToValue.set(idx, opt.value);
                } else {
                    this.valueToIndex.set(idx, idx);
                    this.indexToValue.set(idx, idx);
                }
                return {
                    label: opt.label !== undefined ? opt.label : String(opt.value),
                    disabled: opt.disabled,
                    note: opt.note
                };
            }
            this.valueToIndex.set(idx, idx);
            this.indexToValue.set(idx, idx);
            return opt;
        });

        // Read initial value from paramStore
        let initialValue = options.value !== undefined ? options.value : (this.hasExplicitValues ? (this.rawOptions[0]?.value ?? 0) : 0);
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        // Convert APVTS/domain value -> Select index
        const index = this.findIndexByValue(initialValue);

        this.select = new Select(container, {
            options: entries,
            value: index,
            label: this.label,
            id: options.id,
            disabled: options.disabled,
            skin: options.skin || 'ms2000',
            onChange: (idx) => this.handleChange(idx),
        });

        if (this.paramId) {
            paramStore.register(this.paramId, this);
        }
    }

    findIndexByValue(value) {
        if (this.hasExplicitValues) {
            if (this.valueToIndex.has(value)) return this.valueToIndex.get(value);
            const n = Math.round(Number(value));
            if (this.valueToIndex.has(n)) return this.valueToIndex.get(n);
            for (let i = 0; i < this.rawOptions.length; i++) {
                if (this.rawOptions[i] && this.rawOptions[i].value === n) return i;
            }
        }
        return this.clampIndex(value);
    }

    clampIndex(index) {
        const count = this.rawOptions.length;
        if (count === 0) return 0;
        const n = Math.round(Number(index));
        return Math.min(count - 1, Math.max(0, Number.isFinite(n) ? n : 0));
    }

    handleChange(idx) {
        const realVal = this.indexToValue.get(idx) ?? idx;
        if (this.paramId) {
            paramStore.set(this.paramId, realVal);
            bridge.setParam(this.paramId, realVal);
        }
        if (this.onChange) {
            this.onChange(realVal, this.paramId);
        }
    }

    /** Programmatic update — does NOT fire onChange. */
    setValue(value, notify = false) {
        const idx = this.findIndexByValue(value);
        this.select.setValue(idx);
        if (notify && this.onChange) {
            const realVal = this.indexToValue.get(idx) ?? idx;
            this.onChange(realVal, this.paramId);
        }
    }

    getValue() {
        const idx = this.select.getValue();
        return this.indexToValue.get(idx) ?? idx;
    }

    /** Update disabled options (e.g. when another parameter changes available choices). */
    setDisabled(spec) {
        this.select.setDisabled(spec);
    }

    /** Check if current value is on a disabled option (host state we can't offer). */
    isDivergent() {
        return this.select.isDivergent();
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.select.destroy();
    }
}