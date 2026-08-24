import { bridge } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';

let currentOctave = 0;
let currentTheme = 'ms2000';
let isAudioActive = false;

document.addEventListener('DOMContentLoaded', () => {
  console.log(`[ABDMS2000 App Init] Version: ${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber})`);

  setupThemeSelector();
  setupNavbarMenus();
  setupAudioInitButton();
  setupKeyboard();
  setupButtons();
  setupAboutModal();
  setupKeyboardShortcuts();
});

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

  if (mode === 'ms2000') {
    document.documentElement.removeAttribute('data-theme');
  } else {
    document.documentElement.setAttribute('data-theme', mode);
  }
  currentTheme = mode;
  console.log('[Theme Switch]:', mode);
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
      console.log('[Preset Reset to Default]');
      break;
    case 'randomize-patch':
    case 'randomize':
      console.log('[Randomize Parameters]');
      break;
    case 'panic':
      bridge.allNotesOff();
      document.querySelectorAll('.white-key, .black-key').forEach(k => k.classList.remove('active'));
      break;
  }
}

function setupAudioInitButton() {
  const btn = document.getElementById('btn-audio-init');
  if (!btn) return;

  btn.addEventListener('click', async () => {
    try {
      // Resume browser AudioContext if running in Web/WASM
      if (window.AudioContext || window.webkitAudioContext) {
        if (!window.__audioCtx) {
          const AudioCtxClass = window.AudioContext || window.webkitAudioContext;
          window.__audioCtx = new AudioCtxClass();
        }
        if (window.__audioCtx.state === 'suspended') {
          await window.__audioCtx.resume();
        }
      }

      isAudioActive = true;
      btn.classList.add('active');
      const textSpan = btn.querySelector('.audio-text');
      if (textSpan) textSpan.textContent = 'AUDIO ON';
      console.log('[Web Audio Initialized]');
    } catch (err) {
      console.warn('[Web Audio Init Warning]:', err);
    }
  });
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
}

function openAboutModal() {
  const modal = document.getElementById('about-modal');
  if (modal) {
    modal.classList.remove('hidden');
    modal.style.setProperty('display', 'flex', 'important');
  }
}

function setupKeyboard() {
  const pianoContainer = document.getElementById('piano-keyboard');
  if (!pianoContainer || pianoContainer.children.length > 0) return;

  // 2-Octave White Key map (C3 = 48 to C5 = 72) with corresponding accidental sharp notes
  const whiteKeyDefs = [
    { note: 48, sharp: 49 }, // C3 -> C#3
    { note: 50, sharp: 51 }, // D3 -> D#3
    { note: 52, sharp: null }, // E3
    { note: 53, sharp: 54 }, // F3 -> F#3
    { note: 55, sharp: 56 }, // G3 -> G#3
    { note: 57, sharp: 58 }, // A3 -> A#3
    { note: 59, sharp: null }, // B3
    { note: 60, sharp: 61 }, // C4 -> C#4
    { note: 62, sharp: 63 }, // D4 -> D#4
    { note: 64, sharp: null }, // E4
    { note: 65, sharp: 66 }, // F4 -> F#4
    { note: 67, sharp: 68 }, // G4 -> G#4
    { note: 69, sharp: 70 }, // A4 -> A#4
    { note: 71, sharp: null }, // B4
    { note: 72, sharp: null }  // C5
  ];

  const bindKeyEvents = (elem, midiNote) => {
    const playNote = (e) => {
      e.stopPropagation();
      e.preventDefault();
      elem.classList.add('active');
      bridge.noteOn(midiNote + currentOctave * 12, 0.85);
    };

    const stopNote = (e) => {
      e.stopPropagation();
      elem.classList.remove('active');
      bridge.noteOff(midiNote + currentOctave * 12, 0.0);
    };

    elem.addEventListener('mousedown', playNote);
    elem.addEventListener('mouseup', stopNote);
    elem.addEventListener('mouseleave', () => {
      if (elem.classList.contains('active')) stopNote({ stopPropagation: () => {} });
    });

    // Touch support for mobile / tablets
    elem.addEventListener('touchstart', playNote, { passive: false });
    elem.addEventListener('touchend', stopNote, { passive: false });
    elem.addEventListener('touchcancel', stopNote, { passive: false });
  };

  whiteKeyDefs.forEach(def => {
    const whiteKey = document.createElement('div');
    whiteKey.className = 'white-key';
    whiteKey.dataset.note = def.note;
    bindKeyEvents(whiteKey, def.note);

    if (def.sharp !== null) {
      const blackKey = document.createElement('div');
      blackKey.className = 'black-key';
      blackKey.dataset.note = def.sharp;
      bindKeyEvents(blackKey, def.sharp);
      whiteKey.appendChild(blackKey);
    }

    pianoContainer.appendChild(whiteKey);
  });

  // Octave Shift Buttons & LEDs
  const octDown = document.getElementById('oct-down');
  const octUp = document.getElementById('oct-up');
  const ledDown = document.getElementById('led-down');
  const ledUp = document.getElementById('led-up');

  function updateOctaveUI() {
    if (!ledDown || !ledUp) return;
    ledDown.className = 'oct-led';
    ledUp.className = 'oct-led';

    if (currentOctave === -1) ledDown.classList.add('blink');
    else if (currentOctave <= -2) ledDown.classList.add('solid');

    if (currentOctave === 1) ledUp.classList.add('blink');
    else if (currentOctave >= 2) ledUp.classList.add('solid');
  }

  octDown?.addEventListener('click', () => {
    if (currentOctave > -3) {
      currentOctave--;
      updateOctaveUI();
    }
  });

  octUp?.addEventListener('click', () => {
    if (currentOctave < 3) {
      currentOctave++;
      updateOctaveUI();
    }
  });
}

