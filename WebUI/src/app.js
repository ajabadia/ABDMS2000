import { bridge, devLog } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';
import { MS2000_THEMES, synthModeIndexOf, themeIdOfSynthMode } from './contracts/themes.js';

import { paramStore } from './contracts/paramStore.js';
import { LcdProgrammer } from './ui/lcdProgrammer.js';
import { createKeyboard } from '@abdsynths/midi-keyb';
import { ThemeSwitcher, mountFitStage, createAboutDialog, createLiveSysExInspector } from '@abdsynths/shared/components';
import '@abdsynths/shared/styles/components/widgets.css';
import '@abdsynths/shared/styles/components/skins-junio.css';
import '@abdsynths/shared/styles/components/about-dialog.css';
import '@abdsynths/shared/styles/components/live-sysex-inspector.css';
import '@abdsynths/midi-keyb/keyboard.css';
import '@abdsynths/shared/styles/components/bank.css';
import './styles/slideDrawer.css';
import { slideDrawer } from './components/slideDrawer.js';
import { openWavetableBrowser, syncCatalogFromEngine, getWaveName } from './ui/wavetableBrowser.js';
import { diagnosticModal } from './ui/diagnosticModal.js';
import { firmwareUpdaterModal } from './ui/firmwareUpdaterModal.js';
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
import { FACTORY_PROGRAMS, generateMS2000SysExBuffer } from './contracts/factoryPresets.js';

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
// Diálogo reactivo About con AutoUpdater
let aboutDialogInstance = null;
let liveSysExInspectorInstance = null;
let isSysExInspectorVisible = false;
let timbreClipboard = null;
let globalMidiChannel = 1;
let activeTimbre = 1; // 1 = Timbre 1, 2 = Timbre 2
let activeModSeqTrack = 1; // 1 = SEQ 1, 2 = SEQ 2, 3 = SEQ 3

// Korg MS2000 16-Button Hardware Strip State
// ms2kStripSeqMode: 0 = Program/Edit, 1 = SEQ 1 / [CH LEVEL], 2 = SEQ 2 / [CH PAN], 3 = SEQ 3
let ms2kStripSeqMode = 0;
let ms2kPadKbdMode = false;
let ms2kPadOctaveShift = 0; // -3 .. +3 (0 = C4..D#5, MIDI 60..75)
const ms2kLatchedPadNotes = new Set();
const ms2kActivePadNotes = new Map();
const ms2kVocoderBandPans = new Float32Array(16); // -63 .. +63 (0 = CNT)
const ms2kVocoderSavedLevels = new Float32Array(16).fill(127);
let ms2kStepChaserIdx = 0;
let ms2kStepChaserTimer = null;

/**
 * Resolves a base parameter ID (e.g. 'osc1Wave') to the currently active timbre's
 * parameter ID ('osc1Wave' for Timbre 1, 't2Osc1Wave' for Timbre 2 if per-timbre).
 */
function tParam(baseId) {
  if (activeTimbre === 2) {
    const t2Id = 't2' + baseId.charAt(0).toUpperCase() + baseId.slice(1);
    if (PARAM_LOOKUP[t2Id]) return t2Id;
  }
  return baseId;
}

function setActiveTimbre(timbreNum, silent = false) {
  activeTimbre = timbreNum === 2 ? 2 : 1;
  const btnT1 = document.getElementById('btn-timbre-1');
  const btnT2 = document.getElementById('btn-timbre-2');
  if (btnT1) btnT1.classList.toggle('active', activeTimbre === 1);
  if (btnT2) btnT2.classList.toggle('active', activeTimbre === 2);
  document.body.setAttribute('data-active-timbre', String(activeTimbre));

  if (lcdProgrammer) {
    lcdProgrammer.activeTimbre = activeTimbre;
  }

  slideDrawer.setTimbre(activeTimbre);
  refreshAllDashboardCards();
  refreshMs2kBottomStrip();

  if (slideDrawer.isOpenState) {
    slideDrawer.refresh();
  }
  if (!silent && lcdProgrammer) {
    lcdProgrammer.flashMessage(
      `EDIT TIMBRE ${activeTimbre}`,
      activeTimbre === 2 ? 'SPLIT/LAYER T2' : 'MAIN TIMBRE 1'
    );
  }
}

function getLiveSysExInspector() {
  if (!liveSysExInspectorInstance) {
    const prog = (lcdProgrammer && lcdProgrammer.getCurrentProgram) ? lcdProgrammer.getCurrentProgram() : FACTORY_PROGRAMS[0];
    const paramsMap = paramStore.values ? Object.fromEntries(paramStore.values) : (prog ? prog.params : {});
    const initialBuf = generateMS2000SysExBuffer(paramsMap, prog ? prog.name : 'Init Prog', globalMidiChannel, prog);

    liveSysExInspectorInstance = createLiveSysExInspector({
      title: 'MS2000 Live SysEx Inspector',
      floating: true,
      bytesPerRow: 16,
      initialBuffer: initialBuf,
      onClose: () => {
        isSysExInspectorVisible = false;
      },
      onOpen: () => {
        isSysExInspectorVisible = true;
      },
      onExport: (blob, filename) => {
        const p = (lcdProgrammer && lcdProgrammer.getCurrentProgram) ? lcdProgrammer.getCurrentProgram() : { name: 'MS2000_Patch' };
        const fname = filename || `${p.name.trim().replace(/\s+/g, '_')}.syx`;
        const url = URL.createObjectURL(blob);
        const a = document.createElement('a');
        a.href = url;
        a.download = fname;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        URL.revokeObjectURL(url);
      }
    });
    liveSysExInspectorInstance.hide();
  }
  return liveSysExInspectorInstance;
}

function toggleLiveSysExInspector() {
  const inspector = getLiveSysExInspector();
  inspector.toggle();
  isSysExInspectorVisible = inspector.isOpen();
  if (isSysExInspectorVisible) {
    updateLiveSysExInspector();
  }
}

function updateLiveSysExInspector() {
  if (!liveSysExInspectorInstance || !liveSysExInspectorInstance.isOpen()) return;
  const prog = (lcdProgrammer && lcdProgrammer.getCurrentProgram) ? lcdProgrammer.getCurrentProgram() : FACTORY_PROGRAMS[0];
  const paramsMap = paramStore.values ? Object.fromEntries(paramStore.values) : (prog ? prog.params : {});
  const buf = generateMS2000SysExBuffer(paramsMap, prog ? prog.name : 'Init Prog', globalMidiChannel, prog);
  liveSysExInspectorInstance.updateBuffer(buf);
}

window.__onMS2000ParamMutated = (paramId, val) => {
  if (isSysExInspectorVisible) {
    updateLiveSysExInspector();
  }
  if (lcdProgrammer && paramId) {
    lcdProgrammer.showParameterFeedback(paramId, val);
  }
};

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
  setupMidiConfigModal();
  setupKeyboardShortcuts();
  diagnosticModal.init();
  firmwareUpdaterModal.init();
  oscilloscopeModal = new OscilloscopeModal(bridge);

  // Wire SlideDrawer timbre switcher to global activeTimbre
  slideDrawer.onTimbreChange = (t) => setActiveTimbre(t);

  // Instantiate Universal Bank Manager Modal (Full Native ABDBankManager WebUI).
  // Supports direct DOM mounting (mode: 'dom') or sandboxed iframe (mode: 'iframe').
  bankManagerModal = new BankManagerModal({
    mode: 'dom',
    synthBridge: bridge
  });

  document.getElementById('btn-open-bank-manager')?.addEventListener('click', () => {
    bankManagerModal?.toggle();
  });

  document.getElementById('btn-open-scope')?.addEventListener('click', () => {
    oscilloscopeModal?.toggle();
  });

  document.getElementById('diag-btn-open-full-scope')?.addEventListener('click', () => {
    oscilloscopeModal?.open();
  });

  // Sync catalog from engine
  syncCatalogFromEngine(bridge);

  // Instantiate LCD Programmer Navigation Controller
  lcdProgrammer = new LcdProgrammer(bridge, {
    onTimbreModeMutated: () => {
      refreshAllDashboardCards();
      refreshMs2kBottomStrip();
      if (slideDrawer.isOpenState) slideDrawer.refresh();
    },
    onStateSync: () => {
      refreshMs2kBottomStrip();
    },
    onExitButton: () => {
      if (slideDrawer.isOpenState) {
        slideDrawer.close();
      }
      ms2kStripSeqMode = 0;
      if (ms2kStripPadKbdMode) {
        ms2kStripPadKbdMode = false;
        releaseAllPadLatchedNotes();
      }
      refreshAllDashboardCards();
      refreshMs2kBottomStrip();
    },
    onGlobalMidiChannelChanged: (ch) => {
      setGlobalMidiChannel(ch, false);
    }
  });

  // Mount the Korg MS2000 16-Button Hardware Strip
  setupMs2000BottomStrip();

  // Listen for full parameter state sync from C++ APVTS / SysEx
  bridge.on('syncAllParams', (data) => {
    if (data) {
      paramStore.syncAll(data);
      refreshAllDashboardCards();
      refreshMs2kBottomStrip();
      if (slideDrawer.isOpenState) slideDrawer.refresh();
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
    slideDrawer.open({ title: 'PITCH BEND, GLIDE & SCALE', badge: 'PITCH', sectionId: 'pitch', render: renderPitchDrawer });
  });
  document.getElementById('btn-edit-global')?.addEventListener('click', () => {
    slideDrawer.open({ title: 'MASTER VOLUME & PEDALS', badge: 'MASTER', sectionId: 'global', render: renderGlobalMasterDrawer });
  });

  // UX2.3: Clicking anywhere on a dashboard card header or body opens its primary drawer
  document.querySelectorAll('.dashboard-card').forEach((card) => {
    const header = card.querySelector('.card-header');
    if (header) header.style.cursor = 'pointer';
    card.addEventListener('click', (e) => {
      if (e.target.closest('button, a, input, select')) return;
      const primaryBtn = card.querySelector('.card-actions-bar .card-action-btn');
      if (primaryBtn) primaryBtn.click();
    });
  });

  setupDashboardObservers();
  devLog('[Dashboard Workspace: All 8 Cards & 19 Sub-actions Mounted]');
}

/* ─────────────────────────────────────────────────────────────
   1. OSCILLATORS & MIXER DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderOsc1Drawer(container) {
  const waveParam = tParam('osc1Wave');
  const dwgsParam = tParam('osc1DwgsWave');
  const ctrl1Param = tParam('osc1Ctrl1');
  const ctrl2Param = tParam('osc1Ctrl2');

  const currentWave = paramStore.get(waveParam) || 0;
  const isDwgs = currentWave === 5;
  const currentDwgsSlot = paramStore.get(dwgsParam) || 0;
  const dwgsName = getWaveName(currentDwgsSlot);

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>OSCILLATOR 1 WAVEFORM (TIMBRE ${activeTimbre})</h3>
      <div id="drawer-sel-osc1-wave" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-osc1ctrl1"></div>
        <div id="drawer-ctrl-osc1ctrl2"></div>
      </div>
    </div>

    <div class="drawer-section-card" id="drawer-dwgs-section" style="display: ${isDwgs ? 'block' : 'none'}; border-color: rgba(0, 212, 255, 0.4); background: rgba(0, 212, 255, 0.04);">
      <div style="display: flex; align-items: center; justify-content: space-between; margin-bottom: 10px;">
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
    paramId: waveParam,
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
    paramId: ctrl1Param, label: 'CONTROL 1', min: 0, max: 127, defaultValue: 64, size: 52
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-osc1ctrl2'), {
    paramId: ctrl2Param, label: 'CONTROL 2', min: 0, max: 127, defaultValue: 0, size: 52
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-dwgswave'), {
    paramId: dwgsParam, label: 'DWGS WAVE', min: 0, max: currentTheme() === 'advanced' ? 511 : 63, defaultValue: 0, size: 48,
    displayFormatter: (v) => `#${Math.round(v) + 1}`,
    onChange: (val) => {
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
    const curSlot = paramStore.get(dwgsParam) || 0;
    const synthMode = paramStore.get('synthMode') ?? (currentTheme() === 'advanced' ? 2 : (currentTheme() === 'microkorg' ? 1 : 0));
    openWavetableBrowser(bridge, curSlot, (slot) => {
      osc1WaveSel.setValue(5, true);
      bridge.setParam(dwgsParam, slot);
      paramStore.set(dwgsParam, slot);

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
      <h3>OSCILLATOR 2 &amp; MODULATION (TIMBRE ${activeTimbre})</h3>
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
    paramId: tParam('osc2Wave'),
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
    paramId: tParam('osc2ModType'),
    label: 'MODULATION TYPE',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'RING MOD' },
      { value: 2, label: 'SYNC' },
      { value: 3, label: 'RING + SYNC' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-osc2semi'), {
    paramId: tParam('osc2Semitone'), label: 'SEMITONE', min: -24, max: 24, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} st`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-osc2tune'), {
    paramId: tParam('osc2Tune'), label: 'FINE TUNE', min: -50, max: 50, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} cents`
  });
}

function renderMixerDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>AUDIO MIXER LEVELS (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-mixosc1"></div>
        <div id="drawer-ctrl-mixosc2"></div>
        <div id="drawer-ctrl-mixnoise"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-mixosc1'), {
    paramId: tParam('mixOsc1Level'), label: 'OSC 1 VOL', min: 0, max: 127, defaultValue: 127, size: 54
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-mixosc2'), {
    paramId: tParam('mixOsc2Level'), label: 'OSC 2 VOL', min: 0, max: 127, defaultValue: 0, size: 54
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-mixnoise'), {
    paramId: tParam('mixNoiseLevel'), label: 'NOISE VOL', min: 0, max: 127, defaultValue: 0, size: 54
  });
}

/* ─────────────────────────────────────────────────────────────
   2. VOCODER DRAWERS (CONFIG & 16-BAND GRAPHIC CONSOLE)
   ───────────────────────────────────────────────────────────── */
function renderVocoderConfigDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MODE &amp; FORMANT CONTROL</h3>
      <div style="display: flex; flex-direction: column; gap: 10px; margin-bottom: 8px;">
        <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px;">
          <div id="drawer-sel-vocoder-mode"></div>
          <div id="drawer-sel-vocoder-hold"></div>
        </div>
        <div id="drawer-sel-vocoder-shift"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>SYNTHESIS FILTER &amp; FC MODULATION</h3>
      <div class="drawer-controls-row" style="margin-bottom: 10px;">
        <div id="drawer-ctrl-voccutoff"></div>
        <div id="drawer-ctrl-vocres"></div>
        <div id="drawer-ctrl-vocmodint"></div>
        <div id="drawer-ctrl-vocefsense"></div>
      </div>
      <div id="drawer-sel-vocoder-modsrc"></div>
    </div>
    <div class="drawer-section-card">
      <h3>AUDIO IN, GATE &amp; SIBILANCE (HPF)</h3>
      <div style="margin-bottom: 10px;">
        <div id="drawer-sel-vocoder-hpfgate"></div>
      </div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-vochpf"></div>
        <div id="drawer-ctrl-vocgate"></div>
        <div id="drawer-ctrl-vocthresh"></div>
        <div id="drawer-ctrl-vocdirect"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-mode'), {
    paramId: 'synthVocoderMode',
    label: 'VOICE MODE',
    options: [
      { value: 0, label: 'SYNTH' },
      { value: 1, label: 'VOCODER' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-hold'), {
    paramId: 'vocoderHold',
    label: 'FORMANT HOLD',
    options: [
      { value: 0, label: 'LIVE' },
      { value: 1, label: 'HOLD (FREEZE)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-shift'), {
    paramId: 'vocoderFormantShift',
    label: 'FORMANT SHIFT',
    options: [
      { value: 0, label: '-2 (LOW)' },
      { value: 1, label: '-1' },
      { value: 2, label: '0 (NORM)' },
      { value: 3, label: '+1' },
      { value: 4, label: '+2 (HIGH)' }
    ],
    variant: 'strip',
    size: 'md',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-voccutoff'), {
    paramId: 'vocoderCutoff', label: 'FC OFFSET', min: -63, max: 63, defaultValue: 0, bipolar: true, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocres'), {
    paramId: 'vocoderResonance', label: 'RESONANCE', min: 0, max: 127, defaultValue: 64, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocmodint'), {
    paramId: 'vocoderModInt', label: 'FC MOD INT', min: -63, max: 63, defaultValue: 0, bipolar: true, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocefsense'), {
    paramId: 'vocoderEfSense', label: 'E.F. SENSE', min: 0, max: 127, defaultValue: 64, size: 46
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-modsrc'), {
    paramId: 'vocoderModSource',
    label: 'FC MOD SOURCE',
    options: [
      { value: 0, label: 'EG1' },
      { value: 1, label: 'EG2' },
      { value: 2, label: 'LFO1' },
      { value: 3, label: 'LFO2' },
      { value: 4, label: 'VEL' },
      { value: 5, label: 'KBD' },
      { value: 6, label: 'BEND' },
      { value: 7, label: 'WHEEL' }
    ],
    variant: 'grid',
    columns: 4,
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-vocoder-hpfgate'), {
    paramId: 'vocoderHpfGate',
    label: 'HPF GATE (SIBILANCE)',
    options: [
      { value: 0, label: 'DISABLE (DIRECT HI)' },
      { value: 1, label: 'ENABLE (NOISE GATE)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-vochpf'), {
    paramId: 'vocoderHpfLevel', label: 'HPF LEVEL', min: 0, max: 127, defaultValue: 64, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocgate'), {
    paramId: 'vocoderGateSense', label: 'GATE SENSE', min: 0, max: 127, defaultValue: 50, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocthresh'), {
    paramId: 'vocoderThreshold', label: 'THRESHOLD', min: 0, max: 127, defaultValue: 12, size: 46
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-vocdirect'), {
    paramId: 'vocoderDirectLevel', label: 'DIRECT LVL', min: 0, max: 127, defaultValue: 0, size: 46
  });
}

// Centros de banda del vocoder MS2000: 16 bandas repartidas geometricamente entre los
// extremos documentados por Korg (75 Hz .. 5000 Hz). Solo es una etiqueta de lectura.
const vocoderBandHz = (idx) => {
  const hz = 75 * Math.pow(5000 / 75, (idx + 0.5) / 16);
  return hz >= 1000 ? `${(hz / 1000).toFixed(1)}k` : `${Math.round(hz)}`;
};

let vocoderBandSliders = [];
let vocoderBandsEditMode = 'levels'; // 'levels' | 'pans'

function renderVocoderBandsDrawer(container) {
  // Destroy previous instances to avoid memory leaks
  vocoderBandSliders.forEach((s) => s.destroy());
  vocoderBandSliders = [];

  const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
  const isPanMode = vocoderBandsEditMode === 'pans';
  const titleText = isMicrokorg
    ? (isPanMode ? 'FILTER BANK PANPOTS (8 CH - microKORG)' : 'FILTER BANK LEVELS (8 CH - microKORG)')
    : (isPanMode ? 'FILTER BANK PANPOTS (16 BANDS - MS2000)' : 'FILTER BANK LEVELS (16 BANDS - MS2000)');
  const resetLabel = isPanMode ? 'STEREO SPREAD' : 'ALL 100%';

  container.innerHTML = `
    <div class="drawer-section-card">
      <div style="display: flex; justify-content: space-between; align-items: center; gap: 8px; margin-bottom: 10px; flex-wrap: wrap;">
        <h3 style="margin: 0; border: none; padding: 0;">${titleText}</h3>
        <div style="display: flex; gap: 6px; align-items: center;">
          <button id="btn-vocoder-mode-levels" class="card-action-btn" style="padding: 4px 10px; opacity: ${!isPanMode ? '1' : '0.55'};">LEVELS (1-16)</button>
          <button id="btn-vocoder-mode-pans" class="card-action-btn" style="padding: 4px 10px; opacity: ${isPanMode ? '1' : '0.55'};">PANPOTS (1-16)</button>
          <button id="btn-vocoder-reset-bands" class="card-action-btn" style="padding: 4px 10px;">${resetLabel}</button>
          ${isPanMode ? `
          <button id="btn-vocoder-pan-center" class="card-action-btn" style="padding: 4px 10px;" title="Todas las bandas al centro">CENTER</button>
          <button id="btn-vocoder-pan-alt" class="card-action-btn" style="padding: 4px 10px;" title="Alternancia izquierda/derecha">ALT L/R</button>
          <button id="btn-vocoder-pan-lowhi" class="card-action-btn" style="padding: 4px 10px;" title="Graves centrados, agudos abriendose">LOW-C / HI-W</button>` : ''}
        </div>
      </div>
      <div class="vocoder-bands-grid" id="vocoder-bands-grid-el"></div>
    </div>
  `;

  const gridEl = container.querySelector('#vocoder-bands-grid-el');
  const paramPrefix = isPanMode ? 'vocoderBandPan' : 'vocoderBandLevel';
  const minVal = isPanMode ? -63 : 0;
  const maxVal = isPanMode ? 63 : 127;

  if (isMicrokorg) {
    for (let ch = 1; ch <= 8; ch++) {
      const cell = document.createElement('div');
      cell.className = 'vocoder-band-cell';
      gridEl.appendChild(cell);

      const bA = (ch * 2) - 1;
      const bB = ch * 2;
      const defVal = isPanMode ? ((ch % 2 === 1) ? -22 : 22) : 127;
      const valA = paramStore.get(`${paramPrefix}${bA}`) ?? defVal;
      const valB = paramStore.get(`${paramPrefix}${bB}`) ?? defVal;
      const initialVal = Math.round((valA + valB) / 2);

      const slider = new ABDVerticalSlider(cell, {
        paramId: `${paramPrefix}${bA}`,
        label: `CH ${ch}`,
        min: minVal,
        max: maxVal,
        defaultValue: initialVal,
        height: 110,
        trackWidth: 6,
        thumbSize: 16,
        showLabel: true,
        showValue: true,
        onChange: (val) => {
          bridge.setParam(`${paramPrefix}${bB}`, val);
          paramStore.set(`${paramPrefix}${bB}`, val);
        }
      });
      vocoderBandSliders.push(slider);
    }
  } else {
    for (let i = 1; i <= 16; i++) {
      const cell = document.createElement('div');
      cell.className = 'vocoder-band-cell';
      gridEl.appendChild(cell);

      const defVal = isPanMode ? ((i % 2 === 1) ? -22 : 22) : 127;
      const slider = new ABDVerticalSlider(cell, {
        paramId: `${paramPrefix}${i}`,
        label: `B${i} · ${vocoderBandHz(i - 1)}`,
        min: minVal,
        max: maxVal,
        defaultValue: defVal,
        height: 110,
        trackWidth: 6,
        thumbSize: 16,
        showLabel: true,
        showValue: true
      });
      vocoderBandSliders.push(slider);
    }
  }

  container.querySelector('#btn-vocoder-mode-levels')?.addEventListener('click', () => {
    vocoderBandsEditMode = 'levels';
    renderVocoderBandsDrawer(container);
  });

  container.querySelector('#btn-vocoder-mode-pans')?.addEventListener('click', () => {
    vocoderBandsEditMode = 'pans';
    renderVocoderBandsDrawer(container);
  });

  const applyPanMacro = (fn) => {
    vocoderBandSliders.forEach((slider, idx) => slider.setValue(fn(idx), true));
  };
  container.querySelector('#btn-vocoder-pan-center')?.addEventListener('click', () => applyPanMacro(() => 0));
  container.querySelector('#btn-vocoder-pan-alt')?.addEventListener('click',
    () => applyPanMacro((i) => (i % 2 === 0 ? -48 : 48)));
  container.querySelector('#btn-vocoder-pan-lowhi')?.addEventListener('click', () =>
    applyPanMacro((i) => {
      if (i < 8) return 0;                      // graves centrados
      const t = (i - 8) / 7;                    // 0..1 hacia los agudos
      return Math.round((i % 2 === 0 ? -1 : 1) * 48 * t);
    }));

  container.querySelector('#btn-vocoder-reset-bands')?.addEventListener('click', () => {
    vocoderBandSliders.forEach((slider, idx) => {
      const target = isPanMode ? ((idx % 2 === 0) ? -22 : 22) : 127;
      slider.setValue(target, true);
    });
  });
}

/* ─────────────────────────────────────────────────────────────
   3. FILTER MULTIMODE DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderFilterMainDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>FILTER TYPE &amp; FREQUENCY RESPONSE (TIMBRE ${activeTimbre})</h3>
      <div id="drawer-sel-filter-type" style="margin-bottom: 14px;"></div>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-cutoff"></div>
        <div id="drawer-ctrl-reso"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-filter-type'), {
    paramId: tParam('filterType'),
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
    paramId: tParam('filterCutoff'), label: 'CUTOFF', min: 0, max: 127, defaultValue: 127, size: 56,
    displayFormatter: (v) => {
      const hz = Math.round(20 * Math.pow(1000, v / 127));
      return hz >= 1000 ? `${(hz / 1000).toFixed(1)} kHz` : `${hz} Hz`;
    }
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-reso'), {
    paramId: tParam('filterResonance'), label: 'RESONANCE', min: 0, max: 127, defaultValue: 0, size: 56
  });
}

function renderFilterModDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>CUTOFF MODULATION: EG1 / KEY / VEL (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1int"></div>
        <div id="drawer-ctrl-keytrack"></div>
        <div id="drawer-ctrl-velsens"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-eg1int'), {
    paramId: tParam('filterEg1Int'), label: 'EG1 INT', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-keytrack'), {
    paramId: tParam('filterKeyTrack'), label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-velsens'), {
    paramId: tParam('filterVelo'), label: 'VEL SENS', min: -63, max: 63, defaultValue: 0, size: 52, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
}

/* ─────────────────────────────────────────────────────────────
   4. AMP (VCA) & ENVELOPES DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderAmpMainDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>VCA AMPLIFIER &amp; DISTORTION (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-ctrl-amplevel"></div>
        <div id="drawer-ctrl-amppan"></div>
        <div id="drawer-ctrl-ampdist" style="min-width: 110px; max-width: 140px;"></div>
        <div id="drawer-ctrl-ampgate" style="min-width: 130px; max-width: 170px;"></div>
        <div id="drawer-ctrl-ampkeytrack"></div>
        <div id="drawer-ctrl-ampvelo"></div>
      </div>
    </div>
  `;

  new ABDKnob(container.querySelector('#drawer-ctrl-amplevel'), {
    paramId: tParam('ampLevel'), label: 'AMP LEVEL', min: 0, max: 127, defaultValue: 100, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-amppan'), {
    paramId: tParam('ampPan'), label: 'PAN', min: -64, max: 63, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
  new ABDSegmented(container.querySelector('#drawer-ctrl-ampdist'), {
    paramId: tParam('ampDistortion'),
    label: 'DISTORTION',
    options: [
      { value: 0, label: 'OFF' },
      { value: 1, label: 'ON' }
    ],
    variant: 'strip',
    size: 'sm',
  });
  new ABDSegmented(container.querySelector('#drawer-ctrl-ampgate'), {
    paramId: tParam('ampGate'),
    label: 'AMP SWITCH',
    options: [
      { value: 0, label: 'EG2' },
      { value: 1, label: 'GATE (ORGAN)' }
    ],
    variant: 'strip',
    size: 'sm',
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-ampkeytrack'), {
    paramId: tParam('ampKeyTrack'), label: 'KEY TRACK', min: -63, max: 63, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-ampvelo'), {
    paramId: tParam('ampVelo'), label: 'VEL SENS', min: -63, max: 63, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });
}

function renderEg1Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVELOPE 1: FILTER &amp; PITCH ADSR (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg1a"></div>
        <div id="drawer-ctrl-eg1d"></div>
        <div id="drawer-ctrl-eg1s"></div>
        <div id="drawer-ctrl-eg1r"></div>
        <div id="drawer-sel-eg1-reset"></div>
      </div>
    </div>
  `;
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1a'), {
    paramId: tParam('eg1Attack'), label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1d'), {
    paramId: tParam('eg1Decay'), label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1s'), {
    paramId: tParam('eg1Sustain'), label: 'SUSTAIN', min: 0, max: 127, defaultValue: 0, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg1r'), {
    paramId: tParam('eg1Release'), label: 'RELEASE', min: 0, max: 127, defaultValue: 40, size: 48
  });
  new ABDSegmented(container.querySelector('#drawer-sel-eg1-reset'), {
    paramId: tParam('eg1Reset'),
    label: 'RESET',
    options: [{ value: 0, label: 'CONTINUE' }, { value: 1, label: 'FROM ZERO' }],
    variant: 'strip',
    size: 'sm',
  });
}

function renderEg2Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>ENVELOPE 2: AMPLITUDE ADSR (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-eg2a"></div>
        <div id="drawer-ctrl-eg2d"></div>
        <div id="drawer-ctrl-eg2s"></div>
        <div id="drawer-ctrl-eg2r"></div>
        <div id="drawer-sel-eg2-reset"></div>
      </div>
    </div>
  `;
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2a'), {
    paramId: tParam('eg2Attack'), label: 'ATTACK', min: 0, max: 127, defaultValue: 0, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2d'), {
    paramId: tParam('eg2Decay'), label: 'DECAY', min: 0, max: 127, defaultValue: 64, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2s'), {
    paramId: tParam('eg2Sustain'), label: 'SUSTAIN', min: 0, max: 127, defaultValue: 127, size: 48
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-eg2r'), {
    paramId: tParam('eg2Release'), label: 'RELEASE', min: 0, max: 127, defaultValue: 20, size: 48
  });
  new ABDSegmented(container.querySelector('#drawer-sel-eg2-reset'), {
    paramId: tParam('eg2Reset'),
    label: 'RESET',
    options: [{ value: 0, label: 'CONTINUE' }, { value: 1, label: 'FROM ZERO' }],
    variant: 'strip',
    size: 'sm',
  });
}

/* ─────────────────────────────────────────────────────────────
   5. LFOS & VIRTUAL PATCH DRAWERS
   ───────────────────────────────────────────────────────────── */
const LFO_SYNC_NOTES_15 = [
  { value: 0, label: '1/1' },  { value: 1, label: '3/4' },  { value: 2, label: '2/3' },
  { value: 3, label: '1/2' },  { value: 4, label: '3/8' },  { value: 5, label: '1/3' },
  { value: 6, label: '1/4' },  { value: 7, label: '3/16' }, { value: 8, label: '1/6' },
  { value: 9, label: '1/8' },  { value: 10, label: '3/32' },{ value: 11, label: '1/12' },
  { value: 12, label: '1/16' },{ value: 13, label: '1/24' },{ value: 14, label: '1/32' }
];

function renderLfo1Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>LFO 1: WAVEFORM &amp; FREQUENCY (TIMBRE ${activeTimbre})</h3>
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
    paramId: tParam('lfo1Wave'),
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
    paramId: tParam('lfo1KeySync'),
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
    paramId: tParam('lfo1TempoSync'),
    label: 'TEMPO SYNC',
    options: [
      { value: 0, label: 'OFF (FREE)' },
      { value: 1, label: 'ON (BPM)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo1-syncnote'), {
    paramId: tParam('lfo1SyncNote'),
    label: 'SYNC NOTE DIVISION (15 VALUES)',
    options: LFO_SYNC_NOTES_15,
    layout: 'grid-5',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-lfo1freq'), {
    paramId: tParam('lfo1Freq'), label: 'FREQUENCY', min: 0, max: 127, defaultValue: 30, size: 52,
    displayFormatter: (v) => `${(0.01 + Math.pow(v / 127, 2) * 20).toFixed(2)} Hz`
  });
}

function renderLfo2Drawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>LFO 2: WAVEFORM &amp; FREQUENCY (TIMBRE ${activeTimbre})</h3>
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
    paramId: tParam('lfo2Wave'),
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
    paramId: tParam('lfo2KeySync'),
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
    paramId: tParam('lfo2TempoSync'),
    label: 'TEMPO SYNC',
    options: [
      { value: 0, label: 'OFF (FREE)' },
      { value: 1, label: 'ON (BPM)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-lfo2-syncnote'), {
    paramId: tParam('lfo2SyncNote'),
    label: 'SYNC NOTE DIVISION (15 VALUES)',
    options: LFO_SYNC_NOTES_15,
    layout: 'grid-5',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-lfo2freq'), {
    paramId: tParam('lfo2Freq'), label: 'FREQUENCY', min: 0, max: 127, defaultValue: 50, size: 52,
    displayFormatter: (v) => `${(0.01 + Math.pow(v / 127, 2) * 20).toFixed(2)} Hz`
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
      <h3>VIRTUAL MODULATION MATRIX: 4 SLOTS (TIMBRE ${activeTimbre})</h3>
      ${slotsHtml}
    </div>
  `;

  for (let s = 1; s <= 4; s++) {
    new ABDSegmented(container.querySelector(`#drawer-patch${s}-src-sel`), {
      paramId: tParam(`patch${s}Source`),
      label: 'SOURCE',
      options: sources,
      layout: 'grid-8',
      size: 'sm',
    });

    new ABDSegmented(container.querySelector(`#drawer-patch${s}-dest-sel`), {
      paramId: tParam(`patch${s}Destination`),
      label: 'DESTINATION',
      options: dests,
      layout: 'grid-8',
      size: 'sm',
    });

    new ABDKnob(container.querySelector(`#drawer-ctrl-patch${s}-int`), {
      paramId: tParam(`patch${s}Intensity`), label: 'INT', min: -63, max: 63, defaultValue: 0, size: 40, isBipolar: true,
      displayFormatter: (v) => `${Math.round(v)}`
    });
  }
}

/* ─────────────────────────────────────────────────────────────
   6. ARPEGGIATOR & MOD SEQUENCER DRAWERS
   ───────────────────────────────────────────────────────────── */
