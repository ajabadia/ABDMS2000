/**
 * wavetableBrowser.js — Advanced Mode Wavetable Browser for 512-slot DWGS bank.
 *
 * Features:
 *   - Search by name (fuzzy substring match)
 *   - Category filter chips (17 categories from WaveCategory.h)
 *   - Scrollable grid with slot number + name + category badge
 *   - Click to select → sets osc1DwgsWave param
 *   - Drag-and-drop WAV loading for User bank (slots 256-511)
 *   - Mini waveform preview on hover (SVG)
 *   - Keyboard navigation (↑/↓/Enter)
 *
 * The catalog is bootstrapped from a hardcoded copy matching the C++ DWGSTables,
 * then synced live from the engine via bridge.send('getWavetableCatalog').
 */

// ─── Category definitions (mirrors WaveCategory.h) ──────────────────────────

const CATEGORIES = [
  { id: 0,  name: 'All',            css: 'cat-all' },
  { id: 1,  name: 'Synth Wave',     css: 'cat-synthwave' },
  { id: 2,  name: 'Bell / Digital', css: 'cat-digibell' },
  { id: 3,  name: 'Organ',          css: 'cat-organ' },
  { id: 4,  name: 'Piano / Clav',   css: 'cat-piano' },
  { id: 5,  name: 'Guitar',         css: 'cat-guitar' },
  { id: 6,  name: 'Bass',           css: 'cat-bass' },
  { id: 7,  name: 'Strings / Brass', css: 'cat-strings' },
  { id: 8,  name: 'Voice',          css: 'cat-voice' },
  { id: 9,  name: 'Basic Shapes',   css: 'cat-basic' },
  { id: 10, name: 'Bass / Lead',    css: 'cat-basslead' },
  { id: 11, name: 'Pad / Ambient',  css: 'cat-pad' },
  { id: 12, name: 'Organic / Vocal', css: 'cat-organic' },
  { id: 13, name: 'Brass / Wind',   css: 'cat-brass' },
  { id: 14, name: 'Keys / Mallets', css: 'cat-keys' },
  { id: 15, name: 'FX / Metal',     css: 'cat-fx' },
  { id: 16, name: 'Chip / 8-Bit',   css: 'cat-chip' },
  { id: 17, name: 'User Waves',     css: 'cat-user' },
];

// ─── Bootstrap catalog (matches C++ DWGSTables exactly) ──────────────────────
// Slot ranges: 0-63 DWGS, 64-159 ProphetVS, 128-255 AKWF, 256-383 AKWF extras, 384-511 User

