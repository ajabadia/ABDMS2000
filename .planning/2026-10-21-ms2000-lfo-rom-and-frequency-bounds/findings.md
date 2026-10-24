# Findings & Decisions

## Requirements
- Reproducibly compare all 128 values in both LFO increment LUTs against the official Korg v1.07 ROM bytes.
- Audit frequency upper bounds for manual/base values, tempo sync, LFO1 ModSeq, LFO2 Virtual Patch/ModSeq and WASM ModSeq.
- Preserve the established 20 Hz default LFO range and 500 Hz hardware-mode ceiling; do not infer undocumented physical behavior beyond the known ROM/software paths.

## Research Findings
- `DWGSBinary::x811v107_sys` is already embedded as a 256 KiB binary in the C++ test link. ROM words at `0x295E8` and `0x29BE8` are big-endian 32-bit containers with the 24-bit DSP increment in the high 24 bits; expected value is `word >> 8`.
- Tables in `Source/Core/MS2000FirmwareTables.h` are `uint32_t` increments (e.g. knob 114 is `0x207A`, ROM bytes are `00 20 7A 00`).
- Existing base conversion in `SynthEngine::readTimbreParams` maps knob bytes via these tables. `Voice::applyBlockParams` sets both oscillators to hardware mode, whose existing max is 500 Hz. LFO2 per-sample Virtual Patch and ModSeq are applied together in `Voice::renderNextSample`, then clamped by LFO::setFrequencyHz.
- LFO1 ModSeq is applied in SynthEngine and separately in WasmBridge with max 20,000 Hz, but those are intermediate frequency values; the actual `Voice` LFO is clamped to 500 Hz. Add checks for actual output/limit, and align needless intermediate clamp to the canonical 500 Hz where safe, so UI/native/WASM do not imply >500 Hz capability.
- In `ABDShared/Code/SynthCore/LFO.cpp` 20 and 500 are duplicated in `setFrequencyHz`, `setTempoSync`, `setBpm`, and FM `getNextSample`; comment incorrectly cites ~260 Hz. `ABDShared` is a separate sibling repository with extensive unrelated dirty changes, so avoid editing that repository. Instead define constants in ABDMS2000’s LFO compatibility shim or local hardware constants and use a narrow wrapper only if it can preserve public API; otherwise test shared module behavior and report the required shared-code correction explicitly.

## Technical Decisions
| Decision | Rationale |
|----------|-----------|
| Test exact embedded ROM data, not a disk path | Runs consistently on developer systems and validates the shipped firmware artifact |
| Use test-only helper in `DSPCoreTests.cpp` for 2 ROM ranges | Existing test binary links generated `DWGSBinary`; no new target/dependency needed |
| Clamp any app-side ModSeq intermediate to shared `kLfoHardwareMaxHz = 500` | Prevent misleading values beyond final oscillator range, keeping native/WASM semantics consistent |
| Do not modify the sibling ABDShared checkout unless unavoidable | It has many unrelated dirty files and is not the project's current repo; preserve user changes |

## Issues Encountered
| Issue | Resolution |
|-------|------------|
| `code_search` intermittently cannot launch packaged ripgrep | Shell grep/python and file readers worked |
| `Source/Core/Constants.h` path does not exist | The actual project limits live in `Source/Core/HardwareConstants.h` and sibling SynthCore implementation |

## Resources
- Official system ROM: `DOCS/firmware/x811v107.sys` (embedded by CMake as `DWGSBinary::x811v107_sys`)
- Runtime lookups: `Source/Core/MS2000FirmwareTables.h`
- Native voice routing: `Source/Core/SynthEngine.cpp`, `Source/Core/Voice.cpp`
- Shared LFO implementation: `../ABDShared/Code/SynthCore/LFO.cpp` (dirty sibling repo; read-only for this task)
- WASM modulation path: `Source/Wasm/WasmBridge.cpp`
