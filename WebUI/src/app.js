import { bridge } from './bridge/bridgeCore.js';
import { BUILD_INFO } from './contracts/buildVersion.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';

let currentOctave = 0;
let currentTheme = 'ms2000';

document.addEventListener('DOMContentLoaded', () => {
  console.log(`[ABDMS2000 App Init] Version: ${BUILD_INFO.version} (Build ${BUILD_INFO.buildNumber})`);

  setupThemeSelector();
  setupNavbarMenus();
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
      const menuType = btn.dataset.menu;
      const targetDropdown = document.getElementById(`menu-${menuType}`);
      const isAlreadyOpen = targetDropdown && targetDropdown.classList.contains('show');

      // Close all other dropdowns
      closeAllDropdowns();

      if (targetDropdown && !isAlreadyOpen) {
        targetDropdown.classList.add('show');
        btn.classList.add('active');
      }
    });

    // Hover to switch open dropdown when another is already active
    btn.addEventListener('mouseenter', () => {
      const anyOpen = Array.from(dropdowns).some(d => d.classList.contains('show'));
      if (anyOpen) {
        closeAllDropdowns();
        const menuType = btn.dataset.menu;
        const targetDropdown = document.getElementById(`menu-${menuType}`);
        if (targetDropdown) {
          targetDropdown.classList.add('show');
          btn.classList.add('active');
        }
      }
    });
  });

  // Close menus when clicking outside
  document.addEventListener('click', (e) => {
    if (!e.target.closest('.nav-menu-bar')) {
      closeAllDropdowns();
    }
  });

  // Handle dropdown actions
  const dropdownItems = document.querySelectorAll('.dropdown-item');
  dropdownItems.forEach(item => {
    item.addEventListener('click', () => {
      const action = item.dataset.action;
      handleMenuAction(action);
      closeAllDropdowns();
    });
  });
}

function closeAllDropdowns() {
  document.querySelectorAll('.dropdown-menu').forEach(d => d.classList.remove('show'));
  document.querySelectorAll('.nav-menu-bar .menu-btn').forEach(b => b.classList.remove('active'));
}

function handleMenuAction(action) {
  console.log('[Menu Action]:', action);
  switch (action) {
    case 'new-patch':
    case 'init-sound':
      bridge.allNotesOff();
      updateLcdText('A.01 Init Program', 'VA Saw + MultiFilter');
      break;

    case 'save-patch':
      alert('Save Patch: Saved to local store.');
      break;

    case 'randomize':
      updateLcdText('A.01 Random Patch', 'DWGS + BandPass');
      break;

    case 'skin-ms2000':
      setTheme('ms2000');
      break;

    case 'skin-microkorg':
      setTheme('microkorg');
      break;

    case 'skin-advanced':
      setTheme('advanced');
      break;

    case 'zoom-100':
      document.body.style.transform = 'scale(1.0)';
      document.body.style.transformOrigin = 'top left';
      break;

    case 'zoom-125':
      document.body.style.transform = 'scale(1.25)';
      document.body.style.transformOrigin = 'top left';
      break;

    case 'zoom-150':
      document.body.style.transform = 'scale(1.5)';
      document.body.style.transformOrigin = 'top left';
      break;

    case 'github-repo':
      window.open('https://github.com/ajabadia/ABDMS2000', '_blank');
      break;

    case 'about':
      openAboutModal();
      break;

    default:
      console.log(`Action ${action} triggered.`);
      break;
  }
}

function updateLcdText(line1, line2) {
  const l1 = document.getElementById('lcd-line-1');
  const l2 = document.getElementById('lcd-line-2');
  if (l1) l1.textContent = line1;
  if (l2) l2.textContent = line2;
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
  if (pianoContainer && pianoContainer.children.length === 0) {
    // Generate 2 Octaves (C3 = 48 to B4 = 71)
    const notes = [
      { n: 48, isBlack: false }, { n: 49, isBlack: true },
      { n: 50, isBlack: false }, { n: 51, isBlack: true },
      { n: 52, isBlack: false },
      { n: 53, isBlack: false }, { n: 54, isBlack: true },
      { n: 55, isBlack: false }, { n: 56, isBlack: true },
      { n: 57, isBlack: false }, { n: 58, isBlack: true },
      { n: 59, isBlack: false },
      { n: 60, isBlack: false }, { n: 61, isBlack: true },
      { n: 62, isBlack: false }, { n: 63, isBlack: true },
      { n: 64, isBlack: false },
      { n: 65, isBlack: false }, { n: 66, isBlack: true },
      { n: 67, isBlack: false }, { n: 68, isBlack: true },
      { n: 69, isBlack: false }, { n: 70, isBlack: true },
      { n: 71, isBlack: false },
      { n: 72, isBlack: false }
    ];

    notes.forEach(noteObj => {
      const key = document.createElement('div');
      key.className = noteObj.isBlack ? 'black-key' : 'white-key';
      key.dataset.note = noteObj.n;

      key.addEventListener('mousedown', () => {
        key.classList.add('active');
        bridge.noteOn(noteObj.n + currentOctave * 12, 0.85);
      });

      key.addEventListener('mouseup', () => {
        key.classList.remove('active');
        bridge.noteOff(noteObj.n + currentOctave * 12, 0.0);
      });

      key.addEventListener('mouseleave', () => {
        if (key.classList.contains('active')) {
          key.classList.remove('active');
          bridge.noteOff(noteObj.n + currentOctave * 12, 0.0);
        }
      });

      pianoContainer.appendChild(key);
    });
  }

  // Octave buttons
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
    handleMenuAction('randomize');
  });

  const compareBtn = document.getElementById('btn-compare');
  compareBtn?.addEventListener('click', () => {
    console.log('[Compare A/B Toggled]');
  });
}

function setupKeyboardShortcuts() {
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      closeAllDropdowns();
      document.getElementById('about-modal')?.classList.add('hidden');
    }

    if (e.ctrlKey || e.metaKey) {
      switch (e.key.toLowerCase()) {
        case 'n': e.preventDefault(); handleMenuAction('new-patch'); break;
        case 'o': e.preventDefault(); handleMenuAction('open-patch'); break;
        case 's': e.preventDefault(); handleMenuAction('save-patch'); break;
        case 'z': e.preventDefault(); handleMenuAction('undo'); break;
        case 'y': e.preventDefault(); handleMenuAction('redo'); break;
        case 'r': e.preventDefault(); handleMenuAction('randomize'); break;
        case 'i': e.preventDefault(); handleMenuAction('init-sound'); break;
      }
    }
  });
}
