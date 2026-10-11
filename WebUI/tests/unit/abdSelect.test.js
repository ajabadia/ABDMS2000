import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { ABDSelect } from '../../src/components/shared/ABDSelect.js';
import { paramStore } from '../../src/contracts/paramStore.js';
import { bridge } from '../../src/bridge/bridgeCore.js';

class MockElement {
  constructor(tagName = 'div') {
    this.tagName = tagName;
    this.className = '';
    this.classList = {
      _classes: new Set(),
      add: (c) => this.classList._classes.add(c),
      remove: (c) => this.classList._classes.delete(c),
      contains: (c) => this.classList._classes.has(c),
      toggle: (c, force) => {
        if (force === undefined) force = !this.classList._classes.has(c);
        if (force) this.classList._classes.add(c);
        else this.classList._classes.delete(c);
        return force;
      },
    };
    this.style = {
      setProperty: vi.fn(),
      getPropertyValue: vi.fn(),
    };
    this.dataset = {};
    this.attributes = new Map();
    this.children = [];
    this.options = [];
    this.value = '0';
    this.textContent = '';
    this.id = '';
  }

  setAttribute(k, v) { this.attributes.set(k, String(v)); }
  getAttribute(k) { return this.attributes.get(k); }
  removeAttribute(k) { this.attributes.delete(k); }
  appendChild(child) {
    this.children.push(child);
    if (this.tagName === 'select' && child.tagName === 'option') {
      this.options.push(child);
    }
    return child;
  }
  remove() {}
  addEventListener() {}
  removeEventListener() {}
}

describe('ABDSelect Component', () => {
  let container;

  beforeEach(() => {
    vi.stubGlobal('document', {
      createElement: (tag) => new MockElement(tag),
      body: new MockElement('body'),
    });
    container = new MockElement('div');
  });

  afterEach(() => {
    vi.unstubAllGlobals();
    vi.restoreAllMocks();
  });

  it('works with standard zero-indexed string options', () => {
    const select = new ABDSelect(container, {
      paramId: 'osc2Wave',
      label: 'WAVE',
      options: ['SAW', 'SQUARE', 'TRIANGLE'],
    });

    expect(select.getValue()).toBe(0);
    select.setValue(2);
    expect(select.getValue()).toBe(2);
    select.destroy();
  });

  it('correctly maps 1-based discrete options ({ value, label }) for seqLastStep', () => {
    const stepOptions = Array.from({ length: 16 }, (_, i) => ({
      value: i + 1,
      label: `${i + 1} STEPS`,
    }));

    paramStore.set('seqLastStep', 16);

    const bridgeSpy = vi.spyOn(bridge, 'setParam');

    const select = new ABDSelect(container, {
      paramId: 'seqLastStep',
      label: 'LAST STEP',
      options: stepOptions,
    });

    // Should read initial value 16 and map to internal select index 15
    expect(select.getValue()).toBe(16);

    // setValue to 8 (index 7)
    select.setValue(8);
    expect(select.getValue()).toBe(8);

    // User selection interaction via handleChange
    select.handleChange(0); // index 0 corresponds to value 1
    expect(select.getValue()).toBe(1);
    expect(paramStore.get('seqLastStep')).toBe(1);
    expect(bridgeSpy).toHaveBeenCalledWith('seqLastStep', 1);

    select.destroy();
  });

  it('clamps invalid values defensively within mapped options range', () => {
    const stepOptions = Array.from({ length: 16 }, (_, i) => ({
      value: i + 1,
      label: `${i + 1} STEPS`,
    }));

    const select = new ABDSelect(container, {
      options: stepOptions,
      value: 1,
    });

    // value outside range clamped to last or first
    expect(select.findIndexByValue(99)).toBe(15);
    expect(select.findIndexByValue(0)).toBe(0);

    select.destroy();
  });
});
