import { bridge } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';

let currentOctave = 0;
let currentTheme = 'ms2000';

document.addEventListener('DOMContentLoaded', () => {
  console.log(`[ABDMS2000 App Init] Version: ${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber})`);

  setupThemeSelector();
  setupKeyboard();
  setupButtons();
});

function setupThemeSelector() {
  const tabs = document.querySelectorAll('.mode-tab');
  tabs.forEach(tab => {
    tab.addEventListener('click', () => {
      tabs.forEach(t => t.classList.remove('active'));
      tab.classList.add('active');

      const mode = tab.dataset.mode;
      if (mode === 'ms2000') {
        document.documentElement.removeAttribute('data-theme');
      } else {
        document.documentElement.setAttribute('data-theme', mode);
      }
      currentTheme = mode;
      console.log('[Theme Switch]:', mode);
    });
  });
}

function setupKeyboard() {
  const keys = document.querySelectorAll('.white-key, .black-key');
  keys.forEach(key => {
    const note = parseInt(key.dataset.note, 10);

    key.addEventListener('mousedown', () => {
      key.classList.add('active');
      bridge.noteOn(note + currentOctave * 12, 0.85);
    });

    key.addEventListener('mouseup', () => {
      key.classList.remove('active');
      bridge.noteOff(note + currentOctave * 12, 0.0);
    });

    key.addEventListener('mouseleave', () => {
      if (key.classList.contains('active')) {
        key.classList.remove('active');
        bridge.noteOff(note + currentOctave * 12, 0.0);
      }
    });
  });

  // Octave buttons
  const octDown = document.getElementById('btn-oct-down');
  const octUp = document.getElementById('btn-oct-up');
  const ledDown = document.getElementById('led-oct-down');
  const ledUp = document.getElementById('led-oct-up');

  function updateOctaveUI() {
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
}
