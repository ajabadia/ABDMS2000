import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { ABDSegmented } from '../../src/components/shared/ABDSegmented.js';
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
    this.value = '0';
    this.textContent = '';
    this.id = '';
  }

  setAttribute(k, v) { this.attributes.set(k, String(v)); }
  getAttribute(k) { return this.attributes.get(k); }
  removeAttribute(k) { this.attributes.delete(k); }
  appendChild(child) {
    this.children.push(child);
    return child;
  }
  remove() {}
  addEventListener() {}
  removeEventListener() {}
  querySelectorAll(sel) {
    const res = [];
    for (const c of this.children) {
      if (c.className && c.className.includes(sel.replace('.', ''))) res.push(c);
      if (c.querySelectorAll) res.push(...c.querySelectorAll(sel));
    }
    return res;
  }
}

describe('ABDSegmented Component', () => {
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

  it('applies layout class to wrapper when layout is specified', () => {
    const seg = new ABDSegmented(container, {
      paramId: 'modSeqResolution',
      label: 'TIME RESOLUTION',
      options: [
        { value: 0, label: '1/48' },
        { value: 1, label: '1/32' },
      ],
      layout: 'grid-5',
    });

    expect(seg.segmented.wrapper.classList.contains('abd-segmented--grid-5')).toBe(true);
    seg.destroy();
  });

  it('supports 1-based mapped discrete values for seqLastStep with grid-8', () => {
    const stepOptions = Array.from({ length: 16 }, (_, i) => ({
      value: i + 1,
      label: `${i + 1}`,
    }));

    paramStore.set('seqLastStep', 16);
    const bridgeSpy = vi.spyOn(bridge, 'setParam');

    const seg = new ABDSegmented(container, {
      paramId: 'seqLastStep',
      label: 'LAST STEP',
      options: stepOptions,
      layout: 'grid-8',
    });

    expect(seg.segmented.wrapper.classList.contains('abd-segmented--grid-8')).toBe(true);
    expect(seg.getValue()).toBe(16);

    // setValue to 4
    seg.setValue(4);
    expect(seg.getValue()).toBe(4);

    // handleChange click simulation on index 0 -> value 1
    seg.handleChange(0);
    expect(seg.getValue()).toBe(1);
    expect(paramStore.get('seqLastStep')).toBe(1);
    expect(bridgeSpy).toHaveBeenCalledWith('seqLastStep', 1);

    seg.destroy();
  });

  it('supports LFO sync notes with grid-6 layout', () => {
    const syncNotes = [
      { value: 0, label: '1/1' }, { value: 1, label: '3/4' }, { value: 2, label: '1/2' },
      { value: 3, label: '1/2T' }, { value: 4, label: '1/4' }, { value: 5, label: '1/4T' },
      { value: 6, label: '1/8' }, { value: 7, label: '1/8T' }, { value: 8, label: '1/16' },
      { value: 9, label: '1/16T' }, { value: 10, label: '1/32' }
    ];

    const seg = new ABDSegmented(container, {
      paramId: 'lfo1SyncNote',
      label: 'SYNC NOTE DIVISION',
      options: syncNotes,
      layout: 'grid-6',
    });

    expect(seg.segmented.wrapper.classList.contains('abd-segmented--grid-6')).toBe(true);
    seg.setValue(4); // '1/4'
    expect(seg.getValue()).toBe(4);

    seg.destroy();
  });
});
