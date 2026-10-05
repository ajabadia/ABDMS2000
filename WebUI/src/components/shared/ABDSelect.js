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
        this.options = options.options || []; // array of strings or {label, disabled?, note?}
        this.onChange = options.onChange || null;

        // Read initial value from paramStore
        let initialValue = options.value !== undefined ? options.value : 0;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        // Convert APVTS value -> Select index (clamped)
        const index = this.clampIndex(initialValue);

        this.select = new Select(container, {
            options: this.options,
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

    clampIndex(index) {
        const count = this.options.length;
        if (count === 0) return 0;
        const n = Math.round(Number(index));
        return Math.min(count - 1, Math.max(0, Number.isFinite(n) ? n : 0));
    }

    handleChange(idx) {
        const realVal = idx; // APVTS stores the index directly
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
        const idx = this.clampIndex(value);
        this.select.setValue(idx);
        if (notify && this.onChange) {
            this.onChange(idx, this.paramId);
        }
    }

    getValue() {
        return this.select.getValue();
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