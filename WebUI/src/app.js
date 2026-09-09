import { bridge } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';

import { createScopePanel } from './panels/panelScope.js';
import { renderGroupPanel, renderAllPanels } from './ui/panelFactory.js';
import { paramStore } from './contracts/paramStore.js';
import { LcdProgrammer } from './ui/lcdProgrammer.js';
import { createKeyboard } from '@abdsynths/midi-keyb'; // Single source: ABDSharedCode/MidiKeyboard — no local fork
import '@abdsynths/midi-keyb/keyboard.css'; // Shared CSS (wheels + kbd-buttons via ABDSharedAssets)
import { slideDrawer } from './components/slideDrawer.js';
import { SliderFilmstrip, initFilmstrips } from './components/sliderFilmstrip.js';
import { RotaryKnob } from './components/rotaryKnob.js';
import { openWavetableBrowser, syncCatalogFromEngine, getWaveName } from './ui/wavetableBrowser.js';
import { SegmentedSelector, LcdDropdown, WAVE_ICONS, FILTER_ICONS } from './components/customSelectors.js';
import { diagnosticModal } from './ui/diagnosticModal.js';
import { BankManagerModal } from './components/bank/BankManagerModal.js';
import { OscilloscopeModal } from './components/OscilloscopeModal.js';

let currentTheme = 'ms2000';
let isAudioActive = false;
let lcdProgrammer = null;
let keyboardInstance = null;
let bankManagerModal = null;
let oscilloscopeModal = null;
let timbreClipboard = null;
let globalMidiChannel = 1;

document.addEventListener('DOMContentLoaded', () => {
  console.log(`[ABDMS2000 App Init] Version: ${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber})`);

  setupThemeSelector();
  setupNavbarMenus();
  setupAudioInitButton();
  setupDashboardCards();
  setupKeyboard();
  setupButtons();
  setupAboutModal();
  setupKeyboardShortcuts();
  diagnosticModal.init();
  oscilloscopeModal = new OscilloscopeModal(bridge);

  // Instantiate Universal Bank Manager Modal (Full Native ABDBankManager WebUI).
  // The iframe URL resolves automatically to the same-origin /abdbank/ copy so
  // it works inside JUCE WebView2 (https://juce.backend/...) and in the browser.
  bankManagerModal = new BankManagerModal({
    synthBridge: bridge
  });

  document.getElementById('btn-open-bank-manager')?.addEventListener('click', () => {
    bankManagerModal?.toggle();
  });

  // Sync catalog from engine
  syncCatalogFromEngine(bridge);

  // Instantiate LCD Programmer Navigation Controller
  lcdProgrammer = new LcdProgrammer(bridge);

  // Listen for full parameter state sync from C++ APVTS / SysEx
  bridge.on('syncAllParams', (data) => {
    if (data) {
      paramStore.syncAll(data);
      if (lcdProgrammer) lcdProgrammer.updateDisplay();
      console.log('[ParamStore]: Synced all UI controls with APVTS state');
    }
  });

  // Request full initial state from host
  bridge.send('requestFullState', {});
});


function setupDashboardCards() {
  // 1. OSCILLATORS & MIXER CARD BUTTONS
  document.getElementById('btn-edit-osc1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILADOR 1 (VA & DWGS / VOX)', badge: 'OSC 1', sectionId: 'osc1', render: renderOsc1Drawer });
  });
  document.getElementById('btn-edit-osc2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILADOR 2 & MODULACIÓN', badge: 'OSC 2', sectionId: 'osc2', render: renderOsc2Drawer });
  });
  document.getElementById('btn-edit-mixer')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MEZCLADOR DE AUDIO (MIXER)', badge: 'MIXER', sectionId: 'mixer', render: renderMixerDrawer });
  });

  // 2. VOCODER 16 BANDS CARD BUTTONS
  document.getElementById('btn-edit-vocoder-cfg')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'VOCODER: CONFIGURACIÓN GLOBAL', badge: 'VOCODER', sectionId: 'vocoder-cfg', render: renderVocoderConfigDrawer });
  });
  document.getElementById('btn-edit-vocoder-bands')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'VOCODER: NIVELES DE 16 BANDAS', badge: '16 BANDS', sectionId: 'vocoder-bands', render: renderVocoderBandsDrawer });
  });

  // 3. FILTER MULTIMODE CARD BUTTONS
  document.getElementById('btn-edit-filter-main')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'FILTRO MULTIMODO & RESONANCIA', badge: 'FILTRO', sectionId: 'filter-main', render: renderFilterMainDrawer });
  });
  document.getElementById('btn-edit-filter-mod')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'FILTRO: MODULACIÓN & KEY TRACK', badge: 'FILTER MOD', sectionId: 'filter-mod', render: renderFilterModDrawer });
  });

  // 4. AMP & ENVELOPES CARD BUTTONS
  document.getElementById('btn-edit-amp-main')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'AMPLIFICADOR PRINCIPAL (VCA)', badge: 'AMP VCA', sectionId: 'amp-main', render: renderAmpMainDrawer });
  });
  document.getElementById('btn-edit-eg1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ENVOLVENTE 1: FILTRO (EG1)', badge: 'EG 1', sectionId: 'eg1', render: renderEg1Drawer });
  });
  document.getElementById('btn-edit-eg2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ENVOLVENTE 2: AMPLITUD (EG2)', badge: 'EG 2', sectionId: 'eg2', render: renderEg2Drawer });
  });

  // 5. LFOS & MOD MATRIX CARD BUTTONS
  document.getElementById('btn-edit-lfo1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILADOR DE BAJA FRECUENCIA 1', badge: 'LFO 1', sectionId: 'lfo1', render: renderLfo1Drawer });
  });
  document.getElementById('btn-edit-lfo2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILADOR DE BAJA FRECUENCIA 2', badge: 'LFO 2', sectionId: 'lfo2', render: renderLfo2Drawer });
  });
  document.getElementById('btn-edit-modmatrix')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MATRIZ DE MODULACIÓN (4 SLOTS)', badge: 'V-PATCH', sectionId: 'modmatrix', render: renderModMatrixDrawer });
  });

  // 6. SEQ & ARPEGGIATOR CARD BUTTONS
  document.getElementById('btn-edit-arp')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ARPEGGIATOR PROGRAMABLE', badge: 'ARP', sectionId: 'arp', render: renderArpDrawer });
  });
  document.getElementById('btn-edit-modseq')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MODULATION SEQUENCER (3x16)', badge: 'MOD SEQ', sectionId: 'modseq', render: renderModSeqDrawer });
  });

  // 7. FX & EQUALIZER CARD BUTTONS
  document.getElementById('btn-edit-modfx')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MODULATION FX (CHORUS/PHASER)', badge: 'MOD FX', sectionId: 'modfx', render: renderModFxDrawer });
  });
  document.getElementById('btn-edit-delayfx')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'DELAY FX ESTÉREO', badge: 'DELAY', sectionId: 'delayfx', render: renderDelayFxDrawer });
  });
  document.getElementById('btn-edit-eq')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ECUALIZADOR MASTER (2 BANDAS)', badge: 'EQ MASTER', sectionId: 'eq', render: renderEqDrawer });
  });

  // 8. VOICE, PITCH & MASTER CARD BUTTONS
  document.getElementById('btn-edit-voice')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ASIGNACIÓN DE VOZ & UNISON', badge: 'VOICE', sectionId: 'voice', render: renderVoiceDrawer });
  });
  document.getElementById('btn-edit-pitch')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'PITCH BEND & PORTAMENTO GLIDE', badge: 'PITCH', sectionId: 'pitch', render: renderPitchDrawer });
  });
  document.getElementById('btn-edit-global')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MASTER VOLUME & PEDALES', badge: 'MASTER', sectionId: 'global', render: renderGlobalMasterDrawer });
  });

  setupDashboardObservers();
  console.log('[Dashboard Workspace: All 8 Cards & 19 Sub-actions Mounted]');
}

