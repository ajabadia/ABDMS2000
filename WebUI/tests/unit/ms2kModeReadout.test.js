import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  buildMs2kModeReadout,
  buildMs2kModeNotice,
  buildMs2kExitNotice,
  buildMs2kPadLatchNotice,
  buildMs2kOctaveNotice,
  buildMs2kStepValueNotice,
  buildMs2kBandLevelNotice,
  buildMs2kBandPanNotice,
  buildMs2kPadNoteNotice,
  buildMs2kSeqLengthNotice,
  formatPadMidiNote,
  formatVocoderPan,
  MS2K_LATCH_CLEARED_NOTICE,
} from '../../src/contracts/ms2kModeReadout.js';

const APP_SOURCE = readFileSync(fileURLToPath(new URL('../../src/app.js', import.meta.url)), 'utf8');

describe('MS2000 strip mode · identity readout', () => {
  const cases = [
    ['program default', {}, 'PROGRAM / EDIT · A.01'],
    ['program bank C slot 7', { bankIndex: 2, progSlot: 7 }, 'PROGRAM / EDIT · C.07'],
    ['edit page 01', { pageNumber: 1 }, 'PROGRAM / EDIT · P01'],
    ['edit page 06', { pageNumber: 6 }, 'PROGRAM / EDIT · P06'],
    ['SEQ 1 timbre 1', { stripSeqMode: 1, activeTimbre: 1 }, 'SEQ 1 EDIT · T1'],
    ['SEQ 3 timbre 2', { stripSeqMode: 3, activeTimbre: 2 }, 'SEQ 3 EDIT · T2'],
    ['vocoder CH LEVEL', { isVocoder: true, stripSeqMode: 1 }, 'CH LEVEL EDIT · T1'],
    ['vocoder CH PAN', { isVocoder: true, stripSeqMode: 2 }, 'CH PAN EDIT · T1'],
    ['KBD octave 0', { padKbdMode: true }, '16-PAD KBD · OCT 0'],
    ['KBD octave +2', { padKbdMode: true, octaveShift: 2 }, '16-PAD KBD · OCT +2'],
    ['KBD octave -1', { padKbdMode: true, octaveShift: -1 }, '16-PAD KBD · OCT -1'],
    ['KBD latched', { padKbdMode: true, latchedCount: 3 }, '16-PAD KBD · HOLD 3'],
  ];

  it.each(cases)('%s -> %s', (_name, state, expected) => {
    expect(buildMs2kModeReadout(state).label).toBe(expected);
  });

  it('KBD takes precedence over a residual SEQ mode', () => {
    const readout = buildMs2kModeReadout({ padKbdMode: true, stripSeqMode: 2 });
    expect(readout.label).toBe('16-PAD KBD · OCT 0');
  });

  it('always renders "MODE · IDENTITY" with the mode text as prefix', () => {
    for (const [, state] of cases) {
      const { modeText, identText, label } = buildMs2kModeReadout(state);
      expect(modeText.length).toBeGreaterThan(0);
      expect(label.startsWith(modeText)).toBe(true);
      expect(label).toBe(`${modeText} · ${identText}`);
    }
  });

  it('badge and LCD notice both print the shared label', () => {
    // Badge path in refreshMs2kBottomStrip()
    expect(APP_SOURCE).toMatch(/const nextBadge = getMs2kModeReadout\(\)\.label;/);
    // LCD notice path in flashStripModeNotice()
    expect(APP_SOURCE).toMatch(/const notice = buildMs2kModeNotice\(getMs2kModeState\(\)\);/);
    // Both are fed by the shared contract, never by a locally rebuilt mode string
    expect(APP_SOURCE).toMatch(/return buildMs2kModeReadout\(getMs2kModeState\(\)\);/);;
    expect(APP_SOURCE).not.toContain("'PROGRAM / EDIT'");
    expect(APP_SOURCE).not.toContain("'CH LEVEL EDIT'");
    expect(APP_SOURCE).not.toContain("'CH PAN EDIT'");
  });
});

