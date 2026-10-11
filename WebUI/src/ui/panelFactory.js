/**
 * panelFactory.js — Registry-driven auto-generated panel renderer.
 *
 * Reads PARAMETER_SPEC (from registry.gen.js) and produces HTML + event wiring
 * for any logical group of parameters. Eliminates all hand-coded param HTML.
 *
 * Widget mapping from registry types:
 *   boolean  → toggle checkbox
 *   choice   → <select> dropdown
 *   integer  → RotaryKnob (or <select> if max-min <= 10 and has choices array)
 *   continuous → RotaryKnob
 *
 * Layout modes:
 *   "knobs"      — renders params as a row of RotaryKnobs (default for integer/continuous)
 *   "selects"    — renders params as stacked select rows (default for choice)
 *   "mixed"      — selects on left, knobs on right (2-col)
 *   "toggle+knobs" — boolean toggles on top, then knobs row
 *   "2col"       — splits params evenly into two submodule-boxes
 *   "custom"     — user-supplied renderFn handles HTML, factory only wires events
 */

import { ABDKnob, ABDSelect, ABDToggle, ABDFilmstripFader, ABDSegmented, ABDVerticalSlider } from '../components/shared/index.js';
import { PARAM_LOOKUP } from '../contracts/registry.gen.js';
import { paramStore } from '../contracts/paramStore.js';
import { openWavetableBrowser, syncCatalogFromEngine } from './wavetableBrowser.js';


// ─── HTML Helpers ────────────────────────────────────────────────────────────

