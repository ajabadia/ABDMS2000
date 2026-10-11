import { describe, it, expect, beforeEach, vi } from 'vitest';

// Minimal mock DOM node for node environment
class MockElement {
    constructor(tagName = 'div') {
        this.tagName = tagName;
        this.className = '';
        this.classList = {
            _classes: new Set(),
            add: (c) => this.classList._classes.add(c),
            remove: (c) => this.classList._classes.delete(c),
            toggle: (c, force) => {
                if (force === undefined) {
                    force = !this.classList._classes.has(c);
                }
                if (force) this.classList._classes.add(c);
                else this.classList._classes.delete(c);
                return force;
            },
            contains: (c) => this.classList._classes.has(c)
        };
        this.style = {
            setProperty: vi.fn(),
            getPropertyValue: vi.fn()
        };
        this.attributes = new Map();
        this.children = [];
        this.listeners = new Map();
        this.innerHTML = '';
        this.textContent = '';
    }

    setAttribute(k, v) { this.attributes.set(k, String(v)); }
    getAttribute(k) { return this.attributes.get(k); }
    appendChild(child) {
        this.children.push(child);
        return child;
    }
    querySelector(sel) {
        if (sel === '.abd-switch__control') return this.children[1] || null;
        return null;
    }
    addEventListener(event, fn) {
        if (!this.listeners.has(event)) this.listeners.set(event, []);
        this.listeners.get(event).push(fn);
    }
    removeEventListener(event, fn) {
        if (!this.listeners.has(event)) return;
        const list = this.listeners.get(event).filter(f => f !== fn);
        this.listeners.set(event, list);
    }
    dispatchEvent(event) {
        const fns = this.listeners.get(event.type) || [];
        fns.forEach(fn => fn(event));
    }
    remove() {}
}

// Install mock global document if missing
if (typeof globalThis.document === 'undefined') {
    globalThis.document = {
        createElement: (tag) => new MockElement(tag),
        querySelector: () => new MockElement('div')
    };
}

import { ABDSwitch } from '../../src/components/shared/ABDSwitch.js';
import { paramStore } from '../../src/contracts/paramStore.js';

describe('ABDSwitch Component', () => {
    let container;

    beforeEach(() => {
        container = new MockElement('div');
    });

    it('instantiates with default options (OFF / 0)', () => {
        const sw = new ABDSwitch(container, {
            paramId: 'testSwitch',
            label: 'TEST SWITCH',
            defaultValue: 0
        });

        expect(sw.getValue()).toBe(0);
        expect(sw.switchEl.getAttribute('aria-checked')).toBe('false');
        expect(sw.valueEl.textContent).toBe('OFF');
    });

    it('sets initial value to ON / 1 when defaultValue is 1', () => {
        const sw = new ABDSwitch(container, {
            paramId: 'testSwitchOn',
            label: 'DISTORTION',
            defaultValue: 1
        });

        expect(sw.getValue()).toBe(1);
        expect(sw.switchEl.getAttribute('aria-checked')).toBe('true');
        expect(sw.valueEl.textContent).toBe('ON');
    });

    it('setValue updates state and DOM correctly', () => {
        const sw = new ABDSwitch(container, {
            paramId: 'testToggle',
            defaultValue: 0
        });

        sw.setValue(1);
        expect(sw.getValue()).toBe(1);
        expect(sw.switchEl.getAttribute('aria-checked')).toBe('true');
        expect(sw.valueEl.textContent).toBe('ON');

        sw.setValue(0);
        expect(sw.getValue()).toBe(0);
        expect(sw.switchEl.getAttribute('aria-checked')).toBe('false');
        expect(sw.valueEl.textContent).toBe('OFF');
    });

    it('setValue with notify=true calls onChange', () => {
        const onChange = vi.fn();
        const sw = new ABDSwitch(container, {
            paramId: 'testNotify',
            defaultValue: 0,
            onChange
        });

        sw.setValue(1, true);
        expect(onChange).toHaveBeenCalledTimes(1);
        expect(onChange).toHaveBeenCalledWith(1, 'testNotify');
    });

    it('click interaction toggles value and notifies', () => {
        const onChange = vi.fn();
        const sw = new ABDSwitch(container, {
            paramId: 'testClick',
            defaultValue: 0,
            onChange
        });

        // Simulate click
        sw.switchEl.dispatchEvent({ type: 'click', preventDefault: vi.fn() });
        expect(sw.getValue()).toBe(1);
        expect(onChange).toHaveBeenCalledWith(1, 'testClick');

        // Second click toggles back to 0
        sw.switchEl.dispatchEvent({ type: 'click', preventDefault: vi.fn() });
        expect(sw.getValue()).toBe(0);
        expect(onChange).toHaveBeenCalledWith(0, 'testClick');
    });

    it('destroy unregisters from paramStore and removes wrapper', () => {
        const sw = new ABDSwitch(container, {
            paramId: 'testDestroy',
            defaultValue: 0
        });

        const unregisterSpy = vi.spyOn(paramStore, 'unregister');
        sw.destroy();
        expect(unregisterSpy).toHaveBeenCalledWith('testDestroy', sw);
    });
});