describe('MS2000 strip exit notice (caption leaves a pad mode)', () => {
  const cases = [
    [
      'KBD -> page 06 FILTER',
      { padKbdMode: true, pageNumber: 6, pageName: 'FILTER' },
      { line1: 'EXIT 16-PAD KBD', line2: 'PAGE 06: FILTER' },
    ],
    [
      'SEQ 1 -> page 01 COMMON',
      { stripSeqMode: 1, pageNumber: 1, pageName: 'COMMON' },
      { line1: 'EXIT SEQ 1 EDIT', line2: 'PAGE 01: COMMON' },
    ],
    [
      'SEQ 3 -> page 16 UTILITY',
      { stripSeqMode: 3, pageNumber: 16, pageName: 'UTILITY' },
      { line1: 'EXIT SEQ 3 EDIT', line2: 'PAGE 16: UTILITY' },
    ],
    [
      'vocoder CH LEVEL -> page 01',
      { stripSeqMode: 1, isVocoder: true, pageNumber: 1, pageName: 'COMMON' },
      { line1: 'EXIT CH LEVEL', line2: 'PAGE 01: COMMON' },
    ],
    [
      'vocoder CH PAN -> page 02',
      { stripSeqMode: 2, isVocoder: true, pageNumber: 2, pageName: 'VOICE' },
      { line1: 'EXIT CH PAN', line2: 'PAGE 02: VOICE' },
    ],
    [
      'unnamed caption',
      { stripSeqMode: 2, pageNumber: 9 },
      { line1: 'EXIT SEQ 2 EDIT', line2: 'PAGE 09' },
    ],
  ];

  it.each(cases)('%s', (_name, state, expected) => {
    expect(buildMs2kExitNotice(state)).toEqual(expected);
  });

  it('trims a caption name and keeps the page number padded', () => {
    expect(buildMs2kExitNotice({ padKbdMode: true, pageNumber: 4, pageName: '  PATCH  ' })).toEqual({
      line1: 'EXIT 16-PAD KBD',
      line2: 'PAGE 04: PATCH',
    });
  });

  it('caption handler prints the shared exit notice', () => {
    expect(APP_SOURCE).toMatch(/const exitNotice = buildMs2kExitNotice\(\{/);
    expect(APP_SOURCE).toMatch(/flashMessage\(exitNotice\.line1, exitNotice\.line2, 1600\)/);
  });
});

describe('MS2000 strip latch notices (KBD mode)', () => {
  it('announces a latched pad with its note', () => {
    expect(buildMs2kPadLatchNotice({ padNumber: 1, noteName: 'C4', latched: true })).toEqual({
      line1: 'PAD 1 (C4)',
      line2: 'LATCHED (HOLD)',
    });
  });

  it('announces an unlatched pad with its note', () => {
    expect(buildMs2kPadLatchNotice({ padNumber: 16, noteName: 'D#5', latched: false })).toEqual({
      line1: 'PAD 16 (D#5)',
      line2: 'UNLATCHED',
    });
  });

  it('exposes the latch-cleared notice', () => {
    expect(MS2K_LATCH_CLEARED_NOTICE).toEqual({ line1: '16-PAD KEYBOARD', line2: 'LATCH CLEARED' });
  });

  it('badge shows the latch count while KBD is latched', () => {
    expect(buildMs2kModeReadout({ padKbdMode: true, latchedCount: 1 }).label).toBe('16-PAD KBD · HOLD 1');
    expect(buildMs2kModeReadout({ padKbdMode: true, latchedCount: 16 }).label).toBe('16-PAD KBD · HOLD 16');
    // Octave suffix returns once nothing is latched
    expect(buildMs2kModeReadout({ padKbdMode: true, latchedCount: 0, octaveShift: 2 }).label)
      .toBe('16-PAD KBD · OCT +2');
  });

  it('pad and KBD handlers print the shared latch notices', () => {
    expect(APP_SOURCE).toMatch(/const latchNotice = buildMs2kPadLatchNotice\(\{/);
    expect(APP_SOURCE).toMatch(/flashMessage\(latchNotice\.line1, latchNotice\.line2, 800\)/);
    expect(APP_SOURCE).toMatch(/flashMessage\(MS2K_LATCH_CLEARED_NOTICE\.line1, MS2K_LATCH_CLEARED_NOTICE\.line2\)/);
  });
});

describe('MS2000 strip pad notices', () => {
  it('mode notice repeats the badge readout and says what the pads do', () => {
    expect(buildMs2kModeNotice({})).toEqual({ line1: 'PROGRAM / EDIT · A.01', line2: 'PADS = PAGE JUMP' });
    expect(buildMs2kModeNotice({ stripSeqMode: 2 })).toEqual({
      line1: 'SEQ 2 EDIT · T1',
      line2: 'PADS = STEPS 01-16',
    });
    expect(buildMs2kModeNotice({ isVocoder: true, stripSeqMode: 1 })).toEqual({
      line1: 'CH LEVEL EDIT · T1',
      line2: 'PADS = CH 01-16',
    });
    expect(buildMs2kModeNotice({ padKbdMode: true })).toEqual({
      line1: '16-PAD KBD · OCT 0',
      line2: 'C4 .. D#5 · SHIFT=LATCH',
    });
    expect(buildMs2kModeNotice({ padKbdMode: true, octaveShift: 1 })).toEqual({
      line1: '16-PAD KBD · OCT +1',
      line2: 'C5 .. D#6 · SHIFT=LATCH',
    });
  });

  it('octave notice reports the 16-pad span', () => {
    expect(buildMs2kOctaveNotice({ octaveShift: 0 })).toEqual({ line1: 'PAD KBD OCTAVE', line2: 'C4 .. D#5' });
    expect(buildMs2kOctaveNotice({ octaveShift: 2 })).toEqual({ line1: 'PAD KBD OCTAVE', line2: 'C6 .. D#7' });
    expect(buildMs2kOctaveNotice({ octaveShift: -3 })).toEqual({ line1: 'PAD KBD OCTAVE', line2: 'C1 .. D#2' });
  });

  it('formats pad note labels', () => {
    expect(formatPadMidiNote(60)).toBe('C4');
    expect(formatPadMidiNote(75)).toBe('D#5');
    expect(formatPadMidiNote(61)).toBe('C#4');
    expect(formatPadMidiNote(-10)).toBe('C-1');
    expect(formatPadMidiNote(400)).toBe('G9');
  });

  it('step value notice pads the step number and signs positive values', () => {
    expect(buildMs2kStepValueNotice({ stripSeqMode: 3, padNumber: 4, value: 12 })).toEqual({
      line1: 'SEQ 3 STEP 04',
      line2: 'Value: +12',
    });
    expect(buildMs2kStepValueNotice({ stripSeqMode: 1, padNumber: 16, value: 0 }).line2).toBe('Value: 0');
    expect(buildMs2kStepValueNotice({ stripSeqMode: 2, padNumber: 1, value: -30 }).line2).toBe('Value: -30');
  });

  it('vocoder pan labels', () => {
    expect(formatVocoderPan(0)).toBe('CNT');
    expect(formatVocoderPan(12)).toBe('R12');
    expect(formatVocoderPan(-30)).toBe('L30');
  });

  it('vocoder band level / pan notices', () => {
    expect(buildMs2kBandLevelNotice({ padNumber: 5, level: 127 })).toEqual({
      line1: 'VOCODER CH 05',
      line2: 'Level: 127',
    });
    expect(buildMs2kBandPanNotice({ padNumber: 16, pan: -30 })).toEqual({
      line1: 'VOCODER CH 16',
      line2: 'Panpot: L30',
    });
    expect(buildMs2kBandPanNotice({ padNumber: 1, pan: 0 }).line2).toBe('Panpot: CNT');
  });

  it('pad keyboard note notice and sequence length notice', () => {
    expect(buildMs2kPadNoteNotice({ noteName: 'C4', midiNote: 60 })).toEqual({
      line1: 'PAD KEYBOARD',
      line2: 'Note: C4 (MIDI 60)',
    });
    expect(buildMs2kSeqLengthNotice({ steps: 12 })).toEqual({
      line1: 'SEQ LAST STEP',
      line2: 'LENGTH: 12 STEPS',
    });
  });

  it('app.js prints these notices through the shared builders', () => {
    expect(APP_SOURCE).toMatch(/const notice = buildMs2kOctaveNotice\(\{ octaveShift: ms2kPadOctaveShift \}\);/);
    expect(APP_SOURCE).toMatch(/const stepNotice = buildMs2kStepValueNotice\(\{/);
    expect(APP_SOURCE).toMatch(/const levelNotice = buildMs2kBandLevelNotice\(\{/);
    expect(APP_SOURCE).toMatch(/const panNotice = buildMs2kBandPanNotice\(\{/);
    expect(APP_SOURCE).toMatch(/const noteNotice = buildMs2kPadNoteNotice\(\{/);
    expect(APP_SOURCE).toMatch(/const lengthNotice = buildMs2kSeqLengthNotice\(\{/);
    // No hand-composed strip notice strings left in app.js
    expect(APP_SOURCE).not.toContain("'PADS = PAGE JUMP'");
    expect(APP_SOURCE).not.toContain("'PADS = STEPS 01-16'");
    expect(APP_SOURCE).not.toContain("'PAD KBD OCTAVE'");
    expect(APP_SOURCE).not.toContain("'PAD KEYBOARD'");
    expect(APP_SOURCE).not.toContain("'SEQ LAST STEP'");
    expect(APP_SOURCE).not.toContain('`VOCODER CH ');
    expect(APP_SOURCE).not.toContain('Panpot: ');
    expect(APP_SOURCE).not.toContain('LENGTH: ');
  });
});