/* ─────────────────────────────────────────────────────────────
   1. OSCILLATORS & MIXER DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderOsc1Drawer(container) {
  const currentWave = paramStore.get('osc1Wave') || 0;
  const isDwgs = currentWave === 5;
  const currentDwgsSlot = paramStore.get('osc1DwgsWave') || 0;
  const dwgsName = getWaveName(currentDwgsSlot);

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>FORMA DE ONDA OSCILADOR 1</h3>
      <div id="drawer-sel-osc1-wave" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-osc1ctrl1"></div>
        <div id="drawer-ctrl-osc1ctrl2"></div>
      </div>
    </div>

    <div class="drawer-section-card" id="drawer-dwgs-section" style="display: ${isDwgs ? 'block' : 'none'}; border-color: rgba(0, 212, 255, 0.4); background: rgba(0, 212, 255, 0.04);">
        <h3 style="margin: 0; border: none; padding: 0; color: var(--color-accent); display: flex; align-items: center; gap: 6px;">
          <svg class="ui-icon" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12c2-2 4-2 6 0s4 2 6 0 4-2 6 0"/><path d="M2 17c2-2 4-2 6 0s4 2 6 0 4-2 6 0"/></svg>
          TABLAS DWGS (64 / 512)
        </h3>
        <span class="drawer-badge" id="drawer-dwgs-name">${dwgsName}</span>
      </div>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-ctrl-dwgswave"></div>
        <div style="display: flex; flex-direction: column; gap: 8px; flex: 1; max-width: 280px;">
          <button id="btn-drawer-dwgs-browser" class="card-action-btn" style="background: linear-gradient(180deg, #0284c7 0%, #0369a1 100%); color: #fff; padding: 8px; display: flex; align-items: center; justify-content: center; gap: 6px;">
            <svg class="ui-btn-icon" viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
            <span>ABRIR CATÁLOGO DWGS</span>
          </button>
          <div style="font-size: 0.72rem; color: #94a3b8;">
            Slot: <strong id="drawer-dwgs-slot-num" style="color: #38bdf8;">#${currentDwgsSlot + 1}</strong> &mdash; <span id="drawer-dwgs-desc-name">${dwgsName}</span>
          </div>
        </div>
      </div>
    </div>
  `;

  const osc1WaveSel = new SegmentedSelector(container.querySelector('#drawer-sel-osc1-wave'), {
    paramId: 'osc1Wave',
    label: 'FORMA DE ONDA',
    layout: 'grid-4',
    size: 'sm',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQR', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'TRI', iconSvg: WAVE_ICONS.triangle },
      { value: 3, label: 'SINE', iconSvg: WAVE_ICONS.sine },
      { value: 4, label: 'VOX', iconSvg: WAVE_ICONS.vox },
      { value: 5, label: 'DWGS', iconSvg: WAVE_ICONS.dwgs },
      { value: 6, label: 'NOISE', iconSvg: WAVE_ICONS.noise },
      { value: 7, label: 'AUDIO IN', iconSvg: WAVE_ICONS.audioIn }
    ],
    onChange: (val) => {
      const dwgsSec = container.querySelector('#drawer-dwgs-section');
      if (dwgsSec) dwgsSec.style.display = Number(val) === 5 ? 'block' : 'none';
    }
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-osc1ctrl1'), {
    paramId: 'osc1Ctrl1', label: 'CONTROL 1', min: 0, max: 127, defaultValue: 64, size: 52,
    onChange: (val) => bridge.setParam('osc1Ctrl1', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-osc1ctrl2'), {
    paramId: 'osc1Ctrl2', label: 'CONTROL 2', min: 0, max: 127, defaultValue: 0, size: 52,
    onChange: (val) => bridge.setParam('osc1Ctrl2', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-dwgswave'), {
    paramId: 'osc1DwgsWave', label: 'DWGS WAVE', min: 0, max: currentTheme === 'advanced' ? 511 : 63, defaultValue: 0, size: 48,
    displayFormatter: (v) => `#${Math.round(v) + 1}`,
    onChange: (val) => {
      bridge.setParam('osc1DwgsWave', val);
      const name = getWaveName(val);
      const nameBadge = container.querySelector('#drawer-dwgs-name');
      const descName = container.querySelector('#drawer-dwgs-desc-name');
      const slotNum = container.querySelector('#drawer-dwgs-slot-num');
      if (nameBadge) nameBadge.textContent = name;
      if (descName) descName.textContent = name;
      if (slotNum) slotNum.textContent = `#${val + 1}`;
    }
  });

  container.querySelector('#btn-drawer-dwgs-browser')?.addEventListener('click', () => {
    const curSlot = paramStore.get('osc1DwgsWave') || 0;
    const synthMode = paramStore.get('synthMode') ?? (currentTheme === 'advanced' ? 2 : (currentTheme === 'microkorg' ? 1 : 0));
    openWavetableBrowser(bridge, curSlot, (slot) => {
      osc1WaveSel.setValue(5, true);
      bridge.setParam('osc1DwgsWave', slot);
      paramStore.set('osc1DwgsWave', slot);

      const dwgsSec = container.querySelector('#drawer-dwgs-section');
      if (dwgsSec) dwgsSec.style.display = 'block';

      const name = getWaveName(slot);
      const nameBadge = container.querySelector('#drawer-dwgs-name');
      const descName = container.querySelector('#drawer-dwgs-desc-name');
      const slotNum = container.querySelector('#drawer-dwgs-slot-num');
      if (nameBadge) nameBadge.textContent = name;
      if (descName) descName.textContent = name;
      if (slotNum) slotNum.textContent = `#${slot + 1}`;
    }, synthMode);
  });
}

function renderOsc2Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>OSCILADOR 2 &amp; MODULACIÓN</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 10px;">
        <div id="drawer-sel-osc2-wave"></div>
        <div id="drawer-sel-osc2-mod"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>AFINACIÓN Y TONALIDAD OSC 2</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-osc2semi"></div>
        <div id="drawer-ctrl-osc2tune"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-osc2-wave'), {
    paramId: 'osc2Wave',
    label: 'FORMA DE ONDA OSC 2',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQUARE', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'TRIANGLE', iconSvg: WAVE_ICONS.triangle }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-osc2-mod'), {
    paramId: 'osc2ModType',
    label: 'TIPO DE MODULACIÓN',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'RING MOD' },
      { value: 2, label: 'SYNC' },
      { value: 3, label: 'CROSS MOD' }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-osc2semi'), {
    paramId: 'osc2Semitone', label: 'SEMITONO', min: -24, max: 24, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} st`,
    onChange: (val) => bridge.setParam('osc2Semitone', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-osc2tune'), {
    paramId: 'osc2Tune', label: 'FINE TUNE', min: -50, max: 50, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} cents`,
    onChange: (val) => bridge.setParam('osc2Tune', val)
  });
}

function renderMixerDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MEZCLADOR DE NIVELES (AUDIO MIXER)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-mixosc1"></div>
        <div id="drawer-ctrl-mixosc2"></div>
        <div id="drawer-ctrl-mixnoise"></div>
      </div>
    </div>
  `;

  new RotaryKnob(container.querySelector('#drawer-ctrl-mixosc1'), {
    paramId: 'mixOsc1Level', label: 'OSC 1 VOL', min: 0, max: 127, defaultValue: 127, size: 54,
    onChange: (val) => bridge.setParam('mixOsc1Level', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-mixosc2'), {
    paramId: 'mixOsc2Level', label: 'OSC 2 VOL', min: 0, max: 127, defaultValue: 0, size: 54,
    onChange: (val) => bridge.setParam('mixOsc2Level', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-mixnoise'), {
    paramId: 'mixNoiseLevel', label: 'NOISE VOL', min: 0, max: 127, defaultValue: 0, size: 54,
    onChange: (val) => bridge.setParam('mixNoiseLevel', val)
  });
}

/* ─────────────────────────────────────────────────────────────
   2. VOCODER DRAWERS (CONFIG & 16-BAND GRAPHIC CONSOLE)
   ───────────────────────────────────────────────────────────── */
function renderVocoderConfigDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODO Y FUENTE PORTADORA (CARRIER)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-vocoder-mode"></div>
        <div id="drawer-sel-vocoder-carrier"></div>
        <div id="drawer-sel-vocoder-shift"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>DINÁMICA &amp; NIVEL DE ENTRADA</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-vochpf"></div>
        <div id="drawer-ctrl-vocgate"></div>
        <div id="drawer-ctrl-vocdirect"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-vocoder-mode'), {
    paramId: 'synthVocoderMode',
    label: 'MODO SINTETIZADOR',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'SYNTH (NORMAL)' },
      { value: 1, label: 'VOCODER (VOCAL)' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-vocoder-carrier'), {
    paramId: 'vocoderCarrierSrc',
    label: 'CARRIER SOURCE',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'INTERNAL SYNTH' },
      { value: 1, label: 'EXTERNAL AUDIO IN' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-vocoder-shift'), {
    paramId: 'vocoderFormantShift',
    label: 'FORMANT SHIFT',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: '-2 (GRAVE)' },
      { value: 1, label: '-1' },
      { value: 2, label: '0 (NORMAL)' },
      { value: 3, label: '+1' },
      { value: 4, label: '+2 (AGUDO)' }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-vochpf'), {
    paramId: 'vocoderHpfLevel', label: 'HPF LEVEL', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('vocoderHpfLevel', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-vocgate'), {
    paramId: 'vocoderGateSense', label: 'GATE SENSE', min: 0, max: 127, defaultValue: 50, size: 48,
    onChange: (val) => bridge.setParam('vocoderGateSense', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-vocdirect'), {
    paramId: 'vocoderDirectLevel', label: 'DIRECT LEVEL', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('vocoderDirectLevel', val)
  });
}

