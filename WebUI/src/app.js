import { bridge, devLog } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';
import { MS2000_THEMES, synthModeIndexOf, themeIdOfSynthMode } from './contracts/themes.js';

import { createScopePanel } from './panels/panelScope.js';
import { renderGroupPanel, renderAllPanels } from './ui/panelFactory.js';
import { paramStore } from './contracts/paramStore.js';
import { LcdProgrammer } from './ui/lcdProgrammer.js';
import { createKeyboard } from '@abdsynths/midi-keyb';
import { ThemeSwitcher, mountFitStage, createOverlayFocus, createAboutDialog } from '@abdsynths/shared/components';
import '@abdsynths/shared/styles/components/widgets.css';
import '@abdsynths/shared/styles/components/skins-junio.css';
import '@abdsynths/shared/styles/components/about-dialog.css';
import '@abdsynths/midi-keyb/keyboard.css';
import '@abdsynths/shared/styles/components/bank.css';
import './styles/slideDrawer.css';
import { slideDrawer } from './components/slideDrawer.js';
import { openWavetableBrowser, syncCatalogFromEngine, getWaveName } from './ui/wavetableBrowser.js';
import { diagnosticModal } from './ui/diagnosticModal.js';
import { BankManagerModal } from '@abdsynths/bank-manager-ui';
import {
  ABDKnob,
  ABDSlider,
  ABDFilmstripFader,
  ABDSelect,
  ABDSegmented,
  ABDSwitch,
  ABDVerticalSlider,
} from './components/shared/index.js';
import { WAVE_ICONS, FILTER_ICONS } from '@abdsynths/shared/components';
import { OscilloscopeModal } from './components/OscilloscopeModal.js';

let themeSwitcher = null; // selector universal (instalado en DOMContentLoaded)

// Tema activo: fuente de verdad el switcher compartido (data-theme en <html>).
// Igual de barato de leer que la variable que era antes.
function currentTheme() {
  return themeSwitcher?.value ?? 'ms2000';
}

let isAudioActive = false;
let lcdProgrammer = null;
let keyboardInstance = null;
let bankManagerModal = null;
let oscilloscopeModal = null;
// Contrato de foco del About (overlayFocus.js) y diálogo reactivo con AutoUpdater
let aboutFocusContract = null;
let aboutDialogInstance = null;
let timbreClipboard = null;
let globalMidiChannel = 1;

document.addEventListener('DOMContentLoaded', () => {
  devLog(`[ABDMS2000 App Init] Version: ${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber})`);

  // Fit-to-viewport (shared fitStage): below the 1080x680 design floor the app
  // scales (whole UI always visible); above it MS2000 stays fluid untouched.
  // Overlays (drawer, modals) live on document.body — outside the stage.
  mountFitStage(document.getElementById('app'), { width: 1080, height: 680, onlyShrink: true });

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
  // Supports direct DOM mounting (mode: 'dom') or sandboxed iframe (mode: 'iframe').
  bankManagerModal = new BankManagerModal({
    mode: 'dom',
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
      devLog('[ParamStore]: Synced all UI controls with APVTS state');
    }
  });

  // Expose bridge globally for headless testing / console access
  window.__ABD_BRIDGE__ = bridge;

  // Request full initial state from host
  bridge.send('requestFullState', {});
});


