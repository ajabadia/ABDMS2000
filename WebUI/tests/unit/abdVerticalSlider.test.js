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
            contains: (c) => this.classList._classes.has(c)
        };
        this.style = {};
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
    getBoundingClientRect() {
        return { top: 100, bottom: 210, height: 110, left: 10, right: 38, width: 28 };
    }
    setPointerCapture() {}
    releasePointerCapture() {}
    hasPointerCapture() { return false; }
    remove() {}
}

// Install mock global document if missing
if (typeof globalThis.document === 'undefined') {
    globalThis.document = {
        createElement: (tag) => new MockElement(tag),
        querySelector: () => new MockElement('div')
    };
}

import { ABDVerticalSlider } from '../../src/components/shared/ABDVerticalSlider.js';
import { paramStore } from '../../src/contracts/paramStore.js';

describe('ABDVerticalSlider Component', () => {
    let container;

    beforeEach(() => {
        container = new MockElement('div');
    });

    it('instantiates with default options (0..127, default 127)', () => {
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testBandLevel',
            label: 'B1',
            defaultValue: 127
        });

        expect(slider.getValue()).toBe(127);
        expect(slider.wrapper.getAttribute('role')).toBe('slider');
        expect(slider.wrapper.getAttribute('aria-orientation')).toBe('vertical');
        expect(slider.wrapper.getAttribute('aria-valuenow')).toBe('127');
        expect(slider.valueEl.textContent).toBe('127');
        expect(slider.fillEl.style.height).toBe('100%');
        expect(slider.thumbEl.style.bottom).toBe('100%');
    });

    it('setValue updates visual fill, thumb position, and ARIA value', () => {
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testSliderSetVal',
            min: 0,
            max: 100,
            defaultValue: 50
        });

        expect(slider.getValue()).toBe(50);
        expect(slider.fillEl.style.height).toBe('50%');

        slider.setValue(25);
        expect(slider.getValue()).toBe(25);
        expect(slider.fillEl.style.height).toBe('25%');
        expect(slider.thumbEl.style.bottom).toBe('25%');
        expect(slider.valueEl.textContent).toBe('25');
        expect(slider.wrapper.getAttribute('aria-valuenow')).toBe('25');
    });

    it('setValue with notify=true dispatches onChange and updates paramStore', () => {
        const onChange = vi.fn();
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testSliderNotify',
            min: 0,
            max: 127,
            defaultValue: 64,
            onChange
        });

        slider.setValue(96, true);
        expect(onChange).toHaveBeenCalledTimes(1);
        expect(onChange).toHaveBeenCalledWith(96, 'testSliderNotify');
        expect(paramStore.get('testSliderNotify')).toBe(96);
    });

    it('double click resets to defaultValue', () => {
        const onChange = vi.fn();
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testSliderDblClick',
            min: 0,
            max: 127,
            defaultValue: 127,
            onChange
        });

        slider.setValue(40);
        expect(slider.getValue()).toBe(40);

        slider.trackContainer.dispatchEvent({ type: 'dblclick', preventDefault: vi.fn() });
        expect(slider.getValue()).toBe(127);
        expect(onChange).toHaveBeenCalledWith(127, 'testSliderDblClick');
    });

    it('keyboard navigation modifies value correctly', () => {
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testSliderKeys',
            min: 0,
            max: 127,
            defaultValue: 50,
            step: 1
        });

        // ArrowUp increases by 1
        slider.wrapper.dispatchEvent({ type: 'keydown', key: 'ArrowUp', preventDefault: vi.fn() });
        expect(slider.getValue()).toBe(51);

        // ArrowDown decreases by 1
        slider.wrapper.dispatchEvent({ type: 'keydown', key: 'ArrowDown', preventDefault: vi.fn() });
        expect(slider.getValue()).toBe(50);

        // Home sets to min
        slider.wrapper.dispatchEvent({ type: 'keydown', key: 'Home', preventDefault: vi.fn() });
        expect(slider.getValue()).toBe(0);

        // End sets to max
        slider.wrapper.dispatchEvent({ type: 'keydown', key: 'End', preventDefault: vi.fn() });
        expect(slider.getValue()).toBe(127);
    });

    it('destroy unregisters from paramStore and removes wrapper', () => {
        const slider = new ABDVerticalSlider(container, {
            paramId: 'testSliderDestroy',
            defaultValue: 100
        });

        const unregisterSpy = vi.spyOn(paramStore, 'unregister');
        slider.destroy();
        expect(unregisterSpy).toHaveBeenCalledWith('testSliderDestroy', slider);
    });
});