function renderVocoderBandsDrawer(container) {
  let bandsHtml = '<div class="vocoder-bands-grid">';
  for (let i = 1; i <= 16; i++) {
    const val = paramStore.get(`vocoderBandLevel${i}`) ?? 127;
    bandsHtml += `
      <div class="vocoder-band-cell">
        <span class="vocoder-band-num">B${i}</span>
        <input type="range" class="vocoder-v-slider" id="v-slider-b${i}" min="0" max="127" value="${val}">
        <span class="vocoder-band-val" id="v-val-b${i}">${val}</span>
      </div>
    `;
  }
  bandsHtml += '</div>';

  container.innerHTML = `
    <div class="drawer-section-card">
      <div style="display: flex; justify-content: space-between; align-items: center;">
        <h3 style="margin: 0; border: none; padding: 0;">NIVELES DE BANCO DE FILTROS (16 BANDAS)</h3>
        <button id="btn-vocoder-reset-bands" class="card-action-btn" style="max-width: 120px; padding: 4px 8px;">ALL 100%</button>
      </div>
      ${bandsHtml}
    </div>
  `;

  for (let i = 1; i <= 16; i++) {
    const slider = container.querySelector(`#v-slider-b${i}`);
    const valTxt = container.querySelector(`#v-val-b${i}`);
    slider?.addEventListener('input', (e) => {
      const v = parseInt(e.target.value, 10);
      if (valTxt) valTxt.textContent = v;
      bridge.setParam(`vocoderBandLevel${i}`, v);
    });
  }

  container.querySelector('#btn-vocoder-reset-bands')?.addEventListener('click', () => {
    for (let i = 1; i <= 16; i++) {
      bridge.setParam(`vocoderBandLevel${i}`, 127);
      paramStore.set(`vocoderBandLevel${i}`, 127);
      const slider = container.querySelector(`#v-slider-b${i}`);
      const valTxt = container.querySelector(`#v-val-b${i}`);
      if (slider) {
        slider.value = 127;
        slider.dispatchEvent(new Event('input'));
      }
      if (valTxt) valTxt.textContent = 127;
    }
  });

  // Apply photorealistic hardware filmstrip sprites to all band sliders
  initFilmstrips(container);
}