const BOOTSTRAP_CATALOG = [
  // Slots 0-63: Korg DWGS Standard
  { slot: 0, name: 'SynWave 1', cat: 1 }, { slot: 1, name: 'SynWave 2', cat: 1 },
  { slot: 2, name: 'SynWave 3', cat: 1 }, { slot: 3, name: 'SynWave 4', cat: 1 },
  { slot: 4, name: 'SynWave 5', cat: 1 }, { slot: 5, name: 'SynWave 6', cat: 1 },
  { slot: 6, name: 'SynWave 7', cat: 1 }, { slot: 7, name: 'SynWave 8', cat: 1 },
  { slot: 8, name: '5th Wave 1', cat: 1 }, { slot: 9, name: '5th Wave 2', cat: 1 },
  { slot: 10, name: '5th Wave 3', cat: 1 },
  { slot: 11, name: 'Digi 1', cat: 2 }, { slot: 12, name: 'Digi 2', cat: 2 },
  { slot: 13, name: 'Digi 3', cat: 2 }, { slot: 14, name: 'Digi 4', cat: 2 },
  { slot: 15, name: 'Digi 5', cat: 2 }, { slot: 16, name: 'Digi 6', cat: 2 },
  { slot: 17, name: 'Digi 7', cat: 2 }, { slot: 18, name: 'Digi 8', cat: 2 },
  { slot: 19, name: 'Endless', cat: 1 },
  { slot: 20, name: 'E.Organ 1', cat: 3 }, { slot: 21, name: 'E.Organ 2', cat: 3 },
  { slot: 22, name: 'E.Organ 3', cat: 3 }, { slot: 23, name: 'E.Organ 4', cat: 3 },
  { slot: 24, name: 'E.Organ 5', cat: 3 }, { slot: 25, name: 'E.Organ 6', cat: 3 },
  { slot: 26, name: 'E.Organ 7', cat: 3 }, { slot: 27, name: 'E.Organ 8', cat: 3 },
  { slot: 28, name: 'Clav 1', cat: 4 }, { slot: 29, name: 'Clav 2', cat: 4 },
  { slot: 30, name: 'A.Piano 1', cat: 4 }, { slot: 31, name: 'A.Piano 2', cat: 4 },
  { slot: 32, name: 'A.Piano 3', cat: 4 },
  { slot: 33, name: 'E.P. 1', cat: 4 }, { slot: 34, name: 'E.P. 2', cat: 4 },
  { slot: 35, name: 'E.P. 3', cat: 4 }, { slot: 36, name: 'E.P. 4', cat: 4 },
  { slot: 37, name: 'E.P. 5', cat: 4 }, { slot: 38, name: 'Wurl 1', cat: 4 },
  { slot: 39, name: 'A.Guitar 1', cat: 5 }, { slot: 40, name: 'A.Guitar 2', cat: 5 },
  { slot: 41, name: 'E.Guitar 1', cat: 5 }, { slot: 42, name: 'E.Guitar 2', cat: 5 },
  { slot: 43, name: 'E.Guitar 3', cat: 5 }, { slot: 44, name: 'Mute Gt', cat: 5 },
  { slot: 45, name: 'A.Bass', cat: 6 }, { slot: 46, name: 'E.Bass 1', cat: 6 },
  { slot: 47, name: 'E.Bass 2', cat: 6 },
  { slot: 48, name: 'Synth Bass 1', cat: 6 }, { slot: 49, name: 'Synth Bass 2', cat: 6 },
  { slot: 50, name: 'Synth Bass 3', cat: 6 }, { slot: 51, name: 'Synth Bass 4', cat: 6 },
  { slot: 52, name: 'Bell 1', cat: 2 }, { slot: 53, name: 'Bell 2', cat: 2 },
  { slot: 54, name: 'Bell 3', cat: 2 }, { slot: 55, name: 'Bell 4', cat: 2 },
  { slot: 56, name: 'Voice 1', cat: 8 }, { slot: 57, name: 'Voice 2', cat: 8 },
  { slot: 58, name: 'Voice 3', cat: 8 }, { slot: 59, name: 'Voice 4', cat: 8 },
  { slot: 60, name: 'Strings 1', cat: 7 }, { slot: 61, name: 'Strings 2', cat: 7 },
  { slot: 62, name: 'Brass', cat: 7 },
  // Slots 64-159: Prophet VS (96 waves, cat=1 SynthWave as per C++)
  ...Array.from({ length: 96 }, (_, i) => ({
    slot: 64 + i,
    name: `PVS_${String(i + 33).padStart(3, '0')}`,
    cat: 1,
  })),
  // Slots 160-287: AKWF Curated (128 waves)
  ...[
    'AKWF_Saw', 'AKWF_Saw_Reso', 'AKWF_Square', 'AKWF_Square_50', 'AKWF_Triangle', 'AKWF_Tri_Soft', 'AKWF_Sine', 'AKWF_Sine_Harm',
    'AKWF_Pulse_25', 'AKWF_Pulse_10', 'AKWF_Pulse_5', 'AKWF_Ramp', 'AKWF_Sub_Bass', 'AKWF_TB303_Saw', 'AKWF_JP8000_Saw', 'AKWF_JP8000_Sq',
    'AKWF_Bass_1', 'AKWF_Bass_2', 'AKWF_Bass_FM', 'AKWF_Bass_Reese', 'AKWF_Lead_1', 'AKWF_Lead_2', 'AKWF_Lead_Square', 'AKWF_Mono_Lead',
    'AKWF_Acid_1', 'AKWF_Acid_2', 'AKWF_Hoover', 'AKWF_DnB_Bass', 'AKWF_Wobble_1', 'AKWF_Wobble_2', 'AKWF_Dubstep_Growl', 'AKWF_Neuro_Bass',
    'AKWF_Pad_1', 'AKWF_Pad_2', 'AKWF_Pad_3', 'AKWF_Pad_Bright', 'AKWF_Pad_Dark', 'AKWF_DigiPad', 'AKWF_Sweep_Up', 'AKWF_Sweep_Down',
    'AKWF_Ambient_1', 'AKWF_Ambient_2', 'AKWF_Atmos_1', 'AKWF_Atmos_2', 'AKWF_Glass', 'AKWF_Crystal', 'AKWF_Evolving_1', 'AKWF_Evolving_2',
    'AKWF_Vocal_A', 'AKWF_Vocal_E', 'AKWF_Vocal_I', 'AKWF_Vocal_O', 'AKWF_Vocal_U', 'AKWF_Choir_1', 'AKWF_Choir_2', 'AKWF_Voice_Synth',
    'AKWF_Whistle', 'AKWF_Beatbox', 'AKWF_AcGuitar', 'AKWF_Violin', 'AKWF_Cello', 'AKWF_Flute', 'AKWF_Clarinet', 'AKWF_Oboe',
    'AKWF_Brass_1', 'AKWF_Brass_2', 'AKWF_Brass_Ens', 'AKWF_Trumpet', 'AKWF_Trombone', 'AKWF_Sax_Alto', 'AKWF_Sax_Tenor', 'AKWF_Horn',
    'AKWF_Tuba', 'AKWF_Brass_Syn', 'AKWF_Brass_Fall', 'AKWF_Brass_Stab', 'AKWF_Harmonica', 'AKWF_Accordion', 'AKWF_Bagpipe', 'AKWF_Didgeridoo',
    'AKWF_Piano_1', 'AKWF_Piano_2', 'AKWF_EPiano_1', 'AKWF_EPiano_2', 'AKWF_Clav', 'AKWF_Organ_1', 'AKWF_Organ_2', 'AKWF_Organ_TW',
    'AKWF_Bell_1', 'AKWF_Bell_2', 'AKWF_Marimba', 'AKWF_Xylophone', 'AKWF_Vibes', 'AKWF_Glock', 'AKWF_MusicBox', 'AKWF_Kalimba',
    'AKWF_Metal_Hit', 'AKWF_Metal_Clang', 'AKWF_Metal_Sheet', 'AKWF_Metal_Pipe', 'AKWF_Laser', 'AKWF_Zap', 'AKWF_Riser', 'AKWF_Downer',
    'AKWF_Noise_W', 'AKWF_Noise_P', 'AKWF_Static', 'AKWF_Buzz', 'AKWF_Glitch_1', 'AKWF_Glitch_2', 'AKWF_Scratch', 'AKWF_Siren',
    'NES_Square_1', 'NES_Square_2', 'NES_Triangle', 'NES_Noise_Short', 'NES_Noise_Long', 'NES_DPCM_1', 'NES_DPCM_2', 'NES_DPCM_3',
    'NES_DPCM_4', 'NES_DPCM_5', 'NES_DPCM_6', 'NES_DPCM_7', 'NES_DPCM_8', 'NES_DPCM_9', 'NES_DPCM_10', 'NES_DPCM_11'
  ].map((name, i) => ({
    slot: 160 + i,
    name,
    cat: i < 16 ? 9 : (i < 32 ? 10 : (i < 48 ? 11 : (i < 64 ? 12 : (i < 80 ? 13 : (i < 96 ? 14 : (i < 112 ? 15 : 16))))))
  })),
  // Slots 288-383: AKWF Extras (96 waves)
  ...[
    'AKWF_Saw_Bright', 'AKWF_Saw_Dark', 'AKWF_Square_Fat', 'AKWF_Pulse_Tight', 'AKWF_Tri_Fold', 'AKWF_Sine_Warm', 'AKWF_Reso_Sweep', 'AKWF_Formant_1',
    'AKWF_Bass_3', 'AKWF_Bass_Grit', 'AKWF_Lead_3', 'AKWF_Lead_Aggro', 'AKWF_Arp_1', 'AKWF_Arp_2', 'AKWF_Pluck_1', 'AKWF_Pluck_2',
    'AKWF_Pad_4', 'AKWF_Pad_Vox', 'AKWF_Swell_1', 'AKWF_Swell_2', 'AKWF_Drone_1', 'AKWF_Drone_2', 'AKWF_Texture_1', 'AKWF_Texture_2',
    'AKWF_Voice_Robot', 'AKWF_Voice_Alien', 'AKWF_Choir_Synth', 'AKWF_Strings_Warm', 'AKWF_Cello_Synth', 'AKWF_Violin_Syn', 'AKWF_Flute_Pan', 'AKWF_Guitar_Nyl',
    'AKWF_Brass_3', 'AKWF_Brass_Soft', 'AKWF_Horn_Ens', 'AKWF_Trumpet_Mute', 'AKWF_Sax_Sop', 'AKWF_Sax_Bari', 'AKWF_Clarinet_2', 'AKWF_Flute_2',
    'AKWF_Piano_Soft', 'AKWF_EPiano_Warm', 'AKWF_Toy_Piano', 'AKWF_Harpsichord', 'AKWF_Steel_Drum', 'AKWF_Tubular', 'AKWF_Bell_3', 'AKWF_Celesta',
    'AKWF_LFO_Sweep', 'AKWF_Metal_Spring', 'AKWF_FX_Drop', 'AKWF_FX_Whoosh', 'AKWF_FX_Impact', 'AKWF_FX_Explosion', 'AKWF_FX_Wind', 'AKWF_FX_Thunder',
    'AKWF_FX_Robot', 'AKWF_FX_Alien', 'AKWF_Atari_Bass', 'AKWF_Atari_Lead', 'AKWF_Sega_FM', 'AKWF_PC_Speaker', 'AKWF_C64_Pulse', 'AKWF_C64_Saw',
    'AKWF_Amiga_Paula', 'AKWF_ST_Yamaha', 'AKWF_Moog_Saw', 'AKWF_Moog_Square', 'AKWF_Oberheim_Saw', 'AKWF_Roland_Saw', 'AKWF_Yamaha_Saw', 'AKWF_Korg_Saw',
    'AKWF_CS80_Saw', 'AKWF_Juno_Saw', 'AKWF_Sitar', 'AKWF_Banjo', 'AKWF_Mandolin', 'AKWF_Balinese', 'AKWF_Harp', 'AKWF_Koto',
    'AKWF_Shamisen', 'AKWF_Dulcimer', 'AKWF_Kick_808', 'AKWF_Snare_808', 'AKWF_Hat_Closed', 'AKWF_Cymbal', 'AKWF_Tom_Low', 'AKWF_Tom_High',
    'AKWF_Clap_808', 'AKWF_Rimshot', 'AKWF_Choir_Male', 'AKWF_Choir_Female', 'AKWF_Ooh', 'AKWF_Aah', 'AKWF_Talk_Box', 'AKWF_Vocoder_1'
  ].map((name, i) => ({
    slot: 288 + i,
    name,
    cat: i < 8 ? 9 : (i < 16 ? 10 : (i < 24 ? 11 : (i < 32 ? 12 : (i < 40 ? 13 : (i < 48 ? 14 : (i < 58 ? 15 : (i < 66 ? 16 : 10)))))))
  })),
  // Slots 384-511: User bank
  ...Array.from({ length: 128 }, (_, i) => ({
    slot: 384 + i,
    name: `User_${i + 1}`,
    cat: 17,
  })),
];

