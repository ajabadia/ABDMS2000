/**
 * ABDMS2000 — Hardware-styled Synth Toggle Switch (0 / 1 boolean).
 *
 * Designed to seamlessly accompany ABDKnob in synth panels and drawers:
 *   - Shares exact label typography, dimensions, and theme tokens
 *   - Authentic physical toggle switch aesthetic (beveled collar, metallic lever, LED)
 *   - Accessible (role="switch", aria-checked, keyboard navigable)
 *   - Synchronized with paramStore and bridgeCore
 */
import { paramStore } from '../../contracts/paramStore.js';
import { bridge } from '../../bridge/bridgeCore.js';

export class ABDSwitch {
    constructor(container, options = {}) {
        this.container = typeof container === 'string'
            ? document.querySelector(container)
            : container;

        if (!this.container) {
            throw new Error('ABDSwitch: container not found');
        }

        this.paramId = options.paramId || '';
        this.label = options.label || 'SWITCH';
        this.size = options.size || 50;
        this.onChange = options.onChange || null;

        // Determine initial value from paramStore or options
        let initialValue = options.defaultValue !== undefined ? options.defaultValue : 0;
        if (this.paramId) {
            const stored = paramStore.get(this.paramId);
            if (stored !== undefined) initialValue = stored;
        }
        this.value = initialValue > 0.5 ? 1 : 0;

        this.buildDom();
        this.attachInteraction();

        if (this.paramId) {
            paramStore.register(this.paramId, this);
        }

        this.render();
    }

    buildDom() {
        this.wrapper = document.createElement('div');
        this.wrapper.className = 'abd-switch';
        this.wrapper.style.setProperty('--abd-switch-size', `${this.size}px`);

        // Label matching ABDKnob label
        if (this.label) {
            this.labelEl = document.createElement('span');
            this.labelEl.className = 'abd-switch__label';
            this.labelEl.textContent = this.label;
            this.wrapper.appendChild(this.labelEl);
        }

        // Switch control element (accessible role="switch")
        this.switchEl = document.createElement('div');
        this.switchEl.className = 'abd-switch__control';
        this.switchEl.tabIndex = 0;
        this.switchEl.setAttribute('role', 'switch');
        this.switchEl.setAttribute('aria-label', this.label);
        this.switchEl.setAttribute('aria-checked', this.value === 1 ? 'true' : 'false');

        // Internal hardware structure: bezel collar, slot, lever, LED indicator
        this.switchEl.innerHTML = `
            <div class="abd-switch__bezel">
                <div class="abd-switch__led" aria-hidden="true"></div>
                <div class="abd-switch__slot">
                    <div class="abd-switch__lever" aria-hidden="true">
                        <div class="abd-switch__lever-tip"></div>
                    </div>
                </div>
            </div>
        `;

        this.wrapper.appendChild(this.switchEl);

        // State indicator text below matching knob format
        this.valueEl = document.createElement('span');
        this.valueEl.className = 'abd-switch__value';
        this.valueEl.textContent = this.value === 1 ? 'ON' : 'OFF';
        this.wrapper.appendChild(this.valueEl);

        this.container.appendChild(this.wrapper);
    }

    attachInteraction() {
        const toggle = () => {
            const next = this.value === 1 ? 0 : 1;
            this.setValue(next, true);
            if (this.paramId) {
                paramStore.set(this.paramId, next);
                bridge.setParam(this.paramId, next);
            }
        };

        this.switchEl.addEventListener('click', (e) => {
            e.preventDefault();
            toggle();
        });

        this.switchEl.addEventListener('keydown', (e) => {
            if (e.key === ' ' || e.key === 'Enter') {
                e.preventDefault();
                toggle();
            } else if (e.key === 'ArrowUp' || e.key === 'ArrowRight') {
                e.preventDefault();
                if (this.value === 0) toggle();
            } else if (e.key === 'ArrowDown' || e.key === 'ArrowLeft') {
                e.preventDefault();
                if (this.value === 1) toggle();
            }
        });
    }

    render() {
        const isOn = this.value === 1;
        this.switchEl.setAttribute('aria-checked', isOn ? 'true' : 'false');
        this.wrapper.classList.toggle('abd-switch--active', isOn);

        if (this.valueEl) {
            this.valueEl.textContent = isOn ? 'ON' : 'OFF';
            this.valueEl.classList.toggle('active', isOn);
        }
    }

    /**
     * Programmatic update — does NOT fire onChange unless notify=true.
     */
    setValue(value, notify = false) {
        const next = value > 0.5 ? 1 : 0;
        const changed = next !== this.value;
        this.value = next;
        this.render();

        if (changed && notify && this.onChange) {
            this.onChange(this.value, this.paramId);
        }
    }

    getValue() {
        return this.value;
    }

    destroy() {
        if (this.paramId) {
            paramStore.unregister(this.paramId, this);
        }
        this.wrapper.remove();
    }
}