/* ─────────────────────────────────────────────────────────────
   3. FILTER MULTIMODE DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderFilterMainDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>TIPO DE FILTRO &amp; RESPUESTA EN FRECUENCIA</h3>
      <div id="drawer-sel-filter-type" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-cutoff"></div>
        <div id="drawer-ctrl-reso"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-filter-type'), {
    paramId: 'filterType',
    label: 'TIPO DE FILTRO',
    layout: 'grid-4',
    size: 'md',
    options: [
      { value: 0, label: 'LPF 24dB', iconSvg: FILTER_ICONS.lpf24 },
      { value: 1, label: 'LPF 12dB', iconSvg: FILTER_ICONS.lpf12 },
      { value: 2, label: 'BPF 12dB', iconSvg: FILTER_ICONS.bpf12 },
      { value: 3, label: 'HPF 12dB', iconSvg: FILTER_ICONS.hpf12 }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-cutoff'), {
    paramId: 'filterCutoff', label: 'CUTOFF', min: 0, max: 127, defaultValue: 127, size: 56,
    displayFormatter: (v) => {
      const hz = Math.round(20 * Math.pow(1000, v / 127));
      return hz >= 1000 ? `${(hz / 1000).toFixed(1)} kHz` : `${hz} Hz`;
    },
    onChange: (val) => bridge.setParam('filterCutoff', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-reso'), {
    paramId: 'filterResonance', label: 'RESONANCIA', min: 0, max: 127, defaultValue: 0, size: 56,
    onChange: (val) => bridge.setParam('filterResonance', val)
  });
}

function renderFilterModDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODULACIÓN DE CORTE (EG1 / KEY / VEL)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1int"></div>
        <div id="drawer-ctrl-keytrack"></div>
        <div id="drawer-ctrl-velsens"></div>
      </div>
    </div>
  `;

  new RotaryKnob(container.querySelector('#drawer-ctrl-eg1int'), {
    paramId: 'filterEg1Int', label: 'EG1 INT', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('filterEg1Int', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-keytrack'), {
    paramId: 'filterKeyTrack', label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('filterKeyTrack', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-velsens'), {
    paramId: 'filterVelSens', label: 'VEL SENS', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('filterVelSens', val)
  });
}

/* ─────────────────────────────────────────────────────────────
   4. AMP (VCA) & ENVELOPES DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderAmpMainDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>AMPLIFICADOR VCA &amp; DISTORSIÓN</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-amplevel"></div>
        <div id="drawer-ctrl-amppan"></div>
        <div id="drawer-ctrl-ampdist"></div>
        <div id="drawer-ctrl-ampkeytrack"></div>
      </div>
    </div>
  `;

  new RotaryKnob(container.querySelector('#drawer-ctrl-amplevel'), {
    paramId: 'ampLevel', label: 'NIVEL AMP', min: 0, max: 127, defaultValue: 100, size: 50,
    onChange: (val) => bridge.setParam('ampLevel', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-amppan'), {
    paramId: 'ampPan', label: 'PANORAMA', min: -64, max: 63, defaultValue: 0, size: 50, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('ampPan', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-ampdist'), {
    paramId: 'ampDistortion', label: 'DISTORTION', min: 0, max: 1, defaultValue: 0, size: 50,
    displayFormatter: (v) => v > 0.5 ? 'ON' : 'OFF',
    onChange: (val) => bridge.setParam('ampDistortion', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-ampkeytrack'), {
    paramId: 'ampKeyTrack', label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 50, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('ampKeyTrack', val)
  });
}

function renderEg1Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVOLVENTE 1: FILTRO &amp; PITCH (ADSR)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1a"></div>
        <div id="drawer-ctrl-eg1d"></div>
        <div id="drawer-ctrl-eg1s"></div>
        <div id="drawer-ctrl-eg1r"></div>
      </div>
    </div>
  `;
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg1a'), {
    paramId: 'eg1Attack', label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg1Attack', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg1d'), {
    paramId: 'eg1Decay', label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('eg1Decay', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg1s'), {
    paramId: 'eg1Sustain', label: 'SUSTAIN', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg1Sustain', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg1r'), {
    paramId: 'eg1Release', label: 'RELEASE', min: 0, max: 127, defaultValue: 40, size: 48,
    onChange: (val) => bridge.setParam('eg1Release', val)
  });
}

function renderEg2Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVOLVENTE 2: AMPLITUD (ADSR)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg2a"></div>
        <div id="drawer-ctrl-eg2d"></div>
        <div id="drawer-ctrl-eg2s"></div>
        <div id="drawer-ctrl-eg2r"></div>
      </div>
    </div>
  `;
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg2a'), {
    paramId: 'eg2Attack', label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg2Attack', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg2d'), {
    paramId: 'eg2Decay', label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('eg2Decay', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg2s'), {
    paramId: 'eg2Sustain', label: 'SUSTAIN', min: 0, max: 127, defaultValue: 127, size: 48,
    onChange: (val) => bridge.setParam('eg2Sustain', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-eg2r'), {
    paramId: 'eg2Release', label: 'RELEASE', min: 0, max: 127, defaultValue: 20, size: 48,
    onChange: (val) => bridge.setParam('eg2Release', val)
  });
}

/* ─────────────────────────────────────────────────────────────
   5. LFOS & VIRTUAL PATCH DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderLfo1Drawer(container) {
  const syncNotes = [
    { value: 0, label: '1/1' }, { value: 1, label: '3/4' }, { value: 2, label: '1/2' },
    { value: 3, label: '1/2T' }, { value: 4, label: '1/4' }, { value: 5, label: '1/4T' },
    { value: 6, label: '1/8' }, { value: 7, label: '1/8T' }, { value: 8, label: '1/16' },
    { value: 9, label: '1/16T' }, { value: 10, label: '1/32' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>LFO 1: ONDA &amp; FRECUENCIA</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-lfo1-wave"></div>
        <div class="drawer-controls-row">
          <div id="drawer-ctrl-lfo1freq"></div>
        </div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>SINCRONIZACIÓN LFO 1</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-lfo1-keysync" style="flex: 1; min-width: 140px;"></div>
          <div id="drawer-sel-lfo1-temposync" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-lfo1-syncnote"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo1-wave'), {
    paramId: 'lfo1Wave',
    label: 'FORMA DE ONDA LFO 1',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQUARE', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'TRIANGLE', iconSvg: WAVE_ICONS.triangle },
      { value: 3, label: 'S&H', iconSvg: WAVE_ICONS.sh }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo1-keysync'), {
    paramId: 'lfo1KeySync',
    label: 'KEY SYNC',
    layout: 'row',
    size: 'sm',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'TIMBRE' },
      { value: 2, label: 'VOICE' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo1-temposync'), {
    paramId: 'lfo1TempoSync',
    label: 'TEMPO SYNC',
    layout: 'row',
    size: 'sm',
    options: [
      { value: 0, label: 'OFF (LIBRE)' },
      { value: 1, label: 'ON (BPM)' }
    ]
  });

  new LcdDropdown(container.querySelector('#drawer-sel-lfo1-syncnote'), {
    paramId: 'lfo1SyncNote',
    label: 'DIVISIÓN RÍTMICA (SYNC NOTE)',
    options: syncNotes
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-lfo1freq'), {
    paramId: 'lfo1Freq', label: 'FRECUENCIA', min: 0, max: 127, defaultValue: 30, size: 52,
    displayFormatter: (v) => `${(0.01 + Math.pow(v / 127, 2) * 20).toFixed(2)} Hz`,
    onChange: (val) => bridge.setParam('lfo1Freq', val)
  });
}

function renderLfo2Drawer(container) {
  const syncNotes = [
    { value: 0, label: '1/1' }, { value: 1, label: '3/4' }, { value: 2, label: '1/2' },
    { value: 3, label: '1/2T' }, { value: 4, label: '1/4' }, { value: 5, label: '1/4T' },
    { value: 6, label: '1/8' }, { value: 7, label: '1/8T' }, { value: 8, label: '1/16' },
    { value: 9, label: '1/16T' }, { value: 10, label: '1/32' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>LFO 2: ONDA &amp; FRECUENCIA</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-lfo2-wave"></div>
        <div class="drawer-controls-row">
          <div id="drawer-ctrl-lfo2freq"></div>
        </div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>SINCRONIZACIÓN LFO 2</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-lfo2-keysync" style="flex: 1; min-width: 140px;"></div>
          <div id="drawer-sel-lfo2-temposync" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-lfo2-syncnote"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo2-wave'), {
    paramId: 'lfo2Wave',
    label: 'FORMA DE ONDA LFO 2',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQR+', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'SINE', iconSvg: WAVE_ICONS.sine },
      { value: 3, label: 'S&H', iconSvg: WAVE_ICONS.sh }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo2-keysync'), {
    paramId: 'lfo2KeySync',
    label: 'KEY SYNC',
    layout: 'row',
    size: 'sm',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'TIMBRE' },
      { value: 2, label: 'VOICE' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-lfo2-temposync'), {
    paramId: 'lfo2TempoSync',
    label: 'TEMPO SYNC',
    layout: 'row',
    size: 'sm',
    options: [
      { value: 0, label: 'OFF (LIBRE)' },
      { value: 1, label: 'ON (BPM)' }
    ]
  });

  new LcdDropdown(container.querySelector('#drawer-sel-lfo2-syncnote'), {
    paramId: 'lfo2SyncNote',
    label: 'DIVISIÓN RÍTMICA (SYNC NOTE)',
    options: syncNotes
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-lfo2freq'), {
    paramId: 'lfo2Freq', label: 'FRECUENCIA', min: 0, max: 127, defaultValue: 50, size: 52,
    displayFormatter: (v) => `${(0.01 + Math.pow(v / 127, 2) * 20).toFixed(2)} Hz`,
    onChange: (val) => bridge.setParam('lfo2Freq', val)
  });
}

function renderModMatrixDrawer(container) {
  const sources = [
    { value: 0, label: 'EG1' },
    { value: 1, label: 'EG2' },
    { value: 2, label: 'LFO1' },
    { value: 3, label: 'LFO2' },
    { value: 4, label: 'VEL' },
    { value: 5, label: 'KBD' },
    { value: 6, label: 'BEND' },
    { value: 7, label: 'MOD' }
  ];

  const dests = [
    { value: 0, label: 'PITCH' },
    { value: 1, label: 'OSC2' },
    { value: 2, label: 'CTRL1' },
    { value: 3, label: 'NOISE' },
    { value: 4, label: 'CUTOFF' },
    { value: 5, label: 'AMP' },
    { value: 6, label: 'PAN' },
    { value: 7, label: 'LFO2' }
  ];

  let slotsHtml = '';
  for (let s = 1; s <= 4; s++) {
    slotsHtml += `
      <div class="patch-slot-card" style="margin-bottom: 10px; background: rgba(14, 19, 29, 0.7); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; padding: 10px;">
        <div style="display: flex; align-items: center; gap: 6px; margin-bottom: 8px;">
          <span class="patch-slot-badge">PATCH ${s}</span>
        </div>
        <div style="display: grid; grid-template-columns: 1fr 1fr 58px; gap: 8px; align-items: start;">
          <div id="drawer-patch${s}-src-sel"></div>
          <div id="drawer-patch${s}-dest-sel"></div>
          <div id="drawer-ctrl-patch${s}-int" style="display: flex; justify-content: center;"></div>
        </div>
      </div>
    `;
  }

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MATRIZ DE MODULACIÓN VIRTUAL (4 SLOTS)</h3>
      ${slotsHtml}
    </div>
  `;

  for (let s = 1; s <= 4; s++) {
    new SegmentedSelector(container.querySelector(`#drawer-patch${s}-src-sel`), {
      paramId: `patch${s}Source`,
      label: 'FUENTE',
      layout: 'grid-4',
      size: 'sm',
      options: sources
    });

    new SegmentedSelector(container.querySelector(`#drawer-patch${s}-dest-sel`), {
      paramId: `patch${s}Destination`,
      label: 'DESTINO',
      layout: 'grid-4',
      size: 'sm',
      options: dests
    });

    new RotaryKnob(container.querySelector(`#drawer-ctrl-patch${s}-int`), {
      paramId: `patch${s}Intensity`, label: 'INT', min: -63, max: 63, defaultValue: 0, size: 44, isBipolar: true,
      displayFormatter: (v) => `${Math.round(v)}`,
      onChange: (val) => bridge.setParam(`patch${s}Intensity`, val)
    });
  }
}

/* ─────────────────────────────────────────────────────────────
   6. ARPEGGIATOR & MOD SEQUENCER DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderArpDrawer(container) {
  const arpResolutions = [
    { value: 0, label: '1/48' }, { value: 1, label: '1/32' }, { value: 2, label: '1/24' },
    { value: 3, label: '1/16' }, { value: 4, label: '1/12' }, { value: 5, label: '1/8' },
    { value: 6, label: '1/6' },  { value: 7, label: '1/4' },  { value: 8, label: '1/3' }, { value: 9, label: '1/2' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ESTADO Y MODO DE ARPEGIADOR</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-arp-on" style="flex: 1; min-width: 120px;"></div>
          <div id="drawer-sel-arp-latch" style="flex: 1; min-width: 120px;"></div>
          <div id="drawer-sel-arp-range" style="flex: 1.5; min-width: 160px;"></div>
        </div>
        <div id="drawer-sel-arp-type"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>TEMPO &amp; RESOLUCIÓN RÍTMICA</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-arptempo"></div>
        <div id="drawer-ctrl-arpgate"></div>
        <div id="drawer-sel-arp-res" style="flex: 1; min-width: 140px;"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-arp-on'), {
    paramId: 'arpOn',
    label: 'ARPEGGIATOR',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-arp-latch'), {
    paramId: 'arpLatch',
    label: 'LATCH',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-arp-range'), {
    paramId: 'arpRange',
    label: 'OCTAVAS (RANGE)',
    layout: 'row',
    size: 'sm',
    options: [
      { value: 1, label: '1 OCT' },
      { value: 2, label: '2 OCT' },
      { value: 3, label: '3 OCT' },
      { value: 4, label: '4 OCT' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-arp-type'), {
    paramId: 'arpType',
    label: 'TIPO DE ARPEGIO',
    layout: 'grid-4',
    size: 'md',
    options: [
      { value: 0, label: 'UP' },
      { value: 1, label: 'DOWN' },
      { value: 2, label: 'ALT 1' },
      { value: 3, label: 'ALT 2' },
      { value: 4, label: 'RANDOM' },
      { value: 5, label: 'TRIGGER' }
    ]
  });

  new LcdDropdown(container.querySelector('#drawer-sel-arp-res'), {
    paramId: 'arpResolution',
    label: 'RESOLUCIÓN RÍTMICA',
    options: arpResolutions
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-arptempo'), {
    paramId: 'arpTempo', label: 'TEMPO', min: 20, max: 300, defaultValue: 120, size: 50,
    displayFormatter: (v) => `${Math.round(v)} BPM`,
    onChange: (val) => bridge.setParam('arpTempo', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-arpgate'), {
    paramId: 'arpGate', label: 'GATE TIME', min: 0, max: 127, defaultValue: 100, size: 50,
    displayFormatter: (v) => `${Math.round((v / 127) * 100)}%`,
    onChange: (val) => bridge.setParam('arpGate', val)
  });
}

function renderModSeqDrawer(container) {
  const modSeqResolutions = [
    { value: 0, label: '1/48' }, { value: 1, label: '1/32' }, { value: 2, label: '1/24' },
    { value: 3, label: '1/16' }, { value: 4, label: '1/12' }, { value: 5, label: '1/8' },
    { value: 6, label: '1/6' },  { value: 7, label: '1/4' },  { value: 8, label: '1/3' }, { value: 9, label: '1/2' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>SECUENCIADOR DE MODULACIÓN (3 PISTAS x 16 PASOS)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-modseq-on" style="flex: 1; min-width: 120px;"></div>
          <div id="drawer-sel-modseq-smooth" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-modseq-type"></div>
        <div id="drawer-sel-modseq-res"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-modseq-on'), {
    paramId: 'modSeqOn',
    label: 'MOD SEQUENCER',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-modseq-smooth'), {
    paramId: 'modSeqSmooth',
    label: 'TRANSICIÓN ENTRE PASOS',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'STEP (ESCALONADO)' }, { value: 1, label: 'SMOOTH (SUAVE)' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-modseq-type'), {
    paramId: 'modSeqType',
    label: 'DIRECCIÓN DE SECUENCIA',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'FORWARD' },
      { value: 1, label: 'REVERSE' },
      { value: 2, label: 'BOUNCE' },
      { value: 3, label: 'RANDOM' }
    ]
  });

  new LcdDropdown(container.querySelector('#drawer-sel-modseq-res'), {
    paramId: 'modSeqResolution',
    label: 'RESOLUCIÓN TEMPORAL',
    options: modSeqResolutions
  });
}

/* ─────────────────────────────────────────────────────────────
   7. FX (MOD FX, DELAY, EQ) DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderModFxDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>EFECTOS DE MODULACIÓN (CHORUS / ENSEMBLE / PHASER)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-modfx-on"></div>
        <div id="drawer-sel-modfx-type"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>CONTROLES DE MOD FX</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-modfxspeed"></div>
        <div id="drawer-ctrl-modfxdepth"></div>
        <div id="drawer-ctrl-modfxfb"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-modfx-on'), {
    paramId: 'modFxOn',
    label: 'MOD FX POWER',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'BYPASS' }, { value: 1, label: 'ACTIVE (ON)' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-modfx-type'), {
    paramId: 'modFxType',
    label: 'ALGORITMO DE MODULACIÓN',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'CHORUS / FLANGER' },
      { value: 1, label: 'ENSEMBLE' },
      { value: 2, label: 'PHASER' }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-modfxspeed'), {
    paramId: 'modFxSpeed', label: 'SPEED', min: 0, max: 127, defaultValue: 40, size: 50,
    onChange: (val) => bridge.setParam('modFxSpeed', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-modfxdepth'), {
    paramId: 'modFxDepth', label: 'DEPTH', min: 0, max: 127, defaultValue: 64, size: 50,
    onChange: (val) => bridge.setParam('modFxDepth', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-modfxfb'), {
    paramId: 'modFxFeedback', label: 'FEEDBACK', min: 0, max: 127, defaultValue: 0, size: 50,
    onChange: (val) => bridge.setParam('modFxFeedback', val)
  });
}

function renderDelayFxDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>DELAY ESTÉREO</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-delay-on"></div>
        <div id="drawer-sel-delay-type"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>TIEMPO &amp; REPETICIONES DELAY</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-delaytime"></div>
        <div id="drawer-ctrl-delaydepth"></div>
        <div id="drawer-ctrl-delayfb"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-delay-on'), {
    paramId: 'delayOn',
    label: 'DELAY POWER',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'BYPASS' }, { value: 1, label: 'ACTIVE (ON)' }]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-delay-type'), {
    paramId: 'delayType',
    label: 'TIPO DE DELAY',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'STEREO DELAY' },
      { value: 1, label: 'CROSS (PING-PONG)' },
      { value: 2, label: 'LEFT / RIGHT' }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-delaytime'), {
    paramId: 'delayTime', label: 'TIME', min: 0, max: 127, defaultValue: 40, size: 50,
    onChange: (val) => bridge.setParam('delayTime', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-delaydepth'), {
    paramId: 'delayDepth', label: 'DEPTH (WET)', min: 0, max: 127, defaultValue: 50, size: 50,
    onChange: (val) => bridge.setParam('delayDepth', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-delayfb'), {
    paramId: 'delayFeedback', label: 'FEEDBACK', min: 0, max: 127, defaultValue: 40, size: 50,
    onChange: (val) => bridge.setParam('delayFeedback', val)
  });
}

function renderEqDrawer(container) {
  const lowFreqs = [
    { value: 0, label: '160 Hz' }, { value: 1, label: '250 Hz' },
    { value: 2, label: '400 Hz' }, { value: 3, label: '600 Hz' }
  ];
  const highFreqs = [
    { value: 0, label: '4.0 kHz' }, { value: 1, label: '6.0 kHz' },
    { value: 2, label: '8.0 kHz' }, { value: 3, label: '12.0 kHz' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ECUALIZADOR MASTER (2 BANDAS SHELVING)</h3>
      <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
        <div style="display: flex; flex-direction: column; gap: 10px; align-items: center;">
          <div id="drawer-sel-eq-lowfreq" style="width: 100%;"></div>
          <div id="drawer-ctrl-eqlowgain"></div>
        </div>
        <div style="display: flex; flex-direction: column; gap: 10px; align-items: center;">
          <div id="drawer-sel-eq-highfreq" style="width: 100%;"></div>
          <div id="drawer-ctrl-eqhighgain"></div>
        </div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-eq-lowfreq'), {
    paramId: 'eqLowFreq',
    label: 'FREC. GRAVES (LOW)',
    layout: 'grid-2',
    size: 'sm',
    options: lowFreqs
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-eqlowgain'), {
    paramId: 'eqLowGain', label: 'LOW GAIN', min: 0, max: 127, defaultValue: 64, size: 50, isBipolar: true,
    displayFormatter: (v) => `${((v - 64) * (12 / 64)).toFixed(1)} dB`,
    onChange: (val) => bridge.setParam('eqLowGain', val)
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-eq-highfreq'), {
    paramId: 'eqHighFreq',
    label: 'FREC. AGUDOS (HIGH)',
    layout: 'grid-2',
    size: 'sm',
    options: highFreqs
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-eqhighgain'), {
    paramId: 'eqHighGain', label: 'HIGH GAIN', min: 0, max: 127, defaultValue: 64, size: 50, isBipolar: true,
    displayFormatter: (v) => `${((v - 64) * (12 / 64)).toFixed(1)} dB`,
    onChange: (val) => bridge.setParam('eqHighGain', val)
  });
}

/* ─────────────────────────────────────────────────────────────
   8. VOICE, PITCH & MASTER DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderVoiceDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODO DE ASIGNACIÓN DE VOZ &amp; UNISON</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-voice-mode"></div>
        <div id="drawer-sel-synth-profile"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>UNISON DETUNE &amp; APERTURA ESTÉREO</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-unisondetune"></div>
        <div id="drawer-ctrl-unisonspread"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-voice-mode'), {
    paramId: 'voiceMode',
    label: 'VOICE ASSIGN (POLIFONÍA)',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'MONO' },
      { value: 1, label: 'POLY (4V)' },
      { value: 2, label: 'UNISON (4V)' }
    ]
  });

  new SegmentedSelector(container.querySelector('#drawer-sel-synth-profile'), {
    paramId: 'synthMode',
    label: 'SYNTH PROFILE / ARQUITECTURA',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'MS2000' },
      { value: 1, label: 'MICROKORG' },
      { value: 2, label: 'ADVANCED' }
    ]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-unisondetune'), {
    paramId: 'unisonDetune', label: 'UNISON DETUNE', min: 0, max: 99, defaultValue: 10, size: 50,
    displayFormatter: (v) => `${Math.round(v)} c`,
    onChange: (val) => bridge.setParam('unisonDetune', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-unisonspread'), {
    paramId: 'unisonSpread', label: 'UNISON SPREAD', min: 0, max: 1, defaultValue: 0.5, size: 50,
    displayFormatter: (v) => `${Math.round(v * 100)}%`,
    onChange: (val) => bridge.setParam('unisonSpread', val)
  });
}

function renderPitchDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>PORTAMENTO (GLIDE) &amp; PITCH BEND</h3>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-sel-porta-on" style="flex: 1; max-width: 180px;"></div>
        <div id="drawer-ctrl-portatime"></div>
      </div>
    </div>
  `;

  new SegmentedSelector(container.querySelector('#drawer-sel-porta-on'), {
    paramId: 'portamentoOn',
    label: 'PORTAMENTO',
    layout: 'row',
    size: 'sm',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }]
  });

  new RotaryKnob(container.querySelector('#drawer-ctrl-portatime'), {
    paramId: 'portamentoTime', label: 'PORTA TIME', min: 0, max: 127, defaultValue: 0, size: 52,
    onChange: (val) => bridge.setParam('portamentoTime', val)
  });
}

function renderGlobalMasterDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>VOLUMEN MASTER &amp; CONTROLADORES ASIGNABLES</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-mastervol"></div>
        <div id="drawer-ctrl-pedal"></div>
        <div id="drawer-ctrl-assign-switch"></div>
      </div>
    </div>
  `;

  new RotaryKnob(container.querySelector('#drawer-ctrl-mastervol'), {
    paramId: 'masterVolume', label: 'MASTER VOL', min: 0, max: 1, defaultValue: 0.8, size: 54,
    displayFormatter: (v) => `${Math.round(v * 100)}%`,
    onChange: (val) => bridge.setParam('masterVolume', val)
  });
  new RotaryKnob(container.querySelector('#drawer-ctrl-pedal'), {
    paramId: 'assignablePedal', label: 'ASSIGN PEDAL', min: 0, max: 127, defaultValue: 0, size: 54,
    onChange: (val) => bridge.setParam('assignablePedal', val)
  });

  new SegmentedSelector(container.querySelector('#drawer-ctrl-assign-switch'), {
    paramId: 'assignableSwitch',
    label: 'ASSIGN SWITCH',
    layout: 'row',
    size: 'md',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'ON' }
    ]
  });
}

/* ─────────────────────────────────────────────────────────────
   DASHBOARD OBSERVERS (UPDATES 8 CARDS)
   ───────────────────────────────────────────────────────────── */