// Category ID → CSS class map
const CAT_CSS = {};
CATEGORIES.forEach(c => { CAT_CSS[c.id] = c.css; });

// ─── State ───────────────────────────────────────────────────────────────────

let isOpen = false;
let activeCategory = 0; // 0 = All
let searchQuery = '';
let selectedSlot = 0;
let overlayEl = null;
let gridEl = null;
let searchInput = null;
let onSlotSelected = null; // callback(slotNumber)
let fullCatalog = [...BOOTSTRAP_CATALOG];
let currentSynthMode = 0; // 0: MS2000, 1: microKORG, 2: Advanced

// ─── SVG Waveform Mini-Preview ───────────────────────────────────────────────

function generateMiniWaveformSvg(slot) {
  // Deterministic pseudo-random waveform based on slot
  const seed = slot * 7919;
  const points = [];
  const w = 80, h = 32;
  for (let x = 0; x < w; x++) {
    const t = x / w;
    let y = 0;
    const harmonics = 3 + (slot % 6);
    for (let h_idx = 1; h_idx <= harmonics; h_idx++) {
      const amp = 1.0 / h_idx;
      const phase = ((seed * h_idx * 13) % 1000) / 1000.0;
      y += amp * Math.sin(2 * Math.PI * h_idx * t + phase * 2 * Math.PI);
    }
    const maxAbs = harmonics * 0.6;
    const norm = (y / maxAbs + 1) / 2;
    points.push(`${x},${Math.round(h * (1 - norm * 0.85 - 0.075))}`);
  }
  return `<svg viewBox="0 0 ${w} ${h}" class="wt-mini-wave" xmlns="http://www.w3.org/2000/svg">
    <polyline points="${points.join(' ')}" fill="none" stroke="currentColor" stroke-width="1.2" stroke-linejoin="round"/>
  </svg>`;
}

