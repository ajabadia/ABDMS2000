# Task Plan: Render factory presets from the MS2000 emulator + DSP parity ladder

## Goal
Render the eight selected factory programs through the real MS2000 emulator at 48 kHz with C4/velocity 100, validate the audio, and run ABDMS2000's existing A/B comparator only if the reference captures are valid. Phase 2 (completed): isolate per-subsystem DSP gaps with an 8-rung diagnostic ladder comparing ABDMS2000 against the firmware emulator.

## Next Step
Inspect the emulator/DSP LFO phase accumulator and verify the non-monotonic rate estimates at the high-rate endpoints; the ABD rate agrees with the ROM at all seven sampled knobs. Supporting results and estimator limitations are in `ladder/FINDINGS.md` and `ladder/lfo_rate_sweep/rate_results.csv`.

Phase 1 artifacts complete; phase 2 ladder rendered and compared 8/8; cutoff law read from DSP and applied; follow-ups completed for FEG, LFO patch depth/sign, delay tail and EG2 timing. Other open work: locate the delay-time ROM table (docs cite `0x28356`) and confirm EG1→cutoff span with a short-span rung.

## Current Phase
Phase 5 (complete) + Ladder complete

## Phases

### Phase 1: Requirements & Discovery
- [x] Read the existing plan and verification requirements
- [x] Locate emulator, ROMs, VST3 host, and A/B comparator interfaces
- **Status:** complete

### Phase 2: Planning & Structure
- [x] Choose isolated temp copies to protect original ROM/NVRAM
- [x] Identify host SMF-injection interface and expected 24k-frame binary format
- **Status:** complete

### Phase 3: Implementation
- [x] Reuse the headless MS2000 emulator with isolated ROM/NVRAM copies and render all eight program changes
- **Status:** complete

### Phase 4: Testing & Verification
- [x] Validate audio, crop note-on captures to 24,000 frames and convert to comparator format
- [x] Run the existing A/B comparator only on validated captures
- **Status:** complete (project rebuild blocked; see test results)

### Phase 5: Delivery
- [x] Inspect output metrics and report artifacts, limitations, and any changes
- **Status:** complete

### Phase 6: Ladder isolation (2026-10-22)
- [x] Unlock program injection into the running voice (edit-buffer SysEx dump, Korg byte order)
- [x] Generate 8 isolation rungs + patched ABD execs + isolated emu homes
- [x] Render all 8 rungs and run the A/B comparator on each
- [x] Write per-rung conclusions and prioritized DSP gaps (`ladder/FINDINGS.md`)
- **Status:** complete

### Phase 7: Multi-knob LFO rate sweep (2026-10-23)
- [x] Render paired emulator/ABD captures at seven LFO1 Speed values
- [x] Validate ROM rates, estimator self-test and document the observed curve plus uncertainty
- **Status:** complete; follow-up is DSP accumulator inspection, not applying an unverified rate multiplier

## Decisions Made
| Decision | Rationale |
|----------|-----------|
| Use the existing built headless emulator's native time-scheduled MIDI path rather than rebuilding or editing emulator sources | It accepts timestamps at firmware time and produces WAV directly |
| Isolate ROM/NVRAM in a temporary machine folder | Preserve original ROM inputs; only an isolated temporary user bank is seeded from PRELOAD |
| Use existing comparator executable after independently validating its eight input files | The source rebuild is blocked by missing VS v145 props; existing comparator has the desired interface | 
| Render outputs into a fresh temporary machine folder | The emulator changes process working directory and may write NVRAM; source ROMs must remain untouched |

## Errors Encountered
| Error | Resolution |
|-------|------------|
| Initial MSBuild invocation consumed shell-style switches from MSBuild.rsp | Reran with `-noAutoResponse`; host test compiled successfully |
| Early MIDI messages were lost while SCI1 receiver was disabled | Scheduled Program Change/Note On at 4.0/4.2 seconds after firmware setup |
| Original USER slots are not all factory programs | Exact ROM audit found 100/128 same-index USER/PRELOAD matches; the eight captured indices are from that set and were rerendered from original USER bytes |
| First probe WAV was all zero | Waited through boot and sent verified MIDI Note On after the receiver became active |
| CalibrationTests project rebuild failed due to missing Visual Studio v145 component props | Used existing compiled `build/Release/CalibrationTests.exe`; documented rebuild failure |