function setupDashboardObservers() {
  const osc1Names = ['Saw', 'Square', 'Triangle', 'Sine', 'VoxWave', 'DWGS', 'Noise', 'AudioIn'];
  const osc2Names = ['Saw', 'Square', 'Triangle'];
  const modNames = ['Off', 'Ring', 'Sync', 'CrossMod'];
  const filterNames = ['LPF 24dB', 'LPF 12dB', 'BPF 12dB', 'HPF 12dB'];
  const voiceModes = ['Mono', 'Poly (4V)', 'Unison (4V)'];

  // Card 1: OSC 1 / OSC 2 / MIX
  paramStore.onChange('osc1Wave', (val) => {
    const badge = document.getElementById('badge-osc1-wave');
    const txt = document.getElementById('val-osc1-wave');
    if (val === 5) {
      const dwgsSlot = paramStore.get('osc1DwgsWave') || 0;
      const dwgsName = getWaveName(dwgsSlot);
      if (badge) badge.textContent = `DWGS #${dwgsSlot + 1}`;
      if (txt) txt.textContent = `DWGS (${dwgsName})`;
    } else {
      const name = osc1Names[val] || 'Saw';
      if (badge) badge.textContent = `VA ${name.toUpperCase()}`;
      if (txt) txt.textContent = name;
    }
  });

  paramStore.onChange('osc2Wave', (val) => {
    const txt = document.getElementById('val-osc2-mod');
    const modVal = paramStore.get('osc2ModType') || 0;
    if (txt) txt.textContent = `${osc2Names[val] || 'Square'} (${modNames[modVal] || 'Off'})`;
  });

  paramStore.onChange('osc2ModType', (val) => {
    const txt = document.getElementById('val-osc2-mod');
    const waveVal = paramStore.get('osc2Wave') || 0;
    if (txt) txt.textContent = `${osc2Names[waveVal] || 'Square'} (${modNames[val] || 'Off'})`;
  });

  paramStore.onChange('mixOsc1Level', (val) => {
    const bar = document.getElementById('bar-mix-osc1');
    if (bar) bar.style.width = `${(val / 127) * 100}%`;
  });
  paramStore.onChange('mixOsc2Level', (val) => {
    const bar = document.getElementById('bar-mix-osc2');
    if (bar) bar.style.width = `${(val / 127) * 100}%`;
  });
  paramStore.onChange('mixNoiseLevel', (val) => {
    const bar = document.getElementById('bar-mix-noise');
    if (bar) bar.style.width = `${(val / 127) * 100}%`;
  });

  // Card 2: VOCODER
  paramStore.onChange('synthVocoderMode', (val) => {
    const badge = document.getElementById('badge-vocoder-mode');
    const txt = document.getElementById('val-vocoder-status');
    if (badge) badge.textContent = val > 0.5 ? 'VOCODER' : 'SYNTH';
    if (txt) txt.textContent = val > 0.5 ? 'ON (16 Bands Active)' : 'OFF (Synth Mode)';
  });

  // Card 3: FILTER
  paramStore.onChange('filterType', (val) => {
    const badge = document.getElementById('badge-filter-type');
    const txt = document.getElementById('val-filter-type');
    const name = filterNames[Math.round(val)] || 'LPF 24dB';
    if (badge) badge.textContent = name;
    if (txt) txt.textContent = name;
  });
  paramStore.onChange('filterCutoff', (val) => {
    const txt = document.getElementById('val-filter-cutoff');
    if (txt) {
      const v = Math.round(val);
      const hz = Math.round(20 * Math.pow(1000, v / 127));
      txt.textContent = `${v} (${hz >= 1000 ? (hz / 1000).toFixed(1) + ' kHz' : hz + ' Hz'})`;
    }
  });
  paramStore.onChange('filterResonance', (val) => {
    const txt = document.getElementById('val-filter-reso-eg1');
    const eg1 = Math.round(paramStore.get('filterEg1Int') || 0);
    const r = Math.round(val);
    if (txt) txt.textContent = `Reso: ${r} | EG1: ${eg1 >= 0 ? '+' : ''}${eg1}`;
  });
  paramStore.onChange('filterEg1Int', (val) => {
    const txt = document.getElementById('val-filter-reso-eg1');
    const reso = Math.round(paramStore.get('filterResonance') || 0);
    const e = Math.round(val);
    if (txt) txt.textContent = `Reso: ${reso} | EG1: ${e >= 0 ? '+' : ''}${e}`;
  });

  // Card 4: AMP & ENVELOPES
  paramStore.onChange('ampLevel', (val) => {
    const txt = document.getElementById('val-amp-level-pan');
    const pan = paramStore.get('ampPan') || 0;
    if (txt) txt.textContent = `Vol: ${val} | Pan: ${pan === 0 ? 'C' : (pan > 0 ? 'R' + pan : 'L' + Math.abs(pan))}`;
  });

  // Card 6: SEQ & ARPEGGIATOR
  const updateArpCard = () => {
    const txt = document.getElementById('val-arp-status');
    const on = (paramStore.get('arpOn') || 0) > 0.5;
    if (!txt) return;
    if (!on) {
      txt.textContent = 'OFF';
    } else {
      const types = ['Up', 'Down', 'Alt1', 'Alt2', 'Random', 'Trigger'];
      const t = types[paramStore.get('arpType') || 0] || 'Up';
      const r = paramStore.get('arpRange') || 1;
      txt.textContent = `ON (${t} / ${r} Oct)`;
    }
  };
  paramStore.onChange('arpOn', updateArpCard);
  paramStore.onChange('arpType', updateArpCard);
  paramStore.onChange('arpRange', updateArpCard);

  const updateModSeqCard = () => {
    const txt = document.getElementById('val-modseq-status');
    const on = (paramStore.get('modSeqOn') || 0) > 0.5;
    if (!txt) return;
    if (!on) {
      txt.textContent = 'OFF';
    } else {
      const dirs = ['Forward', 'Reverse', 'Bounce', 'Random'];
      const dir = dirs[paramStore.get('modSeqType') || 0] || 'Forward';
      const smooth = (paramStore.get('modSeqSmooth') || 0) > 0 ? 'Smooth' : 'Step';
      txt.textContent = `ON (${dir} / ${smooth})`;
    }
  };
  paramStore.onChange('modSeqOn', updateModSeqCard);
  paramStore.onChange('modSeqType', updateModSeqCard);
  paramStore.onChange('modSeqSmooth', updateModSeqCard);

  // Card 7: FX & EQUALIZER
  const updateModFxCard = () => {
    const txt = document.getElementById('val-fx-mod');
    if (!txt) return;
    const on = (paramStore.get('modFxOn') || 1) > 0.5;
    if (!on) {
      txt.textContent = 'BYPASS';
    } else {
      const types = ['Chorus/Flanger', 'Ensemble', 'Phaser'];
      const t = types[paramStore.get('modFxType') || 0] || 'Chorus';
      const depth = paramStore.get('modFxDepth') || 64;
      txt.textContent = `${t} (Depth: ${depth})`;
    }
  };
  paramStore.onChange('modFxOn', updateModFxCard);
  paramStore.onChange('modFxType', updateModFxCard);
  paramStore.onChange('modFxDepth', updateModFxCard);

  const updateDelayCard = () => {
    const txt = document.getElementById('val-fx-delay');
    if (!txt) return;
    const on = (paramStore.get('delayOn') || 1) > 0.5;
    if (!on) {
      txt.textContent = 'BYPASS';
    } else {
      const types = ['Stereo', 'Cross (Ping-Pong)', 'L/R'];
      const t = types[paramStore.get('delayType') || 0] || 'Stereo';
      const time = paramStore.get('delayTime') || 40;
      txt.textContent = `${t} (Time: ${time})`;
    }
  };
  paramStore.onChange('delayOn', updateDelayCard);
  paramStore.onChange('delayType', updateDelayCard);
  paramStore.onChange('delayTime', updateDelayCard);

  const updateEqCard = () => {
    const txt = document.getElementById('val-fx-eq');
    if (!txt) return;
    const lg = paramStore.get('eqLowGain') !== undefined ? paramStore.get('eqLowGain') : 64;
    const hg = paramStore.get('eqHighGain') !== undefined ? paramStore.get('eqHighGain') : 64;
    const ldB = ((lg - 64) * (12 / 64)).toFixed(1);
    const hdB = ((hg - 64) * (12 / 64)).toFixed(1);
    txt.textContent = `Low ${ldB >= 0 ? '+' : ''}${ldB}dB / High ${hdB >= 0 ? '+' : ''}${hdB}dB`;
  };
  paramStore.onChange('eqLowGain', updateEqCard);
  paramStore.onChange('eqHighGain', updateEqCard);

  // Card 8: VOICE & MASTER
  paramStore.onChange('voiceMode', (val) => {
    const badge = document.getElementById('badge-voice-mode');
    const txt = document.getElementById('val-voice-mode');
    const modeStr = voiceModes[val] || 'Poly';
    if (badge) badge.textContent = modeStr.toUpperCase().split(' ')[0];
    if (txt) txt.textContent = modeStr;
  });
  paramStore.onChange('masterVolume', (val) => {
    const txt = document.getElementById('val-master-vol');
    if (txt) txt.textContent = `${Math.round(val * 100)}%`;
  });
}