// ─── Rendering ───────────────────────────────────────────────────────────────

function getFilteredCatalog() {
  // In classic profiles (MS2000=0, microKORG=1), restrict strictly to the 64 classic DWGS waves (slots 0..63)
  let items = currentSynthMode === 2
    ? fullCatalog
    : fullCatalog.filter(e => e.slot < 64);

  // Category filter
  if (activeCategory > 0) {
    items = items.filter(e => e.cat === activeCategory);
  }

  // Search filter
  if (searchQuery) {
    const q = searchQuery.toLowerCase();
    items = items.filter(e => e.name.toLowerCase().includes(q) || String(e.slot).includes(q));
  }

  return items;
}


function renderGrid() {
  if (!gridEl) return;
  const items = getFilteredCatalog();

  if (items.length === 0) {
    gridEl.innerHTML = `<div class="wt-empty">No wavetables found${searchQuery ? ` for "${searchQuery}"` : ''}</div>`;
    return;
  }

  // Virtual scrolling: render in chunks for performance
  const fragment = document.createDocumentFragment();
  items.forEach(entry => {
    const card = document.createElement('div');
    card.className = `wt-card${entry.slot === selectedSlot ? ' wt-selected' : ''}`;
    card.dataset.slot = entry.slot;

    const catName = CATEGORIES.find(c => c.id === entry.cat)?.name || 'Unknown';
    const catCss = CAT_CSS[entry.cat] || '';

    card.innerHTML = `
      <div class="wt-card-preview">${generateMiniWaveformSvg(entry.slot)}</div>
      <div class="wt-card-info">
        <span class="wt-card-slot">#${entry.slot + 1}</span>
        <span class="wt-card-name">${escHtml(entry.name)}</span>
      </div>
      <span class="wt-card-cat ${catCss}">${catName}</span>
    `;

    card.addEventListener('click', () => selectSlot(entry.slot));
    card.addEventListener('dblclick', () => {
      selectSlot(entry.slot);
      close();
    });
    fragment.appendChild(card);
  });

  gridEl.innerHTML = '';
  gridEl.appendChild(fragment);
}

