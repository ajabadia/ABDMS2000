# Task Plan: MS2000 LFO ROM Parity and Frequency Bounds

## Goal
Add reproducible tests that compare the LFO1/LFO2 lookup tables against the embedded Korg v1.07 ROM and verify consistent 500 Hz hardware-mode limits for base and modulated LFO frequencies.

## Next Step
Implement ROM extraction parity checks in the existing firmware parity test, then add tests for the shared hardware LFO frequency limit and clamp modulation identically.

## Current Phase
Phase 2

## Phases

### Phase 1: Requirements & Discovery
- [x] Confirm scope: ROM comparison plus LFO1/LFO2 frequency/modulation limit audit.
- [x] Locate embedded official ROM resource and existing DSP test suite.
- [x] Trace base/manual, tempo-sync, patch, ModSeq and WASM LFO frequency paths.
- **Status:** complete

### Phase 2: Implementation
- [ ] Compare all 128 ROM words at 0x295E8 and 0x29BE8 against their code LUTs in tests.
- [ ] Define one shared 500 Hz hardware-mode LFO limit and use it for manual, tempo-sync, and per-sample frequency modulation.
- [ ] Correct obsolete 260 Hz comment and ensure SynthEngine/WASM ModSeq intermediate limits do not conflict with the final 500 Hz LFO clamp.
- [ ] Add test assertions for LFO1 and LFO2 at 20 Hz legacy max, 500 Hz hardware max, and over-limit modulation.
- **Status:** in_progress

### Phase 3: Verification
- [ ] Build relevant test targets.
- [ ] Run focused firmware parity / LFO tests and full C++ tests where available.
- [ ] Review only owned changes; preserve pre-existing worktree changes.
- **Status:** pending

### Phase 4: Delivery
- [ ] Report verified outcomes, any blocked checks, and modified files.
- **Status:** pending

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Parse the embedded `DWGSBinary::x811v107_sys` from its known ROM offsets in the existing C++ test | Avoid an external ROM path and verify the exact firmware shipped in the project |
| Centralize a 500 Hz hardware-mode ceiling while retaining the 20 Hz legacy ceiling | Both oscillators are hardware-mode enabled in Voice; a shared cap prevents inconsistent final limits |
| Leave documentation under ignored `DOCS/` untouched for this task | This follow-up targets executable verification and implementation audit, while avoiding invisible ignored-file changes |

## Findings
- Official embedded system image is 256 KiB, so both ROM table regions are present.
- Big-endian 32-bit ROM words store the DSP Q23 increment in the upper 24 bits; extract as `word >> 8`.
- LFO1 ROM starts at file offset `0x295E8`, LFO2 at `0x29BE8`; each contains 128 4-byte entries.
- Existing runtime uses a 500 Hz hardware-mode cap in `ABDShared/Code/SynthCore/LFO.cpp`; stale comment says ~260 Hz although ROM max is ~194.904 Hz.
- SynthEngine LFO1 ModSeq temporarily allows up to 20 kHz but Voice's `setFrequencyHz` eventually caps it to 500 Hz; WASM follows the same pattern. LFO2 patch and ModSeq modulation both converge in Voice and are capped there.
- Tempo-sync has separately hard-coded 500/20 Hz caps in the shared LFO implementation; replace these duplicated values with a single constant.

## Errors Encountered
| Error | Resolution |
|-------|------------|
| `code_search` intermittently fails because bundled ripgrep is missing | Use shell `rg`/`grep`/Python and direct file reads; no source change needed |
| Current `build` is configured for Visual Studio 18 2026, not a native Make backend | Use `cmake --build` with MSBuild and run Windows executables via Python subprocess if shell path conversion interferes |
