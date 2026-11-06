// Shared wording for the KORG MS2000 16-button strip: the mode badge next to
// BANK / OCT and the LCD notices print the same "MODE · IDENTITY" string, so the
// mode label and the bank / page / timbre / octave identity can't drift apart.

export const MS2K_MODE_LABELS = {
  program: 'PROGRAM / EDIT',
  seq: (n) => `SEQ ${n} EDIT`,
  chLevel: 'CH LEVEL EDIT',
  chPan: 'CH PAN EDIT',
  kbd: '16-PAD KBD',
};

const BANK_LETTERS = ['A', 'B', 'C', 'D', 'E', 'F', 'G', 'H'];

export const MS2K_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

/** MIDI note number -> MS2000 pad label (60 -> "C4", 75 -> "D#5"). */
export function formatPadMidiNote(midiNote) {
  const m = Math.max(0, Math.min(127, Math.round(midiNote)));
  const name = MS2K_NOTE_NAMES[m % 12];
  const oct = Math.floor(m / 12) - 1;
  return `${name}${oct}`;
}

function padNoteRange(octaveShift) {
  const base = 60 + octaveShift * 12;
  return `${formatPadMidiNote(base)} .. ${formatPadMidiNote(base + 15)}`;
}

/**
 * Builds the strip readout from a plain snapshot of the strip state.
 *
 * @param {object} state
 * @param {boolean} [state.isVocoder]     >0.5 synthVocoderMode
 * @param {boolean} [state.padKbdMode]    16-pad chromatic keyboard active
 * @param {number}  [state.latchedCount]  latched pad notes in KBD mode
 * @param {number}  [state.stripSeqMode]  0 = program/edit, 1/2/3 = SEQ 1/2/3 or CH LEVEL/PAN
 * @param {number}  [state.octaveShift]   KBD octave shift (-3..+3)
 * @param {number}  [state.activeTimbre]  1 or 2
 * @param {number|null} [state.pageNumber] open edit page, null outside edit mode
 * @param {number}  [state.bankIndex]     0..7 (A..H)
 * @param {number}  [state.progSlot]      1..16
 * @returns {{ modeText: string, identText: string, label: string }}
 */
export function buildMs2kModeReadout(state) {
  const {
    isVocoder = false,
    padKbdMode = false,
    latchedCount = 0,
    stripSeqMode = 0,
    octaveShift = 0,
    activeTimbre = 1,
    pageNumber = null,
    bankIndex = 0,
    progSlot = 1,
  } = state || {};

  let modeText = MS2K_MODE_LABELS.program;
  let identText = '';

  if (padKbdMode) {
    modeText = MS2K_MODE_LABELS.kbd;
    identText = latchedCount > 0
      ? `HOLD ${latchedCount}`
      : `OCT ${octaveShift > 0 ? `+${octaveShift}` : octaveShift}`;
  } else if (stripSeqMode > 0) {
    modeText = isVocoder
      ? (stripSeqMode === 1 ? MS2K_MODE_LABELS.chLevel : MS2K_MODE_LABELS.chPan)
      : MS2K_MODE_LABELS.seq(stripSeqMode);
    identText = `T${activeTimbre}`;
  } else if (pageNumber) {
    identText = `P${String(pageNumber).padStart(2, '0')}`;
  } else {
    identText = `${BANK_LETTERS[bankIndex] || 'A'}.${String(progSlot).padStart(2, '0')}`;
  }

  return { modeText, identText, label: identText ? `${modeText} · ${identText}` : modeText };
}

/**
 * LCD notice shown when a page caption leaves SEQ EDIT / CH LEVEL / CH PAN / KBD.
 *
 * @param {object} state
 * @param {boolean} [state.padKbdMode]  the released mode was the 16-pad keyboard
 * @param {number}  [state.stripSeqMode] released SEQ 1/2/3 or CH LEVEL/PAN
 * @param {boolean} [state.isVocoder]
 * @param {number}  [state.pageNumber]  page opened by the click
 * @param {string}  [state.pageName]    caption of that page ("COMMON", "FILTER", ...)
 * @returns {{ line1: string, line2: string }}
 */