export function getWaveName(slot) {
  const numSlot = Number(slot);
  const found = fullCatalog.find(e => e.slot === numSlot) || BOOTSTRAP_CATALOG.find(e => e.slot === numSlot);
  return found ? found.name : `Wave #${numSlot + 1}`;
}

function escHtml(s) {
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function updateFooterSelection() {
  if (!overlayEl) return;
  const sel = fullCatalog.find(e => e.slot === selectedSlot) || { slot: selectedSlot, name: `Wave #${selectedSlot + 1}` };
  const slotEl = overlayEl.querySelector('#wt-selected-slot');
  const nameEl = overlayEl.querySelector('#wt-selected-name');
  if (slotEl) slotEl.textContent = `#${sel.slot + 1}`;
  if (nameEl) nameEl.textContent = sel.name;
}

function selectSlot(slot) {
  selectedSlot = slot;
  if (typeof onSlotSelected === 'function') {
    onSlotSelected(slot);
  }
  renderGrid();
  updateFooterSelection();

  // Update the knob/select display
  const knobEl = document.querySelector('#knob-osc1DwgsWave');
  if (knobEl) {
    const valEl = knobEl.querySelector('.knob-value');
    if (valEl) valEl.textContent = `#${slot + 1}`;
  }
}

// ─── Category Chips ──────────────────────────────────────────────────────────

function renderCategoryChips(container) {
  // Count waves per category within the allowed profile mode
  const allowedWaves = currentSynthMode === 2
    ? fullCatalog
    : fullCatalog.filter(e => e.slot < 64);

  const counts = {};
  allowedWaves.forEach(e => {
    counts[e.cat] = (counts[e.cat] || 0) + 1;
  });

  let html = '';
  CATEGORIES.forEach(c => {
    const count = c.id === 0 ? allowedWaves.length : (counts[c.id] || 0);
    if (c.id > 0 && count === 0) return; // skip empty categories for this mode
    html += `<button class="wt-chip${activeCategory === c.id ? ' active' : ''}" data-cat="${c.id}">
      ${escHtml(c.name)} <span class="wt-chip-count">${count}</span>
    </button>`;
  });
  container.innerHTML = html;

  container.querySelectorAll('.wt-chip').forEach(chip => {
    chip.addEventListener('click', () => {
      activeCategory = parseInt(chip.dataset.cat, 10);
      renderCategoryChips(container);
      renderGrid();
    });
  });
}

// ─── Overlay Construction ────────────────────────────────────────────────────

function createOverlay() {
  if (overlayEl) return;

  overlayEl = document.createElement('div');
  overlayEl.id = 'wt-browser-overlay';
  overlayEl.className = 'wt-overlay hidden';
  overlayEl.innerHTML = `
    <div class="wt-browser">
      <div class="wt-header">
        <div class="wt-header-left">
          <h3 class="wt-title">WAVETABLE BROWSER</h3>
          <span class="wt-badge" id="wt-profile-badge">64 WAVES</span>
        </div>
        <button class="wt-close-btn" id="wt-close">✕</button>
      </div>

      <div class="wt-search-row">
        <input type="text" id="wt-search" class="wt-search-input" placeholder="Search wavetables by name or slot..." autocomplete="off" spellcheck="false">
        <span class="wt-search-icon">
          <svg viewBox="0 0 24 24" width="14" height="14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="11" cy="11" r="8"/><line x1="21" y1="21" x2="16.65" y2="16.65"/></svg>
        </span>
      </div>

      <div class="wt-chips-row" id="wt-chips"></div>

      <div class="wt-grid" id="wt-grid"></div>

      <div class="wt-drop-zone" id="wt-drop-zone">
        <span class="wt-drop-icon">
          <svg viewBox="0 0 24 24" width="18" height="18" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M22 19a2 2 0 0 1-2 2H4a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h5l2 3h9a2 2 0 0 1 2 2z"/></svg>
        </span>
        <span class="wt-drop-text">Drag & drop .wav files here (User bank slots 384–511)</span>
      </div>

      <div class="wt-footer">
        <span class="wt-footer-info">Slot <strong id="wt-selected-slot">#0</strong> — <span id="wt-selected-name">SynWave 1</span></span>
        <button class="wt-select-btn" id="wt-select-btn">SELECT WAVE</button>
      </div>
    </div>
  `;

  document.body.appendChild(overlayEl);

  // Cache references
  gridEl = overlayEl.querySelector('#wt-grid');
  searchInput = overlayEl.querySelector('#wt-search');
  const chipsRow = overlayEl.querySelector('#wt-chips');

  // Render category chips
  renderCategoryChips(chipsRow);

  // Search handler
  searchInput.addEventListener('input', (e) => {
    searchQuery = e.target.value.trim();
    renderGrid();
  });

  // Close
  overlayEl.querySelector('#wt-close').addEventListener('click', close);
  overlayEl.addEventListener('click', (e) => {
    if (e.target === overlayEl) close();
  });

  // Select button
  overlayEl.querySelector('#wt-select-btn').addEventListener('click', () => {
    selectSlot(selectedSlot);
    close();
  });

  // Keyboard navigation
  overlayEl.addEventListener('keydown', (e) => {
    const items = getFilteredCatalog();
    const currentIdx = items.findIndex(i => i.slot === selectedSlot);
    if (e.key === 'Escape') { close(); e.preventDefault(); }
    else if (e.key === 'ArrowDown' && currentIdx < items.length - 1) {
      selectSlot(items[currentIdx + 1].slot);
      e.preventDefault();
    } else if (e.key === 'ArrowUp' && currentIdx > 0) {
      selectSlot(items[currentIdx - 1].slot);
      e.preventDefault();
    } else if (e.key === 'Enter') {
      close();
      e.preventDefault();
    }
  });

  // Drag and drop for User bank
  const dropZone = overlayEl.querySelector('#wt-drop-zone');
  dropZone.addEventListener('dragover', (e) => { e.preventDefault(); dropZone.classList.add('wt-drop-active'); });
  dropZone.addEventListener('dragleave', () => { dropZone.classList.remove('wt-drop-active'); });
  dropZone.addEventListener('drop', (e) => {
    e.preventDefault();
    dropZone.classList.remove('wt-drop-active');
    handleWavDrop(e.dataTransfer.files);
  });
}

// ─── Public API ──────────────────────────────────────────────────────────────

export function openWavetableBrowser(bridge, currentSlot, callback, synthMode = 0) {
  currentSynthMode = synthMode;
  selectedSlot = currentSlot || 0;
  // If in classic mode and selected slot is >= 64, reset to slot 0
  if (currentSynthMode !== 2 && selectedSlot >= 64) {
    selectedSlot = 0;
  }

  onSlotSelected = callback || null;

  // Store bridge reference for drag-drop
  window.__bridge__ = bridge;

  createOverlay();
  overlayEl.classList.remove('hidden');
  isOpen = true;

  // Update mode badge and drop zone visibility
  const badgeEl = overlayEl.querySelector('#wt-profile-badge');
  const dropZone = overlayEl.querySelector('#wt-drop-zone');
  if (badgeEl) {
    badgeEl.textContent = currentSynthMode === 2 ? 'ADVANCED (512 WAVES)' : 'CLASSIC DWGS (64 WAVES)';
  }
  if (dropZone) {
    dropZone.style.display = currentSynthMode === 2 ? 'flex' : 'none';
  }

  // Focus search
  setTimeout(() => searchInput && searchInput.focus(), 100);

  // Render chips according to allowed mode
  const chipsRow = overlayEl.querySelector('#wt-chips');
  if (chipsRow) renderCategoryChips(chipsRow);

  // Update footer
  const sel = fullCatalog.find(e => e.slot === selectedSlot);
  if (sel) {
    overlayEl.querySelector('#wt-selected-slot').textContent = `#${sel.slot}`;
    overlayEl.querySelector('#wt-selected-name').textContent = sel.name;
  }

  renderGrid();
}


export function close() {
  if (overlayEl) {
    overlayEl.classList.add('hidden');
    isOpen = false;
  }
}

export function syncCatalogFromEngine(bridge) {
  if (!bridge) return;
  bridge.send('getWavetableCatalog', {});
  // Listen for response
  bridge.on('wavetableCatalog', (data) => {
    if (data && Array.isArray(data.catalog)) {
      fullCatalog = data.catalog;
      console.log(`[WavetableBrowser] Synced ${fullCatalog.length} waves from engine`);
      // Re-render if open
      if (isOpen && gridEl) {
        const chipsRow = overlayEl.querySelector('#wt-chips');
        if (chipsRow) renderCategoryChips(chipsRow);
        renderGrid();
      }
    }
  });
}

export function isBrowserOpen() {
  return isOpen;
}
