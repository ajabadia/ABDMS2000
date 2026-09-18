#!/usr/bin/env node
/**
 * Generate valid DX7 SysEx fixtures for testing.
 * Run: node fixtures/sysex/yamaha-dx7/generate-fixtures.mjs
 *
 * Produces:
 *   single-voice.syx       — One 128-byte voice
 *   bulk-32voices.syx      — Full 32-voice bank (4096 bytes payload)
 *   e-piano-bank.syx       — 32-voice bank with named patches
 *   multi-voice.syx        — 3 separate single-voice messages
 *
 * The DX7 VCED layout places the patch name at bytes 118–127 (10 bytes ASCII).
 */
import { writeFileSync, mkdirSync } from 'fs';
import { resolve, dirname } from 'path';
import { fileURLToPath } from 'url';

const __dirname = dirname(fileURLToPath(import.meta.url));
const outDir = resolve(__dirname, 'fixtures');
mkdirSync(outDir, { recursive: true });

const DX7_SIZE = 128;
const DX7_CMD = 0x09;
const DX7_SUB = 0x20;

function dx7Checksum(bytes) {
  let sum = 0;
  for (const b of bytes) sum += b;
  return (128 - (sum % 128)) & 0x7F;
}

function buildDx7Voice(rawData, channel = 0) {
  const data = new Uint8Array(DX7_SIZE);
  data.set(rawData.slice(0, DX7_SIZE));
  // Standard DX7 6-byte header: F0 43 gg 09 20 00
  const header = new Uint8Array([0xF0, 0x43, 0x10 | (channel & 0x0F), DX7_CMD, DX7_SUB, 0x00]);
  const payload = new Uint8Array(header.length + DX7_SIZE);
  payload.set(header, 0);
  payload.set(data, header.length);
  // Checksum covers data after the 6-byte header (verified against real ROM dumps)
  const checksum = dx7Checksum(payload.slice(6));
  const result = new Uint8Array(payload.length + 2);
  result.set(payload, 0);
  result[payload.length] = checksum;
  result[payload.length + 1] = 0xF7;
  return result;
}

function buildDx7Bulk(voices, channel = 0) {
  // Standard DX7 6-byte header: F0 43 gg 09 20 00
  const header = new Uint8Array([0xF0, 0x43, 0x10 | (channel & 0x0F), DX7_CMD, DX7_SUB, 0x00]);
  const payload = new Uint8Array(header.length + 32 * DX7_SIZE);
  payload.set(header, 0);
  for (let i = 0; i < 32; i++) {
    const voice = voices[i] || new Uint8Array(DX7_SIZE);
    payload.set(voice.slice(0, DX7_SIZE), header.length + i * DX7_SIZE);
  }
  // Checksum covers data after the 6-byte header
  const checksum = dx7Checksum(payload.slice(6));
  const result = new Uint8Array(payload.length + 2);
  result.set(payload, 0);
  result[payload.length] = checksum;
  result[payload.length + 1] = 0xF7;
  return result;
}

function createVoice(name, algo = 0, lfoSpeed = 50) {
  const data = new Uint8Array(DX7_SIZE);

  // VMEM layout: 6 ops × 17 bytes = 102 bytes, globals at 102-127
  // OP6 parameters (offset 0–16)
  data[0] = 99;   // OP6 EG Rate 1
  data[14] = 80;  // OP6 Output Level (VMEM offset 14)

  // OP1 parameters (offset 85–101)
  data[85 + 14] = 75;   // OP1 Output Level (VMEM offset 99)

  // Global parameters — write BEFORE name (name occupies 118-127)
  data[102] = 50;       // Pitch EG Rate 1 (VMEM offset 102)
  data[110] = algo;     // Algorithm (VMEM offset 110)

  // Patch name at bytes 118–127 (ASCII) — written LAST to avoid overwrites
  for (let i = 0; i < Math.min(name.length, 10); i++) {
    data[118 + i] = name.charCodeAt(i);
  }

  return data;
}

// ─── Generate fixtures ───

// 1. Single voice
const ePianoVoice = createVoice('E.PIANO 1', 5, 45);
const singleVoice = buildDx7Voice(ePianoVoice);
writeFileSync(resolve(outDir, 'single-voice.syx'), singleVoice);
console.log(`✅ single-voice.syx: ${singleVoice.length} bytes`);

// 2. Bulk 32 voices (generic)
const bulkVoices = [];
for (let i = 0; i < 32; i++) {
  bulkVoices.push(createVoice(`PATCH ${String(i + 1).padStart(2, '0')}`, i % 32, 30 + i));
}
const bulk32 = buildDx7Bulk(bulkVoices);
writeFileSync(resolve(outDir, 'bulk-32voices.syx'), bulk32);
console.log(`✅ bulk-32voices.syx: ${bulk32.length} bytes`);

// 3. E.Piano bank (realistic names)
const ePianoNames = [
  'E.PIANO 1', 'E.PIANO 2', 'E.PIANO 3', 'TINE 1',
  'TINE 2', 'WURLI 1', 'WURLI 2', 'CLAV 1',
  'CLAV 2', 'PIANO 1', 'PIANO 2', 'PIANO 3',
  'HRPSCHRD', 'VIBES 1', 'VIBES 2', 'MARIMBA',
  'SYN.BELL', 'TUB BELLS', 'GM BELL', 'TINKLE',
  'FANTASY1', 'FANTASY2', 'CRYSTAL', 'PADS 1',
  'PADS 2', 'STRINGS1', 'STRINGS2', 'BRASS 1',
  'BRASS 2', 'BASS 1', 'BASS 2', 'BASS 3'
];
const ePianoBank = ePianoNames.map((name, i) => createVoice(name, i % 32, 40 + (i % 20)));
const ePianoSyx = buildDx7Bulk(ePianoBank);
writeFileSync(resolve(outDir, 'e-piano-bank.syx'), ePianoSyx);
console.log(`✅ e-piano-bank.syx: ${ePianoSyx.length} bytes`);

// 4. Multi-voice (3 separate single voice messages)
const voice1 = createVoice('BASS 1', 4, 60);
const voice2 = createVoice('LEAD 1', 7, 55);
const voice3 = createVoice('PAD 1', 3, 35);
const sv1 = buildDx7Voice(voice1);
const sv2 = buildDx7Voice(voice2);
const sv3 = buildDx7Voice(voice3);
const multiVoice = new Uint8Array(sv1.length + sv2.length + sv3.length);
multiVoice.set(sv1, 0);
multiVoice.set(sv2, sv1.length);
multiVoice.set(sv3, sv1.length + sv2.length);
writeFileSync(resolve(outDir, 'multi-voice.syx'), multiVoice);
console.log(`✅ multi-voice.syx: ${multiVoice.length} bytes (${sv1.length}×3)`);

console.log(`\nAll fixtures in ${outDir}`);
