/**
 * ABDMS2000 — embedded Bank Manager (WebUI/abdbank) end-to-end import of a
 * REAL single-program MS2000 dump (297 bytes on the wire).
 *
 * The wire format is NOT multiple-of-8: a 254-byte program travels as
 * F0 42 3n 58 40 + 291 payload bytes (36 full groups + 1 control + 2 data) + F7.
 * A decoder that required a multiple of 8 dropped the last 2 bytes of every
 * program. The embedded copy must import the frame through the exact entry
 * point the embedded UI calls (importFile), with the patch name (0x00, 12
 * chars) intact and every byte preserved — including the partial group.
 */

import { describe, it, expect } from 'vitest';
import { importFile } from '../../abdbank/src/core/importEngine.js';
import { getModelContract } from '../../abdbank/src/contracts/modelContracts.js';

const NAME = 'BRASS LEAD';
const PATCH_SIZE = 254;
const PAYLOAD_SIZE = 291;   // 36 * 8 + 3
const FRAME_SIZE = 297;     // 5 header + 291 payload + 1 F7

/** Real-looking 254-byte program: name at 0x00, and the LAST two bytes with
 *  the MSB set — the exact bytes a strict decoder used to lose. */
function makeProgram() {
  const data = new Uint8Array(PATCH_SIZE);
  for (let i = 0; i < PATCH_SIZE; i++) data[i] = (i * 37 + 13) & 0xFF;

  data.set(new TextEncoder().encode(NAME.padEnd(12, ' ')), 0x00);

  data[PATCH_SIZE - 2] = 0xFF; // byte 252: travels in the partial group
  data[PATCH_SIZE - 1] = 0xA5; // byte 253: travels in the partial group
  return data;
}

/** F0 42 30 58 40 [no-pad packed program] F7 — what a real MS2000 sends. */
function makeRealSingleDump(program) {
  // Korg-order no-pad pack, identical to the C++ packer on the wire.
  const packed = [];
  for (let i = 0; i < program.length; i += 7) {
    const count = Math.min(7, program.length - i);
    let control = 0;
    for (let j = 0; j < count; j++) {
      if ((program[i + j] & 0x80) !== 0) control |= 1 << (6 - j);
    }
    packed.push(control);
    for (let j = 0; j < count; j++) packed.push(program[i + j] & 0x7F);
  }
  expect(packed.length).toBe(PAYLOAD_SIZE);
  return new Uint8Array([0xF0, 0x42, 0x30, 0x58, 0x40, ...packed, 0xF7]);
}

function fileLike(bytes, name) {
  const buffer = bytes.buffer.slice(bytes.byteOffset, bytes.byteOffset + bytes.byteLength);
  return { name, arrayBuffer: async () => buffer };
}

describe('embedded Bank Manager — real 297 B MS2000 dump end to end', () => {
  it('imports the frame as one korg-ms2000 patch with its name and every byte intact', async () => {
    const program = makeProgram();
    const frame = makeRealSingleDump(program);

    expect(frame.length).toBe(FRAME_SIZE);
    expect(frame[1]).toBe(0x42); // Korg
    expect(frame[3]).toBe(0x58); // MS2000
    expect(frame[4]).toBe(0x40); // Program Data Dump
    expect(frame[FRAME_SIZE - 1]).toBe(0xF7);

    const result = await importFile(fileLike(frame, 'MS2000_INIT.syx'));
    expect(result.success).toBe(true);
    expect(result.warnings ?? []).toEqual([]);

    expect(result.bank.modelId).toBe('korg-ms2000');
    expect(result.patches).toHaveLength(1);

    const patch = result.patches[0];
    expect(patch.rawData.length).toBe(PATCH_SIZE);
    expect(patch.name).toBe(NAME); // from 0x00, space-padded, trimmed

    // Byte-exact, INCLUDING the last 2 bytes of the partial group
    for (let i = 0; i < PATCH_SIZE; i++) expect(patch.rawData[i]).toBe(program[i]);
    expect(patch.rawData[252]).toBe(0xFF);
    expect(patch.rawData[253]).toBe(0xA5);
  });

  it('re-exports through the embedded contract to a byte-identical frame', async () => {
    const program = makeProgram();
    const frame = makeRealSingleDump(program);

    const first = await importFile(fileLike(frame, 'MS2000_INIT.syx'));
    expect(first.success).toBe(true);

    const contract = getModelContract('korg-ms2000');
    const rebuilt = contract.buildPatchSysEx(first.patches[0].rawData, 0, 0);

    expect(rebuilt.length).toBe(FRAME_SIZE);
    for (let i = 0; i < FRAME_SIZE; i++) expect(rebuilt[i]).toBe(frame[i]);
  });
});