function setupButtons() {
  const panicBtn = document.getElementById('btn-panic');
  panicBtn?.addEventListener('click', () => {
    bridge.allNotesOff();
    document.querySelectorAll('.white-key, .black-key').forEach(k => k.classList.remove('active'));
    console.log('[Panic All Notes Off]');
  });

  const randomBtn = document.getElementById('btn-random');
  randomBtn?.addEventListener('click', () => {
    handleMenuAction('randomize-patch');
  });

  const compareBtn = document.getElementById('btn-compare');
  compareBtn?.addEventListener('click', () => {
    console.log('[Compare A/B Toggled]');
  });
}

function setupKeyboardShortcuts() {
  // QWERTY Key mapping (White keys: Z, S, X, D, C, V, G, B, H, N, J, M)
  const keyMap = {
    'z': 48, 's': 49, 'x': 50, 'd': 51, 'c': 52,
    'v': 53, 'g': 54, 'b': 55, 'h': 56, 'n': 57, 'j': 58, 'm': 59,
    'q': 60, '2': 61, 'w': 62, '3': 63, 'e': 64,
    'r': 65, '5': 66, 't': 67, '6': 68, 'y': 69, '7': 70, 'u': 71, 'i': 72
  };

  const activeHeldKeys = new Set();

  document.addEventListener('keydown', (e) => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

    if (e.key === 'Escape') {
      closeAllDropdowns();
      const modal = document.getElementById('about-modal');
      if (modal) {
        modal.style.setProperty('display', 'none', 'important');
        modal.classList.add('hidden');
      }
      return;
    }

    const key = e.key.toLowerCase();
    if (keyMap[key] && !activeHeldKeys.has(key)) {
      activeHeldKeys.add(key);
      const midiNote = keyMap[key] + currentOctave * 12;
      bridge.noteOn(midiNote, 0.85);

      const keyElem = document.querySelector(`[data-note="${keyMap[key]}"]`);
      keyElem?.classList.add('active');
    }
  });

  document.addEventListener('keyup', (e) => {
    const key = e.key.toLowerCase();
    if (keyMap[key] && activeHeldKeys.has(key)) {
      activeHeldKeys.delete(key);
      const midiNote = keyMap[key] + currentOctave * 12;
      bridge.noteOff(midiNote, 0.0);

      const keyElem = document.querySelector(`[data-note="${keyMap[key]}"]`);
      keyElem?.classList.remove('active');
    }
  });
}
