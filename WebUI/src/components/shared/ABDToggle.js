/**
 * ABDMS2000 — SharedAssets Toggle wrapper (boolean MIDI/APVTS <-> boolean).
 *
 * Wraps native checkbox with paramStore/bridge binding for panelFactory.
 * Keeps the existing toggle HTML but adds programmatic sync.
 */
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDToggle {
    constructor(container, options = {}) {
        this.paramId = options.paramId || '';
        this.onChange = options.onChange || null;

        // Find the checkbox element
        this.checkbox = container.querySelector(`#param-${this.paramId}`);
        if (!this.checkbox) return;

        // Read initial value from paramStore
        let initialValue = 0;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }

        this.checkbox.checked = initialValue > 0.5;

        // Register with paramStore
        if (this.paramId) {
            paramStore.register(this.paramId, this);
        }

        // Wire change event
        this.checkbox.addEventListener('change', (e) => {
            const val = e.target.checked ? 1 : 0;
            if (this.paramId) {
                paramStore.set(this.paramId, val);
                bridge.setParam(this.paramId, val);
            }
            if (this.onChange) {
                this.onChange(val, this.paramId);
            }
        });
    }

    /** Programmatic update — does NOT fire onChange. */
    setValue(value, notify = false) {
        const val = value > 0.5 ? 1 : 0;
        this.checkbox.checked = val > 0.5;
        if (notify && this.onChange) {
            this.onChange(val, this.paramId);
        }
    }

    getValue() {
        return this.checkbox.checked ? 1 : 0;
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
    }
}