function setupAudioInitButton() {
  const btn = document.getElementById('btn-audio-init');
  if (!btn) return;

  const updateVisibility = () => {
    if (bridge.isWebView) {
      btn.style.display = 'none';
    } else {
      btn.style.display = 'inline-flex';
    }
  };

  updateVisibility();
  window.addEventListener('DOMContentLoaded', updateVisibility);
  setTimeout(updateVisibility, 100);
  setTimeout(updateVisibility, 500);
  setTimeout(updateVisibility, 1500);

  btn.addEventListener('click', async () => {
    const success = await bridge.initWebAudio();
    if (success) {
      isAudioActive = true;
      btn.classList.add('active');
      const text = btn.querySelector('.audio-text');
      if (text) text.textContent = 'AUDIO RUNNING';
    }
  });
}

function setupThemeSelector() {
  const tabs = document.querySelectorAll('.mode-tab');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      setTheme(tab.dataset.mode);
    });
  });
}

function setTheme(mode) {
  const tabs = document.querySelectorAll('.mode-tab');
  tabs.forEach(t => {
    t.classList.toggle('active', t.dataset.mode === mode);
  });

  document.documentElement.setAttribute('data-theme', mode);
  document.body.className = 'skin-' + (mode === 'advanced' ? 'cyberpunk' : mode);
  currentTheme = mode;

  const modeIdx = mode === 'advanced' ? 2 : (mode === 'microkorg' ? 1 : 0);
  paramStore.set('synthMode', modeIdx);
  bridge.setParam('synthMode', modeIdx);
  bankManagerModal?.setTheme(mode);
  console.log('[Theme Switch]:', mode, 'modeIdx:', modeIdx);
}