function renderArpDrawer(container) {
  const arpResolutions = [
    { value: 0, label: '1/48' },
    { value: 1, label: '1/32' },
    { value: 2, label: '1/24' },
    { value: 3, label: '1/16' },
    { value: 4, label: '1/12' },
    { value: 5, label: '1/8' },
    { value: 6, label: '1/6' },
    { value: 7, label: '1/4' },
    { value: 8, label: '1/3' },
    { value: 9, label: '1/2' },
    { value: 10, label: '1/1' }
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
          <div id="drawer-sel-arp-keysync"></div>
          <div id="drawer-sel-arp-target"></div>
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

  new ABDSegmented(container.querySelector('#drawer-sel-arp-keysync'), {
    paramId: 'arpKeySync',
    label: 'KEY SYNC',
    options: [
      { value: 0, label: 'OFF (FREE RUN)' },
      { value: 1, label: 'ON (RESET ON KEY)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-arp-target'), {
    paramId: 'arpTarget',
    label: 'TARGET TIMBRE',
    options: [
      { value: 0, label: 'BOTH' },
      { value: 1, label: 'TIMBRE 1' },
      { value: 2, label: 'TIMBRE 2' }
    ],
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
    layout: 'grid-6',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-arptempo'), {
    paramId: 'arpTempo', label: 'TEMPO', min: 20, max: 300, defaultValue: 120, size: 50,
    displayFormatter: (v) => `${Math.round(v)} BPM`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-arpgate'), {
    paramId: 'arpGate', label: 'GATE TIME', min: 0, max: 127, defaultValue: 100, size: 50,
    displayFormatter: (v) => `${Math.round((v / 127) * 100)}%`
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
      onChange: () => {
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

const MODSEQ_DESTINATIONS_31 = [
  { value: 0,  label: '00: None (Off)' },
  { value: 1,  label: '01: Pitch' },
  { value: 2,  label: '02: Step Pitch' },
  { value: 3,  label: '03: OSC 1 Ctrl 1' },
  { value: 4,  label: '04: OSC 1 Ctrl 2' },
  { value: 5,  label: '05: OSC 2 Semitone' },
  { value: 6,  label: '06: OSC 2 Fine Tune' },
  { value: 7,  label: '07: Mixer OSC 1 Level' },
  { value: 8,  label: '08: Mixer OSC 2 Level' },
  { value: 9,  label: '09: Mixer Noise Level' },
  { value: 10, label: '10: Filter Cutoff' },
  { value: 11, label: '11: Filter Resonance' },
  { value: 12, label: '12: Filter EG1 Int' },
  { value: 13, label: '13: Filter Key Track' },
  { value: 14, label: '14: Amp Level' },
  { value: 15, label: '15: Amp Panpot' },
  { value: 16, label: '16: EG 1 Attack' },
  { value: 17, label: '17: EG 1 Decay' },
  { value: 18, label: '18: EG 1 Sustain' },
  { value: 19, label: '19: EG 1 Release' },
  { value: 20, label: '20: EG 2 Attack' },
  { value: 21, label: '21: EG 2 Decay' },
  { value: 22, label: '22: EG 2 Sustain' },
  { value: 23, label: '23: EG 2 Release' },
  { value: 24, label: '24: LFO 1 Frequency' },
  { value: 25, label: '25: LFO 2 Frequency' },
  { value: 26, label: '26: V-Patch 1 Int' },
  { value: 27, label: '27: V-Patch 2 Int' },
  { value: 28, label: '28: V-Patch 3 Int' },
  { value: 29, label: '29: V-Patch 4 Int' },
  { value: 30, label: '30: Portamento Time' }
];

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

  const getDestShortName = (trk) => {
    const destId = tParam(`seq${trk}Dest`);
    const knobId = tParam(`seq${trk}Knob`);
    const knobVal = Math.round(paramStore.get(destId) ?? paramStore.get(knobId) ?? 0);
    const entry = MODSEQ_DESTINATIONS_31[knobVal];
    return entry ? entry.label.replace(/^\d+:\s*/, '') : 'None';
  };

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>MOD SEQUENCER GLOBAL (TIMBRE ${activeTimbre})</h3>
      <div style="display: flex; flex-direction: column; gap: 10px;">
        <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;">
          <div id="drawer-sel-modseq-on"></div>
          <div id="drawer-sel-modseq-runmode"></div>
          <div id="drawer-sel-modseq-keysync"></div>
        </div>
        <div id="drawer-sel-modseq-type"></div>
        <div id="drawer-sel-modseq-res"></div>
        <div id="drawer-sel-modseq-laststep"></div>
      </div>
    </div>

    <div class="drawer-section-card" id="modseq-track-editor-card">
      <div style="display: flex; align-items: center; justify-content: space-between; gap: 8px; margin-bottom: 10px; flex-wrap: wrap;">
        <div style="display: flex; gap: 6px;" id="modseq-track-tabs">
          <button type="button" class="card-action-btn modseq-trk-tab ${activeModSeqTrack === 1 ? 'is-editing-btn' : ''}" data-trk="1" style="padding: 5px 10px; font-size: 11px;">
            SEQ 1: <span id="modseq-tab-dest-1">${getDestShortName(1)}</span>
          </button>
          <button type="button" class="card-action-btn modseq-trk-tab ${activeModSeqTrack === 2 ? 'is-editing-btn' : ''}" data-trk="2" style="padding: 5px 10px; font-size: 11px;">
            SEQ 2: <span id="modseq-tab-dest-2">${getDestShortName(2)}</span>
          </button>
          <button type="button" class="card-action-btn modseq-trk-tab ${activeModSeqTrack === 3 ? 'is-editing-btn' : ''}" data-trk="3" style="padding: 5px 10px; font-size: 11px;">
            SEQ 3: <span id="modseq-tab-dest-3">${getDestShortName(3)}</span>
          </button>
        </div>
      </div>

      <div style="display: grid; grid-template-columns: 1.5fr 1fr; gap: 12px; align-items: end; margin-bottom: 12px;">
        <div style="display: flex; flex-direction: column; gap: 4px;">
          <label style="font-size: 10px; font-weight: 700; letter-spacing: 0.08em; color: rgba(255,255,255,0.65);">TRACK ${activeModSeqTrack} DESTINATION (KNOB ASSIGN)</label>
          <select id="modseq-dest-select" style="background: #0d131d; color: #38bdf8; border: 1px solid rgba(56, 189, 248, 0.35); border-radius: 5px; padding: 6px 8px; font-size: 12px; font-weight: 600; outline: none; cursor: pointer;">
            ${MODSEQ_DESTINATIONS_31.map((d) => `<option value="${d.value}">${d.label}</option>`).join('')}
          </select>
        </div>
        <div id="drawer-sel-modseq-motion"></div>
      </div>

      <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 6px; gap: 4px; flex-wrap: wrap;">
        <span style="font-size: 10px; color: rgba(255,255,255,0.55); font-family: var(--font-mono);">DRAG ACROSS BARS TO PAINT STEPS (-63..+63) &bull; DOUBLE-CLICK = 0</span>
        <div style="display: flex; gap: 4px; flex-wrap: wrap;">
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="clear" style="padding: 3px 7px; font-size: 10px;">CLEAR</button>
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="ramp-up" style="padding: 3px 7px; font-size: 10px;">RAMP &nearr;</button>
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="ramp-down" style="padding: 3px 7px; font-size: 10px;">RAMP &searr;</button>
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="sine" style="padding: 3px 7px; font-size: 10px;">SINE</button>
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="invert" style="padding: 3px 7px; font-size: 10px;">INVERT</button>
          <button type="button" class="card-action-btn modseq-macro-btn" data-macro="random" style="padding: 3px 7px; font-size: 10px;">RANDOM</button>
        </div>
      </div>

      <div id="modseq-bars-grid" style="display: grid; grid-template-columns: repeat(16, 1fr); gap: 4px; height: 150px; background: rgba(7, 10, 15, 0.85); border: 1px solid rgba(255,255,255,0.1); border-radius: 6px; padding: 8px 6px; user-select: none; touch-action: none;"></div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-on'), {
    paramId: 'modSeqOn',
    label: 'MOD SEQ',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-runmode'), {
    paramId: tParam('seqRunMode'),
    label: 'RUN MODE',
    options: [{ value: 0, label: 'LOOP' }, { value: 1, label: '1-SHOT' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-keysync'), {
    paramId: tParam('seqKeySync'),
    label: 'KEY SYNC',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'TIMB' }, { value: 2, label: 'VOICE' }],
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
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-res'), {
    paramId: 'modSeqResolution',
    label: 'TIME RESOLUTION',
    options: modSeqResolutions,
    layout: 'grid-5',
    size: 'sm',
  });

  const trkDestParam = tParam(`seq${activeModSeqTrack}Dest`);
  const trkKnobParam = tParam(`seq${activeModSeqTrack}Knob`);
  const trkMotionParam = tParam(`seq${activeModSeqTrack}Motion`);
  const lastStepParam = tParam('seqLastStep');

  const destSelect = container.querySelector('#modseq-dest-select');
  if (destSelect) {
    destSelect.value = String(Math.round(paramStore.get(trkDestParam) ?? paramStore.get(trkKnobParam) ?? 0));
    destSelect.addEventListener('change', () => {
      const nextDest = parseInt(destSelect.value, 10) || 0;
      paramStore.set(trkDestParam, nextDest);
      bridge.setParam(trkDestParam, nextDest);
      paramStore.set(trkKnobParam, nextDest);
      bridge.setParam(trkKnobParam, nextDest);
      const tabSpan = container.querySelector(`#modseq-tab-dest-${activeModSeqTrack}`);
      if (tabSpan) tabSpan.textContent = getDestShortName(activeModSeqTrack);
    });
  }

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-motion'), {
    paramId: trkMotionParam,
    label: `TRACK ${activeModSeqTrack} MOTION`,
    options: [{ value: 0, label: 'STEP' }, { value: 1, label: 'SMOOTH' }],
    variant: 'strip',
    size: 'sm',
  });

  const barsGrid = container.querySelector('#modseq-bars-grid');

  const renderStepBars = () => {
    if (!barsGrid) return;
    const lastStep = Math.round(paramStore.get(lastStepParam) || 16);
    barsGrid.innerHTML = '';

    let isPainting = false;

    const setStepFromPointer = (stepIdx, clientY, colEl) => {
      const rect = colEl.querySelector('.modseq-bar-track').getBoundingClientRect();
      const rel = Math.max(0, Math.min(1, 1 - (clientY - rect.top) / rect.height));
      const val = Math.round(rel * 126 - 63);
      const stepParamId = tParam(`seq${activeModSeqTrack}Step${stepIdx}`);
      paramStore.set(stepParamId, val);
      bridge.setParam(stepParamId, val);
      updateSingleBarVisual(colEl, val);
      refreshMs2kBottomStrip();
    };

    const updateSingleBarVisual = (colEl, val) => {
      const fill = colEl.querySelector('.modseq-bar-fill');
      const valLbl = colEl.querySelector('.modseq-bar-val');
      const v = Math.max(-63, Math.min(63, Math.round(val)));
      if (valLbl) valLbl.textContent = v > 0 ? `+${v}` : `${v}`;
      if (fill) {
        const pct = (Math.abs(v) / 63) * 50;
        if (v >= 0) {
          fill.style.bottom = '50%';
          fill.style.top = 'auto';
          fill.style.height = `${pct}%`;
          fill.style.background = 'linear-gradient(180deg, #38bdf8 0%, #0284c7 100%)';
        } else {
          fill.style.top = '50%';
          fill.style.bottom = 'auto';
          fill.style.height = `${pct}%`;
          fill.style.background = 'linear-gradient(180deg, #f59e0b 0%, #d97706 100%)';
        }
      }
    };

    for (let s = 1; s <= 16; s++) {
      const stepParamId = tParam(`seq${activeModSeqTrack}Step${s}`);
      const curVal = Math.round(paramStore.get(stepParamId) || 0);
      const inRange = s <= lastStep;

      const col = document.createElement('div');
      col.className = 'modseq-bar-col';
      col.dataset.step = String(s);
      col.style.cssText = `
        display: flex;
        flex-direction: column;
        align-items: center;
        height: 100%;
        opacity: ${inRange ? '1' : '0.32'};
        cursor: pointer;
      `;

      col.innerHTML = `
        <span style="font-size: 9px; font-family: var(--font-mono); color: rgba(255,255,255,0.55); margin-bottom: 3px;">${s}</span>
        <div class="modseq-bar-track" style="position: relative; flex: 1; width: 100%; background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.1); border-radius: 3px; overflow: hidden;">
          <div style="position: absolute; left: 0; right: 0; top: 50%; height: 1px; background: rgba(255,255,255,0.22); z-index: 1;"></div>
          <div class="modseq-bar-fill" style="position: absolute; left: 1px; right: 1px; border-radius: 2px; transition: height 0.04s linear;"></div>
        </div>
        <span class="modseq-bar-val" style="font-size: 9px; font-family: var(--font-mono); color: #e2e8f0; margin-top: 3px; font-weight: 700;">0</span>
      `;

      updateSingleBarVisual(col, curVal);

      col.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        isPainting = true;
        setStepFromPointer(s, e.clientY, col);
      });

      col.addEventListener('pointerenter', (e) => {
        if (isPainting || e.buttons === 1) {
          setStepFromPointer(s, e.clientY, col);
        }
      });

      col.addEventListener('pointermove', (e) => {
        if (isPainting || e.buttons === 1) {
          setStepFromPointer(s, e.clientY, col);
        }
      });

      col.addEventListener('dblclick', () => {
        paramStore.set(stepParamId, 0);
        bridge.setParam(stepParamId, 0);
        updateSingleBarVisual(col, 0);
        refreshMs2kBottomStrip();
      });

      barsGrid.appendChild(col);
    }

    window.addEventListener('pointerup', () => {
      isPainting = false;
    }, { once: true });
  };

  new ABDSegmented(container.querySelector('#drawer-sel-modseq-laststep'), {
    paramId: lastStepParam,
    label: 'LAST STEP (1..16)',
    options: Array.from({ length: 16 }, (_, i) => ({
      value: i + 1,
      label: `${i + 1}`
    })),
    layout: 'grid-8',
    size: 'sm',
    onChange: () => {
      renderStepBars();
      refreshMs2kBottomStrip();
    }
  });

  // Track switch buttons [SEQ 1] [SEQ 2] [SEQ 3]
  container.querySelectorAll('.modseq-trk-tab').forEach((btn) => {
    btn.addEventListener('click', () => {
      activeModSeqTrack = parseInt(btn.dataset.trk, 10) || 1;
      if (ms2kStripSeqMode > 0) {
        ms2kStripSeqMode = activeModSeqTrack;
      }
      renderModSeqDrawer(container);
      refreshMs2kBottomStrip();
    });
  });

  // Macro generator buttons
  container.querySelectorAll('.modseq-macro-btn').forEach((btn) => {
    btn.addEventListener('click', () => {
      const macro = btn.dataset.macro;
      for (let s = 1; s <= 16; s++) {
        const stepParamId = tParam(`seq${activeModSeqTrack}Step${s}`);
        const cur = Math.round(paramStore.get(stepParamId) || 0);
        let next = 0;
        if (macro === 'clear') next = 0;
        else if (macro === 'ramp-up') next = Math.round(-63 + ((s - 1) / 15) * 126);
        else if (macro === 'ramp-down') next = Math.round(63 - ((s - 1) / 15) * 126);
        else if (macro === 'sine') next = Math.round(Math.sin(((s - 1) / 16) * Math.PI * 2) * 63);
        else if (macro === 'invert') next = Math.max(-63, Math.min(63, -cur));
        else if (macro === 'random') next = Math.round((Math.random() * 126) - 63);
        paramStore.set(stepParamId, next);
        bridge.setParam(stepParamId, next);
      }
      renderStepBars();
      refreshMs2kBottomStrip();
    });
  });

  renderStepBars();
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
    paramId: 'modFxSpeed', label: 'SPEED', min: 0, max: 127, defaultValue: 40, size: 50
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-modfxdepth'), {
    paramId: 'modFxDepth', label: 'DEPTH', min: 0, max: 127, defaultValue: 64, size: 50
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-modfxfb'), {
    paramId: 'modFxFeedback', label: 'FEEDBACK', min: 0, max: 127, defaultValue: 0, size: 50
  });
}