export function buildMs2kExitNotice(state) {
  const { padKbdMode = false, stripSeqMode = 0, isVocoder = false, pageNumber, pageName = '' } = state || {};
  const line1 = padKbdMode
    ? 'EXIT 16-PAD KBD'
    : (isVocoder
      ? `EXIT CH ${stripSeqMode === 1 ? 'LEVEL' : 'PAN'}`
      : `EXIT SEQ ${stripSeqMode} EDIT`);
  const pageLabel = `PAGE ${pageNumber ? String(pageNumber).padStart(2, '0') : '--'}`;
  const cleanName = String(pageName || '').trim();
  return { line1, line2: cleanName ? `${pageLabel}: ${cleanName}` : pageLabel };
}

/**
 * LCD notice for Shift+click on a pad in 16-pad keyboard mode.
 *
 * @param {object} state
 * @param {number} state.padNumber 1..16
 * @param {string} state.noteName  e.g. "C4"
 * @param {boolean} state.latched  true when the note was just latched
 * @returns {{ line1: string, line2: string }}
 */
export function buildMs2kPadLatchNotice({ padNumber, noteName, latched }) {
  return {
    line1: `PAD ${padNumber} (${noteName})`,
    line2: latched ? 'LATCHED (HOLD)' : 'UNLATCHED',
  };
}

/** LCD notice for Shift+click on KBD, which releases every latched note. */
export const MS2K_LATCH_CLEARED_NOTICE = { line1: '16-PAD KEYBOARD', line2: 'LATCH CLEARED' };

/**
 * LCD notice for entering / cycling through the pad modes: the badge readout plus
 * what the 16 pads do in that mode.
 *
 * @param {object} state same snapshot as buildMs2kModeReadout()
 * @returns {{ line1: string, line2: string }}
 */
export function buildMs2kModeNotice(state) {
  const { padKbdMode = false, stripSeqMode = 0, isVocoder = false, octaveShift = 0 } = state || {};
  const { label } = buildMs2kModeReadout(state);
  let line2 = 'PADS = PAGE JUMP';
  if (padKbdMode) {
    line2 = `${padNoteRange(octaveShift)} · SHIFT=LATCH`;
  } else if (stripSeqMode > 0) {
    line2 = isVocoder ? 'PADS = CH 01-16' : 'PADS = STEPS 01-16';
  }
  return { line1: label, line2 };
}

/** LCD notice for the OCT -/+ buttons while the 16-pad keyboard is active. */
export function buildMs2kOctaveNotice({ octaveShift = 0 } = {}) {
  return { line1: 'PAD KBD OCTAVE', line2: padNoteRange(octaveShift) };
}

/** LCD notice while dragging / stepping a Mod Sequence step. */
export function buildMs2kStepValueNotice({ stripSeqMode, padNumber, value }) {
  const v = Math.round(value);
  return {
    line1: `SEQ ${stripSeqMode} STEP ${String(padNumber).padStart(2, '0')}`,
    line2: `Value: ${v > 0 ? `+${v}` : v}`,
  };
}

/** Vocoder panpot label: 0 -> "CNT", +12 -> "R12", -30 -> "L30". */
export function formatVocoderPan(pan) {
  const v = Math.round(pan);
  if (v === 0) return 'CNT';
  return v > 0 ? `R${v}` : `L${Math.abs(v)}`;
}

/** LCD notice for a vocoder band level (CH LEVEL mode). */
export function buildMs2kBandLevelNotice({ padNumber, level }) {
  return {
    line1: `VOCODER CH ${String(padNumber).padStart(2, '0')}`,
    line2: `Level: ${Math.round(level)}`,
  };
}

/** LCD notice for a vocoder band pan (CH PAN mode). */
export function buildMs2kBandPanNotice({ padNumber, pan }) {
  return {
    line1: `VOCODER CH ${String(padNumber).padStart(2, '0')}`,
    line2: `Panpot: ${formatVocoderPan(pan)}`,
  };
}

/** LCD notice while playing the pads as a chromatic keyboard. */
export function buildMs2kPadNoteNotice({ noteName, midiNote }) {
  return { line1: 'PAD KEYBOARD', line2: `Note: ${noteName} (MIDI ${midiNote})` };
}

/** LCD notice for Shift+click on a pad in SEQ EDIT, which sets the loop length. */
export function buildMs2kSeqLengthNotice({ steps }) {
  return { line1: 'SEQ LAST STEP', line2: `LENGTH: ${steps} STEPS` };
}