function setupNavbarMenus() {
  const menuButtons = document.querySelectorAll('.nav-menu-bar .menu-btn');
  const dropdowns = document.querySelectorAll('.dropdown-menu');

  menuButtons.forEach(btn => {
    btn.addEventListener('click', (e) => {
      e.stopPropagation();
      const menuId = btn.dataset.menu;
      const targetDropdown = document.getElementById(menuId);
      const isAlreadyOpen = targetDropdown && targetDropdown.classList.contains('show');

      closeAllDropdowns();

      if (targetDropdown && !isAlreadyOpen) {
        targetDropdown.classList.add('show');
        btn.classList.add('active');
      }
    });

    btn.addEventListener('mouseenter', () => {
      const anyOpen = Array.from(dropdowns).some(d => d.classList.contains('show'));
      if (anyOpen) {
        closeAllDropdowns();
        const menuId = btn.dataset.menu;
        const targetDropdown = document.getElementById(menuId);
        if (targetDropdown) {
          targetDropdown.classList.add('show');
          btn.classList.add('active');
        }
      }
    });
  });

  // Dropdown item actions
  const menuItems = document.querySelectorAll('.dropdown-item');
  menuItems.forEach(item => {
    item.addEventListener('click', (e) => {
      e.stopPropagation();
      const action = item.dataset.action;
      closeAllDropdowns();
      if (action) handleMenuAction(action);
    });
  });

  // Click outside closes menus
  document.addEventListener('click', (e) => {
    if (!e.target.closest('.menu-item-container')) {
      closeAllDropdowns();
    }
  });
}

function closeAllDropdowns() {
  document.querySelectorAll('.dropdown-menu').forEach(d => d.classList.remove('show'));
  document.querySelectorAll('.nav-menu-bar .menu-btn').forEach(b => b.classList.remove('active'));
}

function handleMenuAction(action) {
  console.log(`[Menu Action Triggered]: ${action}`);
  switch (action) {
    case 'open-bank-manager':
      bankManagerModal?.open();
      break;
    case 'open-sysex':
      triggerSysexFileInput();
      break;
    case 'save-patch':
      saveCurrentPatch();
      break;
    case 'export-sysex':
      bridge.send('exportSysexProgram', {});
      break;
    case 'open-about':
      openAboutModal();
      break;
    case 'skin-ms2000':
      setTheme('ms2000');
      break;
    case 'skin-microkorg':
      setTheme('microkorg');
      break;
    case 'skin-cyberpunk':
      setTheme('advanced');
      break;
    case 'new-patch':
    case 'factory-reset':
      paramStore.generateInit(bridge);
      bridge.send('initPatch', {});
      const lcdInit = document.getElementById('lcd-line-2');
      if (lcdInit) lcdInit.textContent = '[Init Synth]    ';
      console.log('[Preset Reset to Default]');
      break;
    case 'randomize-patch':
    case 'randomize':
      paramStore.generateMusicalRandom(bridge);
      bridge.send('randomizePatch', {});
      const lcdRand = document.getElementById('lcd-line-2');
      if (lcdRand) lcdRand.textContent = '[Random Patch]  ';
      console.log('[Musical Random Patch Generated]');
      break;
    case 'panic':
      bridge.allNotesOff();
      if (keyboardInstance) keyboardInstance.panic();
      break;
    case 'toggle-oscilloscope':
      oscilloscopeModal?.toggle();
      break;
    case 'toggle-virtual-keys':
      if (keyboardInstance) {
        keyboardInstance.toggleCollapse();
      }
      break;
    case 'undo':
      paramStore.undo?.();
      break;
    case 'redo':
      paramStore.redo?.();
      break;
    case 'copy-timbre':
      timbreClipboard = {};
      Object.keys(PARAM_LOOKUP).forEach(id => {
        if (id.includes('1') || id.startsWith('osc1_') || id.startsWith('filter1_') || id.startsWith('eg1_')) {
          timbreClipboard[id] = paramStore.get(id);
        }
      });
      console.log('[Timbre 1 Copied to Clipboard]');
      break;
    case 'paste-timbre':
      if (timbreClipboard) {
        Object.entries(timbreClipboard).forEach(([k, v]) => {
          const targetKey = k.replace('1', '2');
          paramStore.set(targetKey, v);
          bridge.setParam(targetKey, v);
        });
        console.log('[Timbre Pasted to Timbre 2]');
      }
      break;
    case 'zoom-100':
      document.body.style.zoom = '100%';
      break;
    case 'zoom-125':
      document.body.style.zoom = '125%';
      break;
    case 'zoom-150':
      document.body.style.zoom = '150%';
      break;
    case 'midi-setup':
      diagnosticModal?.open();
      break;
    case 'midi-channel':
      globalMidiChannel = (globalMidiChannel % 16) + 1;
      const chItem = document.querySelector('[data-action="midi-channel"] span');
      if (chItem) chItem.textContent = `Global MIDI Channel: ${globalMidiChannel}`;
      bridge.send('setMidiChannel', { channel: globalMidiChannel });
      break;
    case 'open-manual':
      window.open('https://github.com/ajabadia/ABDMS2000#readme', '_blank');
      break;
    case 'midi-cc-chart':
      diagnosticModal?.open();
      break;
  }
}

