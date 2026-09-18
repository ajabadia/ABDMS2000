/**
 * ABDMS2000 — embedded Bank Manager (WebUI/abdbank) end-to-end import of the
 * plugin's OWN preset format: F0 7D 0A 40 + 439 payload bytes + F7 (444 B),
 * carrying the native 384-byte program block (v2).
 *
 * The ABDSynths codec is NOT Korg's: the control byte carries the MSB of data
 * byte `i` in bit `i` (Korg uses bit `6-j`), and the last partial group is not
 * padded (384 = 54×7 + 6 → 54×8 + 1 + 6 = 439). A frame packed in the Korg
 * order still decodes to 384 bytes but to the WRONG ones, so this test pins the
 * byte `i` order and the partial group's last two bytes.
 *
 * Spec: DOCS/ABDSynths_SysEx_Spec.md (§2.2 tamaños, §4 despacho, §5 import).
 * Sibling test: abdbankMs2000SingleDumpE2E.test.js (the real 297 B Korg frame).
 */

import { describe, it, expect } from 'vitest';
import { importFile } from '../../abdbank/src/core/importEngine.js';
import { getModelContract } from '../../abdbank/src/contracts/modelContracts.js';

const NAME = 'ABD NATIVE';
const PROGRAM_SIZE = 384;
const PAYLOAD_SIZE = 439;   // 54 * 8 + 7
const FRAME_SIZE = 444;     // 4 header + 439 payload + 1 F7

/** 384-byte native block: name at 0x00 and the LAST two bytes with the MSB set
 *  — the bytes a padded or Korg-ordered codec would corrupt. */
function makeNativeProgram() {
  const data = new Uint8Array(PROGRAM_SIZE);
  for (let i = 0; i < PROGRAM_SIZE; i++) data[i] = (i * 53 + 7) & 0xFF;

  data.set(new TextEncoder().encode(NAME.padEnd(12, ' ')), 0x00);

  data[PROGRAM_SIZE - 2] = 0xB7; // travelled in the partial 6-byte group
  data[PROGRAM_SIZE - 1] = 0xE3;
  return data;
}

/** F0 7D 0A 40 [ABDSynths packed block] F7 — control bit `i`, no padding. */
function makeOwnFormatFrame(program) {
  const packed = [];
  for (let i = 0; i < program.length; i += 7) {
    const count = Math.min(7, program.length - i);
    let control = 0;
    for (let j = 0; j < count; j++) {
      if ((program[i + j] & 0x80) !== 0) control |= 1 << j; // bit i, not 6-j
    }
    packed.push(control);
    for (let j = 0; j < count; j++) packed.push(program[i + j] & 0x7F);
  }
  expect(packed.length).toBe(PAYLOAD_SIZE);
  return new Uint8Array([0xF0, 0x7D, 0x0A, 0x40, ...packed, 0xF7]);
}

function fileLike(bytes, name) {
  const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
  return { name, arrayBuffer: async () => buffer };
}

describe('embedded Bank Manager — 444 B ABDSynths own-format dump end to end', () => {
  it('imports F0 7D 0A 40 as one abd-sm002 patch with its name and every byte intact', async () => {
    const program = makeNativeProgram();
    const frame = makeOwnFormatFrame(program);

    expect(frame.length).toBe(FRAME_SIZE);
    expect(frame[1]).toBe(0x7D); // ABDSynths (non-commercial MIDI 1.0 id)
    expect(frame[2]).toBe(0x0A); // ABD MS2000
    expect(frame[3]).toBe(0x40); // Program Data Dump
    expect(frame[FRAME_SIZE - 1]).toBe(0xF7);

    const result = await importFile(fileLike(frame, 'ABD_NATIVE.syx'));
    expect(result.success).toBe(true);
    expect(result.bank.modelId).toBe('abd-sm002'); // NOT korg-ms2000
    expect(result.patches).toHaveLength(1);

    const patch = result.patches[0];
    expect(patch.rawData.length).toBe(PROGRAM_SIZE);
    expect(patch.name).toBe(NAME); // from 0x00, space-padded, trimmed

    // Byte-exact, INCLUDING the last 2 bytes of the partial group
    for (let i = 0; i < PROGRAM_SIZE; i++) expect(patch.rawData[i]).toBe(program[i]);
    expect(patch.rawData[PROGRAM_SIZE - 2]).toBe(0xB7);
    expect(patch.rawData[PROGRAM_SIZE - 1]).toBe(0xE3);
  });

  it('re-exports through the abd-sm002 contract to a byte-identical 444 B frame', async () => {
    const program = makeNativeProgram();
    const frame = makeOwnFormatFrame(program);

    const first = await importFile(fileLike(frame, 'ABD_NATIVE.syx'));
    expect(first.success).toBe(true);

    const contract = getModelContract('abd-sm002');
    const rebuilt = contract.buildPatchSysEx(first.patches[0].rawData, 0, 1);

    expect(rebuilt.length).toBe(FRAME_SIZE);
    for (let i = 0; i < FRAME_SIZE; i++) expect(rebuilt[i]).toBe(frame[i]);
  });
});