function renderDelayFxDrawer(container) {
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>STEREO DELAY</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-delay-on"></div>
        <div id="drawer-sel-delay-type"></div>
        <div id="drawer-sel-delay-sync"></div>
        <div id="drawer-sel-delay-syncnote"></div>
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

  new ABDSegmented(container.querySelector('#drawer-sel-delay-sync'), {
    paramId: 'delayTempoSync',
    label: 'TEMPO SYNC',
    options: [{ value: 0, label: 'FREE (ms)' }, { value: 1, label: 'SYNC TO BPM' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-delay-syncnote'), {
    paramId: 'delaySyncNote',
    label: 'SYNC NOTE (DELAY FLAGS 0..3)',
    options: [
      { value: 0, label: '1/32' }, { value: 1, label: '1/24' }, { value: 2, label: '1/16' },
      { value: 3, label: '1/12' }, { value: 4, label: '3/32' }, { value: 5, label: '1/8' },
      { value: 6, label: '1/6' }, { value: 7, label: '3/16' }, { value: 8, label: '1/4' },
      { value: 9, label: '1/3' }, { value: 10, label: '3/8' }, { value: 11, label: '1/2' },
      { value: 12, label: '2/3' }, { value: 13, label: '3/4' }, { value: 14, label: '1/1' }
    ],
    layout: 'grid-6',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-delaytime'), {
    paramId: 'delayTime', label: 'TIME', min: 0, max: 127, defaultValue: 40, size: 50
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-delaydepth'), {
    paramId: 'delayDepth', label: 'DEPTH (WET)', min: 0, max: 127, defaultValue: 50, size: 50
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-delayfb'), {
    paramId: 'delayFeedback', label: 'FEEDBACK', min: 0, max: 127, defaultValue: 40, size: 50
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
    displayFormatter: (v) => `${((v - 64) * (12 / 64)).toFixed(1)} dB`
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
    displayFormatter: (v) => `${((v - 64) * (12 / 64)).toFixed(1)} dB`
  });
}

/* ─────────────────────────────────────────────────────────────
   8. VOICE, PITCH & MASTER DRAWERS
   ───────────────────────────────────────────────────────────── */
function getTimbreParamPairs() {
  const pairs = [];
  Object.keys(PARAM_LOOKUP).forEach((id) => {
    if (id.startsWith('t2') && id.length > 2) {
      const t1Id = id.charAt(2).toLowerCase() + id.slice(3);
      if (PARAM_LOOKUP[t1Id]) {
        pairs.push([t1Id, id]);
      }
    }
  });
  return pairs;
}

function copyTimbre1To2() {
  getTimbreParamPairs().forEach(([t1Id, t2Id]) => {
    const val = paramStore.get(t1Id);
    if (val !== undefined) {
      paramStore.set(t2Id, val);
    }
  });
  bridge.send('copyTimbre1To2', {});
  if (lcdProgrammer) lcdProgrammer.flashMessage('TIMBRE 1 -> 2', 'COPY COMPLETED');
  devLog('[Timbre 1 Copied to Timbre 2 (115 params)]');
}

function copyTimbre2To1() {
  getTimbreParamPairs().forEach(([t1Id, t2Id]) => {
    const val = paramStore.get(t2Id);
    if (val !== undefined) {
      paramStore.set(t1Id, val);
    }
  });
  bridge.send('copyTimbre2To1', {});
  if (lcdProgrammer) lcdProgrammer.flashMessage('TIMBRE 2 -> 1', 'COPY COMPLETED');
  devLog('[Timbre 2 Copied to Timbre 1 (115 params)]');
}

function swapTimbres1And2() {
  getTimbreParamPairs().forEach(([t1Id, t2Id]) => {
    const v1 = paramStore.get(t1Id);
    const v2 = paramStore.get(t2Id);
    if (v1 !== undefined && v2 !== undefined) {
      paramStore.set(t1Id, v2);
      paramStore.set(t2Id, v1);
    }
  });
  bridge.send('swapTimbres', {});
  if (lcdProgrammer) lcdProgrammer.flashMessage('TIMBRE 1 <-> 2', 'SWAP COMPLETED');
  devLog('[Timbre 1 & Timbre 2 Swapped (115 params)]');
}

function renderVoiceDrawer(container) {
  const noteNames = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
  const formatMidiNote = (n) => {
    const m = Math.max(0, Math.min(127, Math.round(n)));
    return `${noteNames[m % 12]}${Math.floor(m / 12) - 1} (${m})`;
  };

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>VOICE ASSIGN &amp; UNISON MODE (TIMBRE ${activeTimbre})</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-voice-mode"></div>
        <div id="drawer-sel-synth-profile"></div>
        <div id="drawer-sel-trigger-mode"></div>
  <div id="drawer-sel-key-priority" style="width: 100%;"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>UNISON DETUNE &amp; STEREO SPREAD (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-unisondetune"></div>
        <div id="drawer-ctrl-unisonspread"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>TIMBRE ARCHITECTURE (SINGLE / SPLIT / LAYER)</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-sel-timbre-mode"></div>
        <div id="drawer-sel-timbre-voices"></div>
      </div>
      <div class="drawer-controls-row" style="justify-content: space-around; align-items: center;">
        <div id="drawer-ctrl-splitpoint"></div>
        <div style="display: flex; flex-direction: column; gap: 6px; min-width: 170px;">
          <button id="btn-drawer-copy-t1-t2" class="card-action-btn" style="padding: 6px 10px;">COPY TIMBRE 1 &rarr; 2</button>
          <button id="btn-drawer-copy-t2-t1" class="card-action-btn" style="padding: 6px 10px;">COPY TIMBRE 2 &rarr; 1</button>
          <button id="btn-drawer-swap-timbres" class="card-action-btn" style="padding: 6px 10px; border-color: var(--color-accent); color: var(--color-accent);">SWAP TIMBRE 1 &harr; 2</button>
        </div>
      </div>
    </div>
  `;

  const curSynthMode = Math.round(paramStore.get('synthMode') || 0);
  const vCountLabel = curSynthMode === 2 ? '16V' : '4V';

  new ABDSegmented(container.querySelector('#drawer-sel-voice-mode'), {
    paramId: tParam('voiceMode'),
    label: 'VOICE ASSIGN',
    options: [
      { value: 0, label: 'MONO' },
      { value: 1, label: `POLY (${vCountLabel})` },
      { value: 2, label: `UNISON (${vCountLabel})` }
    ],
    variant: 'strip',
    size: 'md',
    onChange: () => {
      refreshAllDashboardCards();
    }
  });

  new ABDSegmented(container.querySelector('#drawer-sel-synth-profile'), {
    paramId: 'synthMode',
    label: 'SYNTH PROFILE',
    options: [
      { value: 0, label: 'MS2000 (4V)' },
      { value: 1, label: 'microKORG (4V)' },
      { value: 2, label: 'ADVANCED (16V)' }
    ],
    variant: 'strip',
    size: 'md',
    onChange: (val) => {
      const targetTheme = themeIdOfSynthMode(val);
      if (themeSwitcher) {
        themeSwitcher.setValue(targetTheme);
      } else {
        applyThemeEffects(targetTheme, Number(val));
      }
    }
  });

  new ABDSegmented(container.querySelector('#drawer-sel-trigger-mode'), {
    paramId: tParam('triggerMode'),
    label: 'TRIGGER MODE',
    options: [
      { value: 0, label: 'SINGLE (LEGATO)' },
      { value: 1, label: 'MULTI (RETRIGGER)' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-key-priority'), {
    paramId: tParam('keyPriority'),
    label: 'KEY PRIORITY (MONO)',
    options: [
      { value: 0, label: 'LAST' },
      { value: 1, label: 'LOW' },
      { value: 2, label: 'HIGH' }
    ],
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-unisondetune'), {
    paramId: tParam('unisonDetune'), label: 'UNISON DETUNE', min: 0, max: 99, defaultValue: 10, size: 50,
    displayFormatter: (v) => `${Math.round(v)} c`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-unisonspread'), {
    paramId: tParam('unisonSpread'), label: 'UNISON SPREAD', min: 0, max: 1, defaultValue: 0.5, size: 50,
    displayFormatter: (v) => `${Math.round(v * 100)}%`
  });

  new ABDSegmented(container.querySelector('#drawer-sel-timbre-mode'), {
    paramId: 'timbreMode',
    label: 'TIMBRE VOICE MODE',
    options: [
      { value: 0, label: 'SINGLE' },
      { value: 1, label: 'SPLIT' },
      { value: 2, label: 'LAYER (DUAL)' }
    ],
    variant: 'strip',
    size: 'md'
  });

  new ABDSegmented(container.querySelector('#drawer-sel-timbre-voices'), {
    paramId: 'timbreVoices',
    label: 'TIMBRE VOICE BALANCE (T1+T2)',
    options: curSynthMode === 2
      ? [
          { value: 0, label: '8 + 8 VOICES' },
          { value: 1, label: '8 + 8 VOICES' },
          { value: 2, label: '8 + 8 VOICES' }
        ]
      : [
          { value: 0, label: '1 + 3 VOICES' },
          { value: 1, label: '2 + 2 VOICES' },
          { value: 2, label: '3 + 1 VOICES' }
        ],
    variant: 'strip',
    size: 'sm'
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-splitpoint'), {
    paramId: 'splitPoint', label: 'SPLIT KEY', min: 0, max: 127, defaultValue: 60, size: 50,
    displayFormatter: formatMidiNote
  });

  container.querySelector('#btn-drawer-copy-t1-t2')?.addEventListener('click', copyTimbre1To2);
  container.querySelector('#btn-drawer-copy-t2-t1')?.addEventListener('click', copyTimbre2To1);
  container.querySelector('#btn-drawer-swap-timbres')?.addEventListener('click', swapTimbres1And2);
}

function renderPitchDrawer(container) {
  const scaleTypes = [
    { value: 0, label: 'EQUAL' },
    { value: 1, label: 'PURE MAJ' },
    { value: 2, label: 'PURE MIN' },
    { value: 3, label: 'ARABIC' },
    { value: 4, label: 'PYTHAG' },
    { value: 5, label: 'WERCK' },
    { value: 6, label: 'KIRNBG' },
    { value: 7, label: 'SLENDRO' },
    { value: 8, label: 'PELOG' },
    { value: 9, label: 'USER' }
  ];
  const scaleKeys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'].map((k, idx) => ({
    value: idx,
    label: k
  }));

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>PORTAMENTO (GLIDE) &amp; PITCH BEND (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row" style="align-items: center;">
        <div id="drawer-sel-porta-on" style="flex: 1; max-width: 150px;"></div>
        <div id="drawer-ctrl-portatime"></div>
        <div id="drawer-ctrl-bendrange"></div>
        <div id="drawer-ctrl-vibratoint"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>TIMBRE TUNING (TIMBRE ${activeTimbre})</h3>
      <div class="drawer-controls-row">
        <div id="drawer-ctrl-transpose"></div>
        <div id="drawer-ctrl-pitchtune"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>PROGRAM SCALE TEMPERAMENT &amp; KEY</h3>
      <div style="display: flex; flex-direction: column; gap: 10px;">
        <div id="drawer-sel-scale-type"></div>
        <div id="drawer-sel-scale-key"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-sel-porta-on'), {
    paramId: tParam('portamentoOn'),
    label: 'PORTAMENTO',
    options: [{ value: 0, label: 'OFF' }, { value: 1, label: 'ON' }],
    variant: 'strip',
    size: 'sm',
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-portatime'), {
    paramId: tParam('portamentoTime'), label: 'PORTA TIME', min: 0, max: 127, defaultValue: 0, size: 48
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-bendrange'), {
    paramId: tParam('pitchBendRange'), label: 'BEND RANGE', min: -12, max: 12, defaultValue: 2, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} st`
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-vibratoint'), {
    paramId: tParam('vibratoInt'), label: 'VIBRATO INT', min: -63, max: 63, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)}`
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-transpose'), {
    paramId: tParam('pitchTranspose'), label: 'TRANSPOSE', min: -24, max: 24, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} st`
  });

  new ABDKnob(container.querySelector('#drawer-ctrl-pitchtune'), {
    paramId: tParam('pitchTune'), label: 'FINE TUNE', min: -50, max: 50, defaultValue: 0, size: 48, isBipolar: true,
    displayFormatter: (v) => `${Math.round(v)} c`
  });

  new ABDSegmented(container.querySelector('#drawer-sel-scale-type'), {
    paramId: 'scaleType',
    label: 'TEMPERAMENT SCALE',
    options: scaleTypes,
    layout: 'grid-5',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-sel-scale-key'), {
    paramId: 'scaleKey',
    label: 'SCALE KEY',
    options: scaleKeys,
    layout: 'grid-6',
    size: 'sm',
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
    displayFormatter: (v) => `${Math.round(v * 100)}%`
  });
  new ABDKnob(container.querySelector('#drawer-ctrl-pedal'), {
    paramId: 'assignablePedal', label: 'ASSIGN PEDAL', min: 0, max: 127, defaultValue: 0, size: 54
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
   DASHBOARD OBSERVERS (UPDATES ALL 8 CARDS FOR ACTIVE TIMBRE)
   ───────────────────────────────────────────────────────────── */
const OSC1_WAVE_NAMES = ['Saw', 'Square', 'Triangle', 'Sine', 'VoxWave', 'DWGS', 'Noise', 'AudioIn'];
const OSC2_WAVE_NAMES = ['Saw', 'Square', 'Triangle'];
const OSC2_MOD_NAMES = ['Off', 'Ring', 'Sync', 'Ring+Sync'];
const FILTER_TYPE_NAMES = ['LPF 24dB', 'LPF 12dB', 'BPF 12dB', 'HPF 12dB'];
const LFO1_WAVE_NAMES = ['Saw', 'Square', 'Triangle', 'S&H'];
const LFO2_WAVE_NAMES = ['Saw', 'Sqr+', 'Sine', 'S&H'];
const VPATCH_SRC_NAMES = ['EG1', 'EG2', 'LFO1', 'LFO2', 'VEL', 'KBD', 'BEND', 'MOD'];
const VPATCH_DST_NAMES = ['Pitch', 'OSC2', 'Ctrl1', 'Noise', 'Cutoff', 'Amp', 'Pan', 'LFO2'];
const VOICE_ASSIGN_NAMES = ['Mono', 'Poly (4V)', 'Unison (4V)'];
const TIMBRE_MODE_NAMES = ['SINGLE', 'SPLIT', 'LAYER'];

function updateArpCard() {
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
}

function refreshAllDashboardCards() {
  // Update Hero Timbre Mode pill
  const tMode = Math.round(paramStore.get('timbreMode') || 0);
  const modeBtn = document.getElementById('btn-hero-timbre-mode');
  if (modeBtn) {
    modeBtn.textContent = TIMBRE_MODE_NAMES[tMode] || 'SINGLE';
    modeBtn.classList.toggle('is-dual', tMode > 0);
  }

  // Card 1: OSC 1 / OSC 2 / MIXER (Active Timbre)
  const osc1Wave = Math.round(paramStore.get(tParam('osc1Wave')) || 0);
  const badgeOsc1 = document.getElementById('badge-osc1-wave');
  const txtOsc1 = document.getElementById('val-osc1-wave');
  if (osc1Wave === 5) {
    const dwgsSlot = Math.round(paramStore.get(tParam('osc1DwgsWave')) || 0);
    const dwgsName = getWaveName(dwgsSlot);
    if (badgeOsc1) badgeOsc1.textContent = `DWGS #${dwgsSlot + 1}`;
    if (txtOsc1) txtOsc1.textContent = `DWGS (${dwgsName})`;
  } else {
    const name = OSC1_WAVE_NAMES[osc1Wave] || 'Saw';
    if (badgeOsc1) badgeOsc1.textContent = `VA ${name.toUpperCase()}`;
    if (txtOsc1) txtOsc1.textContent = name;
  }

  const osc2Wave = Math.round(paramStore.get(tParam('osc2Wave')) || 0);
  const osc2Mod = Math.round(paramStore.get(tParam('osc2ModType')) || 0);
  const txtOsc2 = document.getElementById('val-osc2-mod');
  if (txtOsc2) {
    txtOsc2.textContent = `${OSC2_WAVE_NAMES[osc2Wave] || 'Square'} (${OSC2_MOD_NAMES[osc2Mod] || 'Off'})`;
  }

  const mix1 = paramStore.get(tParam('mixOsc1Level')) ?? 127;
  const mix2 = paramStore.get(tParam('mixOsc2Level')) ?? 0;
  const mixN = paramStore.get(tParam('mixNoiseLevel')) ?? 0;
  const barMix1 = document.getElementById('bar-mix-osc1');
  const barMix2 = document.getElementById('bar-mix-osc2');
  const barMixN = document.getElementById('bar-mix-noise');
  if (barMix1) barMix1.style.width = `${(mix1 / 127) * 100}%`;
  if (barMix2) barMix2.style.width = `${(mix2 / 127) * 100}%`;
  if (barMixN) barMixN.style.width = `${(mixN / 127) * 100}%`;

  // Card 2: VOCODER
  const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');
  const badgeVoc = document.getElementById('badge-vocoder-mode');
  const txtVoc = document.getElementById('val-vocoder-status');
  const titleVoc = document.getElementById('title-vocoder-card');
  const btnBands = document.getElementById('btn-edit-vocoder-bands');
  const vocOn = (paramStore.get('synthVocoderMode') || 0) > 0.5;
  const vocHold = (paramStore.get('vocoderHold') || 0) > 0.5 || Math.round(paramStore.get('vocoderEfSense') ?? 64) >= 127;
  if (titleVoc) titleVoc.textContent = isMicrokorg ? '8-CHANNEL VOCODER' : '16-BAND VOCODER';
  if (btnBands) btnBands.textContent = isMicrokorg ? '8 CHANNELS' : '16 BANDS';
  if (badgeVoc) badgeVoc.textContent = vocOn ? (vocHold ? 'VOC HOLD' : 'VOCODER') : 'SYNTH';
  if (txtVoc) {
    txtVoc.textContent = vocOn
      ? (isMicrokorg
          ? `ON (8Ch${vocHold ? ' · Formant Hold' : ''})`
          : `ON (16B${vocHold ? ' · Formant Hold' : ''})`)
      : 'OFF (Synth Mode)';
  }
  const fShiftNames = ['-2', '-1', '0', '+1', '+2'];
  const fShift = fShiftNames[Math.round(paramStore.get('vocoderFormantShift') ?? 2)] || '0';
  const fcOff = Math.round(paramStore.get('vocoderCutoff') ?? 0);
  const gSense = Math.round(paramStore.get('vocoderGateSense') ?? 50);
  const txtVocShift = document.getElementById('val-vocoder-shift-gate');
  if (txtVocShift) {
    txtVocShift.textContent = `Shift: ${fShift} | Fc: ${fcOff >= 0 ? '+' : ''}${fcOff} | Gate: ${gSense}`;
  }

  // Card 3: FILTER (Active Timbre)
  const fType = Math.round(paramStore.get(tParam('filterType')) || 0);
  const fName = FILTER_TYPE_NAMES[fType] || 'LPF 24dB';
  const badgeFlt = document.getElementById('badge-filter-type');
  const txtFltType = document.getElementById('val-filter-type');
  if (badgeFlt) badgeFlt.textContent = fName;
  if (txtFltType) txtFltType.textContent = fName;

  const cutoff = Math.round(paramStore.get(tParam('filterCutoff')) ?? 127);
  const hz = Math.round(20 * Math.pow(1000, cutoff / 127));
  const txtCutoff = document.getElementById('val-filter-cutoff');
  if (txtCutoff) {
    txtCutoff.textContent = `${cutoff} (${hz >= 1000 ? (hz / 1000).toFixed(1) + ' kHz' : hz + ' Hz'})`;
  }

  const reso = Math.round(paramStore.get(tParam('filterResonance')) || 0);
  const eg1Int = Math.round(paramStore.get(tParam('filterEg1Int')) || 0);
  const txtReso = document.getElementById('val-filter-reso-eg1');
  if (txtReso) {
    txtReso.textContent = `Reso: ${reso} | EG1: ${eg1Int >= 0 ? '+' : ''}${eg1Int}`;
  }

  // Card 4: AMP & ENVELOPES (Active Timbre)
  const ampVol = Math.round(paramStore.get(tParam('ampLevel')) ?? 100);
  const ampPan = Math.round(paramStore.get(tParam('ampPan')) || 0);
  const txtAmp = document.getElementById('val-amp-level-pan');
  if (txtAmp) {
    txtAmp.textContent = `Vol: ${ampVol} | Pan: ${ampPan === 0 ? 'C' : (ampPan > 0 ? 'R' + ampPan : 'L' + Math.abs(ampPan))}`;
  }

  const eg1A = Math.round(paramStore.get(tParam('eg1Attack')) || 0);
  const eg1D = Math.round(paramStore.get(tParam('eg1Decay')) ?? 64);
  const eg1S = Math.round(paramStore.get(tParam('eg1Sustain')) || 0);
  const eg1R = Math.round(paramStore.get(tParam('eg1Release')) ?? 40);
  const txtEg1 = document.getElementById('val-eg1-adsr');
  if (txtEg1) txtEg1.textContent = `${eg1A} / ${eg1D} / ${eg1S} / ${eg1R}`;

  const eg2A = Math.round(paramStore.get(tParam('eg2Attack')) || 0);
  const eg2D = Math.round(paramStore.get(tParam('eg2Decay')) ?? 64);
  const eg2S = Math.round(paramStore.get(tParam('eg2Sustain')) ?? 127);
  const eg2R = Math.round(paramStore.get(tParam('eg2Release')) ?? 20);
  const txtEg2 = document.getElementById('val-eg2-adsr');
  if (txtEg2) txtEg2.textContent = `${eg2A} / ${eg2D} / ${eg2S} / ${eg2R}`;

  // Card 5: LFOS & MOD MATRIX (Active Timbre)
  const lfo1W = LFO1_WAVE_NAMES[Math.round(paramStore.get(tParam('lfo1Wave')) || 0)] || 'Triangle';
  const lfo1F = paramStore.get(tParam('lfo1Freq')) ?? 30;
  const lfo1Hz = (0.01 + Math.pow(lfo1F / 127, 2) * 20).toFixed(1);
  const txtLfo1 = document.getElementById('val-lfo1');
  if (txtLfo1) txtLfo1.textContent = `${lfo1W} (${lfo1Hz} Hz)`;

  const lfo2W = LFO2_WAVE_NAMES[Math.round(paramStore.get(tParam('lfo2Wave')) || 0)] || 'Sine';
  const lfo2F = paramStore.get(tParam('lfo2Freq')) ?? 50;
  const lfo2Hz = (0.01 + Math.pow(lfo2F / 127, 2) * 20).toFixed(1);
  const txtLfo2 = document.getElementById('val-lfo2');
  if (txtLfo2) txtLfo2.textContent = `${lfo2W} (${lfo2Hz} Hz)`;

  const txtVpatch = document.getElementById('val-vpatch-summary');
  if (txtVpatch) {
    let activeSlotText = '';
    for (let s = 1; s <= 4; s++) {
      const pInt = Math.round(paramStore.get(tParam(`patch${s}Intensity`)) || 0);
      if (pInt !== 0) {
        const src = VPATCH_SRC_NAMES[Math.round(paramStore.get(tParam(`patch${s}Source`)) || 0)] || 'LFO1';
        const dst = VPATCH_DST_NAMES[Math.round(paramStore.get(tParam(`patch${s}Destination`)) || 0)] || 'Pitch';
        activeSlotText = `P${s}: ${src} -> ${dst} (${pInt > 0 ? '+' : ''}${pInt})`;
        break;
      }
    }
    if (!activeSlotText) {
      const src1 = VPATCH_SRC_NAMES[Math.round(paramStore.get(tParam('patch1Source')) || 0)] || 'EG1';
      const dst1 = VPATCH_DST_NAMES[Math.round(paramStore.get(tParam('patch1Destination')) || 0)] || 'Pitch';
      activeSlotText = `P1: ${src1} -> ${dst1} (0)`;
    }
    txtVpatch.textContent = activeSlotText;
  }

  // Card 6: SEQ & ARPEGGIATOR
  updateArpCard();
  const txtModSeq = document.getElementById('val-modseq-status');
  const badgeSeqArp = document.querySelector('#card-seq-arp .card-badge');
  const btnModSeq = document.getElementById('btn-edit-modseq');
  if (badgeSeqArp) badgeSeqArp.textContent = isMicrokorg ? 'ARPEGGIATOR' : '3x16 STEPS';
  if (btnModSeq) {
    btnModSeq.style.opacity = isMicrokorg ? '0.4' : '1';
    btnModSeq.title = isMicrokorg ? 'Modulation Sequencer is not available in microKORG hardware profile' : '';
  }
  if (txtModSeq) {
    if (isMicrokorg) {
      txtModSeq.textContent = 'N/A (microKORG)';
      txtModSeq.style.opacity = '0.6';
    } else {
      txtModSeq.style.opacity = '1';
      const seqOn = (paramStore.get('modSeqOn') || 0) > 0.5;
      if (!seqOn) {
        txtModSeq.textContent = 'OFF';
      } else {
        const dirs = ['Forward', 'Reverse', 'Bounce', 'Random'];
        const dir = dirs[paramStore.get('modSeqType') || 0] || 'Forward';
        const motion = ['Step', 'Smooth'][paramStore.get(tParam('seq1Motion')) || 0] || 'Step';
        const last = Math.round(paramStore.get(tParam('seqLastStep')) || 16);
        txtModSeq.textContent = `ON (${dir} / ${motion} / ${last} steps)`;
      }
    }
  }

  // Card 7: FX & EQUALIZER
  const txtModFx = document.getElementById('val-fx-mod');
  if (txtModFx) {
    const mOn = (paramStore.get('modFxOn') ?? 1) > 0.5;
    if (!mOn) {
      txtModFx.textContent = 'BYPASS';
    } else {
      const types = ['Chorus/Flanger', 'Ensemble', 'Phaser'];
      const t = types[paramStore.get('modFxType') || 0] || 'Chorus';
      const depth = Math.round(paramStore.get('modFxDepth') ?? 64);
      txtModFx.textContent = `${t} (Depth: ${depth})`;
    }
  }

  const txtDelay = document.getElementById('val-fx-delay');
  if (txtDelay) {
    const dOn = (paramStore.get('delayOn') ?? 1) > 0.5;
    if (!dOn) {
      txtDelay.textContent = 'BYPASS';
    } else {
      const types = ['Stereo', 'Cross (Ping-Pong)', 'L/R'];
      const t = types[paramStore.get('delayType') || 0] || 'Stereo';
      const time = Math.round(paramStore.get('delayTime') ?? 40);
      txtDelay.textContent = `${t} (Time: ${time})`;
    }
  }

  const txtEq = document.getElementById('val-fx-eq');
  if (txtEq) {
    const lg = paramStore.get('eqLowGain') ?? 64;
    const hg = paramStore.get('eqHighGain') ?? 64;
    const ldB = ((lg - 64) * (12 / 64)).toFixed(1);
    const hdB = ((hg - 64) * (12 / 64)).toFixed(1);
    txtEq.textContent = `Low ${ldB >= 0 ? '+' : ''}${ldB}dB / High ${hdB >= 0 ? '+' : ''}${hdB}dB`;
  }

  // Card 8: VOICE, PITCH & MASTER (Active Timbre)
  const synthModeVal = Math.round(paramStore.get('synthMode') || 0);
  const polyVoicesLabel = synthModeVal === 2 ? '16V' : '4V';
  const dynVoiceNames = ['Mono', `Poly (${polyVoicesLabel})`, `Unison (${polyVoicesLabel})`];
  const vMode = Math.round(paramStore.get(tParam('voiceMode')) ?? 1);
  const modeStr = dynVoiceNames[vMode] || `Poly (${polyVoicesLabel})`;
  const badgeVoice = document.getElementById('badge-voice-mode');
  const txtVoice = document.getElementById('val-voice-mode');
  if (badgeVoice) badgeVoice.textContent = modeStr.toUpperCase().split(' ')[0];
  if (txtVoice) txtVoice.textContent = `${modeStr} [${TIMBRE_MODE_NAMES[tMode] || 'SINGLE'}]`;

  const uDetune = Math.round(paramStore.get(tParam('unisonDetune')) ?? 10);
  const pOn = (paramStore.get(tParam('portamentoOn')) || 0) > 0.5;
  const pTime = Math.round(paramStore.get(tParam('portamentoTime')) || 0);
  const txtUniGlide = document.getElementById('val-unison-glide');
  if (txtUniGlide) {
    txtUniGlide.textContent = `Detune: ${uDetune}c | Porta: ${pOn ? pTime : 'OFF'}`;
  }

  const mVol = paramStore.get('masterVolume') ?? 0.8;
  const txtMaster = document.getElementById('val-master-vol');
  if (txtMaster) txtMaster.textContent = `${Math.round(mVol * 100)}%`;

  updateModeMenuState();
  refreshMs2kBottomStrip();
}

function updateModeMenuState() {
  const synthModeVal = Math.round(paramStore.get('synthMode') || 0);
  const vMode = Math.round(paramStore.get(tParam('voiceMode')) ?? 1);
  const voicesText = synthModeVal === 2 ? '16 Voices' : '4 Voices';

  const polyLabel = document.getElementById('menu-voice-poly-label');
  const uniLabel = document.getElementById('menu-voice-unison-label');
  if (polyLabel) polyLabel.textContent = `Voice: Poly (${voicesText})`;
  if (uniLabel) uniLabel.textContent = `Voice: Unison Stack (${voicesText})`;

  for (let i = 0; i < 3; i++) {
    const chk = document.getElementById(`check-voice-${i}`);
    if (chk) chk.textContent = vMode === i ? '✓' : '';
  }

  const chkMs2k = document.getElementById('check-mode-ms2000');
  const chkMk = document.getElementById('check-mode-microkorg');
  const chkAdv = document.getElementById('check-mode-advanced');
  if (chkMs2k) chkMs2k.textContent = synthModeVal === 0 ? '✓' : '';
  if (chkMk) chkMk.textContent = synthModeVal === 1 ? '✓' : '';
  if (chkAdv) chkAdv.textContent = synthModeVal === 2 ? '✓' : '';
}

function setGlobalMidiChannel(ch, notifyLcd = true) {
  const clamped = Math.max(1, Math.min(16, Math.round(Number(ch) || 1)));
  globalMidiChannel = clamped;

  const chLabel = document.getElementById('menu-midi-channel-label');
  if (chLabel) chLabel.textContent = `Global MIDI Channel: ${clamped}`;
  const chBadge = document.getElementById('menu-midi-channel-badge');
  if (chBadge) chBadge.innerHTML = `Ch ${clamped} &#9656;`;

  document.querySelectorAll('.midi-ch-option[data-midi-ch]').forEach((btn) => {
    btn.classList.toggle('active', Number(btn.dataset.midiCh) === clamped);
  });

  if (lcdProgrammer && lcdProgrammer.globalState) {
    lcdProgrammer.globalState.midiCh = clamped;
    lcdProgrammer.updateDisplay();
    if (notifyLcd) {
      lcdProgrammer.flashMessage('GLOBAL MIDI CH', `CHANNEL ${String(clamped).padStart(2, '0')}`);
    }
  }

  bridge.send('setMidiChannel', { channel: clamped });
}

function setupDashboardObservers() {
  const watchedParams = [
    'timbreMode', 'synthMode', 'synthVocoderMode', 'vocoderFormantShift', 'vocoderGateSense',
    'osc1Wave', 'osc1DwgsWave', 'osc2Wave', 'osc2ModType', 'mixOsc1Level', 'mixOsc2Level', 'mixNoiseLevel',
    'filterType', 'filterCutoff', 'filterResonance', 'filterEg1Int',
    'ampLevel', 'ampPan', 'eg1Attack', 'eg1Decay', 'eg1Sustain', 'eg1Release',
    'eg2Attack', 'eg2Decay', 'eg2Sustain', 'eg2Release',
    'lfo1Wave', 'lfo1Freq', 'lfo2Wave', 'lfo2Freq',
    'patch1Source', 'patch1Destination', 'patch1Intensity',
    'patch2Source', 'patch2Destination', 'patch2Intensity',
    'patch3Source', 'patch3Destination', 'patch3Intensity',
    'patch4Source', 'patch4Destination', 'patch4Intensity',
    'arpOn', 'arpType', 'arpRange', 'arpLastStep',
    'arpStep1', 'arpStep2', 'arpStep3', 'arpStep4', 'arpStep5', 'arpStep6', 'arpStep7', 'arpStep8',
    'modSeqOn', 'modSeqType', 'seq1Motion', 'seqLastStep',
    'modFxOn', 'modFxType', 'modFxDepth', 'delayOn', 'delayType', 'delayTime', 'eqLowGain', 'eqHighGain',
    'voiceMode', 'unisonDetune', 'portamentoOn', 'portamentoTime', 'masterVolume'
  ];

  watchedParams.forEach((baseId) => {
    paramStore.onChange(baseId, refreshAllDashboardCards);
    const t2Id = 't2' + baseId.charAt(0).toUpperCase() + baseId.slice(1);
    if (PARAM_LOOKUP[t2Id]) {
      paramStore.onChange(t2Id, refreshAllDashboardCards);
    }
  });

  refreshAllDashboardCards();
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
    if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'osc1') {
      renderOsc1Drawer(slideDrawer.contentElem);
    }
    if (slideDrawer?.isOpenState && slideDrawer?.activeSection === 'voice') {
      renderVoiceDrawer(slideDrawer.contentElem);
    }
    refreshAllDashboardCards();
  });
}