function setupDashboardCards() {
  // 1. OSCILLATORS & MIXER CARD BUTTONS
  document.getElementById('btn-edit-osc1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILLATOR 1 (VA & DWGS / VOX)', badge: 'OSC 1', sectionId: 'osc1', render: renderOsc1Drawer });
  });
  document.getElementById('btn-edit-osc2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'OSCILLATOR 2 & MODULATION', badge: 'OSC 2', sectionId: 'osc2', render: renderOsc2Drawer });
  });
  document.getElementById('btn-edit-mixer')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'AUDIO MIXER', badge: 'MIXER', sectionId: 'mixer', render: renderMixerDrawer });
  });

  // 2. VOCODER 16 BANDS CARD BUTTONS
  document.getElementById('btn-edit-vocoder-cfg')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'VOCODER: GLOBAL CONFIGURATION', badge: 'VOCODER', sectionId: 'vocoder-cfg', render: renderVocoderConfigDrawer });
  });
  document.getElementById('btn-edit-vocoder-bands')?.addEventListener('click', () => {
    const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
    slideDrawer.open({
      title: isMicrokorg ? 'VOCODER: 8-CHANNEL LEVELS (microKORG)' : 'VOCODER: 16-BAND LEVELS (MS2000)',
      badge: isMicrokorg ? '8 CHANNELS' : '16 BANDS',
      sectionId: 'vocoder-bands',
      render: renderVocoderBandsDrawer
    });
  });

  // 3. FILTER MULTIMODE CARD BUTTONS
  document.getElementById('btn-edit-filter-main')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MULTIMODE FILTER & RESONANCE', badge: 'FILTER', sectionId: 'filter-main', render: renderFilterMainDrawer });
  });
  document.getElementById('btn-edit-filter-mod')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'FILTER: MODULATION & KEY TRACK', badge: 'FILTER MOD', sectionId: 'filter-mod', render: renderFilterModDrawer });
  });

  // 4. AMP & ENVELOPES CARD BUTTONS
  document.getElementById('btn-edit-amp-main')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MAIN AMPLIFIER (VCA)', badge: 'AMP VCA', sectionId: 'amp-main', render: renderAmpMainDrawer });
  });
  document.getElementById('btn-edit-eg1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ENVELOPE 1: FILTER (EG1)', badge: 'EG 1', sectionId: 'eg1', render: renderEg1Drawer });
  });
  document.getElementById('btn-edit-eg2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'ENVELOPE 2: AMPLITUDE (EG2)', badge: 'EG 2', sectionId: 'eg2', render: renderEg2Drawer });
  });

  // 5. LFOS & MOD MATRIX CARD BUTTONS
  document.getElementById('btn-edit-lfo1')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'LOW FREQUENCY OSCILLATOR 1 (LFO 1)', badge: 'LFO 1', sectionId: 'lfo1', render: renderLfo1Drawer });
  });
  document.getElementById('btn-edit-lfo2')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'LOW FREQUENCY OSCILLATOR 2 (LFO 2)', badge: 'LFO 2', sectionId: 'lfo2', render: renderLfo2Drawer });
  });
  document.getElementById('btn-edit-modmatrix')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MODULATION MATRIX (4 V-PATCH SLOTS)', badge: 'V-PATCH', sectionId: 'modmatrix', render: renderModMatrixDrawer });
  });

  // 6. SEQ & ARPEGGIATOR CARD BUTTONS
  document.getElementById('btn-edit-arp')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'PROGRAMMABLE ARPEGGIATOR', badge: 'ARP', sectionId: 'arp', render: renderArpDrawer });
  });
  document.getElementById('btn-edit-modseq')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MODULATION SEQUENCER (3x16)', badge: 'MOD SEQ', sectionId: 'modseq', render: renderModSeqDrawer });
  });

  // 7. FX & EQUALIZER CARD BUTTONS
  document.getElementById('btn-edit-modfx')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MODULATION FX (CHORUS/PHASER)', badge: 'MOD FX', sectionId: 'modfx', render: renderModFxDrawer });
  });
  document.getElementById('btn-edit-delayfx')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'STEREO DELAY FX', badge: 'DELAY', sectionId: 'delayfx', render: renderDelayFxDrawer });
  });
  document.getElementById('btn-edit-eq')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MASTER EQUALIZER (2 BANDS)', badge: 'EQ MASTER', sectionId: 'eq', render: renderEqDrawer });
  });

  // 8. VOICE, PITCH & MASTER CARD BUTTONS
  document.getElementById('btn-edit-voice')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'VOICE ASSIGN & UNISON', badge: 'VOICE', sectionId: 'voice', render: renderVoiceDrawer });
  });
  document.getElementById('btn-edit-pitch')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'PITCH BEND & PORTAMENTO GLIDE', badge: 'PITCH', sectionId: 'pitch', render: renderPitchDrawer });
  });
  document.getElementById('btn-edit-global')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MASTER VOLUME & PEDALS', badge: 'MASTER', sectionId: 'global', render: renderGlobalMasterDrawer });
  });

  setupDashboardObservers();
  devLog('[Dashboard Workspace: All 8 Cards & 19 Sub-actions Mounted]');
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
      <h3>OSCILLATOR 1 WAVEFORM</h3>
      <div id="drawer-sel-osc1-wave" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-osc1ctrl1"></div>
        <div id="drawer-ctrl-osc1ctrl2"></div>
      </div>
    </div>

    <div class="drawer-section-card" id="drawer-dwgs-section" style="display: ${isDwgs ? 'block' : 'none'}; border-color: rgba(0, 212, 255, 0.4); background: rgba(0, 212, 255, 0.04);">
        <h3 style="margin: 0; border: none; padding: 0; color: var(--color-accent); display: flex; align-items: center; gap: 6px;">
          <svg class="ui-icon" viewBox="0 0 24 24" width="16" height="16" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 12c2-2 4-2 6 0s4 2 6 0 4-2 6 0"/><path d="M2 17c2-2 4-2 6 0s4 2 6 0 4-2 6 0"/></svg>
          DWGS TABLES (64 / 512)
        </h3>
        <span class="drawer-badge" id="drawer-dwgs-name">${dwgsName}</span>
      </div>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-ctrl-dwgswave"></div>
        <div style="display: flex; flex-direction: column; gap: 8px; flex: 1; max-width: 280px;">
          <button id="btn-drawer-dwgs-browser" class="card-action-btn" style="background: linear-gradient(180deg, #0284c7 0%, #0369a1 100%); color: #fff; padding: 8px; display: flex; align-items: center; justify-content: center; gap: 6px;">
            <svg class="ui-btn-icon" viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M4 19.5A2.5 2.5 0 0 1 6.5 17H20"/><path d="M6.5 2H20v20H6.5A2.5 2.5 0 0 1 4 19.5v-15A2.5 2.5 0 0 1 6.5 2z"/></svg>
            <span>OPEN DWGS BROWSER</span>
          </button>
          <div style="font-size: 0.72rem; color: #94a3b8;">
            Slot: <strong id="drawer-dwgs-slot-num" style="color: #38bdf8;">#${currentDwgsSlot + 1}</strong> &mdash; <span id="drawer-dwgs-desc-name">${dwgsName}</span>
          </div>
        </div>
      </div>
    </div>
  `;

  const osc1WaveSel = new ABDSegmented(container.querySelector('#drawer-sel-osc1-wave'), {
    paramId: 'osc1Wave',
    label: 'WAVEFORM',
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
    variant: 'strip',
    size: 'sm',
    onChange: (val) => {
      const dwgsSec = container.querySelector('#drawer-dwgs-section');
      if (dwgsSec) dwgsSec.style.display = Number(val) === 5 ? 'block' : 'none';
    }
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-osc1ctrl1'), {
    paramId: 'osc1Ctrl1', label: 'CONTROL 1', min: 0, max: 127, defaultValue: 64, size: 52,
    onChange: (val) => bridge.setParam('osc1Ctrl1', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-osc1ctrl2'), {
    paramId: 'osc1Ctrl2', label: 'CONTROL 2', min: 0, max: 127, defaultValue: 0, size: 52,
    onChange: (val) => bridge.setParam('osc1Ctrl2', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-dwgswave'), {
    paramId: 'osc1DwgsWave', label: 'DWGS WAVE', min: 0, max: currentTheme() === 'advanced' ? 511 : 63, defaultValue: 0, size: 48,
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
    const synthMode = paramStore.get('synthMode') ?? (currentTheme() === 'advanced' ? 2 : (currentTheme() === 'microkorg' ? 1 : 0));
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
      <h3>OSCILLATOR 2 &amp; MODULATION</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 10px;">
        <div id="drawer-sel-osc2-wave"></div>
        <div id="drawer-sel-osc2-mod"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>OSC 2 TUNING &amp; PITCH</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-osc2semi"></div>
        <div id="drawer-ctrl-osc2tune"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-osc2-wave'), {
    paramId: 'osc2Wave',
    label: 'OSC 2 WAVEFORM',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQUARE', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'TRIANGLE', iconSvg: WAVE_ICONS.triangle }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-osc2-mod'), {
    paramId: 'osc2ModType',
    label: 'MODULATION TYPE',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'RING MOD' },
      { value: 2, label: 'SYNC' },
      { value: 3, label: 'CROSS MOD' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-osc2semi'), {
    paramId: 'osc2Semitone', label: 'SEMITONE', min: -24, max: 24, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} st`,
    onChange: (val) => bridge.setParam('osc2Semitone', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-osc2tune'), {
    paramId: 'osc2Tune', label: 'FINE TUNE', min: -50, max: 50, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} cents`,
    onChange: (val) => bridge.setParam('osc2Tune', val)
  });
}

function renderMixerDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>AUDIO MIXER LEVELS</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-mixosc1"></div>
        <div id="drawer-ctrl-mixosc2"></div>
        <div id="drawer-ctrl-mixnoise"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-mixosc1'), {
    paramId: 'mixOsc1Level', label: 'OSC 1 VOL', min: 0, max: 127, defaultValue: 127, size: 54,
    onChange: (val) => bridge.setParam('mixOsc1Level', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-mixosc2'), {
    paramId: 'mixOsc2Level', label: 'OSC 2 VOL', min: 0, max: 127, defaultValue: 0, size: 54,
    onChange: (val) => bridge.setParam('mixOsc2Level', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-mixnoise'), {
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
      <h3>MODE &amp; CARRIER SOURCE</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-vocoder-mode"></div>
        <div id="drawer-sel-vocoder-carrier"></div>
        <div id="drawer-sel-vocoder-shift"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>DYNAMICS &amp; INPUT LEVEL</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-vochpf"></div>
        <div id="drawer-ctrl-vocgate"></div>
        <div id="drawer-ctrl-vocdirect"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-mode'), {
    paramId: 'synthVocoderMode',
    label: 'SYNTH MODE',
    options: [
      { value: 0, label: 'SYNTH (NORMAL)' },
      { value: 1, label: 'VOCODER (VOCAL)' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-carrier'), {
    paramId: 'vocoderCarrierSrc',
    label: 'CARRIER SOURCE',
    options: [
      { value: 0, label: 'INTERNAL SYNTH' },
      { value: 1, label: 'EXTERNAL AUDIO IN' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-shift'), {
    paramId: 'vocoderFormantShift',
    label: 'FORMANT SHIFT',
    options: [
      { value: 0, label: '-2 (LOW)' },
      { value: 1, label: '-1' },
      { value: 2, label: '0 (NORMAL)' },
      { value: 3, label: '+1' },
      { value: 4, label: '+2 (HIGH)' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-vochpf'), {
    paramId: 'vocoderHpfLevel', label: 'HPF LEVEL', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('vocoderHpfLevel', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocgate'), {
    paramId: 'vocoderGateSense', label: 'GATE SENSE', min: 0, max: 127, defaultValue: 50, size: 48,
    onChange: (val) => bridge.setParam('vocoderGateSense', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocdirect'), {
    paramId: 'vocoderDirectLevel', label: 'DIRECT LEVEL', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('vocoderDirectLevel', val)
  });
}

let vocoderBandSliders = [];

function renderVocoderBandsDrawer(container) {
  // Destroy previous instances to avoid memory leaks
  vocoderBandSliders.forEach((s) => s.destroy());
  vocoderBandSliders = [];

  const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');

  if (isMicrokorg) {
    container.innerHTML = `
      <div class="drawer-section-card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
          <h3 style="margin: 0; border: none; padding: 0;">FILTER BANK LEVELS (8 CHANNELS - microKORG)</h3>
          <button id="btn-vocoder-reset-bands" class="card-action-btn" style="max-width: 120px; padding: 4px 8px;">ALL 100%</button>
        </div>
        <div class="vocoder-bands-grid" id="vocoder-bands-grid-el"></div>
      </div>
    `;

    const gridEl = container.querySelector('#vocoder-bands-grid-el');

    for (let ch = 1; ch <= 8; ch++) {
      const cell = document.createElement('div');
      cell.className = 'vocoder-band-cell';
      gridEl.appendChild(cell);

      const bA = (ch * 2) - 1;
      const bB = ch * 2;
      const valA = paramStore.get(`vocoderBandLevel${bA}`) ?? 127;
      const valB = paramStore.get(`vocoderBandLevel${bB}`) ?? 127;
      const initialVal = Math.round((valA + valB) / 2);

      const slider = new ABDVerticalSlider(cell, {
        paramId: `vocoderBandLevel${bA}`,
        label: `CH ${ch}`,
        min: 0,
        max: 127,
        defaultValue: initialVal,
        height: 110,
        trackWidth: 6,
        thumbSize: 16,
        showLabel: true,
        showValue: true,
        onChange: (val) => {
          bridge.setParam(`vocoderBandLevel${bA}`, val);
          bridge.setParam(`vocoderBandLevel${bB}`, val);
          paramStore.set(`vocoderBandLevel${bA}`, val);
          paramStore.set(`vocoderBandLevel${bB}`, val);
        }
      });
      vocoderBandSliders.push(slider);
    }
  } else {
    container.innerHTML = `
      <div class="drawer-section-card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
          <h3 style="margin: 0; border: none; padding: 0;">FILTER BANK LEVELS (16 BANDS - MS2000)</h3>
          <button id="btn-vocoder-reset-bands" class="card-action-btn" style="max-width: 120px; padding: 4px 8px;">ALL 100%</button>
        </div>
        <div class="vocoder-bands-grid" id="vocoder-bands-grid-el"></div>
      </div>
    `;

    const gridEl = container.querySelector('#vocoder-bands-grid-el');

    for (let i = 1; i <= 16; i++) {
      const cell = document.createElement('div');
      cell.className = 'vocoder-band-cell';
      gridEl.appendChild(cell);

      const slider = new ABDVerticalSlider(cell, {
        paramId: `vocoderBandLevel${i}`,
        label: `B${i}`,
        min: 0,
        max: 127,
        defaultValue: 127,
        height: 110,
        trackWidth: 6,
        thumbSize: 16,
        showLabel: true,
        showValue: true,
        onChange: (val) => {
          bridge.setParam(`vocoderBandLevel${i}`, val);
        }
      });
      vocoderBandSliders.push(slider);
    }
  }

  container.querySelector('#btn-vocoder-reset-bands')?.addEventListener('click', () => {
    vocoderBandSliders.forEach((slider) => {
      slider.setValue(127, true);
    });
  });
}

/* ─────────────────────────────────────────────────────────────
   3. FILTER MULTIMODE DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderFilterMainDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>FILTER TYPE &amp; FREQUENCY RESPONSE</h3>
      <div id="drawer-sel-filter-type" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-cutoff"></div>
        <div id="drawer-ctrl-reso"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-filter-type'), {
    paramId: 'filterType',
    label: 'FILTER TYPE',
    options: [
      { value: 0, label: 'LPF 24dB', iconSvg: FILTER_ICONS.lpf24 },
      { value: 1, label: 'LPF 12dB', iconSvg: FILTER_ICONS.lpf12 },
      { value: 2, label: 'BPF 12dB', iconSvg: FILTER_ICONS.bpf12 },
      { value: 3, label: 'HPF 12dB', iconSvg: FILTER_ICONS.hpf12 }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-cutoff'), {
    paramId: 'filterCutoff', label: 'CUTOFF', min: 0, max: 127, defaultValue: 127, size: 56,
    displayFormatter: (v) => {
      const hz = Math.round(20 * Math.pow(1000, v / 127));
      return hz >= 1000 ? `${(hz / 1000).toFixed(1)} kHz` : `${hz} Hz`;
    },
    onChange: (val) => bridge.setParam('filterCutoff', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-reso'), {
    paramId: 'filterResonance', label: 'RESONANCE', min: 0, max: 127, defaultValue: 0, size: 56,
    onChange: (val) => bridge.setParam('filterResonance', val)
  });
}

function renderFilterModDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>CUTOFF MODULATION (EG1 / KEY / VEL)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1int"></div>
        <div id="drawer-ctrl-keytrack"></div>
        <div id="drawer-ctrl-velsens"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-eg1int'), {
    paramId: 'filterEg1Int', label: 'EG1 INT', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('filterEg1Int', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-keytrack'), {
    paramId: 'filterKeyTrack', label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('filterKeyTrack', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-velsens'), {
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
      <h3>VCA AMPLIFIER &amp; DISTORTION</h3>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-ctrl-amplevel"></div>
        <div id="drawer-ctrl-amppan"></div>
        <div id="drawer-ctrl-ampdist" style="min-width: 120px; max-width: 160px;"></div>
        <div id="drawer-ctrl-ampkeytrack"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-amplevel'), {
    paramId: 'ampLevel', label: 'AMP LEVEL', min: 0, max: 127, defaultValue: 100, size: 50,
    onChange: (val) => bridge.setParam('ampLevel', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-amppan'), {
    paramId: 'ampPan', label: 'PAN', min: -64, max: 63, defaultValue: 0, size: 50, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('ampPan', val)
  });
  new ABDSegmented(container.querySelector('#drawer-ctrl-ampdist'), {
    paramId: 'ampDistortion',
    label: 'DISTORTION',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'ON' }
    ],
    variant: 'strip',
    size: 'sm',
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-ampkeytrack'), {
    paramId: 'ampKeyTrack', label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 50, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`,
    onChange: (val) => bridge.setParam('ampKeyTrack', val)
  });
}

