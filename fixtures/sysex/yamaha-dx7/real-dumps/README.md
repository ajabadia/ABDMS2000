# DX7 Real SysEx Dumps

## Overview

This directory contains real SysEx dumps from DX7-compatible hardware and ROM dumps.

## M-Wave FM-1

The M-Vave FM-1 is a compact DX7-compatible synthesizer (~$70) with:
- DX7 FM engine (6 operators, 32 algorithms)
- 128 patches (4 banks × 32 voices)
- Effects (reverb, delay, phaser, chorus, distortion)
- Step sequencer
- USB MIDI + 5-pin MIDI (via TRS)

### MIDI Capabilities

| Feature | Supported | Notes |
|---|---|---|
| Receive DX7 SysEx (bulk) | ✅ | Standard DX7 format (F0 43 ...) |
| Receive DX7 SysEx (single) | ✅ | Firmware v14+ |
| Receive single parameter SysEx | ✅ | Firmware v14+ (Dexed compatible) |
| **Send DX7 SysEx (bulk)** | ❌ | **Not supported** |
| **Send DX7 SysEx (single)** | ❌ | **Not supported** |

### Limitation: No Bulk Dump Output

The FM-1 cannot export its patches via SysEx. This is a hardware/firmware limitation — the device only supports receiving SysEx, not sending it.

**Workaround:** Use the factory ROM dumps (rom1a-4b.syx) as reference. The FM-1 ships with these as its default sound set.

### Firmware Versions

| Version | Date | Features |
|---|---|---|
| v9 | Initial | Basic DX7 receive |
| v14 | Jul 2026 | Single parameter SysEx, velocity adjust |
| v15 | Aug 2026 | Latest (as of Aug 2026) |

### How to Transfer Patches

1. **Via Dexed:** Connect FM-1 via USB, use Dexed to send patches/banks
2. **Via Benny Sparra Librarian:** Browser-based tool at [fm1-dx7-patch-importer](https://github.com/benny-sparra/fm1-dx7-patch-importer)
3. **Via our WebUI:** Connect FM-1, select DX7 model, use "Send" to transfer patches

### Factory Presets

The FM-1 ships with DX7 factory ROM presets (ROM 1A, 1B, 2A, 2B). These are included in this directory as reference dumps.

## ROM Dumps

| File | Description | Source |
|---|---|---|
| rom1a.syx | DX7 Factory Bank 1A (32 voices) | dxsyx/rogerallen |
| rom1b.syx | DX7 Factory Bank 1B (32 voices) | User dump |
| rom2a.syx | DX7 Factory Bank 2A (32 voices) | User dump |
| rom2b.syx | DX7 Factory Bank 2B (32 voices) | User dump |
| rom3a.syx | DX7 Factory Bank 3A (32 voices) | User dump |
| rom3b.syx | DX7 Factory Bank 3B (32 voices) | User dump |
| rom4a.syx | DX7 Factory Bank 4A (32 voices) | User dump |
| rom4b.syx | DX7 Factory Bank 4B (32 voices) | User dump |

## VRC Dumps (Voice Resource Cards)

Voice Resource Cards were expansion memory for the DX7. 24 dumps included (VRC101-112, A/B banks).

## Community Dumps

3 community-contributed dumps (files: 2.syx, 5.syx, 7.syx).

## Verification

All 35 dumps verified:
- ✅ Correct SysEx header (F0 43 ...)
- ✅ Valid checksum
- ✅ 32 patches per dump
- ✅ Names decodable (ASCII at VMEM offset 118-127)
- ✅ Compatible with project's DX7 contract