/**
 * Los efectos del modo de motor y tema — cambia la arquitectura real del sintetizador:
 * - MS2000 (0): 4 voces, 64 DWGS, Vocoder de 16 bandas, Mod Sequencer 3x16 activo.
 * - microKORG (1): 4 voces, 64 DWGS, Vocoder de 8 canales agrupados, Arpegiador de 8 pasos, sin Mod Seq.
 * - Advanced (2): 16 voces, 512 DWGS, Vocoder de 16 bandas + Mod Seq 3x16 + Arp 8 pasos.
 */
function applyThemeEffects(mode, modeIdx = synthModeIndexOf(mode)) {
  paramStore.set('synthMode', modeIdx);
  bridge.setParam('synthMode', modeIdx);

  // Recortar DWGS a 64 ondas cuando se sale del modo Advanced (512 ondas)
  if (modeIdx !== 2) {
    const dwgs1 = paramStore.get('osc1DwgsWave') || 0;
    if (dwgs1 > 63) {
      paramStore.set('osc1DwgsWave', 63);
      bridge.setParam('osc1DwgsWave', 63);
    }
    const dwgs2 = paramStore.get('t2Osc1DwgsWave') || 0;
    if (dwgs2 > 63) {
      paramStore.set('t2Osc1DwgsWave', 63);
      bridge.setParam('t2Osc1DwgsWave', 63);
    }
  }

  // En modo microKORG (1): desactivar Mod Sequencer y emparejar las 8 bandas del Vocoder
  if (modeIdx === 1) {
    paramStore.set('modSeqOn', 0);
    bridge.setParam('modSeqOn', 0);
    if (PARAM_LOOKUP.t2ModSeqOn) {
      paramStore.set('t2ModSeqOn', 0);
      bridge.setParam('t2ModSeqOn', 0);
    }
    for (let ch = 0; ch < 8; ch++) {
      const bOdd = ch * 2 + 1;
      const bEven = ch * 2 + 2;
      const lvl = paramStore.get(`vocoderBandLevel${bOdd}`) ?? 100;
      paramStore.set(`vocoderBandLevel${bEven}`, lvl);
      bridge.setParam(`vocoderBandLevel${bEven}`, lvl);
    }
  }

  bankManagerModal?.setTheme(mode);
  devLog('[Synth Mode & Profile Switch]:', mode, 'modeIdx:', modeIdx);

  if (slideDrawer?.isOpenState) {
    if (slideDrawer.activeSection === 'vocoder-bands') {
      const isMk = (modeIdx === 1);
      if (slideDrawer.titleElem) {
        slideDrawer.titleElem.textContent = isMk ? 'VOCODER: 8-CHANNEL LEVELS (microKORG)' : 'VOCODER: 16-BAND LEVELS (MS2000)';
      }
      if (slideDrawer.badgeElem) {
        slideDrawer.badgeElem.textContent = isMk ? '8 CHANNELS' : '16 BANDS';
      }
      renderVocoderBandsDrawer(slideDrawer.contentElem);
    } else if (slideDrawer.activeSection === 'arp') {
      renderArpDrawer(slideDrawer.contentElem);
    } else if (slideDrawer.activeSection === 'modseq') {
      renderModSeqDrawer(slideDrawer.contentElem);
    } else if (slideDrawer.activeSection === 'osc1') {
      renderOsc1Drawer(slideDrawer.contentElem);
    } else if (slideDrawer.activeSection === 'voice') {
      renderVoiceDrawer(slideDrawer.contentElem);
    }
  }

  refreshAllDashboardCards();

  if (lcdProgrammer) {
    if (modeIdx === 0) {
      lcdProgrammer.flashMessage('MODE: KORG MS2000', '4V 16B-VOC MSEQ');
    } else if (modeIdx === 1) {
      lcdProgrammer.flashMessage('MODE: microKORG', '4V 8CH-VOC 8ARP');
    } else {
      lcdProgrammer.flashMessage('MODE: ADVANCED', '16V 512-DWGS ALL');
    }
  }
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
        updateModeMenuState();
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
          updateModeMenuState();
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
      if (action === 'toggle-midi-channel-submenu') {
        document.getElementById('midi-channel-submenu-wrap')?.classList.toggle('open');
        return;
      }
      closeAllDropdowns();
      if (action) handleMenuAction(action);
    });
  });

  // 16-Channel Global MIDI Submenu buttons
  document.querySelectorAll('#menu-midi-ch-grid .midi-ch-option').forEach((chBtn) => {
    chBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const ch = Number(chBtn.dataset.midiCh || 1);
      setGlobalMidiChannel(ch, true);
      closeAllDropdowns();
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
  document.getElementById('midi-channel-submenu-wrap')?.classList.remove('open');
}

function handleMenuAction(action) {
  devLog(`[Menu Action Triggered]: ${action}`);
  switch (action) {
    case 'open-bank-manager':
      bankManagerModal?.open();
      break;
    case 'open-firmware-updater':
      firmwareUpdaterModal?.open();
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
      paramStore.set(tParam('voiceMode'), 0);
      bridge.setParam(tParam('voiceMode'), 0);
      refreshAllDashboardCards();
      if (lcdProgrammer) lcdProgrammer.flashMessage('VOICE ASSIGN', 'SINGLE / MONO');
      devLog('[Voice Mode]: Single / Mono');
      break;
    case 'voice-mode-poly':
      paramStore.set(tParam('voiceMode'), 1);
      bridge.setParam(tParam('voiceMode'), 1);
      refreshAllDashboardCards();
      if (lcdProgrammer) {
        const vCnt = Math.round(paramStore.get('synthMode') || 0) === 2 ? '16 VOICES' : '4 VOICES';
        lcdProgrammer.flashMessage('VOICE ASSIGN', `POLY (${vCnt})`);
      }
      devLog('[Voice Mode]: Poly');
      break;
    case 'voice-mode-unison':
      paramStore.set(tParam('voiceMode'), 2);
      bridge.setParam(tParam('voiceMode'), 2);
      refreshAllDashboardCards();
      if (lcdProgrammer) {
        const vCnt = Math.round(paramStore.get('synthMode') || 0) === 2 ? '16 VOICES' : '4 VOICES';
        lcdProgrammer.flashMessage('VOICE ASSIGN', `UNISON (${vCnt})`);
      }
      devLog('[Voice Mode]: Unison');
      break;
    case 'mode-ms2000':
    case 'mode-microkorg':
    case 'mode-advanced':
    case 'skin-ms2000':
    case 'skin-microkorg':
    case 'skin-cyberpunk': {
      const themeId = action
        .replace('skin-', '')
        .replace('mode-', '')
        .replace('cyberpunk', 'advanced');
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
      refreshAllDashboardCards();
      if (slideDrawer.isOpenState) slideDrawer.refresh();
      {
        const lcdInit = document.getElementById('lcd-line-2');
        if (lcdInit) lcdInit.textContent = '[Init Synth]    ';
      }
      devLog('[Preset Reset to Default]');
      break;
    case 'randomize-patch':
    case 'randomize':
      if (!bridge.isWebView) {
        paramStore.generateMusicalRandom(bridge);
      }
      bridge.send('randomizePatch', {});
      refreshAllDashboardCards();
      if (slideDrawer.isOpenState) slideDrawer.refresh();
      {
        const lcdRand = document.getElementById('lcd-line-2');
        if (lcdRand) lcdRand.textContent = '[Random Patch]  ';
      }
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
    case 'toggle-sysex-inspector':
      toggleLiveSysExInspector();
      break;
    case 'undo': {
      const res = paramStore.undo?.(bridge);
      if (res?.ok) {
        refreshAllDashboardCards();
        if (slideDrawer.isOpenState) slideDrawer.refresh();
        if (lcdProgrammer) lcdProgrammer.flashMessage('UNDO', res.label || 'STEP RESTORED');
      }
      break;
    }
    case 'redo': {
      const res = paramStore.redo?.(bridge);
      if (res?.ok) {
        refreshAllDashboardCards();
        if (slideDrawer.isOpenState) slideDrawer.refresh();
        if (lcdProgrammer) lcdProgrammer.flashMessage('REDO', res.label || 'STEP APPLIED');
      }
      break;
    }
    case 'copy-timbre':
      timbreClipboard = {};
      getTimbreParamPairs().forEach(([t1Id]) => {
        timbreClipboard[t1Id] = paramStore.get(t1Id);
      });
      {
        const lcdCopy = document.getElementById('lcd-line-2');
        if (lcdCopy) lcdCopy.textContent = '[Timbre 1 Copy] ';
      }
      devLog('[Timbre 1 Copied to Clipboard]');
      break;
    case 'paste-timbre':
      if (timbreClipboard) {
        getTimbreParamPairs().forEach(([t1Id, t2Id]) => {
          if (Object.prototype.hasOwnProperty.call(timbreClipboard, t1Id)) {
            const v = timbreClipboard[t1Id];
            paramStore.set(t2Id, v);
            bridge.setParam(t2Id, v);
          }
        });
        const lcdPaste = document.getElementById('lcd-line-2');
        if (lcdPaste) lcdPaste.textContent = '[Paste -> T2]   ';
        devLog('[Timbre Pasted to Timbre 2]');
      }
      break;
    case 'copy-timbre-1-to-2':
      copyTimbre1To2();
      break;
    case 'copy-timbre-2-to-1':
      copyTimbre2To1();
      break;
    case 'swap-timbres':
      swapTimbres1And2();
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
      openMidiConfigModal('devices');
      break;
    case 'midi-channel': {
      setGlobalMidiChannel((globalMidiChannel % 16) + 1, true);
      break;
    }
    case 'open-manual':
      window.open('https://github.com/ajabadia/ABDMS2000#readme', '_blank');
      break;
    case 'midi-cc-chart':
      openMidiConfigModal('cc-chart');
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
    getJuceBackend: () => (bridge.isWebView ? {
      openBrowserUrl: (url) => bridge.send('openBrowserUrl', { url }),
      checkForUpdates: () => bridge.send('checkForUpdates', {}),
      getUpdateStatus: () => bridge.send('getUpdateStatus', {}),
      addEventListener: (ev, cb) => bridge.on(ev, cb),
    } : null)
  });

  bridge.on('updateCheckResult', (data) => {
    aboutDialogInstance?.renderUpdateResult?.(data);
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
    onHoldChange: (active) => {
      if (lcdProgrammer) {
        lcdProgrammer.flashMessage('KEYBOARD HOLD', active ? 'LATCH ACTIVE' : 'HOLD RELEASED');
      }
    },
    onRetrigChange: (active) => {
      if (lcdProgrammer) {
        lcdProgrammer.flashMessage('AUTO RETRIG', active ? 'REPEATING NOTES' : 'RETRIG OFF');
      }
    },
    onCollapseChange: (collapsed) => {
      const kbdFooter = document.querySelector('.keyboard-container');
      if (kbdFooter) {
        kbdFooter.classList.toggle('collapsed', collapsed);
      }
    },
    onOctaveChange: () => { /* LCD update handled by lcdProgrammer */ },
    config: {
      numOctaves: 4,
      startNote: 36,
      maxOctaveShift: 3,
      velocitySource: 'yPosition',
      velocityCurve: 'normal',
      enableCollapse: true,
      enableIvoryTexture: true,
      enableQwerty: true,
      showQwertyLabels: true,
      enableHold: true,
      enableRetrig: true,
      retrigIntervalMs: 800,
      retrigGateRatio: 0.6,
      enableTouch: true,
      enableResizeObserver: true
    }
  });
  // Expose globally so LCD programmer and other modules can trigger animations
  window.__kbd = keyboardInstance;
}


function setupButtons() {
  // UX2.1: Timbre 1 / Timbre 2 & Timbre Mode Hero Bar Controls
  document.getElementById('btn-timbre-1')?.addEventListener('click', () => {
    setActiveTimbre(1);
  });
  document.getElementById('btn-timbre-2')?.addEventListener('click', () => {
    setActiveTimbre(2);
  });
  document.getElementById('btn-hero-timbre-mode')?.addEventListener('click', () => {
    const cur = Math.round(paramStore.get('timbreMode') || 0);
    const next = (cur + 1) % 3;
    paramStore.set('timbreMode', next);
    bridge.setParam('timbreMode', next);
    refreshAllDashboardCards();
    if (lcdProgrammer) {
      lcdProgrammer.flashMessage('VOICE MODE', TIMBRE_MODE_NAMES[next] || 'SINGLE');
    }
  });

  const panicBtn = document.getElementById('btn-panic');
  panicBtn?.addEventListener('click', () => {
    if (lcdProgrammer) lcdProgrammer.stopDemoSequence();
    releaseAllPadLatchedNotes();
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
    if (lcdProgrammer) {
      lcdProgrammer.toggleCompare();
      refreshAllDashboardCards();
      if (slideDrawer.isOpenState) slideDrawer.refresh();
    }
    devLog('[Compare A/B Toggled]');
  });

  const initBtn = document.getElementById('btn-init-patch');
  initBtn?.addEventListener('click', () => {
    handleMenuAction('new-patch');
    if (lcdProgrammer) lcdProgrammer.updateSysExDisplay();
  });

  const openSysexBtn = document.getElementById('btn-open-sysex-inspector');
  openSysexBtn?.addEventListener('click', () => {
    toggleLiveSysExInspector();
  });
}


function setupKeyboardShortcuts() {
  // QWERTY keyboard is now handled by the unified keyboard component.
  // Global modal shortcuts and Escape key handling
  document.addEventListener('keydown', (e) => {
    if (e.ctrlKey || e.metaKey) {
      if (e.key === 'z' || e.key === 'Z') {
        e.preventDefault();
        if (e.shiftKey) {
          handleMenuAction('redo');
        } else {
          handleMenuAction('undo');
        }
      } else if (e.key === 'y' || e.key === 'Y') {
        e.preventDefault();
        handleMenuAction('redo');
      } else if ((e.key === 'r' || e.key === 'R') && e.shiftKey) {
        e.preventDefault();
        handleMenuAction('randomize-patch');
      } else if (e.key === 'b' || e.key === 'B') {
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
      } else if (e.key === 'i' || e.key === 'I') {
        e.preventDefault();
        toggleLiveSysExInspector();
      }
    }
    if (e.key === 'Escape') {
      closeAllDropdowns();
      closeMidiConfigModal();
      diagnosticModal.close();
      firmwareUpdaterModal.close();
      bankManagerModal?.close();
      closeAboutModal();   // por el contrato: foco al disparador + inert
    }
  });
}

let midiConfigState = {
  inputs: [],
  outputs: [],
  selectedInputId: '',
  selectedOutputId: '',
  isStandalone: false,
  statusText: 'Host DAW / Default MIDI Routing Active'
};

function setupMidiConfigModal() {
  const modal = document.getElementById('midi-config-modal');
  if (!modal) return;

  modal.innerHTML = `
    <div class="modal-box diagnostic-modal-box" style="max-width: 720px; width: 94%; text-align: left;">
      <div class="diag-header">
        <div class="diag-title">
          <span class="diag-badge-icon" style="background: rgba(56, 189, 248, 0.16); color: var(--color-accent);">
            <svg class="ui-icon" viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2">
              <circle cx="12" cy="12" r="9"></circle>
              <circle cx="9" cy="10" r="1.2" fill="currentColor"></circle>
              <circle cx="15" cy="10" r="1.2" fill="currentColor"></circle>
              <circle cx="7.5" cy="14" r="1.2" fill="currentColor"></circle>
              <circle cx="12" cy="15.5" r="1.2" fill="currentColor"></circle>
              <circle cx="16.5" cy="14" r="1.2" fill="currentColor"></circle>
            </svg>
          </span>
          <h3>MIDI DEVICE &amp; ROUTING CONFIGURATION</h3>
        </div>
        <button id="midi-cfg-close-top" class="diag-icon-btn" title="Close (Esc)">✕</button>
      </div>
      <div class="modal-sub" style="margin-bottom: 12px;">
        Configure Hardware MIDI Input/Output Ports, Global MIDI Channel (1&ndash;16), Clock Sync &amp; CC Mapping
      </div>

      <div class="diag-body" style="gap: 12px; max-height: 70vh; overflow-y: auto;">
        <!-- 1. Hardware MIDI Input & Output Ports -->
        <div class="diag-card">
          <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px;">
            <div class="diag-card-title" style="margin: 0;">1. HARDWARE MIDI PORTS (DIRECT SYSEX, NOTES &amp; CC)</div>
            <span id="midi-cfg-status-badge" style="font-size: 0.68rem; font-weight: 700; padding: 2px 8px; border-radius: 10px; background: rgba(56, 189, 248, 0.15); color: var(--color-accent);">
              READY
            </span>
          </div>
          <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-bottom: 10px;">
            <div>
              <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 4px;">MIDI Input Device (Controller / MS2000 MIDI OUT):</label>
              <select id="midi-cfg-input-select" style="width: 100%; padding: 7px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.78rem;">
                <option value="">(Host DAW / Default MIDI Input)</option>
              </select>
            </div>
            <div>
              <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 4px;">MIDI Output Device (To Hardware MS2000 MIDI IN):</label>
              <select id="midi-cfg-output-select" style="width: 100%; padding: 7px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.78rem;">
                <option value="">(None / Host DAW MIDI Output)</option>
              </select>
            </div>
          </div>
          <div style="display: flex; flex-wrap: wrap; justify-content: space-between; align-items: center; gap: 8px;">
            <div style="display: flex; gap: 8px;">
              <button type="button" id="midi-cfg-refresh-btn" class="card-action-btn" style="padding: 6px 12px;">REFRESH PORTS</button>
              <button type="button" id="midi-cfg-apply-btn" class="card-action-btn" style="padding: 6px 14px; border-color: var(--color-accent); color: var(--color-accent); font-weight: 700;">APPLY &amp; CONNECT PORTS</button>
            </div>
            <button type="button" id="midi-cfg-native-audio-btn" class="card-action-btn" style="padding: 6px 12px; display: none;" title="Open Standalone ASIO / CoreAudio / Windows Audio &amp; MIDI Driver Settings">
              STANDALONE AUDIO / DRIVER OPTIONS...
            </button>
          </div>
        </div>

        <!-- 2. Global MIDI Channel (1..16) & Clock Sync -->
        <div class="diag-card">
          <div class="diag-card-title">2. GLOBAL MIDI CHANNEL (1&ndash;16) &amp; CLOCK SYNC</div>
          <div style="display: grid; grid-template-columns: 1.5fr 1fr; gap: 14px; align-items: center;">
            <div>
              <div style="font-size: 0.72rem; color: #94a3b8; margin-bottom: 6px;">Active Global MIDI Channel (Notes, CCs &amp; SysEx Header <code>0x30+ch</code>):</div>
              <div class="midi-ch-grid" id="midi-cfg-modal-ch-grid" style="grid-template-columns: repeat(8, 1fr);">
                ${Array.from({ length: 16 }, (_, idx) => {
                  const ch = idx + 1;
                  return `<button type="button" class="midi-ch-option ${ch === globalMidiChannel ? 'active' : ''}" data-midi-ch="${ch}">Ch ${ch}</button>`;
                }).join('')}
              </div>
            </div>
            <div>
              <label style="display: block; font-size: 0.72rem; color: #94a3b8; margin-bottom: 6px;">MIDI Clock Source (Arpeggiator / Mod Seq / LFO Sync):</label>
              <select id="midi-cfg-clock-select" style="width: 100%; padding: 7px 8px; background: #0f172a; color: #e2e8f0; border: 1px solid #334155; border-radius: 4px; font-size: 0.78rem;">
                <option value="Auto">Auto (Sync to Host DAW / External Clock if present)</option>
                <option value="Internal">Internal Clock Only</option>
                <option value="External">External MIDI Clock</option>
              </select>
              <div id="midi-cfg-activity-monitor" style="margin-top: 8px; padding: 6px 8px; background: #090d14; border: 1px solid #1e293b; border-radius: 4px; font-family: var(--font-lcd); font-size: 0.72rem; color: #94a3b8;">
                MIDI Monitor: Waiting for MIDI activity...
              </div>
            </div>
          </div>
        </div>

        <!-- 3. Korg MS2000 MIDI CC Reference -->
        <div class="diag-card" id="midi-cfg-cc-reference-card">
          <div class="diag-card-title">3. KORG MS2000 HARDWARE MIDI CC MAPPING</div>
          <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 6px 12px; font-size: 0.72rem; color: #cbd5e1;">
            <div><code>CC#01</code> Mod Wheel</div>
            <div><code>CC#05</code> Portamento Time</div>
            <div><code>CC#07</code> Amp Level</div>
            <div><code>CC#10</code> Panpot</div>
            <div><code>CC#12</code> OSC1 Control 1</div>
            <div><code>CC#13</code> OSC1 Control 2</div>
            <div><code>CC#14</code> OSC1 Wave</div>
            <div><code>CC#15</code> OSC2 Wave</div>
            <div><code>CC#16</code>EG1 Attack</div>
            <div><code>CC#18</code> OSC2 Semitone</div>
            <div><code>CC#19</code> OSC2 Tune</div>
            <div><code>CC#20</code> Mix OSC1 Level</div>
            <div><code>CC#21</code> Mix OSC2 Level</div>
            <div><code>CC#22</code> Mix Noise Level</div>
            <div><code>CC#23</code> EG1 Sustain</div>
            <div><code>CC#24</code> EG1 Release</div>
            <div><code>CC#26</code> LFO1 Wave</div>
            <div><code>CC#27</code> LFO1 Frequency</div>
            <div><code>CC#64</code> Sustain Damper</div>
            <div><code>CC#71</code> Filter Resonance</div>
            <div><code>CC#72</code> EG2 Release</div>
            <div><code>CC#73</code> EG2 Attack</div>
            <div><code>CC#74</code> Filter Cutoff</div>
            <div><code>CC#75</code> EG2 Decay</div>
            <div><code>CC#79</code> Filter EG1 Int</div>
            <div><code>CC#83</code> Filter Type</div>
            <div><code>CC#93</code> Mod FX Depth</div>
          </div>
        </div>
      </div>

      <div class="diag-footer" style="margin-top: 12px;">
        <button type="button" id="midi-cfg-close-btn" class="modal-close-btn">CLOSE</button>
      </div>
    </div>
  `;

  const renderPortSelectors = () => {
    const inSel = document.getElementById('midi-cfg-input-select');
    const outSel = document.getElementById('midi-cfg-output-select');
    const nativeBtn = document.getElementById('midi-cfg-native-audio-btn');
    const badge = document.getElementById('midi-cfg-status-badge');

    if (inSel) {
      const curIn = midiConfigState.selectedInputId || inSel.value || '';
      inSel.innerHTML = `<option value="">(Host DAW / Default MIDI Input)</option>` +
        midiConfigState.inputs.map(p => `<option value="${p.identifier}">${p.name}</option>`).join('');
      inSel.value = curIn;
    }
    if (outSel) {
      const curOut = midiConfigState.selectedOutputId || outSel.value || '';
      outSel.innerHTML = `<option value="">(None / Host DAW MIDI Output)</option>` +
        midiConfigState.outputs.map(p => `<option value="${p.identifier}">${p.name}</option>`).join('');
      outSel.value = curOut;
    }
    if (nativeBtn) {
      nativeBtn.style.display = (midiConfigState.isStandalone || bridge.isWebView) ? 'inline-flex' : 'none';
    }
    if (badge) {
      const hasPort = Boolean(midiConfigState.selectedInputId || midiConfigState.selectedOutputId);
      badge.textContent = hasPort ? 'HARDWARE PORT ACTIVE' : 'HOST / DEFAULT MIDI';
    }
  };

  bridge.on('hardware.ports', (payload) => {
    if (!payload) return;
    midiConfigState.inputs = Array.isArray(payload.inputs) ? payload.inputs : [];
    midiConfigState.outputs = Array.isArray(payload.outputs) ? payload.outputs : [];
    if (typeof payload.selectedInputId === 'string') {
      midiConfigState.selectedInputId = payload.selectedInputId;
    }
    if (typeof payload.selectedOutputId === 'string') {
      midiConfigState.selectedOutputId = payload.selectedOutputId;
    }
    if (typeof payload.isStandalone === 'boolean') {
      midiConfigState.isStandalone = payload.isStandalone;
    }
    if (typeof payload.midiChannel === 'number' && payload.midiChannel >= 1 && payload.midiChannel <= 16) {
      setGlobalMidiChannel(payload.midiChannel, false);
    }
    renderPortSelectors();
  });

  bridge.on('midiActivity', (ev) => {
    const mon = document.getElementById('midi-cfg-activity-monitor');
    if (mon && ev) {
      const dir = ev.isIncoming ? 'IN' : 'OUT';
      const paramTxt = ev.paramId ? ` (${ev.paramId})` : '';
      mon.textContent = `MIDI ${dir} | Ch ${ev.channel || globalMidiChannel} | CC#${ev.cc} = ${ev.value}${paramTxt}`;
      mon.style.color = 'var(--color-accent)';
    }
  });

  document.getElementById('midi-cfg-close-top')?.addEventListener('click', closeMidiConfigModal);
  document.getElementById('midi-cfg-close-btn')?.addEventListener('click', closeMidiConfigModal);
  modal.addEventListener('click', (e) => {
    if (e.target === modal) closeMidiConfigModal();
  });

  document.getElementById('midi-cfg-refresh-btn')?.addEventListener('click', () => {
    bridge.send('hardware.listPorts', {});
  });

  document.getElementById('midi-cfg-apply-btn')?.addEventListener('click', () => {
    const inSel = document.getElementById('midi-cfg-input-select');
    const outSel = document.getElementById('midi-cfg-output-select');
    const inputId = inSel ? inSel.value : '';
    const outputId = outSel ? outSel.value : '';
    midiConfigState.selectedInputId = inputId;
    midiConfigState.selectedOutputId = outputId;
    bridge.send('hardware.selectPorts', { inputId, outputId });
    if (inputId) {
      bridge.send('hardware.listen', {});
    }
    const badge = document.getElementById('midi-cfg-status-badge');
    if (badge) {
      badge.textContent = (inputId || outputId) ? 'PORTS CONNECTED' : 'HOST / DEFAULT MIDI';
    }
    if (lcdProgrammer) {
      lcdProgrammer.flashMessage('MIDI PORTS', (inputId || outputId) ? 'HARDWARE LINKED' : 'DEFAULT ROUTING');
    }
  });

  document.getElementById('midi-cfg-native-audio-btn')?.addEventListener('click', () => {
    bridge.send('openAudioDeviceSettings', {});
  });

  document.querySelectorAll('#midi-cfg-modal-ch-grid .midi-ch-option').forEach((btn) => {
    btn.addEventListener('click', () => {
      const ch = Number(btn.dataset.midiCh || 1);
      setGlobalMidiChannel(ch, true);
    });
  });

  document.getElementById('midi-cfg-clock-select')?.addEventListener('change', (e) => {
    const mode = e.target.value || 'Auto';
    if (lcdProgrammer && lcdProgrammer.globalState) {
      lcdProgrammer.globalState.clockMode = mode;
      lcdProgrammer.updateDisplay();
      lcdProgrammer.flashMessage('MIDI CLOCK MODE', mode.toUpperCase());
    }
  });
}

function openMidiConfigModal(section = 'devices') {
  const modal = document.getElementById('midi-config-modal');
  if (!modal) return;
  modal.classList.remove('hidden');
  modal.style.display = 'flex';
  bridge.send('hardware.listPorts', {});

  const clockSel = document.getElementById('midi-cfg-clock-select');
  if (clockSel && lcdProgrammer?.globalState?.clockMode) {
    clockSel.value = lcdProgrammer.globalState.clockMode;
  }

  if (section === 'cc-chart') {
    const ccCard = document.getElementById('midi-cfg-cc-reference-card');
    ccCard?.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
  }
}

function closeMidiConfigModal() {
  const modal = document.getElementById('midi-config-modal');
  if (!modal) return;
  modal.classList.add('hidden');
  modal.style.display = 'none';
}

/* ═══════════════════════════════════════════════════════════════════════════
   KORG MS2000 / MS2000R 16-BUTTON HARDWARE STRIP CONTROLLER
   - 16 Program Buttons (01..16 within Bank A..H)
   - 16 Page Jump Edit Buttons (1: COMMON .. 16: UTILITY)
   - SEQ EDIT [CH PARAM] 3-Track x 16-Step Editor + Step Chaser
   - Vocoder 16-Channel [CH LEVEL] & [CH PAN] Editor
   - 16-Pad Chromatic Keyboard with Shift+Click Latch & Octave Shift
   ═══════════════════════════════════════════════════════════════════════════ */

function renderCommonDrawer(container) {
  const scaleTypes = [
    { value: 0, label: 'EQUAL' },
    { value: 1, label: 'PURE MAJ' },
    { value: 2, label: 'PURE MIN' },
    { value: 3, label: 'ARABIC' },
    { value: 4, label: 'PYTHAG' },
    { value: 5, label: 'WERCK' },
    { value: 6, label: 'KIRNBG' },
    { value: 7, label: 'SLENDRO' },
    { value: 8, label: 'PELOG' },
    { value: 9, label: 'USER' }
  ];
  const scaleKeys = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'].map((k, idx) => ({
    value: idx,
    label: k
  }));

  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>PAGE 01: COMMON VOICE MODE &amp; ENGINE PROFILE</h3>
      <div style="display: flex; flex-direction: column; gap: 12px; margin-bottom: 12px;">
        <div id="drawer-common-timbre-mode"></div>
        <div id="drawer-common-vocoder-mode"></div>
        <div id="drawer-common-synth-profile"></div>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>PROGRAM SCALE TEMPERAMENT &amp; KEY</h3>
      <div style="display: flex; flex-direction: column; gap: 10px;">
        <div id="drawer-common-scale-type"></div>
        <div id="drawer-common-scale-key"></div>
      </div>
    </div>
  `;

  new ABDSegmented(container.querySelector('#drawer-common-timbre-mode'), {
    paramId: 'timbreMode',
    label: 'VOICE MODE (SINGLE / SPLIT / LAYER)',
    options: [
      { value: 0, label: 'SINGLE (1T)' },
      { value: 1, label: 'SPLIT (2T)' },
      { value: 2, label: 'LAYER (2T)' }
    ],
    variant: 'strip',
    size: 'md',
    onChange: () => {
      paramStore.set('synthVocoderMode', 0);
      bridge.setParam('synthVocoderMode', 0);
      refreshAllDashboardCards();
    }
  });

  new ABDSegmented(container.querySelector('#drawer-common-vocoder-mode'), {
    paramId: 'synthVocoderMode',
    label: 'SYNTH / 16-BAND VOCODER MODE',
    options: [
      { value: 0, label: 'SYNTHESIZER' },
      { value: 1, label: '16-BAND VOCODER' }
    ],
    variant: 'strip',
    size: 'md',
    onChange: () => {
      refreshAllDashboardCards();
    }
  });

  new ABDSegmented(container.querySelector('#drawer-common-synth-profile'), {
    paramId: 'synthMode',
    label: 'HARDWARE ENGINE PROFILE',
    options: [
      { value: 0, label: 'MS2000 (4V)' },
      { value: 1, label: 'microKORG (4V)' },
      { value: 2, label: 'ADVANCED (16V)' }
    ],
    variant: 'strip',
    size: 'md',
    onChange: (val) => {
      const targetTheme = themeIdOfSynthMode(val);
      if (themeSwitcher) {
        themeSwitcher.setValue(targetTheme);
      } else {
        applyThemeEffects(targetTheme, Number(val));
      }
    }
  });

  new ABDSegmented(container.querySelector('#drawer-common-scale-type'), {
    paramId: 'scaleType',
    label: 'TEMPERAMENT SCALE',
    options: scaleTypes,
    layout: 'grid-5',
    size: 'sm',
  });

  new ABDSegmented(container.querySelector('#drawer-common-scale-key'), {
    paramId: 'scaleKey',
    label: 'SCALE KEY',
    options: scaleKeys,
    layout: 'grid-6',
    size: 'sm',
  });
}

function renderUtilityDrawer(container) {
  const prog = lcdProgrammer?.getCurrentProgram?.() || { id: 'A.01', name: 'Init Program' };
  container.innerHTML = `
    <div class="drawer-section-card">
      <h3>PAGE 16: TIMBRE EDIT UTILITIES (${prog.id} ${prog.name})</h3>
      <div style="display: grid; grid-template-columns: repeat(3, 1fr); gap: 8px; margin-bottom: 10px;">
        <button type="button" id="util-btn-copy-t1-t2" class="card-action-btn" style="padding: 8px;">COPY T1 &rarr; T2</button>
        <button type="button" id="util-btn-copy-t2-t1" class="card-action-btn" style="padding: 8px;">COPY T2 &rarr; T1</button>
        <button type="button" id="util-btn-swap-timbres" class="card-action-btn" style="padding: 8px; border-color: var(--color-accent); color: var(--color-accent);">SWAP T1 &harr; T2</button>
      </div>
      <div style="display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px;">
        <button type="button" id="util-btn-write" class="card-action-btn" style="padding: 8px; background: #d62828; color: #fff; border-color: #ff4d5a;">WRITE PATCH</button>
        <button type="button" id="util-btn-compare" class="card-action-btn" style="padding: 8px;">COMPARE A/B</button>
        <button type="button" id="util-btn-init" class="card-action-btn" style="padding: 8px;">INIT PATCH</button>
        <button type="button" id="util-btn-random" class="card-action-btn" style="padding: 8px;">RANDOMIZE</button>
      </div>
    </div>
    <div class="drawer-section-card">
      <h3>SYSEX DUMP, FIRMWARE 1.07 &amp; DIAGNOSTICS</h3>
      <div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 8px;">
        <button type="button" id="util-btn-export-syx" class="card-action-btn" style="padding: 8px;">EXPORT .SYX DUMP</button>
        <button type="button" id="util-btn-import-syx" class="card-action-btn" style="padding: 8px;">IMPORT .SYX FILE</button>
        <button type="button" id="util-btn-sysex-inspector" class="card-action-btn" style="padding: 8px;">LIVE SYSEX INSPECTOR</button>
        <button type="button" id="util-btn-midi-setup" class="card-action-btn" style="padding: 8px;">MIDI PORTS &amp; CLOCK</button>
        <button type="button" id="util-btn-firmware" class="card-action-btn" style="padding: 8px;">FIRMWARE 1.07 UPDATER</button>
        <button type="button" id="util-btn-diagnostics" class="card-action-btn" style="padding: 8px;">DSP DIAGNOSTICS &amp; SCOPE</button>
      </div>
    </div>
  `;

  container.querySelector('#util-btn-copy-t1-t2')?.addEventListener('click', copyTimbre1To2);
  container.querySelector('#util-btn-copy-t2-t1')?.addEventListener('click', copyTimbre2To1);
  container.querySelector('#util-btn-swap-timbres')?.addEventListener('click', swapTimbres1And2);
  container.querySelector('#util-btn-write')?.addEventListener('click', () => lcdProgrammer?.writeCurrentProgram());
  container.querySelector('#util-btn-compare')?.addEventListener('click', () => {
    lcdProgrammer?.toggleCompare();
    refreshAllDashboardCards();
  });
  container.querySelector('#util-btn-init')?.addEventListener('click', () => handleMenuAction('new-patch'));
  container.querySelector('#util-btn-random')?.addEventListener('click', () => handleMenuAction('randomize-patch'));
  container.querySelector('#util-btn-export-syx')?.addEventListener('click', () => bridge.send('exportSysexProgram', {}));
  container.querySelector('#util-btn-import-syx')?.addEventListener('click', () => triggerSysexFileInput());
  container.querySelector('#util-btn-sysex-inspector')?.addEventListener('click', () => toggleLiveSysExInspector());
  container.querySelector('#util-btn-midi-setup')?.addEventListener('click', () => openMidiConfigModal('devices'));
  container.querySelector('#util-btn-firmware')?.addEventListener('click', () => firmwareUpdaterModal?.open());
  container.querySelector('#util-btn-diagnostics')?.addEventListener('click', () => diagnosticModal?.open());
}

/**
 * Opens the MS2000 Hardware Edit Page (1..16), syncing both the LCD Programmer
 * and the corresponding SlideDrawer (including 8A/8B EG1/EG2 and 9A/9B LFO1/LFO2 toggles).
 */
function openMs2kHardwarePage(padNum) {
  const k = Math.max(1, Math.min(16, Number(padNum) || 1));
  const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
  const isMicrokorg = (paramStore.get('synthMode') === 1) || (currentTheme() === 'microkorg');

  if (lcdProgrammer) {
    lcdProgrammer.jumpToEditPage(k);
  }

  switch (k) {
    case 1:
      slideDrawer.open({ title: 'PAGE 01: COMMON (TIMBRE MODE & SCALE)', badge: '1 COMMON', sectionId: 'common', render: renderCommonDrawer });
      break;
    case 2:
      slideDrawer.open({ title: 'PAGE 02: VOICE ASSIGN & UNISON', badge: '2 VOICE', sectionId: 'voice', render: renderVoiceDrawer });
      break;
    case 3:
      slideDrawer.open({ title: 'PAGE 03: PITCH, PORTAMENTO & SCALE', badge: '3 PITCH', sectionId: 'pitch', render: renderPitchDrawer });
      break;
    case 4:
      slideDrawer.open({ title: 'PAGE 04: OSCILLATOR 1 (VA & DWGS)', badge: '4 OSC 1', sectionId: 'osc1', render: renderOsc1Drawer });
      break;
    case 5:
      if (isVocoder) {
        slideDrawer.open({ title: 'PAGE 05: VOCODER [AUDIO IN 2] & CARRIER', badge: '5 [AUDIO IN 2]', sectionId: 'vocoder-cfg', render: renderVocoderConfigDrawer });
      } else if (slideDrawer.isOpenState && slideDrawer.activeSection === 'osc2') {
        slideDrawer.open({ title: 'PAGE 05B: AUDIO MIXER (OSC 1 / OSC 2 / NOISE)', badge: '5B MIXER', sectionId: 'mixer', render: renderMixerDrawer });
      } else {
        slideDrawer.open({ title: 'PAGE 05: OSCILLATOR 2 & MODULATION', badge: '5 OSC 2', sectionId: 'osc2', render: renderOsc2Drawer });
      }
      break;
    case 6:
      if (isVocoder) {
        slideDrawer.open({ title: 'PAGE 06: VOCODER FORMANT & FILTER', badge: '6 [FILTER]', sectionId: 'vocoder-cfg', render: renderVocoderConfigDrawer });
      } else if (slideDrawer.isOpenState && slideDrawer.activeSection === 'filter-main') {
        slideDrawer.open({ title: 'PAGE 06B: FILTER MODULATION & KEY TRACK', badge: '6B FILTER MOD', sectionId: 'filter-mod', render: renderFilterModDrawer });
      } else {
        slideDrawer.open({ title: 'PAGE 06A: MULTIMODE FILTER & RESONANCE', badge: '6A FILTER', sectionId: 'filter-main', render: renderFilterMainDrawer });
      }
      break;
    case 7:
      slideDrawer.open({ title: 'PAGE 07: MAIN AMPLIFIER (VCA)', badge: '7 AMP', sectionId: 'amp-main', render: renderAmpMainDrawer });
      break;
    case 8:
      if (slideDrawer.isOpenState && slideDrawer.activeSection === 'eg1') {
        slideDrawer.open({ title: 'PAGE 08B: ENVELOPE 2 (AMPLITUDE EG2)', badge: '8B EG 2', sectionId: 'eg2', render: renderEg2Drawer });
      } else {
        slideDrawer.open({ title: 'PAGE 08A: ENVELOPE 1 (FILTER EG1)', badge: '8A EG 1', sectionId: 'eg1', render: renderEg1Drawer });
      }
      break;
    case 9:
      if (slideDrawer.isOpenState && slideDrawer.activeSection === 'lfo1') {
        slideDrawer.open({ title: 'PAGE 09B: LOW FREQUENCY OSCILLATOR 2 (LFO 2)', badge: '9B LFO 2', sectionId: 'lfo2', render: renderLfo2Drawer });
      } else {
        slideDrawer.open({ title: 'PAGE 09A: LOW FREQUENCY OSCILLATOR 1 (LFO 1)', badge: '9A LFO 1', sectionId: 'lfo1', render: renderLfo1Drawer });
      }
      break;
    case 10:
      if (isVocoder) {
        ms2kStripSeqMode = 1; // [CH LEVEL]
        slideDrawer.open({
          title: isMicrokorg ? 'PAGE 10: VOCODER [CH LEVEL] (8 CHANNELS)' : 'PAGE 10: VOCODER [CH LEVEL] (16 BANDS)',
          badge: '10 [CH LEVEL]',
          sectionId: 'vocoder-bands',
          render: renderVocoderBandsDrawer
        });
      } else {
        slideDrawer.open({ title: 'PAGE 10: VIRTUAL PATCH MATRIX (4 SLOTS)', badge: '10 PATCH', sectionId: 'modmatrix', render: renderModMatrixDrawer });
      }
      break;
    case 11:
      if (isVocoder) {
        ms2kStripSeqMode = 2; // [CH PAN]
        slideDrawer.open({
          title: 'PAGE 11: VOCODER [CH PAN] & BAND LEVELS',
          badge: '11 [CH PAN]',
          sectionId: 'vocoder-bands',
          render: renderVocoderBandsDrawer
        });
      } else {
        if (ms2kStripSeqMode === 0) ms2kStripSeqMode = activeModSeqTrack || 1;
        slideDrawer.open({ title: 'PAGE 11: MODULATION SEQUENCER (3x16)', badge: '11 SEQ', sectionId: 'modseq', render: renderModSeqDrawer });
      }
      break;
    case 12:
      slideDrawer.open({ title: 'PAGE 12: MODULATION FX (CHORUS / ENSEMBLE / PHASER)', badge: '12 MOD FX', sectionId: 'modfx', render: renderModFxDrawer });
      break;
    case 13:
      slideDrawer.open({ title: 'PAGE 13: STEREO DELAY FX', badge: '13 DELAY FX', sectionId: 'delayfx', render: renderDelayFxDrawer });
      break;
    case 14:
      slideDrawer.open({ title: 'PAGE 14: MASTER EQUALIZER (2 BANDS)', badge: '14 EQ', sectionId: 'eq', render: renderEqDrawer });
      break;
    case 15:
      slideDrawer.open({ title: 'PAGE 15: PROGRAMMABLE ARPEGGIATOR', badge: '15 ARPEGGIO', sectionId: 'arp', render: renderArpDrawer });
      break;
    case 16:
      slideDrawer.open({ title: 'PAGE 16: UTILITY, TIMBRE COPY & SYSEX', badge: '16 UTILITY', sectionId: 'utility', render: renderUtilityDrawer });
      break;
  }

  refreshMs2kBottomStrip();
}

const MS2K_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

function formatPadMidiNote(midiNote) {
  const m = Math.max(0, Math.min(127, Math.round(midiNote)));
  const name = MS2K_NOTE_NAMES[m % 12];
  const oct = Math.floor(m / 12) - 1;
  return `${name}${oct}`;
}

function getActiveSectionPadNumber() {
  if (!slideDrawer?.isOpenState || !slideDrawer.activeSection) {
    if (lcdProgrammer?.isEditMode) {
      return (lcdProgrammer.currentPageIndex % 16) + 1;
    }
    return 0;
  }
  const map = {
    common: 1,
    voice: 2,
    pitch: 3,
    osc1: 4,
    osc2: 5,
    mixer: 5,
    'vocoder-cfg': 5,
    'filter-main': 6,
    'filter-mod': 6,
    'amp-main': 7,
    eg1: 8,
    eg2: 8,
    lfo1: 9,
    lfo2: 9,
    modmatrix: 10,
    'vocoder-bands': (ms2kStripSeqMode === 2 ? 11 : 10),
    modseq: 11,
    modfx: 12,
    delayfx: 13,
    eq: 14,
    arp: 15,
    global: 16,
    utility: 16
  };
  return map[slideDrawer.activeSection] || 0;
}

function refreshMs2kBottomStrip() {
  const stripEl = document.getElementById('ms2k-bottom-strip');
  if (!stripEl) return;

  const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
  const isValueEdit = !ms2kPadKbdMode && ms2kStripSeqMode > 0;
  stripEl.classList.toggle('is-vocoder-mode', isVocoder);
  stripEl.classList.toggle('is-value-edit-mode', isValueEdit);
  stripEl.classList.toggle('is-pad-kbd-mode', ms2kPadKbdMode);

  // 1. Update SEQ EDIT [CH PARAM] LEDs (1..3)
  const seqSelectBtn = document.getElementById('btn-ms2k-seq-edit');
  if (seqSelectBtn) {
    seqSelectBtn.classList.toggle('is-active', !ms2kPadKbdMode && ms2kStripSeqMode > 0);
  }
  for (let i = 1; i <= 3; i++) {
    const led = document.getElementById(`led-ms2k-seq-${i}`);
    const row = document.getElementById(`row-ms2k-seq-${i}`);
    const on = !ms2kPadKbdMode && ms2kStripSeqMode === i;
    if (led) led.classList.toggle('is-on', on);
    if (row) row.classList.toggle('is-active', on);
  }

  // 2. Update BANK / OCTAVE & KBD LEDs + Mode Readout Badge
  const kbdBtn = document.getElementById('btn-ms2k-pad-kbd');
  const kbdLed = document.getElementById('led-ms2k-pad-kbd');
  const downLed = document.getElementById('led-ms2k-bank-down');
  const upLed = document.getElementById('led-ms2k-bank-up');
  const modeBadge = document.getElementById('ms2k-strip-mode-badge');

  if (kbdBtn) kbdBtn.classList.toggle('is-active', ms2kPadKbdMode);
  if (kbdLed) {
    kbdLed.classList.toggle('is-on', ms2kPadKbdMode && ms2kLatchedPadNotes.size === 0);
    kbdLed.classList.toggle('is-amber', ms2kPadKbdMode && ms2kLatchedPadNotes.size > 0);
  }

  if (ms2kPadKbdMode) {
    if (downLed) {
      downLed.classList.toggle('is-on', ms2kPadOctaveShift === -1);
      downLed.classList.toggle('is-amber', ms2kPadOctaveShift <= -2);
    }
    if (upLed) {
      upLed.classList.toggle('is-on', ms2kPadOctaveShift === 1);
      upLed.classList.toggle('is-amber', ms2kPadOctaveShift >= 2);
    }
    if (modeBadge) {
      const baseNote = 60 + ms2kPadOctaveShift * 12;
      modeBadge.textContent = ms2kLatchedPadNotes.size > 0
        ? `HOLD (${ms2kLatchedPadNotes.size})`
        : `KBD ${formatPadMidiNote(baseNote)}`;
    }
  } else {
    const bankIdx = lcdProgrammer ? lcdProgrammer.getCurrentBankIndex() : 0;
    const bankLetters = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
    const bankChar = bankLetters[bankIdx] || 'A';
    if (downLed) {
      downLed.classList.toggle('is-on', bankIdx < 4);
      downLed.classList.remove('is-amber');
    }
    if (upLed) {
      upLed.classList.toggle('is-on', bankIdx >= 4);
      upLed.classList.remove('is-amber');
    }
    if (modeBadge) {
      if (ms2kStripSeqMode > 0) {
        if (isVocoder) {
          modeBadge.textContent = ms2kStripSeqMode === 1 ? 'CH LEVEL' : 'CH PAN';
        } else {
          modeBadge.textContent = `SEQ ${ms2kStripSeqMode} [T${activeTimbre}]`;
        }
      } else if (lcdProgrammer?.isEditMode || slideDrawer?.isOpenState) {
        const secPad = getActiveSectionPadNumber();
        modeBadge.textContent = secPad ? `EDIT P${String(secPad).padStart(2, '0')}` : 'EDIT MODE';
      } else {
        const progSlot = lcdProgrammer ? (((lcdProgrammer.currentProgramIndex - 1) % 16) + 1) : 1;
        modeBadge.textContent = `BANK ${bankChar}.${String(progSlot).padStart(2, '0')}`;
      }
    }
  }

  // 3. Update all 16 Pads
  const currentProgPad = lcdProgrammer ? (((lcdProgrammer.currentProgramIndex - 1) % 16) + 1) : 1;
  const activeSectionPad = getActiveSectionPadNumber();
  const lastStep = Math.round(paramStore.get(tParam('seqLastStep')) || 16);
  const seqRunning = ((paramStore.get('modSeqOn') || 0) > 0.5) || ((paramStore.get('arpOn') || 0) > 0.5);

  for (let k = 1; k <= 16; k++) {
    const colEl = stripEl.querySelector(`.ms2k-pad-col[data-pad="${k}"]`);
    const btnEl = document.getElementById(`ms2k-pad-${k}`);
    const subValEl = document.getElementById(`ms2k-pad-val-${k}`);
    const barFillEl = document.getElementById(`ms2k-pad-bar-${k}`);
    if (!colEl || !btnEl) continue;

    let isLit = false;
    let isLatched = false;
    let isBeyondLast = false;
    let subText = '';
    let barWidthPct = 0;

    if (ms2kPadKbdMode) {
      const midiNote = 60 + ms2kPadOctaveShift * 12 + (k - 1);
      subText = formatPadMidiNote(midiNote);
      isLatched = ms2kLatchedPadNotes.has(midiNote);
      isLit = ms2kActivePadNotes.get(k) === midiNote || isLatched;
    } else if (ms2kStripSeqMode > 0) {
      if (isVocoder) {
        if (ms2kStripSeqMode === 1) {
          // Vocoder [CH LEVEL] 1..16 (0..127)
          const lvl = Math.round(paramStore.get(`vocoderBandLevel${k}`) ?? 127);
          subText = `${lvl}`;
          barWidthPct = (lvl / 127) * 100;
          isLit = lvl > 0;
          isLatched = lvl === 0;
        } else {
          // Vocoder [CH PAN] 1..16 (-63..+63)
          const pan = Math.round(ms2kVocoderBandPans[k - 1] || 0);
          subText = pan === 0 ? 'CNT' : (pan > 0 ? `R${pan}` : `L${Math.abs(pan)}`);
          barWidthPct = ((pan + 63) / 126) * 100;
          isLit = pan !== 0;
        }
      } else {
        // Mod Sequencer Track 1..3 Step 1..16 (-63..+63)
        const stepId = tParam(`seq${ms2kStripSeqMode}Step${k}`);
        const stepVal = Math.round(paramStore.get(stepId) || 0);
        isBeyondLast = k > lastStep;
        subText = stepVal > 0 ? `+${stepVal}` : `${stepVal}`;
        barWidthPct = ((stepVal + 63) / 126) * 100;
        isLit = !isBeyondLast && stepVal !== 0;
      }
    } else if (lcdProgrammer?.isEditMode || slideDrawer?.isOpenState) {
      isLit = (activeSectionPad === k);
      subText = isLit ? 'EDIT' : '';
    } else {
      // Program Play Mode (1..16)
      isLit = (currentProgPad === k);
      const bankIdx = lcdProgrammer ? lcdProgrammer.getCurrentBankIndex() : 0;
      const bankLetters = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];
      subText = isLit ? `${bankLetters[bankIdx] || 'A'}.${String(k).padStart(2, '0')}` : '';
    }

    const isChaserStep = seqRunning && (ms2kStepChaserIdx + 1 === k) && (k <= lastStep);

    colEl.classList.toggle('is-active', isLit);
    colEl.classList.toggle('is-editing-section', activeSectionPad === k);
    colEl.classList.toggle('is-beyond-last', isBeyondLast);

    btnEl.classList.toggle('is-active', isLit);
    btnEl.classList.toggle('is-latched', isLatched);
    btnEl.classList.toggle('is-seq-step', isChaserStep);

    if (subValEl) subValEl.textContent = subText;
    if (barFillEl) barFillEl.style.width = `${Math.max(0, Math.min(100, barWidthPct))}%`;
  }
}

function setupMs2000BottomStrip() {
  const stripEl = document.getElementById('ms2k-bottom-strip');
  if (!stripEl) return;

  // Close drawer listener so strip refreshes back to Program mode when drawer closes
  const origClose = slideDrawer.close.bind(slideDrawer);
  slideDrawer.close = () => {
    origClose();
    if (lcdProgrammer) {
      lcdProgrammer.isEditMode = false;
      lcdProgrammer.updateDisplay();
    }
    refreshMs2kBottomStrip();
  };

  // 1. SEQ EDIT [CH PARAM] SELECT button
  document.getElementById('btn-ms2k-seq-edit')?.addEventListener('click', () => {
    ms2kPadKbdMode = false;
    const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
    if (isVocoder) {
      ms2kStripSeqMode = (ms2kStripSeqMode + 1) % 3; // 0 -> 1 (CH LEVEL) -> 2 (CH PAN) -> 0
      if (lcdProgrammer) {
        const names = ['PROGRAM / EDIT MODE', 'VOCODER [CH LEVEL] 1-16', 'VOCODER [CH PAN] 1-16'];
        lcdProgrammer.flashMessage('CH PARAM SELECT', names[ms2kStripSeqMode]);
      }
    } else {
      ms2kStripSeqMode = (ms2kStripSeqMode + 1) % 4; // 0 -> 1 (SEQ 1) -> 2 (SEQ 2) -> 3 (SEQ 3) -> 0
      if (ms2kStripSeqMode > 0) {
        activeModSeqTrack = ms2kStripSeqMode;
        if (slideDrawer.isOpenState && slideDrawer.activeSection === 'modseq') {
          renderModSeqDrawer(slideDrawer.contentElem);
        }
      }
      if (lcdProgrammer) {
        const names = ['PROGRAM / EDIT MODE', `SEQ 1 EDIT (T${activeTimbre})`, `SEQ 2 EDIT (T${activeTimbre})`, `SEQ 3 EDIT (T${activeTimbre})`];
        lcdProgrammer.flashMessage('SEQ EDIT SELECT', names[ms2kStripSeqMode]);
      }
    }
    refreshMs2kBottomStrip();
  });

  // Direct click on LED rows 1..3
  for (let i = 1; i <= 3; i++) {
    document.getElementById(`row-ms2k-seq-${i}`)?.addEventListener('click', () => {
      ms2kPadKbdMode = false;
      const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
      if (isVocoder && i === 3) return;
      ms2kStripSeqMode = (ms2kStripSeqMode === i) ? 0 : i;
      if (!isVocoder && ms2kStripSeqMode > 0) {
        activeModSeqTrack = ms2kStripSeqMode;
        if (slideDrawer.isOpenState && slideDrawer.activeSection === 'modseq') {
          renderModSeqDrawer(slideDrawer.contentElem);
        }
      }
      refreshMs2kBottomStrip();
    });
  }

  // 2. BANK / OCTAVE DOWN & UP buttons
  document.getElementById('btn-ms2k-bank-down')?.addEventListener('click', () => {
    if (ms2kPadKbdMode) {
      ms2kPadOctaveShift = Math.max(-3, ms2kPadOctaveShift - 1);
      const baseNote = 60 + ms2kPadOctaveShift * 12;
      lcdProgrammer?.flashMessage('PAD KBD OCTAVE', `${formatPadMidiNote(baseNote)} .. ${formatPadMidiNote(baseNote + 15)}`);
    } else if (lcdProgrammer) {
      const nextBank = (lcdProgrammer.getCurrentBankIndex() + 7) % 8;
      lcdProgrammer.selectBank(nextBank);
    }
    refreshMs2kBottomStrip();
  });

  document.getElementById('btn-ms2k-bank-up')?.addEventListener('click', () => {
    if (ms2kPadKbdMode) {
      ms2kPadOctaveShift = Math.min(3, ms2kPadOctaveShift + 1);
      const baseNote = 60 + ms2kPadOctaveShift * 12;
      lcdProgrammer?.flashMessage('PAD KBD OCTAVE', `${formatPadMidiNote(baseNote)} .. ${formatPadMidiNote(baseNote + 15)}`);
    } else if (lcdProgrammer) {
      const nextBank = (lcdProgrammer.getCurrentBankIndex() + 1) % 8;
      lcdProgrammer.selectBank(nextBank);
    }
    refreshMs2kBottomStrip();
  });

  // 3. KBD (16-Pad Chromatic Keyboard + Latch) button
  document.getElementById('btn-ms2k-pad-kbd')?.addEventListener('click', (e) => {
    if (e.shiftKey && ms2kLatchedPadNotes.size > 0) {
      for (const note of ms2kLatchedPadNotes) {
        bridge.noteOff(note, 0.0);
      }
      ms2kLatchedPadNotes.clear();
      lcdProgrammer?.flashMessage('16-PAD KEYBOARD', 'LATCH CLEARED');
      refreshMs2kBottomStrip();
      return;
    }
    ms2kPadKbdMode = !ms2kPadKbdMode;
    if (!ms2kPadKbdMode) {
      for (const [, note] of ms2kActivePadNotes) {
        if (!ms2kLatchedPadNotes.has(note)) {
          bridge.noteOff(note, 0.0);
        }
      }
      ms2kActivePadNotes.clear();
    }
    lcdProgrammer?.flashMessage(
      '16-PAD KEYBOARD',
      ms2kPadKbdMode ? 'ACTIVE (SHIFT=LATCH)' : 'PROGRAM / EDIT PADS'
    );
    refreshMs2kBottomStrip();
  });

  // 4. Bottom Section Labels (1: COMMON .. 16: UTILITY) -> Instant Page Jump + Drawer Open
  stripEl.querySelectorAll('.ms2k-pad-label-btn[data-pad-jump]').forEach((lblBtn) => {
    lblBtn.addEventListener('click', (e) => {
      e.stopPropagation();
      const padNum = Number(lblBtn.dataset.padJump) || 1;
      ms2kPadKbdMode = false;
      openMs2kHardwarePage(padNum);
    });
  });

  // 5. 16 Illuminated Rubber Pads (1..16)
  const applyPadValueDelta = (k, newVal) => {
    const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
    if (isVocoder) {
      if (ms2kStripSeqMode === 1) {
        const clamped = Math.max(0, Math.min(127, Math.round(newVal)));
        const pid = `vocoderBandLevel${k}`;
        paramStore.set(pid, clamped);
        bridge.setParam(pid, clamped);
        if (clamped > 0) ms2kVocoderSavedLevels[k - 1] = clamped;
        if (slideDrawer.isOpenState && slideDrawer.activeSection === 'vocoder-bands') {
          renderVocoderBandsDrawer(slideDrawer.contentElem);
        }
        lcdProgrammer?.flashMessage(`VOCODER CH ${String(k).padStart(2, '0')}`, `Level: ${clamped}`, 900);
      } else if (ms2kStripSeqMode === 2) {
        const clamped = Math.max(-63, Math.min(63, Math.round(newVal)));
        ms2kVocoderBandPans[k - 1] = clamped;
        const pid = `vocoderBandPan${k}`;
        paramStore.set(pid, clamped);
        bridge.setParam(pid, clamped);
        const panStr = clamped === 0 ? 'CNT' : (clamped > 0 ? `R${clamped}` : `L${Math.abs(clamped)}`);
        lcdProgrammer?.flashMessage(`VOCODER CH ${String(k).padStart(2, '0')}`, `Panpot: ${panStr}`, 900);
      }
    } else {
      const clamped = Math.max(-63, Math.min(63, Math.round(newVal)));
      const stepId = tParam(`seq${ms2kStripSeqMode}Step${k}`);
      paramStore.set(stepId, clamped);
      bridge.setParam(stepId, clamped);
      if (slideDrawer.isOpenState && slideDrawer.activeSection === 'modseq') {
        renderModSeqDrawer(slideDrawer.contentElem);
      }
      lcdProgrammer?.flashMessage(
        `SEQ ${ms2kStripSeqMode} STEP ${String(k).padStart(2, '0')}`,
        `Value: ${clamped > 0 ? '+' + clamped : clamped}`,
        900
      );
    }
    refreshMs2kBottomStrip();
  };

  for (let k = 1; k <= 16; k++) {
    const padBtn = document.getElementById(`ms2k-pad-${k}`);
    if (!padBtn) continue;

    padBtn.addEventListener('pointerdown', (e) => {
      e.preventDefault();

      // Mode A: 16-Pad Chromatic Keyboard + Shift-Latch
      if (ms2kPadKbdMode) {
        const midiNote = 60 + ms2kPadOctaveShift * 12 + (k - 1);
        bridge.initWebAudio();

        if (e.shiftKey) {
          if (ms2kLatchedPadNotes.has(midiNote)) {
            ms2kLatchedPadNotes.delete(midiNote);
            bridge.noteOff(midiNote, 0.0);
            lcdProgrammer?.flashMessage(`PAD ${k} (${formatPadMidiNote(midiNote)})`, 'UNLATCHED', 800);
          } else {
            ms2kLatchedPadNotes.add(midiNote);
            bridge.noteOn(midiNote, 0.85);
            lcdProgrammer?.flashMessage(`PAD ${k} (${formatPadMidiNote(midiNote)})`, 'LATCHED (HOLD)', 800);
          }
          refreshMs2kBottomStrip();
          return;
        }

        ms2kActivePadNotes.set(k, midiNote);
        bridge.noteOn(midiNote, 0.85);
        lcdProgrammer?.flashMessage(`PAD KEYBOARD`, `Note: ${formatPadMidiNote(midiNote)} (MIDI ${midiNote})`, 700);
        refreshMs2kBottomStrip();

        const releasePadNote = () => {
          const heldNote = ms2kActivePadNotes.get(k);
          ms2kActivePadNotes.delete(k);
          if (heldNote !== undefined && !ms2kLatchedPadNotes.has(heldNote)) {
            bridge.noteOff(heldNote, 0.0);
          }
          refreshMs2kBottomStrip();
        };
        window.addEventListener('pointerup', releasePadNote, { once: true });
        window.addEventListener('pointercancel', releasePadNote, { once: true });
        return;
      }

      // Mode B: SEQ EDIT (1..3) or Vocoder CH PARAM ([LEVEL] / [PAN])
      if (ms2kStripSeqMode > 0) {
        const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
        if (e.shiftKey) {
          if (isVocoder && ms2kStripSeqMode === 1) {
            const pid = `vocoderBandLevel${k}`;
            const cur = Math.round(paramStore.get(pid) ?? 127);
            const next = cur > 0 ? 0 : (ms2kVocoderSavedLevels[k - 1] || 127);
            applyPadValueDelta(k, next);
          } else if (!isVocoder) {
            const lastStepPid = tParam('seqLastStep');
            paramStore.set(lastStepPid, k);
            bridge.setParam(lastStepPid, k);
            if (slideDrawer.isOpenState && slideDrawer.activeSection === 'modseq') {
              renderModSeqDrawer(slideDrawer.contentElem);
            }
            lcdProgrammer?.flashMessage('SEQ LAST STEP', `LENGTH: ${k} STEPS`, 1000);
            refreshMs2kBottomStrip();
          }
          return;
        }

        const startY = e.clientY;
        const startVal = isVocoder
          ? (ms2kStripSeqMode === 1
              ? Math.round(paramStore.get(`vocoderBandLevel${k}`) ?? 127)
              : Math.round(ms2kVocoderBandPans[k - 1] || 0))
          : Math.round(paramStore.get(tParam(`seq${ms2kStripSeqMode}Step${k}`)) || 0);

        let moved = false;
        const onMove = (moveEvt) => {
          const dy = startY - moveEvt.clientY;
          if (Math.abs(dy) > 2) moved = true;
          const delta = Math.round(dy * 0.85);
          applyPadValueDelta(k, startVal + delta);
        };
        const onUp = () => {
          window.removeEventListener('pointermove', onMove);
          if (!moved) {
            // Single click on pad in SEQ EDIT / CH PARAM: open corresponding drawer & show current value
            applyPadValueDelta(k, startVal);
          }
        };
        window.addEventListener('pointermove', onMove);
        window.addEventListener('pointerup', onUp, { once: true });
        return;
      }

      // Mode C: PROGRAM SELECT (01..16) or PAGE JUMP EDIT (when Shift or EDIT mode is active)
      if (e.shiftKey || lcdProgrammer?.isEditMode) {
        openMs2kHardwarePage(k);
      } else if (lcdProgrammer) {
        const bankIdx = lcdProgrammer.getCurrentBankIndex();
        const targetIdx = bankIdx * 16 + (k - 1);
        lcdProgrammer.loadProgramIndex(targetIdx);
        refreshAllDashboardCards();
        refreshMs2kBottomStrip();
      }
    });

    // Double-click resets step/band in SEQ EDIT / CH PARAM, or opens Edit Page in Program mode
    padBtn.addEventListener('dblclick', (e) => {
      e.preventDefault();
      if (ms2kPadKbdMode) return;
      if (ms2kStripSeqMode > 0) {
        const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
        const resetVal = (isVocoder && ms2kStripSeqMode === 1) ? 127 : 0;
        applyPadValueDelta(k, resetVal);
      } else {
        openMs2kHardwarePage(k);
      }
    });

    // Mouse wheel fine-adjusts step/band in SEQ EDIT / CH PARAM
    padBtn.addEventListener('wheel', (e) => {
      if (ms2kPadKbdMode || ms2kStripSeqMode === 0) return;
      e.preventDefault();
      const isVocoder = (paramStore.get('synthVocoderMode') || 0) > 0.5;
      const curVal = isVocoder
        ? (ms2kStripSeqMode === 1
            ? Math.round(paramStore.get(`vocoderBandLevel${k}`) ?? 127)
            : Math.round(ms2kVocoderBandPans[k - 1] || 0))
        : Math.round(paramStore.get(tParam(`seq${ms2kStripSeqMode}Step${k}`)) || 0);
      const step = e.shiftKey ? 4 : 1;
      const delta = e.deltaY < 0 ? step : -step;
      applyPadValueDelta(k, curVal + delta);
    }, { passive: false });
  }

  // 6. Step Chaser Animation when Mod Sequencer or Arpeggiator is active
  if (ms2kStepChaserTimer) clearInterval(ms2kStepChaserTimer);
  ms2kStepChaserTimer = setInterval(() => {
    const seqOn = (paramStore.get('modSeqOn') || 0) > 0.5;
    const arpOn = (paramStore.get('arpOn') || 0) > 0.5;
    if (!seqOn && !arpOn) return;
    const lastStep = Math.max(1, Math.min(16, Math.round(paramStore.get(tParam('seqLastStep')) || 16)));
    const dir = Math.round(paramStore.get('modSeqType') || 0);
    if (dir === 1) {
      ms2kStepChaserIdx = (ms2kStepChaserIdx - 1 + lastStep) % lastStep;
    } else if (dir === 3) {
      ms2kStepChaserIdx = Math.floor(Math.random() * lastStep);
    } else {
      ms2kStepChaserIdx = (ms2kStepChaserIdx + 1) % lastStep;
    }
    refreshMs2kBottomStrip();
  }, 160);

  refreshMs2kBottomStrip();
}
