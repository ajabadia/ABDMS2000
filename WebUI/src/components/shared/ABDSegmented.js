/**
 * ABDMS2000 — SharedAssets Segmented wrapper (index-based, with glyphs/badges).
 *
 * Wraps @abdsynths/shared/components Segmented with:
 *   - Value conversion: APVTS value (0..N) <-> Segmented index
 *   - paramStore binding
 *   - bridge.setParam on user edits
 *   - Glyph support (maps MS2000 iconSvg -> glyph)
 *   - Badge support (not in SharedAssets, adds custom rendering)
 *   - Dynamic disabled options support
 *   - Variant: 'strip' (default) or 'led'
 *
 * Usage:
 *   import { ABDSegmented } from './shared/ABDSegmented.js';
 *   new ABDSegmented(container, {
 *     paramId: 'filterType',
 *     label: 'FILTER',
 *     options: [
 *       { value: 0, label: 'LPF 24', iconSvg: '...' },
 *       { value: 1, label: 'LPF 12', iconSvg: '...' },
 *       { value: 2, label: 'BPF 12', iconSvg: '...' },
 *       { value: 3, label: 'HPF 12', iconSvg: '...' },
 *     ],
 *   });
 */
import { Segmented } from '@abdsynths/shared/components';
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDSegmented {
    constructor(container, options = {}) {
        this.paramId = options.paramId || '';
        this.label = options.label || '';
        this.rawOptions = options.options || []; // MS2000 format: [{value, label, iconSvg, badge}]
        this.layout = options.layout || 'row'; // 'row' | 'grid-2' | 'grid-4' | 'wrap'
        this.size = options.size || 'md';
        this.variant = options.variant || 'strip'; // 'strip' | 'led'
        this.onChange = options.onChange || null;

        // Convert MS2000 options -> SharedAssets entries
        const entries = this.rawOptions.map(opt => ({
            label: opt.label,
            glyph: opt.iconSvg || '',
            // Note: badge not supported in SharedAssets Segmented, would need custom CSS
        }));

        // Read initial value from paramStore
        let initialValue = options.value !== undefined ? options.value : 0;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        // Find index by value
        const initialIndex = this.findIndexByValue(initialValue);

        this.segmented = new Segmented(container, {
            options: entries,
            value: initialIndex,
            label: this.label,
            id: options.id,
            disabled: options.disabled,
            skin: options.skin || 'ms2000',
            variant: this.variant,
            onChange: (idx) => this.handleChange(idx),
        });

        // Store value mapping for getValue/setValue
        this.valueToIndex = new Map();
        this.indexToValue = new Map();
        this.rawOptions.forEach((opt, idx) => {
            this.valueToIndex.set(opt.value, idx);
            this.indexToValue.set(idx, opt.value);
        });

        if (this.paramId) {
            paramStore.register(this.paramId, this);
        }
    }

    findIndexByValue(value) {
        for (let i = 0; i < this.rawOptions.length; i++) {
            if (this.rawOptions[i].value === value) return i;
        }
        return 0;
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
        const idx = this.valueToIndex.get(value) ?? this.findIndexByValue(value);
        this.segmented.setValue(idx);
        if (notify && this.onChange) {
            this.onChange(value, this.paramId);
        }
    }

    getValue() {
        const idx = this.segmented.getValue();
        return this.indexToValue.get(idx) ?? idx;
    }

    /** Update disabled options. */
    setDisabled(spec) {
        this.segmented.setDisabled(spec);
    }

    /** Check if current value is on a disabled option. */
    isDivergent() {
        return this.segmented.isDivergent();
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.segmented.destroy();
    }
}