function esc(str) {
  return String(str).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function selectHtml(param, extraClass = '') {
  if (param.id === 'seqLastStep' || param.id === 't2SeqLastStep') {
    return `<div id="select-${param.id}" class="select-row"></div>`;
  }
  if (!param.choices || param.choices.length === 0) return '';
  return `<div id="select-${param.id}" class="select-row"></div>`;
}

function toggleHtml(param) {
  return `<div class="toggle-row">
    <label class="toggle-label">
      <input type="checkbox" id="param-${param.id}" class="ui-checkbox"${param.default ? ' checked' : ''}>
      <span class="korg-toggle-btn">${esc(param.name).toUpperCase()}</span>
    </label>
  </div>`;
}

function segmentedHtml(param) {
  return `<div id="segmented-${param.id}" class="segmented-row" style="margin: 4px 0; width: 100%;"></div>`;
}

function knobContainerHtml(param) {
  return `<div id="knob-${param.id}"></div>`;
}

function submoduleStart(title) {
  return `<div class="submodule-box"><div class="submodule-title">${esc(title)}</div>`;
}
function submoduleEnd() {
  return `</div>`;
}

// ─── Widget Wiring ──────────────────────────────────────────────────────────

function wireCheckbox(container, paramId, bridge) {
  const el = container.querySelector(`#param-${paramId}`);
  if (!el) return;

  new ABDToggle(el.parentElement, {
    paramId,
    onChange: (val) => bridge.setParam(paramId, val),
  });
}

function wireSelect(container, paramId, bridge, parser = parseInt) {
  const el = container.querySelector(`#select-${paramId}`);
  if (!el) return;

  if (paramId === 'seqLastStep' || paramId === 't2SeqLastStep') {
    new ABDSelect(el, {
      paramId,
      label: 'LAST STEP',
      options: Array.from({ length: 16 }, (_, i) => ({
        value: i + 1,
        label: `${i + 1} STEPS`
      })),
    });
    return;
  }

  const param = PARAM_LOOKUP[paramId];
  if (!param || !param.choices) return;

  new ABDSelect(el, {
    paramId,
    label: param.name,
    options: param.choices,
    onChange: (val) => bridge.setParam(paramId, parser(val, 10)),
  });
}

const SEGMENTED_PARAM_IDS = new Set([
  'arpRange',
  'arpResolution',
  'arpType',
  'modFxType',
  'delayType',
  'osc2Wave',
  'osc2ModType',
  'filterType',
  'voiceMode',
  'lfo1Wave',
  'lfo2Wave',
  'lfo1KeySync',
  'lfo2KeySync',
  'modSeqType',
  'vocoderCarrierSrc',
  'vocoderFormantShift',
]);

function isSegmented(paramId) {
  return SEGMENTED_PARAM_IDS.has(paramId);
}

function wireSegmented(container, paramId, bridge) {
  const el = container.querySelector(`#segmented-${paramId}`);
  if (!el) return;

  if (paramId === 'arpRange') {
    new ABDSegmented(el, {
      paramId: 'arpRange',
      label: 'RANGE',
      options: [
        { value: 1, label: '1 OCT' },
        { value: 2, label: '2 OCT' },
        { value: 3, label: '3 OCT' },
        { value: 4, label: '4 OCT' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('arpRange', val),
    });
  } else if (paramId === 'arpResolution') {
    new ABDSegmented(el, {
      paramId: 'arpResolution',
      label: 'RESOLUTION',
      options: [
        { value: 2, label: '1/24' },
        { value: 3, label: '1/16' },
        { value: 4, label: '1/12' },
        { value: 5, label: '1/8' },
        { value: 6, label: '1/6' },
        { value: 7, label: '1/4' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('arpResolution', val),
    });
  } else if (paramId === 'arpType') {
    new ABDSegmented(el, {
      paramId: 'arpType',
      label: 'TYPE',
      options: [
        { value: 0, label: 'UP' },
        { value: 1, label: 'DOWN' },
        { value: 2, label: 'ALT 1' },
        { value: 3, label: 'ALT 2' },
        { value: 4, label: 'RANDOM' },
        { value: 5, label: 'TRIGGER' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('arpType', val),
    });
  } else if (paramId === 'modFxType') {
    new ABDSegmented(el, {
      paramId: 'modFxType',
      label: 'TYPE',
      options: [
        { value: 0, label: 'CHORUS' },
        { value: 1, label: 'ENSEMBLE' },
        { value: 2, label: 'PHASER' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('modFxType', val),
    });
  } else if (paramId === 'delayType') {
    new ABDSegmented(el, {
      paramId: 'delayType',
      label: 'TYPE',
      options: [
        { value: 0, label: 'STEREO' },
        { value: 1, label: 'CROSS' },
        { value: 2, label: 'L / R' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('delayType', val),
    });
  } else if (paramId === 'osc2Wave') {
    new ABDSegmented(el, {
      paramId: 'osc2Wave',
      label: 'WAVEFORM',
      options: [
        { value: 0, label: 'SAW' },
        { value: 1, label: 'SQUARE' },
        { value: 2, label: 'TRIANGLE' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('osc2Wave', val),
    });
  } else if (paramId === 'osc2ModType') {
    new ABDSegmented(el, {
      paramId: 'osc2ModType',
      label: 'MODULATION',
      options: [
        { value: 0, label: 'OFF' },
        { value: 1, label: 'RING' },
        { value: 2, label: 'SYNC' },
        { value: 3, label: 'CROSS' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('osc2ModType', val),
    });
  } else if (paramId === 'filterType') {
    new ABDSegmented(el, {
      paramId: 'filterType',
      label: 'FILTER TYPE',
      options: [
        { value: 0, label: 'LPF 24dB' },
        { value: 1, label: 'LPF 12dB' },
        { value: 2, label: 'BPF 12dB' },
        { value: 3, label: 'HPF 12dB' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('filterType', val),
    });
  } else if (paramId === 'voiceMode') {
    new ABDSegmented(el, {
      paramId: 'voiceMode',
      label: 'VOICE ASSIGN',
      options: [
        { value: 0, label: 'MONO' },
        { value: 1, label: 'POLY' },
        { value: 2, label: 'UNISON' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('voiceMode', val),
    });
  } else if (paramId === 'lfo1Wave') {
    new ABDSegmented(el, {
      paramId: 'lfo1Wave',
      label: 'WAVEFORM',
      options: [
        { value: 0, label: 'SAW' },
        { value: 1, label: 'SQUARE' },
        { value: 2, label: 'TRIANGLE' },
        { value: 3, label: 'S&H' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('lfo1Wave', val),
    });
  } else if (paramId === 'lfo2Wave') {
    new ABDSegmented(el, {
      paramId: 'lfo2Wave',
      label: 'WAVEFORM',
      options: [
        { value: 0, label: 'SAW' },
        { value: 1, label: 'SQUARE' },
        { value: 2, label: 'SINE' },
        { value: 3, label: 'S&H' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('lfo2Wave', val),
    });
  } else if (paramId === 'lfo1KeySync' || paramId === 'lfo2KeySync') {
    new ABDSegmented(el, {
      paramId,
      label: 'KEY SYNC',
      options: [
        { value: 0, label: 'OFF' },
        { value: 1, label: 'TIMBRE' },
        { value: 2, label: 'VOICE' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam(paramId, val),
    });
  } else if (paramId === 'modSeqType') {
    new ABDSegmented(el, {
      paramId: 'modSeqType',
      label: 'SEQ TYPE',
      options: [
        { value: 0, label: 'FORWARD' },
        { value: 1, label: 'REVERSE' },
        { value: 2, label: 'BOUNCE' },
        { value: 3, label: 'RANDOM' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('modSeqType', val),
    });
  } else if (paramId === 'vocoderCarrierSrc') {
    new ABDSegmented(el, {
      paramId: 'vocoderCarrierSrc',
      label: 'CARRIER',
      options: [
        { value: 0, label: 'SYNTH' },
        { value: 1, label: 'EXT AUDIO' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('vocoderCarrierSrc', val),
    });
  } else if (paramId === 'vocoderFormantShift') {
    new ABDSegmented(el, {
      paramId: 'vocoderFormantShift',
      label: 'FORMANT SHIFT',
      options: [
        { value: 0, label: '-2' },
        { value: 1, label: '-1' },
        { value: 2, label: '0' },
        { value: 3, label: '+1' },
        { value: 4, label: '+2' },
      ],
      variant: 'strip',
      size: 'sm',
      onChange: (val) => bridge.setParam('vocoderFormantShift', val),
    });
  }
}

function createKnob(container, param, bridge, opts = {}) {
  const el = container.querySelector(`#knob-${param.id}`);
  if (!el) return null;

  const isBipolar = param.min < 0;
  const defaultKnob = isBipolar
    ? Math.round((param.max + param.min) / 2)
    : param.default;

  return new ABDKnob(el, {
    paramId: param.id,
    label: opts.label || param.name.split(' ').pop().toUpperCase(),
    min: param.min,
    max: param.max,
    defaultValue: defaultKnob,
    isBipolar,
    step: param.step || 1,
    onChange: (val) => bridge.setParam(param.id, Math.round(val)),
  });
}

function createFilmstripFader(container, param, bridge, opts = {}) {
  const el = container.querySelector(`#fader-${param.id}`);
  if (!el) return null;

  const isVertical = opts.orientation !== 'horizontal';
  const spriteUrl = opts.spriteUrl || (isVertical
    ? 'assets/ST_Fader_58x107_128.png'
    : 'assets/ST_Fader_230x69_128f.png');
  const frameWidth = opts.frameWidth || (isVertical ? 58 : 230);
  const frameHeight = opts.frameHeight || (isVertical ? 107 : 69);
  const frames = opts.frames || 128;
  const viewportWidth = opts.viewportWidth || (isVertical ? 36 : 140);
  const viewportHeight = opts.viewportHeight || (isVertical ? 80 : 32);

  return new ABDFilmstripFader(el, {
    paramId: param.id,
    label: opts.label || param.name.split(' ').pop().toUpperCase(),
    min: param.min,
    max: param.max,
    defaultValue: param.default,
    orientation: isVertical ? 'vertical' : 'horizontal',
    spriteUrl,
    frameWidth,
    frameHeight,
    frames,
    viewportWidth,
    viewportHeight,
    onChange: (val) => bridge.setParam(param.id, Math.round(val)),
  });
}

// ─── Layout Renderers ────────────────────────────────────────────────────────

/**
 * Standard layout: renders all params as selects + knobs inside a panel-card.
 * Layout is auto-determined from param types.
 */
function renderStandardLayout(container, title, badge, params, bridge) {
  const booleans = params.filter(p => p.type === 'boolean');
  const choices = params.filter(p => p.type === 'choice');
  const knobs = params.filter(p => p.type === 'integer' || p.type === 'continuous');

  let html = `<div class="panel-card">
    <div class="panel-header">
      <span class="panel-title">${esc(title)}</span>
      ${badge ? `<span class="panel-badge">${esc(badge)}</span>` : ''}
    </div>`;

  // Booleans as toggles
  if (booleans.length > 0) {
    html += `<div style="display:flex;flex-wrap:wrap;gap:8px;margin-bottom:8px;">`;
    booleans.forEach(p => { html += toggleHtml(p); });
    html += `</div>`;
  }

  // Layout depends on what we have
  if (choices.length > 0 && knobs.length > 0) {
    // 2-col: selects left, knobs right
    html += `<div class="panel-grid-2col">`;
    html += `<div class="submodule-box">`;
    choices.forEach(p => { html += selectHtml(p); });
    html += submoduleEnd();
    html += `<div class="submodule-box"><div class="knobs-row">`;
    knobs.forEach(p => { html += knobContainerHtml(p); });
    html += `</div></div>`;
    html += `</div>`;
  } else if (choices.length > 0) {
    // All selects
    choices.forEach(p => { html += selectHtml(p); });
  } else {
    // All knobs
    html += `<div class="knobs-row">`;
    knobs.forEach(p => { html += knobContainerHtml(p); });
    html += `</div>`;
  }

  html += `</div>`;
  container.innerHTML = html;

  // Wire events
  booleans.forEach(p => wireCheckbox(container, p.id, bridge));
  choices.forEach(p => wireSelect(container, p.id, bridge));
  knobs.forEach(p => createKnob(container, p, bridge));
}

/**
 * 2-column layout: splits params into left/right submodule boxes.
 * Used for dual-submodule panels (e.g., OSC1+OSC2, LFO1+LFO2, EG1+EG2).
 */
function render2ColLayout(container, title, badge, leftParams, rightParams, leftTitle, rightTitle, bridge) {
  let html = `<div class="panel-card">
    <div class="panel-header">
      <span class="panel-title">${esc(title)}</span>
      ${badge ? `<span class="panel-badge">${esc(badge)}</span>` : ''}
    </div>
    <div class="panel-grid-2col">`;

  // Left column
  html += submoduleStart(leftTitle);
  leftParams.forEach(p => {
    if (p.type === 'boolean') html += toggleHtml(p);
    else if (isSegmented(p.id)) html += segmentedHtml(p);
    else if (p.type === 'choice') html += selectHtml(p);
    else html += knobContainerHtml(p);
  });
  html += submoduleEnd();

  // Right column
  html += submoduleStart(rightTitle);
  rightParams.forEach(p => {
    if (p.type === 'boolean') html += toggleHtml(p);
    else if (isSegmented(p.id)) html += segmentedHtml(p);
    else if (p.type === 'choice') html += selectHtml(p);
    else html += knobContainerHtml(p);
  });
  html += submoduleEnd();

  html += `</div></div>`;
  container.innerHTML = html;

  // Wire
  [...leftParams, ...rightParams].forEach(p => {
    if (p.type === 'boolean') wireCheckbox(container, p.id, bridge);
    else if (isSegmented(p.id)) wireSegmented(container, p.id, bridge);
    else if (p.type === 'choice') wireSelect(container, p.id, bridge);
    else createKnob(container, p, bridge);
  });
}

// ─── Panel Definitions ───────────────────────────────────────────────────────
// Each definition maps a DOM container ID to a layout configuration.
// `params` is either:
//   - an array of param IDs (looked up from registry)
//   - a function(paramsByGroup) returning an array of param IDs
//   - "ALL" (all params in the group)

const PANEL_DEFS = [
  // ── OSC1 + OSC2 ──────────────────────────────────────────────────────
  {
    containerId: 'panel-container-osc',
    title: 'OSCILLATORS',
    badge: 'VA / DWGS / VoxWave',
    layout: '2col',
    leftTitle: 'OSC 1',
    rightTitle: 'OSC 2',
    left: ['osc1Wave', 'osc1DwgsWave', 'osc1Ctrl1', 'osc1Ctrl2'],
    right: ['osc2Wave', 'osc2ModType', 'osc2Semitone', 'osc2Tune'],
    mixerGroup: true,
  },

  // ── Filter + Mixer ────────────────────────────────────────────────────
  {
    containerId: 'panel-container-filter',
    title: 'FILTER & MIXER',
    badge: 'Multi-Mode TPT/ZDF',
    layout: '2col',
    leftTitle: 'FILTER',
    rightTitle: 'MIXER',
    left: ['filterType', 'filterCutoff', 'filterResonance', 'filterEg1Int', 'filterKeyTrack'],
    right: ['mixOsc1Level', 'mixOsc2Level', 'mixNoiseLevel'],
  },

  // ── Amp + Voice ───────────────────────────────────────────────────────
  {
    containerId: 'panel-container-amp',
    title: 'AMPLIFIER & VOICE CONTROL',
    badge: 'VCA / OVERDRIVE',
    layout: '2col',
    leftTitle: 'AMP & PAN',
    rightTitle: 'VOICE ASSIGN & GLIDE',
    left: ['ampLevel', 'ampPan', 'ampKeyTrack', 'ampDistortion'],
    right: ['voiceMode', 'portamentoOn', 'portamentoTime'],
  },

  // ── EG1 + EG2 ────────────────────────────────────────────────────────
  {
    containerId: 'panel-container-envelopes',
    title: 'ENVELOPE GENERATORS',
    badge: 'EG1 (Filter) / EG2 (Amp)',
    layout: '2col',
    leftTitle: 'EG1 — FILTER',
    rightTitle: 'EG2 — AMP',
    left: ['eg1Attack', 'eg1Decay', 'eg1Sustain', 'eg1Release'],
    right: ['eg2Attack', 'eg2Decay', 'eg2Sustain', 'eg2Release'],
  },

  // ── LFO1 + LFO2 ──────────────────────────────────────────────────────
  {
    containerId: 'panel-container-lfo',
    title: 'LOW FREQUENCY OSCILLATORS',
    badge: 'LFO 1 & 2 (TEMPO SYNC)',
    layout: '2col',
    leftTitle: 'LFO 1',
    rightTitle: 'LFO 2',
    left: ['lfo1Wave', 'lfo1KeySync', 'lfo1TempoSync', 'lfo1SyncNote', 'lfo1Freq'],
    right: ['lfo2Wave', 'lfo2KeySync', 'lfo2TempoSync', 'lfo2SyncNote', 'lfo2Freq'],
    conditionalVisibility: [
      { toggleId: 'lfo1TempoSync', targetId: 'lfo1SyncNote', parentCol: 'left' },
      { toggleId: 'lfo2TempoSync', targetId: 'lfo2SyncNote', parentCol: 'right' },
    ],
  },

  // ── Virtual Patch ─────────────────────────────────────────────────────
  {
    containerId: 'panel-container-modpatch',
    title: 'VIRTUAL PATCH MATRIX',
    badge: '4 SLOTS × 3 PARAMS',
    layout: 'patchMatrix',
  },

  // ── Arpeggiator ───────────────────────────────────────────────────────
  {
    containerId: 'panel-container-arp',
    title: 'ARPEGGIATOR',
    badge: 'TEMPO SYNC',
    layout: '2col',
    leftTitle: 'ARP',
    rightTitle: 'RANGE & RESOLUTION',
    left: ['arpOn', 'arpLatch', 'arpType'],
    right: ['arpRange', 'arpResolution', 'arpGate'],
  },

  // ── Mod Sequencer ─────────────────────────────────────────────────────
  {
    containerId: 'panel-container-modseq',
    title: 'MODULATION SEQUENCER',
    badge: '3 TRACKS × 16 STEPS',
    layout: 'modSequencer',
  },

  // ── FX ────────────────────────────────────────────────────────────────
  {
    containerId: 'panel-container-fx',
    title: 'EFFECTS PROCESSOR',
    badge: 'MOD FX / DELAY / EQ',
    layout: '3col',
    sections: [
      { title: 'MOD FX', params: ['modFxOn', 'modFxType', 'modFxSpeed', 'modFxDepth', 'modFxFeedback'] },
      { title: 'DELAY FX', params: ['delayOn', 'delayType', 'delayTime', 'delayDepth', 'delayFeedback'] },
      { title: 'MASTER EQ', params: ['eqLowFreq', 'eqLowGain', 'eqHighFreq', 'eqHighGain'] },
    ],
  },

  // ── Vocoder ───────────────────────────────────────────────────────────
  {
    containerId: 'panel-container-vocoder',
    title: '16-BAND DIGITAL VOCODER',
    badge: 'SIBILANCE & FORMANT SHIFT',
    layout: 'vocoder',
  },
];

// ─── Public API ──────────────────────────────────────────────────────────────

/**
 * Render a standard group of params into a container.
 * @param {HTMLElement} container
 * @param {string} title
 * @param {string|null} badge
 * @param {string[]} paramIds - list of param IDs from registry
 * @param {object} bridge
 */
export function renderGroupPanel(container, title, badge, paramIds, bridge) {
  const params = paramIds.map(id => PARAM_LOOKUP[id]).filter(Boolean);
  renderStandardLayout(container, title, badge, params, bridge);
}

/**
 * Render all panels from PANEL_DEFS into the DOM.
 * @param {object} bridge
 */
export function renderAllPanels(bridge) {
  PANEL_DEFS.forEach(def => {
    const container = document.getElementById(def.containerId);
    if (!container) return;

    switch (def.layout) {
      case '2col':
        render2ColPanel(container, def, bridge);
        break;
      case '3col':
        render3ColPanel(container, def, bridge);
        break;
      case 'patchMatrix':
        renderPatchMatrix(container, def, bridge);
        break;
      case 'modSequencer':
        renderModSequencer(container, def, bridge);
        break;
      case 'vocoder':
        renderVocoder(container, def, bridge);
        break;
      default:
        renderStandardLayout(container, def.title, def.badge || '', def.params || [], bridge);
    }
  });
}

// ─── Special Panel Renderers ─────────────────────────────────────────────────

function render2ColPanel(container, def, bridge) {
  const leftParams = def.left.map(id => PARAM_LOOKUP[id]).filter(Boolean);
  const rightParams = def.right.map(id => PARAM_LOOKUP[id]).filter(Boolean);

  render2ColLayout(container, def.title, def.badge, leftParams, rightParams, def.leftTitle, def.rightTitle, bridge);

// Mixer knobs in OSC panel
    if (def.mixerGroup) {
      const mixParams = ['mixOsc1Level', 'mixOsc2Level', 'mixNoiseLevel'].map(id => PARAM_LOOKUP[id]).filter(Boolean);
      const rightBox = container.querySelectorAll('.submodule-box')[1];
      if (rightBox) {
        const fadersRow = document.createElement('div');
        fadersRow.className = 'faders-row';
        fadersRow.style.cssText = 'display:flex;flex-direction:column;gap:12px;margin-top:8px;';
        rightBox.appendChild(fadersRow);
        mixParams.forEach(p => {
          const wrapper = document.createElement('div');
          wrapper.id = `fader-${p.id}`;
          fadersRow.appendChild(wrapper);
          createFilmstripFader(container, p, bridge, { orientation: 'vertical' });
        });
      }

    // Wavetable Browse button next to osc1DwgsWave knob
    const dwgsKnobEl = container.querySelector('#knob-osc1DwgsWave');
    if (dwgsKnobEl) {
      const browseBtn = document.createElement('button');
      browseBtn.className = 'wt-browse-btn';
      browseBtn.textContent = 'BROWSE';
      browseBtn.title = 'Open Wavetable Browser';
      dwgsKnobEl.appendChild(browseBtn);
      browseBtn.addEventListener('click', () => {
        const synthMode = Math.round(paramStore.values.get('synthMode') || 0);
        const currentSlot = Math.max(0, Math.round(paramStore.values.get('osc1DwgsWave') || 1) - 1);
        openWavetableBrowser(bridge, currentSlot, (slot) => {
          const val1Based = slot + 1;
          paramStore.setParamValue('osc1DwgsWave', val1Based);
          bridge.setParam('osc1DwgsWave', val1Based);
        }, synthMode);
      });
    }

    // Dynamic DWGS state: only enabled when OSC1 waveform is DWGS (index 5)
    const updateDwgsState = () => {
      const osc1Select = container.querySelector('#param-osc1Wave');
      const waveVal = osc1Select ? parseInt(osc1Select.value, 10) : Math.round(paramStore.values.get('osc1Wave') || 0);
      const isDwgs = waveVal === 5; // DWGS index in choices ["Saw","Square","Triangle","Sine","VoxWave","DWGS","Noise","AudioIn"]

      const dwgsKnob = container.querySelector('#knob-osc1DwgsWave');
      const browseBtn = container.querySelector('.wt-browse-btn');

      if (dwgsKnob) {
        dwgsKnob.style.opacity = isDwgs ? '1.0' : '0.35';
        dwgsKnob.style.pointerEvents = isDwgs ? 'auto' : 'none';
        dwgsKnob.title = isDwgs ? 'Select DWGS Waveform' : 'DWGS Waveform (Select DWGS in Waveform dropdown to enable)';
      }
      if (browseBtn) {
        browseBtn.disabled = !isDwgs;
        browseBtn.style.opacity = isDwgs ? '1.0' : '0.35';
        browseBtn.style.pointerEvents = isDwgs ? 'auto' : 'none';
      }
    };

    const osc1WaveSelect = container.querySelector('#param-osc1Wave');
    if (osc1WaveSelect) {
      osc1WaveSelect.addEventListener('change', updateDwgsState);
    }
    paramStore.onChange('osc1Wave', updateDwgsState);

    // Profile mode listener: clamp DWGS slots to 64 when not in Advanced mode
    paramStore.onChange('synthMode', (mode) => {
      if (mode !== 2) {
        const currentDwgs = Math.round(paramStore.values.get('osc1DwgsWave') || 1);
        if (currentDwgs > 64) {
          paramStore.setParamValue('osc1DwgsWave', 64);
          bridge.setParam('osc1DwgsWave', 64);
        }
      }
    });

    updateDwgsState();

    // Sync catalog from engine on first render
    syncCatalogFromEngine(bridge);
  }


  // Conditional visibility for LFO tempo sync
  if (def.conditionalVisibility) {
    def.conditionalVisibility.forEach(({ toggleId, targetId }) => {
      const toggle = container.querySelector(`#param-${toggleId}`);
      const targetRow = container.querySelector(`#param-${targetId}`)?.closest('.select-row');
      if (toggle && targetRow) {
        targetRow.style.display = toggle.checked ? 'flex' : 'none';
        toggle.addEventListener('change', (e) => {
          targetRow.style.display = e.target.checked ? 'flex' : 'none';
        });
      }
    });
  }
}

function render3ColPanel(container, def, bridge) {
  let html = `<div class="panel-card">
    <div class="panel-header">
      <span class="panel-title">${esc(def.title)}</span>
      ${def.badge ? `<span class="panel-badge">${esc(def.badge)}</span>` : ''}
    </div>
    <div class="panel-grid-2col" style="grid-template-columns: repeat(3, 1fr);">`;

  def.sections.forEach(section => {
    const params = section.params.map(id => PARAM_LOOKUP[id]).filter(Boolean);
    html += submoduleStart(section.title);
    params.forEach(p => {
      if (p.type === 'boolean') html += toggleHtml(p);
      else if (isSegmented(p.id)) html += segmentedHtml(p);
      else if (p.type === 'choice') html += selectHtml(p);
      else html += knobContainerHtml(p);
    });
    html += submoduleEnd();
  });

  html += `</div></div>`;
  container.innerHTML = html;

  // Wire everything
  def.sections.forEach(section => {
    section.params.forEach(id => {
      const p = PARAM_LOOKUP[id];
      if (!p) return;
      if (p.type === 'boolean') wireCheckbox(container, p.id, bridge);
      else if (isSegmented(p.id)) wireSegmented(container, p.id, bridge);
      else if (p.type === 'choice') wireSelect(container, p.id, bridge);
      else createKnob(container, p, bridge);
    });
  });
}

function renderPatchMatrix(container, def, bridge) {
  const patches = [1, 2, 3, 4];
  let html = `<div class="panel-card">
    <div class="panel-header">
      <span class="panel-title">${esc(def.title)}</span>
      ${def.badge ? `<span class="panel-badge">${esc(def.badge)}</span>` : ''}
    </div>
    <div class="panel-grid-2col">`;

  patches.forEach(n => {
    const src = PARAM_LOOKUP[`patch${n}Source`];
    const dest = PARAM_LOOKUP[`patch${n}Destination`];
    const int = PARAM_LOOKUP[`patch${n}Intensity`];
    html += submoduleStart(`SLOT ${n}`);
    if (src) html += selectHtml(src);
    if (dest) html += selectHtml(dest);
    html += knobContainerHtml(int || { id: `patch${n}Intensity`, name: 'Intensity', min: -63, max: 63, default: 0, type: 'integer' });
    html += submoduleEnd();
  });

  html += `</div></div>`;
  container.innerHTML = html;

  // Wire
  patches.forEach(n => {
    wireSelect(container, `patch${n}Source`, bridge);
    wireSelect(container, `patch${n}Destination`, bridge);
    const intParam = PARAM_LOOKUP[`patch${n}Intensity`];
    if (intParam) createKnob(container, intParam, bridge);
  });
}

function renderModSequencer(container, def, bridge) {
  // El mod sequence es por timbre y cada fila tiene su destino y su transición
  // (bytes 52..107 del bloque de 108 B); el panel enseña aquí las tres filas del Timbre 1.
  const modSeqParams = ['modSeqOn', 'modSeqType', 'modSeqResolution', 'seqLastStep',
                        'seq1Dest', 'seq1Motion', 'seq2Dest', 'seq2Motion', 'seq3Dest', 'seq3Motion'];

  let stepsHtml = '';
  for (let s = 1; s <= 16; ++s) {
    stepsHtml += `
      <div class="seq-step-item" data-step="${s}">
        <div class="seq-step-led" id="seq-led-${s}"></div>
        <button class="seq-step-btn" id="seq-btn-${s}">${s}</button>
      </div>`;
  }

  let html = `<div class="panel-card modseq-panel">
    <div class="panel-header">
      <span class="panel-title">${esc(def.title)}</span>
      ${def.badge ? `<span class="panel-badge">${esc(def.badge)}</span>` : ''}
    </div>
    <div class="seq-header-controls">`;

  modSeqParams.forEach(id => {
    const p = PARAM_LOOKUP[id];
    if (!p) return;
    if (p.type === 'boolean') html += toggleHtml(p);
    else if (isSegmented(p.id)) html += segmentedHtml(p);
    else if (p.type === 'choice' || p.id === 'seqLastStep' || p.id === 't2SeqLastStep') html += selectHtml(p);
    else html += knobContainerHtml(p);
  });

  html += `</div><div class="seq-steps-array">${stepsHtml}</div></div>`;
  container.innerHTML = html;

  // Wire params
  modSeqParams.forEach(id => {
    const p = PARAM_LOOKUP[id];
    if (!p) return;
    if (p.type === 'boolean') wireCheckbox(container, p.id, bridge);
    else if (isSegmented(p.id)) wireSegmented(container, p.id, bridge);
    else if (p.type === 'choice' || p.id === 'seqLastStep' || p.id === 't2SeqLastStep') wireSelect(container, p.id, bridge);
    else createKnob(container, p, bridge);
  });

  // Step buttons
  for (let s = 1; s <= 16; ++s) {
    const btn = container.querySelector(`#seq-btn-${s}`);
    if (btn) btn.addEventListener('click', () => btn.classList.toggle('active'));
  }

  // Playhead update hook
  window.updateSeqPlayhead = (stepIndex) => {
    for (let s = 1; s <= 16; ++s) {
      const led = container.querySelector(`#seq-led-${s}`);
      if (led) led.classList.toggle('active', s === stepIndex + 1);
    }
  };
}

function renderVocoder(container, def, bridge) {
  const vocoderParams = ['synthVocoderMode', 'vocoderCarrierSrc', 'vocoderFormantShift', 'vocoderHpfLevel', 'vocoderGateSense', 'vocoderDirectLevel'];
  const bandParams = [];
  for (let i = 1; i <= 16; i++) {
    bandParams.push(PARAM_LOOKUP[`vocoderBandLevel${i}`]);
  }

  const bandFreqs = ['125', '180', '250', '350', '500', '700', '1k', '1.4k', '2k', '2.8k', '3.6k', '4.2k', '4.8k', '5.2k', '5.5k', '5.7k'];

  let bandFadersHtml = '';
  for (let i = 0; i < 16; ++i) {
    bandFadersHtml += `
      <div class="voc-band-col">
        <span class="voc-band-num">${i + 1}</span>
        <div id="fader-vocoderBandLevel${i + 1}"></div>
        <span class="voc-band-freq">${bandFreqs[i]}</span>
      </div>`;
  }

  let html = `<div class="panel-card vocoder-panel">
    <div class="panel-header">
      <span class="panel-title">${esc(def.title)}</span>
      ${def.badge ? `<span class="panel-badge">${esc(def.badge)}</span>` : ''}
    </div>
    <div class="vocoder-controls-top">`;

  vocoderParams.forEach(id => {
    const p = PARAM_LOOKUP[id];
    if (!p) return;
    if (p.type === 'boolean') html += toggleHtml(p);
    else if (isSegmented(p.id)) html += segmentedHtml(p);
    else if (p.type === 'choice') html += selectHtml(p);
    else html += knobContainerHtml(p);
  });

  html += `</div><div class="voc-bands-container">${bandFadersHtml}</div></div>`;
  container.innerHTML = html;

  // Wire params
  vocoderParams.forEach(id => {
    const p = PARAM_LOOKUP[id];
    if (!p) return;
    if (p.type === 'boolean') wireCheckbox(container, p.id, bridge);
    else if (isSegmented(p.id)) wireSegmented(container, p.id, bridge);
    else if (p.type === 'choice') wireSelect(container, p.id, bridge);
    else createKnob(container, p, bridge);
  });

  // Band faders (vertical vector sliders)
  for (let i = 1; i <= 16; ++i) {
    const p = PARAM_LOOKUP[`vocoderBandLevel${i}`];
    if (p) {
      const el = container.querySelector(`#fader-${p.id}`);
      if (el) {
        new ABDVerticalSlider(el, {
          paramId: p.id,
          min: p.min,
          max: p.max,
          defaultValue: p.default,
          height: 64,
          trackWidth: 4,
          thumbSize: 12,
          showLabel: false,
          showValue: false,
          onChange: (val) => bridge.setParam(p.id, Math.round(val)),
        });
      }
    }
  }
}
