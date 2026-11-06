import { describe, it, expect } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  buildEditTimbreNotice,
  buildEngineProfileNotice,
  buildTimbreRoutingNotice,
  buildVoiceAssignNotice,
} from '../../src/contracts/lcdNotices.js';

const readSource = (rel) => readFileSync(fileURLToPath(new URL(rel, import.meta.url)), 'utf8');
const APP_SOURCE = readSource('../../src/app.js');
const LCD_SOURCE = readSource('../../src/ui/lcdProgrammer.js');
// buildWriteCompletedNotice se promovio al contrato compartido de la suite; sus
// valores se prueban en ABDShared/tests/lcdNotices.test.js.
const LOCAL_CONTRACT_SOURCE = readSource('../../src/contracts/lcdNotices.js');

describe('LCD notices outside the strip', () => {
  it('EDIT TIMBRE uses one wording from app and LcdProgrammer', () => {
    expect(buildEditTimbreNotice({ timbre: 1 })).toEqual({
      line1: 'EDIT TIMBRE 1',
      line2: 'MAIN TIMBRE 1',
    });
    expect(buildEditTimbreNotice({ timbre: 2 })).toEqual({
      line1: 'EDIT TIMBRE 2',
      line2: 'SPLIT/LAYER T2',
    });
    // Anything that is not timbre 2 falls back to timbre 1
    expect(buildEditTimbreNotice({ timbre: 7 }).line1).toBe('EDIT TIMBRE 1');
    expect(buildEditTimbreNotice().line1).toBe('EDIT TIMBRE 1');
  });

  it('engine profile notices follow the synth mode index', () => {
    expect(buildEngineProfileNotice({ modeIndex: 0 })).toEqual({
      line1: 'MODE: KORG MS2000',
      line2: '4V 16B-VOC MSEQ',
    });
    expect(buildEngineProfileNotice({ modeIndex: 1 })).toEqual({
      line1: 'MODE: microKORG',
      line2: '4V 8CH-VOC 8ARP',
    });
    expect(buildEngineProfileNotice({ modeIndex: 2 })).toEqual({
      line1: 'MODE: ADVANCED',
      line2: '16V 512-DWGS ALL',
    });
    // Out of range clamps instead of returning undefined text
    expect(buildEngineProfileNotice({ modeIndex: 9 }).line1).toBe('MODE: ADVANCED');
    expect(buildEngineProfileNotice({ modeIndex: -4 }).line1).toBe('MODE: KORG MS2000');
  });

  it('timbre copy / swap notices', () => {
    expect(buildTimbreRoutingNotice({ from: 1, to: 2 })).toEqual({
      line1: 'TIMBRE 1 -> 2',
      line2: 'COPY COMPLETED',
    });
    expect(buildTimbreRoutingNotice({ from: 2, to: 1 })).toEqual({
      line1: 'TIMBRE 2 -> 1',
      line2: 'COPY COMPLETED',
    });
    expect(buildTimbreRoutingNotice({ from: 1, to: 2, op: 'swap' })).toEqual({
      line1: 'TIMBRE 1 <-> 2',
      line2: 'SWAP COMPLETED',
    });
  });

  it('voice assign notices use the engine voice count', () => {
    expect(buildVoiceAssignNotice({ mode: 0 })).toEqual({
      line1: 'VOICE ASSIGN',
      line2: 'SINGLE / MONO',
    });
    expect(buildVoiceAssignNotice({ mode: 1, voiceCount: 4 }).line2).toBe('POLY (4 VOICES)');
    expect(buildVoiceAssignNotice({ mode: 1, voiceCount: 16 }).line2).toBe('POLY (16 VOICES)');
    expect(buildVoiceAssignNotice({ mode: 2, voiceCount: 16 }).line2).toBe('UNISON (16 VOICES)');
  });

  it('app.js and LcdProgrammer print these through the shared builders', () => {
    expect(APP_SOURCE).toMatch(/const timbreNotice = buildEditTimbreNotice\(\{ timbre: activeTimbre \}\);/);
    expect(APP_SOURCE).toMatch(/const notice = buildEngineProfileNotice\(\{ modeIndex: modeIdx \}\);/);
    expect(APP_SOURCE).toMatch(/const notice = buildTimbreRoutingNotice\(\{ from: 1, to: 2 \}\);/);
    expect(APP_SOURCE).toMatch(/const notice = buildTimbreRoutingNotice\(\{ from: 1, to: 2, op: 'swap' \}\);/);
    expect(APP_SOURCE).toMatch(/buildVoiceAssignNotice\(\{ mode: 1, voiceCount: engineVoiceCount\(\) \}\)/);
    expect(APP_SOURCE).toMatch(/buildVoiceAssignNotice\(\{ mode: 2, voiceCount: engineVoiceCount\(\) \}\)/);
    expect(APP_SOURCE).toMatch(/const notice = buildWriteCompletedNotice\(\);/);
    // WRITE sale del paquete compartido, no de una copia local
    expect(APP_SOURCE).toMatch(/import \{ buildWriteCompletedNotice \} from '@abdsynths\/shared\/components';/);
    expect(LCD_SOURCE).toMatch(/import \{ buildWriteCompletedNotice \} from '@abdsynths\/shared\/components';/);
    // El contrato local solo deja un puntero al compartido: no hay copia exportada
    expect(LOCAL_CONTRACT_SOURCE).not.toMatch(/export\s+function\s+buildWriteCompletedNotice/);

    expect(LCD_SOURCE).toMatch(/const timbreNotice = buildEditTimbreNotice\(\{ timbre: this.activeTimbre \}\);/);
    expect(LCD_SOURCE).toMatch(/const writeNotice = buildWriteCompletedNotice\(\{ programId: p\.id, programName: p\.name \}\);/);

    // These notices own the LCD while they last, so parameter echoes cannot clobber them
    expect(APP_SOURCE).toMatch(/lcdProgrammer\.flashNotice\(notice\.line1, notice\.line2\);/);
    expect(LCD_SOURCE).toMatch(/flashNotice\(writeNotice\.line1, writeNotice\.line2, 1600\);/);
    expect(LCD_SOURCE).toMatch(/if \(Date\.now\(\) < this\._noticeLockUntil\) return;/);

    // No hand-composed copies left behind
    expect(APP_SOURCE).not.toContain("'TIMBRE 1 -> 2'");
    expect(APP_SOURCE).not.toContain("'TIMBRE 2 -> 1'");
    expect(APP_SOURCE).not.toContain("'TIMBRE 1 <-> 2'");
    // 'VOICE ASSIGN' still labels the drawer control, but no longer a flashMessage
    expect(APP_SOURCE).not.toMatch(/flashMessage\('VOICE ASSIGN'/);
    expect(APP_SOURCE).not.toContain("'PATCH SAVED'");
    expect(APP_SOURCE).not.toContain("'MODE: KORG MS2000'");
    expect(LCD_SOURCE).not.toContain('`EDIT TIMBRE ${this.activeTimbre}`');
    expect(LCD_SOURCE).not.toContain('Split / Layer Sub-Timbre');
    expect(LCD_SOURCE).not.toContain('`SAVED TO ${p.id}');
  });
});