function saveCurrentPatch() {
  // Save the current patch: confirm on the LCD (like the WRITE button) and
  // produce a .syx dump via the bridge, which the sysexProgramExported
  // listener downloads as ms2000_patch.syx.
  if (lcdProgrammer) lcdProgrammer.flashMessage('WRITE COMPLETED', 'PATCH SAVED');
  bridge.send('exportSysexProgram', {});
  console.log('[Menu Action]: Save Patch - exporting current patch as .syx');
}

function triggerSysexFileInput() {
  const fileInput = document.createElement('input');
  fileInput.type = 'file';
  fileInput.accept = '.syx,.sysex,.mid,.midi';
  fileInput.style.display = 'none';

  fileInput.onchange = (e) => {
    const file = e.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (loadEvt) => {
      const arrayBuffer = loadEvt.target.result;
      const bytes = new Uint8Array(arrayBuffer);
      let binary = '';
      for (let i = 0; i < bytes.byteLength; i++) {
        binary += String.fromCharCode(bytes[i]);
      }
      const base64Data = btoa(binary);

      bridge.send('importSysexBase64', { dataBase64: base64Data });
      console.log(`[SysEx Import Sent]: ${file.name} (${bytes.length} bytes)`);
    };
    reader.readAsArrayBuffer(file);
  };

  document.body.appendChild(fileInput);
  fileInput.click();
  setTimeout(() => fileInput.remove(), 1000);
}

function setupAboutModal() {
  const modal = document.getElementById('about-modal');
  const closeBtn = document.getElementById('about-close-btn');
  const buildVersionSpan = document.getElementById('about-build-version');

  if (buildVersionSpan) {
    buildVersionSpan.textContent = `${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber} - ${BUILD_INFO.buildDate})`;
  }

  closeBtn?.addEventListener('click', () => {
    if (modal) {
      modal.style.setProperty('display', 'none', 'important');
      modal.classList.add('hidden');
    }
  });

  modal?.addEventListener('click', (e) => {
    if (e.target === modal) {
      modal.style.setProperty('display', 'none', 'important');
      modal.classList.add('hidden');
    }
  });

  setupSysExEvents();
}

function setupSysExEvents() {
  // Bridge event listeners for SysEx import/export

  bridge.on('sysexImportResult', (data) => {
    if (data && data.success) {
      console.log(`[SysEx Import Success]: ${data.programName} (${data.programCount} programs)`);
      const lcdLine2 = document.getElementById('lcd-line-2');
      if (lcdLine2 && data.programName) {
        lcdLine2.textContent = `[${data.programName.trim()}]`;
      }
    } else if (data) {
      console.error(`[SysEx Import Failed]: ${data.errorMessage}`);
      alert(`SysEx Import Error: ${data.errorMessage}`);
    }
  });

  bridge.on('sysexProgramExported', (data) => {
    if (data && data.base64) {
      const binaryString = atob(data.base64);
      const bytes = new Uint8Array(binaryString.length);
      for (let i = 0; i < binaryString.length; i++) {
        bytes[i] = binaryString.charCodeAt(i);
      }
      const blob = new Blob([bytes], { type: 'application/octet-stream' });
      const url = URL.createObjectURL(blob);
      const a = document.createElement('a');
      a.href = url;
      a.download = 'ms2000_patch.syx';
      a.click();
      URL.revokeObjectURL(url);
      console.log(`[SysEx Program Exported]: ${bytes.length} bytes downloaded.`);
    }
  });
}


function openAboutModal() {
  const modal = document.getElementById('about-modal');
  if (modal) {
    modal.classList.remove('hidden');
    modal.style.setProperty('display', 'flex', 'important');
  }
}

function setupKeyboard() {
  keyboardInstance = createKeyboard({
    containerId: 'piano-keyboard',
    wheelPitchId: 'pitch-wheel-container',
    wheelModId: 'mod-wheel-container',
    octUpId: 'oct-up',
    octDownId: 'oct-down',
    ledUpId: 'led-up',
    ledDownId: 'led-down',
    onNoteOn: (note, velocity) => {
      bridge.initWebAudio();
      bridge.noteOn(note, velocity);
    },
    onNoteOff: (note) => bridge.noteOff(note, 0.0),
    onPitchBend: (val) => bridge.pitchBend(val),
    onModWheel: (val) => bridge.modWheel(val),
    onPanic: () => {
      bridge.allNotesOff();
      bridge.sendMidiCC(123, 0);
      bridge.sendMidiCC(120, 0);
    },
    onSustainChange: (active) => {
      bridge.sendMidiCC(64, active ? 127 : 0);
    },
    onCollapseChange: (collapsed) => {
      const kbdFooter = document.querySelector('.keyboard-container');
      if (kbdFooter) {
        kbdFooter.classList.toggle('collapsed', collapsed);
      }
    },
    onOctaveChange: (oct) => { /* LCD update handled by lcdProgrammer */ },
    config: {
      numOctaves: 4,
      startNote: 36,
      maxOctaveShift: 3,
      velocitySource: 'yPosition',
      velocityCurve: 'normal',
      enableCollapse: true,
      enableIvoryTexture: true,
      enableQwerty: true,
      enableTouch: true,
      enableResizeObserver: false
    }
  });
  // Expose globally so LCD programmer and other modules can trigger animations
  window.__kbd = keyboardInstance;
}



function setupButtons() {
  const panicBtn = document.getElementById('btn-panic');
  panicBtn?.addEventListener('click', () => {
    bridge.allNotesOff();
    if (keyboardInstance) keyboardInstance.panic();
    console.log('[Panic All Notes Off]');
  });

  const randomBtn = document.getElementById('btn-random');
  randomBtn?.addEventListener('click', () => {
    handleMenuAction('randomize-patch');
    if (keyboardInstance) keyboardInstance.sweep('right', 8);
    if (lcdProgrammer) lcdProgrammer.updateSysExDisplay();
  });

  const compareBtn = document.getElementById('btn-compare');
  compareBtn?.addEventListener('click', () => {
    console.log('[Compare A/B Toggled]');
  });


  const initBtn = document.getElementById('btn-init-patch');
  initBtn?.addEventListener('click', () => {
    handleMenuAction('new-patch');
    if (lcdProgrammer) lcdProgrammer.updateSysExDisplay();
  });

  const copySysexBtn = document.getElementById('btn-copy-sysex');
  const sysexHexContent = document.getElementById('sysex-hex-content');
  copySysexBtn?.addEventListener('click', () => {
    if (sysexHexContent) {
      const text = sysexHexContent.textContent.trim();
      navigator.clipboard.writeText(text).then(() => {
        const orig = copySysexBtn.textContent;
        copySysexBtn.textContent = 'COPIED!';
        copySysexBtn.style.background = 'var(--color-accent)';
        copySysexBtn.style.color = '#000';
        setTimeout(() => {
          copySysexBtn.textContent = orig;
          copySysexBtn.style.background = '';
          copySysexBtn.style.color = '';
        }, 1200);
      });
    }
  });
}


function setupKeyboardShortcuts() {
  // QWERTY keyboard is now handled by the unified keyboard component.
  // Global modal shortcuts and Escape key handling
  document.addEventListener('keydown', (e) => {
    if (e.ctrlKey || e.metaKey) {
      if (e.key === 'b' || e.key === 'B') {
        e.preventDefault();
        bankManagerModal?.toggle();
      } else if (e.key === 'o' || e.key === 'O') {
        e.preventDefault();
        handleMenuAction('open-sysex');
      } else if (e.key === 's' || e.key === 'S') {
        e.preventDefault();
        handleMenuAction('save-patch');
      } else if (e.key === 'e' || e.key === 'E') {
        e.preventDefault();
        handleMenuAction('export-sysex');
      }
    }
    if (e.key === 'Escape') {
      closeAllDropdowns();
      diagnosticModal.close();
      bankManagerModal?.close();
      const modal = document.getElementById('about-modal');
      if (modal) {
        modal.style.setProperty('display', 'none', 'important');
        modal.classList.add('hidden');
      }
    }
  });
}
