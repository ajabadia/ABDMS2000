// Shared LCD notice wording for messages emitted outside the 16-button strip.
// The same action can be announced from a menu, the LCD panel or a drawer, so the
// text is built once here instead of being composed at each call site.

/** "EDIT TIMBRE n" is flashed both by the menu/app and by LcdProgrammer.setActiveTimbre(). */
export function buildEditTimbreNotice({ timbre = 1 } = {}) {
  const t = timbre === 2 ? 2 : 1;
  return { line1: `EDIT TIMBRE ${t}`, line2: t === 2 ? 'SPLIT/LAYER T2' : 'MAIN TIMBRE 1' };
}

// Hardware profile shown after switching synth mode (Mode menu / theme switcher).
const ENGINE_PROFILE_NOTICES = [
  { line1: 'MODE: KORG MS2000', line2: '4V 16B-VOC MSEQ' },
  { line1: 'MODE: microKORG', line2: '4V 8CH-VOC 8ARP' },
  { line1: 'MODE: ADVANCED', line2: '16V 512-DWGS ALL' },
];

export function buildEngineProfileNotice({ modeIndex = 0 } = {}) {
  const idx = Math.min(ENGINE_PROFILE_NOTICES.length - 1, Math.max(0, Math.round(modeIndex) || 0));
  return { ...ENGINE_PROFILE_NOTICES[idx] };
}

/** Timbre copy / swap notices (T1 -> T2, T2 -> T1, T1 <-> T2). */
export function buildTimbreRoutingNotice({ from = 1, to = 2, op = 'copy' } = {}) {
  if (op === 'swap') {
    return { line1: `TIMBRE ${from} <-> ${to}`, line2: 'SWAP COMPLETED' };
  }
  return { line1: `TIMBRE ${from} -> ${to}`, line2: 'COPY COMPLETED' };
}

/** Voice assign notice: 0 = single/mono, 1 = poly, 2 = unison (voice count from the engine profile). */
export function buildVoiceAssignNotice({ mode = 0, voiceCount = 4 } = {}) {
  const voices = Math.max(1, Math.round(voiceCount) || 4);
  let line2 = 'SINGLE / MONO';
  if (mode === 1) line2 = `POLY (${voices} VOICES)`;
  else if (mode === 2) line2 = `UNISON (${voices} VOICES)`;
  return { line1: 'VOICE ASSIGN', line2 };
}

// buildWriteCompletedNotice vive ya en el contrato compartido de la suite:
// '@abdsynths/shared/components' (ABDShared/Components/lcdNotices.js).