function renderEg1Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVELOPE 1: FILTER &amp; PITCH (ADSR)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1a"></div>
        <div id="drawer-ctrl-eg1d"></div>
        <div id="drawer-ctrl-eg1s"></div>
        <div id="drawer-ctrl-eg1r"></div>
      </div>
    </div>
  `;
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1a'), {
    paramId: 'eg1Attack', label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg1Attack', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1d'), {
    paramId: 'eg1Decay', label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('eg1Decay', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1s'), {
    paramId: 'eg1Sustain', label: 'SUSTAIN', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg1Sustain', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1r'), {
    paramId: 'eg1Release', label: 'RELEASE', min: 0, max: 127, defaultValue: 40, size: 48,
    onChange: (val) => bridge.setParam('eg1Release', val)
  });
}

function renderEg2Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVELOPE 2: AMPLITUDE (ADSR)</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg2a"></div>
        <div id="drawer-ctrl-eg2d"></div>
        <div id="drawer-ctrl-eg2s"></div>
        <div id="drawer-ctrl-eg2r"></div>
      </div>
    </div>
  `;
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2a'), {
    paramId: 'eg2Attack', label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48,
    onChange: (val) => bridge.setParam('eg2Attack', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2d'), {
    paramId: 'eg2Decay', label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48,
    onChange: (val) => bridge.setParam('eg2Decay', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2s'), {
    paramId: 'eg2Sustain', label: 'SUSTAIN', min: 0, max: 127, defaultValue: 127, size: 48,
    onChange: (val) => bridge.setParam('eg2Sustain', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2r'), {
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
      <h3>LFO 1: WAVEFORM &amp; FREQUENCY</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-lfo1-wave"></div>
        <div class="drawer-controls-row">
          <div id="drawer-ctrl-lfo1freq"></div>
        </div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>LFO 1 SYNCHRONIZATION</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-lfo1-keysync" style="flex: 1; min-width: 140px;"></div>
          <div id="drawer-sel-lfo1-temposync" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-lfo1-syncnote"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-lfo1-wave'), {
    paramId: 'lfo1Wave',
    label: 'LFO 1 WAVEFORM',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQUARE', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'TRIANGLE', iconSvg: WAVE_ICONS.triangle },
      { value: 3, label: 'S&H', iconSvg: WAVE_ICONS.sh }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo1-keysync'), {
    paramId: 'lfo1KeySync',
    label: 'KEY SYNC',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'TIMBRE' },
      { value: 2, label: 'VOICE' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo1-temposync'), {
    paramId: 'lfo1TempoSync',
    label: 'TEMPO SYNC',
    options: [
      { value: 0, label: 'OFF (FREE)' },
      { value: 1, label: 'ON (BPM)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo1-syncnote'), {
    paramId: 'lfo1SyncNote',
    label: 'SYNC NOTE DIVISION',
    options: syncNotes,
    layout: 'grid-6',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-lfo1freq'), {
    paramId: 'lfo1Freq', label: 'FREQUENCY', min: 0, max: 127, defaultValue: 30, size: 52,
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
      <h3>LFO 2: WAVEFORM &amp; FREQUENCY</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-lfo2-wave"></div>
        <div class="drawer-controls-row">
          <div id="drawer-ctrl-lfo2freq"></div>
        </div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>LFO 2 SYNCHRONIZATION</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-lfo2-keysync" style="flex: 1; min-width: 140px;"></div>
          <div id="drawer-sel-lfo2-temposync" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-lfo2-syncnote"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-lfo2-wave'), {
    paramId: 'lfo2Wave',
    label: 'LFO 2 WAVEFORM',
    options: [
      { value: 0, label: 'SAW', iconSvg: WAVE_ICONS.saw },
      { value: 1, label: 'SQR+', iconSvg: WAVE_ICONS.square },
      { value: 2, label: 'SINE', iconSvg: WAVE_ICONS.sine },
      { value: 3, label: 'S&H', iconSvg: WAVE_ICONS.sh }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo2-keysync'), {
    paramId: 'lfo2KeySync',
    label: 'KEY SYNC',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'TIMBRE' },
      { value: 2, label: 'VOICE' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo2-temposync'), {
    paramId: 'lfo2TempoSync',
    label: 'TEMPO SYNC',
    options: [
      { value: 0, label: 'OFF (FREE)' },
      { value: 1, label: 'ON (BPM)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo2-syncnote'), {
    paramId: 'lfo2SyncNote',
    label: 'SYNC NOTE DIVISION',
    options: syncNotes,
    layout: 'grid-6',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-lfo2freq'), {
    paramId: 'lfo2Freq', label: 'FREQUENCY', min: 0, max: 127, defaultValue: 50, size: 52,
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
      <div class="patch-slot-card" style="margin-bottom: 14px; background: rgba(14, 19, 29, 0.7); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; padding: 12px;">
        <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 8px;">
          <span class="patch-slot-badge">PATCH ${s}</span>
          <div style="display: flex; align-items: center; gap: 8px;">
            <div id="drawer-ctrl-patch${s}-int"></div>
          </div>
        </div>
        <div style="display: flex; flex-direction: column; gap: 8px;">
          <div id="drawer-patch${s}-src-sel"></div>
          <div id="drawer-patch${s}-dest-sel"></div>
        </div>
      </div>
    `;
  }

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>VIRTUAL MODULATION MATRIX (4 SLOTS)</h3>
      ${slotsHtml}
    </div>
  `;

  for (let s = 1; s <= 4; s++) {
    new ABDSegmented(container.querySelector(`#drawer-patch${s}-src-sel`), {
      paramId: `patch${s}Source`,
      label: 'SOURCE',
      options: sources,
      layout: 'grid-8',
      size: 'sm',
    });

    new ABDSegmented(container.querySelector(`#drawer-patch${s}-dest-sel`), {
      paramId: `patch${s}Destination`,
      label: 'DESTINATION',
      options: dests,
      layout: 'grid-8',
      size: 'sm',
    });

    new ABDKnob(container.querySelector(`#drawer-ctrl-patch${s}-int`), {
      paramId: `patch${s}Intensity`, label: 'INT', min: -63, max: 63, defaultValue: 0, size: 40, isBipolar: true,
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
    { value: 2, label: '1/24' },
    { value: 3, label: '1/16' },
    { value: 4, label: '1/12' },
    { value: 5, label: '1/8' },
    { value: 6, label: '1/6' },
    { value: 7, label: '1/4' }
  ];

  const synthMode = paramStore.get('synthMode') || 0;
  const isMaskSupported = (synthMode !== 0);

  let maskSectionHtml = '';
  if (!isMaskSupported) {
    maskSectionHtml = `
      <div class="drawer-section-card" style="border: 1px dashed rgba(255, 170, 0, 0.35); background: rgba(255, 170, 0, 0.04);">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
          <h3 style="margin: 0; border: none; padding: 0; color: #ffaa00;">STEP RHYTHM MASK (8 STEPS)</h3>
          <span class="card-badge" style="background: rgba(255, 170, 0, 0.15); color: #ffaa00; border: 1px solid rgba(255, 170, 0, 0.3); font-size: 10px; padding: 2px 6px; border-radius: 4px;">microKORG / ADVANCED</span>
        </div>
        <p style="color: rgba(255,255,255,0.7); font-size: 12px; margin: 0 0 12px 0; line-height: 1.5;">
          The hardware <strong>MS2000</strong> plays arpeggiator notes continuously without rhythmic step gating (it relies on its dedicated 3-track Modulation Sequencer). 
          The 8-step rhythm mask (individual step mutes &amp; pattern length) is a signature feature of the <strong>microKORG</strong> and <strong>ADVANCED</strong> engine profiles.
        </p>
        <div style="display: flex; gap: 8px;">
          <button id="btn-arp-switch-microkorg" class="card-action-btn" style="flex: 1; padding: 7px 12px; font-size: 11px;">SWITCH TO microKORG PROFILE</button>
          <button id="btn-arp-switch-advanced" class="card-action-btn" style="flex: 1; padding: 7px 12px; font-size: 11px;">SWITCH TO ADVANCED PROFILE</button>
        </div>
      </div>
    `;
  } else {
    maskSectionHtml = `
      <div class="drawer-section-card">
        <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px;">
          <h3 style="margin: 0; border: none; padding: 0;">STEP RHYTHM MASK (8 STEPS)</h3>
          <span class="card-badge" style="background: rgba(0, 230, 118, 0.15); color: #00e676; border: 1px solid rgba(0, 230, 118, 0.35); font-size: 10px; padding: 2px 6px; border-radius: 4px;">
            ${synthMode === 1 ? 'microKORG ENGINE' : 'ADVANCED ENGINE'}
          </span>
        </div>
        <div style="display: flex; flex-direction: column; gap: 14px;">
          <div style="display: flex; justify-content: space-between; align-items: flex-end; flex-wrap: wrap; gap: 10px;">
            <div id="drawer-sel-arp-laststep" style="flex: 1; min-width: 180px;"></div>
            <div style="display: flex; gap: 6px; align-self: flex-end; margin-bottom: 2px;">
              <button id="btn-arp-mask-all" class="card-action-btn" style="padding: 5px 10px; font-size: 11px;" title="Activate all 8 steps">ALL ON</button>
              <button id="btn-arp-mask-odd" class="card-action-btn" style="padding: 5px 10px; font-size: 11px;" title="Odd steps (1, 3, 5, 7)">ODD</button>
              <button id="btn-arp-mask-even" class="card-action-btn" style="padding: 5px 10px; font-size: 11px;" title="Even steps (2, 4, 6, 8)">EVEN</button>
              <button id="btn-arp-mask-invert" class="card-action-btn" style="padding: 5px 10px; font-size: 11px;" title="Invert current step mutes">INVERT</button>
            </div>
          </div>
          <div id="arp-rhythm-steps-container" style="display: grid; grid-template-columns: repeat(8, 1fr); gap: 6px; margin-top: 4px;"></div>
        </div>
      </div>
    `;
  }

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ARPEGGIATOR STATE &amp; MODE</h3>
      <div style="display: flex; flex-direction: column; gap: 14px;">
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
          <div id="drawer-sel-arp-on"></div>
          <div id="drawer-sel-arp-latch"></div>
        </div>
        <div id="drawer-sel-arp-range" style="width: 100%;"></div>
        <div id="drawer-sel-arp-type" style="width: 100%;"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>TEMPO &amp; RHYTHMIC RESOLUTION</h3>
      <div style="display: flex; flex-direction: column; gap: 16px;">
        <div id="drawer-sel-arp-res" style="width: 100%;"></div>
        <div class="drawer-controls-row" style="display: flex; align-items: center; justify-content: center; gap: 48px; padding-top: 4px;">
          <div id="drawer-ctrl-arptempo"></div>
          <div id="drawer-ctrl-arpgate"></div>
        </div>
      </div>
    </div>
    ${maskSectionHtml}
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-arp-on'), {
    paramId: 'arpOn',
    label: 'ARPEGGIATOR',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-arp-latch'), {
    paramId: 'arpLatch',
    label: 'LATCH',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-arp-range'), {
    paramId: 'arpRange',
    label: 'OCTAVE RANGE',
    options: [
      { value: 1, label: '1 OCT' },
      { value: 2, label: '2 OCT' },
      { value: 3, label: '3 OCT' },
      { value: 4, label: '4 OCT' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-arp-type'), {
    paramId: 'arpType',
    label: 'ARPEGGIO TYPE',
    options: [
      { value: 0, label: 'UP' },
      { value: 1, label: 'DOWN' },
      { value: 2, label: 'ALT 1' },
      { value: 3, label: 'ALT 2' },
      { value: 4, label: 'RANDOM' },
      { value: 5, label: 'TRIGGER' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-arp-res'), {
    paramId: 'arpResolution',
    label: 'RESOLUTION',
    options: arpResolutions,
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-arptempo'), {
    paramId: 'arpTempo', label: 'TEMPO', min: 20, max: 300, defaultValue: 120, size: 50,
    displayFormatter: (v) => `${Math.round(v)} BPM`,
    onChange: (val) => bridge.setParam('arpTempo', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-arpgate'), {
    paramId: 'arpGate', label: 'GATE TIME', min: 0, max: 127, defaultValue: 100, size: 50,
    displayFormatter: (v) => `${Math.round((v / 127) * 100)}%`,
    onChange: (val) => bridge.setParam('arpGate', val)
  });

  if (!isMaskSupported) {
    container.querySelector('#btn-arp-switch-microkorg')?.addEventListener('click', () => {
      paramStore.set('synthMode', 1);
      bridge.setParam('synthMode', 1);
      renderArpDrawer(container);
    });
    container.querySelector('#btn-arp-switch-advanced')?.addEventListener('click', () => {
      paramStore.set('synthMode', 2);
      bridge.setParam('synthMode', 2);
      renderArpDrawer(container);
    });
  } else {
    const stepsContainer = container.querySelector('#arp-rhythm-steps-container');

    const renderStepButtons = () => {
      if (!stepsContainer) return;
      const lastStep = paramStore.get('arpLastStep') ?? 8;
      stepsContainer.innerHTML = '';

      for (let s = 1; s <= 8; s++) {
        const isActive = (paramStore.get(`arpStep${s}`) ?? 1) > 0.5;
        const isWithinLength = s <= lastStep;

        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = `arp-step-btn ${isActive ? 'active' : 'rest'} ${!isWithinLength ? 'out-of-range' : ''}`;
        btn.style.cssText = `
          display: flex;
          flex-direction: column;
          align-items: center;
          justify-content: center;
          gap: 6px;
          padding: 8px 2px;
          background: ${isActive ? 'rgba(0, 230, 118, 0.08)' : 'rgba(255, 255, 255, 0.03)'};
          border: 1px solid ${isActive ? 'rgba(0, 230, 118, 0.3)' : 'rgba(255, 255, 255, 0.1)'};
          border-radius: 6px;
          cursor: ${isWithinLength ? 'pointer' : 'not-allowed'};
          opacity: ${isWithinLength ? '1' : '0.3'};
          transition: all 0.15s ease;
          user-select: none;
        `;

        btn.innerHTML = `
          <span style="font-size: 11px; font-weight: 700; color: rgba(255,255,255,0.8); font-family: monospace;">${s}</span>
          <div style="width: 14px; height: 6px; border-radius: 3px; background: ${isActive ? '#00e676' : 'rgba(255,255,255,0.15)'}; box-shadow: ${isActive ? '0 0 8px rgba(0,230,118,0.8)' : 'none'}; border: 1px solid rgba(0,0,0,0.5);"></div>
          <span style="font-size: 9px; font-weight: 600; letter-spacing: 0.5px; color: ${isActive ? '#00e676' : 'rgba(255,255,255,0.4)'};">${isActive ? 'PLAY' : 'REST'}</span>
        `;

        if (isWithinLength) {
          btn.addEventListener('click', () => {
            const current = (paramStore.get(`arpStep${s}`) ?? 1) > 0.5 ? 1 : 0;
            const nextVal = current === 1 ? 0 : 1;
            paramStore.set(`arpStep${s}`, nextVal);
            bridge.setParam(`arpStep${s}`, nextVal);
            renderStepButtons();
          });
        }

        stepsContainer.appendChild(btn);
      }
    };

    new ABDSegmented(container.querySelector('#drawer-sel-arp-laststep'), {
      paramId: 'arpLastStep',
      label: 'PATTERN LENGTH (LAST STEP)',
      options: [
        { value: 1, label: '1' },
        { value: 2, label: '2' },
        { value: 3, label: '3' },
        { value: 4, label: '4' },
        { value: 5, label: '5' },
        { value: 6, label: '6' },
        { value: 7, label: '7' },
        { value: 8, label: '8' },
      ],
      layout: 'grid-8',
      size: 'sm',
      onChange: (val) => {
        bridge.setParam('arpLastStep', val);
        renderStepButtons();
      }
    });

    container.querySelector('#btn-arp-mask-all')?.addEventListener('click', () => {
      for (let s = 1; s <= 8; s++) {
        paramStore.set(`arpStep${s}`, 1);
        bridge.setParam(`arpStep${s}`, 1);
      }
      renderStepButtons();
    });

    container.querySelector('#btn-arp-mask-odd')?.addEventListener('click', () => {
      for (let s = 1; s <= 8; s++) {
        const val = (s % 2 === 1) ? 1 : 0;
        paramStore.set(`arpStep${s}`, val);
        bridge.setParam(`arpStep${s}`, val);
      }
      renderStepButtons();
    });

    container.querySelector('#btn-arp-mask-even')?.addEventListener('click', () => {
      for (let s = 1; s <= 8; s++) {
        const val = (s % 2 === 0) ? 1 : 0;
        paramStore.set(`arpStep${s}`, val);
        bridge.setParam(`arpStep${s}`, val);
      }
      renderStepButtons();
    });

    container.querySelector('#btn-arp-mask-invert')?.addEventListener('click', () => {
      for (let s = 1; s <= 8; s++) {
        const cur = (paramStore.get(`arpStep${s}`) ?? 1) > 0.5 ? 1 : 0;
        const val = cur === 1 ? 0 : 1;
        paramStore.set(`arpStep${s}`, val);
        bridge.setParam(`arpStep${s}`, val);
      }
      renderStepButtons();
    });

    renderStepButtons();
  }
}

function renderModSeqDrawer(container) {
  const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
  if (isMicrokorg) {
    container.innerHTML = `
      <div class="drawer-section-card" style="text-align: center; padding: 24px 16px;">
        <div style="display: inline-flex; align-items: center; justify-content: center; width: 44px; height: 44px; border-radius: 50%; background: rgba(255, 170, 0, 0.15); color: #ffaa00; margin-bottom: 14px;">
          <svg viewBox="0 0 24 24" width="24" height="24" fill="none" stroke="currentColor" stroke-width="2"><circle cx="12" cy="12" r="10"/><line x1="12" y1="8" x2="12" y2="12"/><line x1="12" y1="16" x2="12.01" y2="16"/></svg>
        </div>
        <h3 style="color: #ffaa00; margin: 0 0 8px 0; border: none; padding: 0;">MODULATION SEQUENCER UNAVAILABLE</h3>
        <p style="color: rgba(255,255,255,0.7); max-width: 440px; margin: 0 auto 14px auto; font-size: 13px; line-height: 1.5;">
          The hardware <strong>microKORG</strong> does not include a physical Modulation Sequencer (exclusive to the Korg MS2000 16-key step architecture).
        </p>
        <p style="color: rgba(255,255,255,0.45); font-size: 12px; margin: 0 auto 18px auto; max-width: 420px;">
          Switch your active profile to <strong>MS2000</strong> or <strong>ADVANCED</strong> to enable 3-track 16-step parameter motion recording.
        </p>
        <button id="btn-modseq-switch-profile" class="card-action-btn" style="padding: 7px 16px; margin: 0 auto; font-size: 12px;">SWITCH TO MS2000 PROFILE</button>
      </div>
    `;
    container.querySelector('#btn-modseq-switch-profile')?.addEventListener('click', () => {
      paramStore.set('synthMode', 0);
      bridge.setParam('synthMode', 0);
    });
    return;
  }

  const modSeqResolutions = [
    { value: 0, label: '1/48' }, { value: 1, label: '1/32' }, { value: 2, label: '1/24' },
    { value: 3, label: '1/16' }, { value: 4, label: '1/12' }, { value: 5, label: '1/8' },
    { value: 6, label: '1/6' },  { value: 7, label: '1/4' },  { value: 8, label: '1/3' }, { value: 9, label: '1/2' }
  ];

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODULATION SEQUENCER (3 TRACKS x 16 STEPS)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px;">
        <div style="display: flex; gap: 12px; flex-wrap: wrap;">
          <div id="drawer-sel-modseq-on" style="flex: 1; min-width: 120px;"></div>
          <div id="drawer-sel-modseq-smooth" style="flex: 1; min-width: 140px;"></div>
        </div>
        <div id="drawer-sel-modseq-type"></div>
        <div id="drawer-sel-modseq-res"></div>
        <div id="drawer-sel-modseq-laststep"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-on'), {
    paramId: 'modSeqOn',
    label: 'MOD SEQUENCER',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  // La transición es **por fila** (bit 0 del byte de movimiento de cada fila): aquí se
  // edita la de la fila A; B y C tienen su propio `seq2Motion` / `seq3Motion`.
  new ABDSegmented(container.querySelector('#drawer-sel-modseq-smooth'), {
    paramId: 'seq1Motion',
    label: 'TRACK 1 MOTION',
    options: [{ value: 0, label: 'STEP' }, { value: 1, label: 'SMOOTH' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-type'), {
    paramId: 'modSeqType',
    label: 'SEQUENCE DIRECTION',
    options: [
      { value: 0, label: 'FORWARD' },
      { value: 1, label: 'REVERSE' },
      { value: 2, label: 'BOUNCE' },
      { value: 3, label: 'RANDOM' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-res'), {
    paramId: 'modSeqResolution',
    label: 'TIME RESOLUTION',
    options: modSeqResolutions,
    layout: 'grid-5',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-laststep'), {
    paramId: 'seqLastStep',
    label: 'LAST STEP',
    options: Array.from({ length: 16 }, (_, i) => ({
      value: i + 1,
      label: `${i + 1}`
    })),
    layout: 'grid-8',
    size: 'sm',
  });
}

/* ─────────────────────────────────────────────────────────────
   7. FX (MOD FX, DELAY, EQ) DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderModFxDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODULATION EFFECTS (CHORUS / ENSEMBLE / PHASER)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-modfx-on"></div>
        <div id="drawer-sel-modfx-type"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>MOD FX PARAMETERS</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-modfxspeed"></div>
        <div id="drawer-ctrl-modfxdepth"></div>
        <div id="drawer-ctrl-modfxfb"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-modfx-on'), {
    paramId: 'modFxOn',
    label: 'MOD FX POWER',
    options: [{ value: 0, label: 'BYPASS' }, { value: 1, label: 'ACTIVE (ON)' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modfx-type'), {
    paramId: 'modFxType',
    label: 'MODULATION TYPE',
    options: [
      { value: 0, label: 'CHORUS / FLANGER' },
      { value: 1, label: 'ENSEMBLE' },
      { value: 2, label: 'PHASER' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-modfxspeed'), {
    paramId: 'modFxSpeed', label: 'SPEED', min: 0, max: 127, defaultValue: 40, size: 50,
    onChange: (val) => bridge.setParam('modFxSpeed', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-modfxdepth'), {
    paramId: 'modFxDepth', label: 'DEPTH', min: 0, max: 127, defaultValue: 64, size: 50,
    onChange: (val) => bridge.setParam('modFxDepth', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-modfxfb'), {
    paramId: 'modFxFeedback', label: 'FEEDBACK', min: 0, max: 127, defaultValue: 0, size: 50,
    onChange: (val) => bridge.setParam('modFxFeedback', val)
  });
}

function renderDelayFxDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>STEREO DELAY</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-delay-on"></div>
        <div id="drawer-sel-delay-type"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>DELAY TIME &amp; FEEDBACK</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-delaytime"></div>
        <div id="drawer-ctrl-delaydepth"></div>
        <div id="drawer-ctrl-delayfb"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-delay-on'), {
    paramId: 'delayOn',
    label: 'DELAY POWER',
    options: [{ value: 0, label: 'BYPASS' }, { value: 1, label: 'ACTIVE (ON)' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-delay-type'), {
    paramId: 'delayType',
    label: 'DELAY TYPE',
    options: [
      { value: 0, label: 'STEREO DELAY' },
      { value: 1, label: 'CROSS (PING-PONG)' },
      { value: 2, label: 'LEFT / RIGHT' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-delaytime'), {
    paramId: 'delayTime', label: 'TIME', min: 0, max: 127, defaultValue: 40, size: 50,
    onChange: (val) => bridge.setParam('delayTime', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-delaydepth'), {
    paramId: 'delayDepth', label: 'DEPTH (WET)', min: 0, max: 127, defaultValue: 50, size: 50,
    onChange: (val) => bridge.setParam('delayDepth', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-delayfb'), {
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
      <h3>MASTER EQUALIZER (2-BAND SHELVING)</h3>
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

  new ABDSegmented(container.querySelector('#drawer-sel-eq-lowfreq'), {
    paramId: 'eqLowFreq',
    label: 'LOW FREQ',
    options: lowFreqs,
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-eqlowgain'), {
    paramId: 'eqLowGain', label: 'LOW GAIN', min: 0, max: 127, defaultValue: 64, size: 50, isBipolar: true,
    displayFormatter: (v) => `${((v - 64) * (12 / 64)).toFixed(1)} dB`,
    onChange: (val) => bridge.setParam('eqLowGain', val)
  });

  new ABDSegmented(container.querySelector('#drawer-sel-eq-highfreq'), {
    paramId: 'eqHighFreq',
    label: 'HIGH FREQ',
    options: highFreqs,
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-eqhighgain'), {
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
      <h3>VOICE ASSIGN &amp; UNISON MODE</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-voice-mode"></div>
        <div id="drawer-sel-synth-profile"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>UNISON DETUNE &amp; STEREO SPREAD</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-unisondetune"></div>
        <div id="drawer-ctrl-unisonspread"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-voice-mode'), {
    paramId: 'voiceMode',
    label: 'VOICE ASSIGN',
    options: [
      { value: 0, label: 'MONO' },
      { value: 1, label: 'POLY (4V)' },
      { value: 2, label: 'UNISON (4V)' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-synth-profile'), {
    paramId: 'synthMode',
    label: 'SYNTH PROFILE',
    options: [
      { value: 0, label: 'MS2000' },
      { value: 1, label: 'MICROKORG' },
      { value: 2, label: 'ADVANCED' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-unisondetune'), {
    paramId: 'unisonDetune', label: 'UNISON DETUNE', min: 0, max: 99, defaultValue: 10, size: 50,
    displayFormatter: (v) => `${Math.round(v)} c`,
    onChange: (val) => bridge.setParam('unisonDetune', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-unisonspread'), {
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

  new ABDSegmented(container.querySelector('#drawer-sel-porta-on'), {
    paramId: 'portamentoOn',
    label: 'PORTAMENTO',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-portatime'), {
    paramId: 'portamentoTime', label: 'PORTA TIME', min: 0, max: 127, defaultValue: 0, size: 52,
    onChange: (val) => bridge.setParam('portamentoTime', val)
  });
}

function renderGlobalMasterDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MASTER VOLUME &amp; ASSIGNABLE CONTROLLERS</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-mastervol"></div>
        <div id="drawer-ctrl-pedal"></div>
        <div id="drawer-ctrl-assign-switch"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-mastervol'), {
    paramId: 'masterVolume', label: 'MASTER VOL', min: 0, max: 1, defaultValue: 0.8, size: 54,
    displayFormatter: (v) => `${Math.round(v * 100)}%`,
    onChange: (val) => bridge.setParam('masterVolume', val)
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-pedal'), {
    paramId: 'assignablePedal', label: 'ASSIGN PEDAL', min: 0, max: 127, defaultValue: 0, size: 54,
    onChange: (val) => bridge.setParam('assignablePedal', val)
  });

  new ABDSegmented(container.querySelector('#drawer-ctrl-assign-switch'), {
    paramId: 'assignableSwitch',
    label: 'ASSIGN SWITCH',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'ON' }
    ],
    variant: 'strip',
    size: 'md',
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
  const updateVocoderCard = () => {
    const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
    const badge = document.getElementById('badge-vocoder-mode');
    const txt = document.getElementById('val-vocoder-status');
    const title = document.getElementById('title-vocoder-card');
    const btnBands = document.getElementById('btn-edit-vocoder-bands');
    const on = (paramStore.get('synthVocoderMode') || 0) > 0.5;

    if (title) title.textContent = isMicrokorg ? '8-CHANNEL VOCODER' : '16-BAND VOCODER';
    if (btnBands) btnBands.textContent = isMicrokorg ? '8 CHANNELS' : '16 BANDS';
    if (badge) badge.textContent = on ? 'VOCODER' : 'SYNTH';
    if (txt) {
      if (on) {
        txt.textContent = isMicrokorg ? 'ON (8 Channels Active)' : 'ON (16 Bands Active)';
      } else {
        txt.textContent = 'OFF (Synth Mode)';
      }
    }
  };
  paramStore.onChange('synthVocoderMode', updateVocoderCard);
  paramStore.onChange('synthMode', updateVocoderCard);

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
      const synthMode = paramStore.get('synthMode') || 0;
      if (synthMode !== 0) {
        const lastStep = paramStore.get('arpLastStep') || 8;
        let activeSteps = 0;
        for (let i = 1; i <= lastStep; i++) {
          if ((paramStore.get(`arpStep${i}`) ?? 1) > 0.5) activeSteps++;
        }
        txt.textContent = `ON (${t} / ${r} Oct) [${activeSteps}/${lastStep} Steps]`;
      } else {
        txt.textContent = `ON (${t} / ${r} Oct)`;
      }
    }
  };
  paramStore.onChange('arpOn', updateArpCard);
  paramStore.onChange('arpType', updateArpCard);
  paramStore.onChange('arpRange', updateArpCard);
  paramStore.onChange('arpLastStep', updateArpCard);
  for (let s = 1; s <= 8; s++) {
    paramStore.onChange(`arpStep${s}`, updateArpCard);
  }

  const updateModSeqCard = () => {
    const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
    const txt = document.getElementById('val-modseq-status');
    const badge = document.querySelector('#card-seq-arp .card-badge');
    const btnModSeq = document.getElementById('btn-edit-modseq');

    if (badge) {
      badge.textContent = isMicrokorg ? 'ARPEGGIATOR' : '3x16 STEPS';
    }
    if (btnModSeq) {
      if (isMicrokorg) {
        btnModSeq.style.opacity = '0.4';
        btnModSeq.title = 'Modulation Sequencer is not available in microKORG hardware profile';
      } else {
        btnModSeq.style.opacity = '1';
        btnModSeq.title = '';
      }
    }

    if (!txt) return;
    if (isMicrokorg) {
      txt.textContent = 'N/A (microKORG)';
      txt.style.opacity = '0.6';
      return;
    }
    txt.style.opacity = '1';
    const on = (paramStore.get('modSeqOn') || 0) > 0.5;
    if (!on) {
      txt.textContent = 'OFF';
    } else {
      const dirs = ['Forward', 'Reverse', 'Bounce', 'Random'];
      const dir = dirs[paramStore.get('modSeqType') || 0] || 'Forward';
      const motion = ['Step', 'Smooth'][paramStore.get('seq1Motion') || 0] || 'Step';
      const last = Math.round(paramStore.get('seqLastStep') || 16);
      txt.textContent = `ON (${dir} / ${motion} / ${last} steps)`;
    }
  };
  paramStore.onChange('modSeqOn', updateModSeqCard);
  paramStore.onChange('modSeqType', updateModSeqCard);
  paramStore.onChange('seq1Motion', updateModSeqCard);
  paramStore.onChange('seqLastStep', updateModSeqCard);
  paramStore.onChange('synthMode', updateModSeqCard);

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

      // Sincronizar todos los parámetros activos de paramStore (incluyendo FX) al motor de audio
      if (paramStore && paramStore.values) {
        for (const [k, v] of paramStore.values.entries()) {
          bridge.setParam(k, v);
        }
      }
    }
  });
}

/**
 * El selector unico de temas: un <select> compacto en la nav-bar (en vez de
 * los tres botones .mode-tab, que la interfaz ya no necesita). La MECANICA es
 * la del ThemeSwitcher compartido; los TEMAS son datos locales (contracts/themes.js)
 * y los efectos (synthMode al bridge, skin en <body>, bank manager) siguen en casa.
 */
function setupThemeSelector() {
  const host = document.getElementById('mode-selector');
  if (!host) return;
  host.innerHTML = '';

  themeSwitcher = new ThemeSwitcher(host, {
    themes: MS2000_THEMES,
    variant: 'select',
    onChange: (mode, modeIdx) => applyThemeEffects(mode, modeIdx),
  });

  // Sincronizacion bidireccional: cuando synthMode cambia en el store (ej. desde el drawer de Voice),
  // reflejar el cambio en el selector de tema y skin visual sin disparar bucle.
  paramStore.onChange('synthMode', (modeIdx) => {
    const targetThemeId = themeIdOfSynthMode(modeIdx);
    if (themeSwitcher && themeSwitcher.value !== targetThemeId) {
      themeSwitcher.setValue(targetThemeId, { fromUser: false });
      bankManagerModal?.setTheme(targetThemeId);
      devLog('[Sync Theme from synthMode]:', targetThemeId, 'modeIdx:', modeIdx);
    }
    if (modeIdx === 1) {
      paramStore.set('modSeqOn', 0);
      bridge.setParam('modSeqOn', 0);
    }
    if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'vocoder-bands') {
      const isMk = (targetThemeId === 'microkorg');
      if (slideDrawer.titleElem) {
        slideDrawer.titleElem.textContent = isMk ? 'VOCODER: 8-CHANNEL LEVELS (microKORG)' : 'VOCODER: 16-BAND LEVELS (MS2000)';
      }
      if (slideDrawer.badgeElem) {
        slideDrawer.badgeElem.textContent = isMk ? '8 CHANNELS' : '16 BANDS';
      }
      renderVocoderBandsDrawer(slideDrawer.contentElem);
    }
    if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'modseq') {
      renderModSeqDrawer(slideDrawer.contentElem);
    }
    if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'arp') {
      renderArpDrawer(slideDrawer.contentElem);
    }
    updateArpCard();
  });
}

/**
 * Los efectos del tema — lo que el MS2000 hace CUANDO cambia el tema. La
 * aplicacion visual (data-theme, clase de skin) la hace el switcher; aqui queda
 * solo lo del synth: synthMode al store y al nativo, y el bank manager.
 */
function applyThemeEffects(mode, modeIdx = synthModeIndexOf(mode)) {
  paramStore.set('synthMode', modeIdx);
  bridge.setParam('synthMode', modeIdx);
  if (modeIdx === 1) {
    paramStore.set('modSeqOn', 0);
    bridge.setParam('modSeqOn', 0);
  }
  bankManagerModal?.setTheme(mode);
  devLog('[Theme Switch]:', mode, 'modeIdx:', modeIdx);
  if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'arp') {
    renderArpDrawer(slideDrawer.contentElem);
  }
  updateArpCard();
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
  devLog(`[Menu Action Triggered]: ${action}`);
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
    case 'check-updates':
      openAboutModal();
      aboutDialogInstance?.checkForUpdates();
      break;
    case 'voice-mode-single':
      paramStore.set('voiceMode', 0);
      bridge.setParam('voiceMode', 0);
      devLog('[Voice Mode]: Single / Mono');
      break;
    case 'voice-mode-poly':
      paramStore.set('voiceMode', 1);
      bridge.setParam('voiceMode', 1);
      devLog('[Voice Mode]: Poly');
      break;
    case 'voice-mode-unison':
      paramStore.set('voiceMode', 2);
      bridge.setParam('voiceMode', 2);
      devLog('[Voice Mode]: Unison');
      break;
    case 'skin-ms2000':
    case 'skin-microkorg':
    case 'skin-cyberpunk': {
      // Ruta unica: el selector universal es el dueno del tema y su onChange
      // aplica los efectos (no repetirlos aqui). Sin switcher, fallback directo.
      const themeId = action.replace('skin-', '').replace('cyberpunk', 'advanced');
      if (themeSwitcher) {
        themeSwitcher.setValue(themeId);
      } else {
        applyThemeEffects(themeId);
      }
      break;
    }
    case 'new-patch':
    case 'factory-reset':
      paramStore.generateInit(bridge);
      bridge.send('initPatch', {});
      const lcdInit = document.getElementById('lcd-line-2');
      if (lcdInit) lcdInit.textContent = '[Init Synth]    ';
      devLog('[Preset Reset to Default]');
      break;
    case 'randomize-patch':
    case 'randomize':
      paramStore.generateMusicalRandom(bridge);
      bridge.send('randomizePatch', {});
      const lcdRand = document.getElementById('lcd-line-2');
      if (lcdRand) lcdRand.textContent = '[Random Patch]  ';
      devLog('[Musical Random Patch Generated]');
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
      devLog('[Timbre 1 Copied to Clipboard]');
      break;
    case 'paste-timbre':
      if (timbreClipboard) {
        Object.entries(timbreClipboard).forEach(([k, v]) => {
          const targetKey = k.replace('1', '2');
          paramStore.set(targetKey, v);
          bridge.setParam(targetKey, v);
        });
        devLog('[Timbre Pasted to Timbre 2]');
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
  devLog('[Menu Action]: Save Patch - exporting current patch as .syx');
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
      devLog(`[SysEx Import Sent]: ${file.name} (${bytes.length} bytes)`);
    };
    reader.readAsArrayBuffer(file);
  };

  document.body.appendChild(fileInput);
  fileInput.click();
  setTimeout(() => fileInput.remove(), 1000);
}

function setupAboutModal() {
  aboutDialogInstance = createAboutDialog({
    id: 'about-modal',
    appName: 'ABDMS2000',
    subtitle: 'Virtual Analog / DWGS Synthesizer Emulator',
    description: 'Tri-Mode PolyBLEP VA + DWGS 64 Waves + 16-Band Vocoder.',
    version: BUILD_INFO.version,
    buildNumber: BUILD_INFO.buildNumber,
    repo: 'ajabadia/ABDMS2000',
    manualUrl: 'https://github.com/ajabadia/ABDMS2000#readme',
    getJuceBackend: () => ({
      openBrowserUrl: (url) => bridge.send('openBrowserUrl', { url }),
      checkForUpdates: () => {},
    })
  });

  setupSysExEvents();
}

function openAboutModal() {
  aboutDialogInstance?.open();
}

function closeAboutModal() {
  aboutDialogInstance?.close();
}

function setupSysExEvents() {
  // Bridge event listeners for SysEx import/export

  bridge.on('sysexImportResult', (data) => {
    if (data && data.success) {
      devLog(`[SysEx Import Success]: ${data.programName} (${data.programCount} programs)`);
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
      devLog(`[SysEx Program Exported]: ${bytes.length} bytes downloaded.`);
    }
  });
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
      enableResizeObserver: true
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
    devLog('[Panic All Notes Off]');
  });

  const randomBtn = document.getElementById('btn-random');
  randomBtn?.addEventListener('click', () => {
    handleMenuAction('randomize-patch');
    if (keyboardInstance) keyboardInstance.sweep('right', 8);
    if (lcdProgrammer) lcdProgrammer.updateSysExDisplay();
  });

  const compareBtn = document.getElementById('btn-compare');
  compareBtn?.addEventListener('click', () => {
    devLog('[Compare A/B Toggled]');
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
      closeAboutModal();   // por el contrato: foco al disparador + inert
    }
  });
}
