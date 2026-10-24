# Progress Log

## Session: 2026-10-22

### Current Status
- **Phase:** 5 - Delivery
- **Started:** 2026-10-22

### Actions Taken
- Read emulator/core paths, the existing `CalibrationTests.cpp` comparator and local verification skill.
- Checked Git status; branch `main` has many pre-existing unrelated modifications. No application source files were edited for this task.
- The earlier built VST3 host probe was silent; investigation showed firmware receiver (`SCI1 RE`) was disabled at early MIDI times. Used the built headless emulator's native time-scheduled `MS2K_MIDIIN` path, with Program Change at 4.0s, Note On C4 velocity 100 at 4.2s, and Note Off at 4.7s.
- After the user pointed out USER programs modified by the emulator owner, audited all 128 USER payloads against PRELOAD and ABDMS2000's factory table byte-for-byte. Confirmed 100 same-index matches and 28 changed owner USER slots.
- Re-rendered all eight comparison inputs from isolated copies of the original ROM **without modifying the USER bank**, then recropped audio and reran the existing A/B comparator. All chosen indices 2, 5, 9, 16, 20, 22, 23 and 24 are exact 254-byte USER/PRELOAD/factory matches.
- Isolated all writable test material under `/tmp/abdms2k-render-final/home`. Copied source `flash.bin`, boot ROM, DSP ROM, and NVRAM there. Initial capture attempt seeded USER slots from PRELOAD only in a disposable copy; after the corrected exact-match audit, those outputs were discarded from comparison and replaced with rerenders using the original USER bank.
- Rendered 6-second 48 kHz WAVs for indexes 2, 5, 9, 16, 20, 22, 23 and 24. Each had 239,902 frames after the emulator removed initial DSP silence and contained audible non-zero samples. Preserved these WAVs in `captures/`.
- Cropped each capture at the first sample above -60 dBFS to 24,000 stereo frames and wrote the comparator's LE uint32 + planar float32 format. Confirmed all eight emulator and eight ABD outputs are finite, non-silent, and exactly 192,004 bytes.
- Ran the existing comparator from an isolated scratch folder. Spectral similarities: A03 91.3%, A06 89.8%, A10 96.3%, B01 81.3%, B05 91.2%, B07 99.1%, B08 92.7%, B09 86.7%. Full output and input/output captures are saved in `captures/`.

## Session continuation: ladder isolation (2026-10-22, afternoon)

- Confirmed the reliable injection channel: **Korg-order Program Data Dump SysEx to the edit buffer** (`F0 42 30 58 40 [291 packed bytes] F7`) via the emulator's native `MS2K_MIDIIN`. Flash-sector patching does NOT reach a running voice; the edit-buffer dump does (R5 pilot rung rendered the program's audio, 86.8% similarity).
- Built `ladder/ladder_build.py`: 8 diagnostic programs (R1 OSC, R2 ENV, R3 FIL, R4 FEG, R5 LFO, R6 LAYER, R7 MFX, R8 DELAY) derived from the exact INIT template, plus patched ABD-side executables (`CalibrationTests_diag_rN.exe`, all 256 bank slots = rung) and isolated per-rung emu homes.
- `ladder/ladder_render.py` orchestrates per rung: seed ROM USER#2, boot headless emulator, inject dump + PC + C4/100, capture WAV, run ABD comparator. Full ladder rendered 8/8 (16:34–16:47), logs in `ladder/compare_r{1..8}.log`, WAVs in `ladder/wave_r{1..8}.wav`.
- Results and prioritized DSP gaps written to `ladder/FINDINGS.md` and summarized in root `findings.md`.
- Verified source ROM hashes after rendering match the hashes recorded before the run. Verified only the disposable flash's user-program range differs from source and that range exactly equals the original preload region; boot and DSP ROM copies have matching SHA-256.
- Housekeeping: the emulator's stdout logs (`ladder/emu_r{1..8}.log`) had grown to ~2.2 GB each (~17 GB total). Trimmed to the last 400 lines each and extracted the MIDI reception lines to `ladder/emu_midi_r{1..8}.log`; the ladder directory is now 37 MB.

## Session continuation: calibración del cutoff (2026-10-22, noche)

- Se montó una medición aislada del filtro: `cutoff/abd_filter_harness.cpp` (solo `MultiModeFilter` + tabla ROM, sin JUCE) contra capturas del emulador con sierra C2 a knobs 20/40/60/80/100/127.
- El ajuste armónico daba un factor óptimo de 3.6x (+1.85 octavas, residuo 2.0 dB contra 11.2 dB sin corrección) y se implementó en el motor.
- El A/B de los 8 presets de fábrica rechazó el factor: media 91.0% -> 86.0%, con B01 81.3% -> 54.5% y A10 96.3% -> 78.8%. Se revirtió el motor a la lectura ROM directa (`getHardwareFilterCutoffHz`), se eliminaron la constante, el helper y su test.
- Se identificó el sesgo del ajuste: la fuente de ruido del emulador cae ~10 dB entre 6 y 12 kHz, y a knob 127 el emu es 8-10 dB más oscuro que el ABD. A knob 40, en cambio, el pico de resonancia del emu está ~1 octava por encima del ABD. El error cambia de signo con el knob: no admite un factor escalar.
- El probe `Cal 8.2` (KeyTrack/EG1) usaba una base de cutoff que, con la curva ROM cruda, dejaba el cierre de 1 octava por debajo del fundamental de C5, así que el centroide se saturaba en el armónico 1 y el cierre no era medible. Se separó la base del probe: KeyTrack a ~2.2 kHz (knob 0.70), EG1 se queda en ~500 Hz (knob 0.50). No se relajó ningún umbral.
- Verificación final: suite completa 568/568 y A/B idéntico al baseline (91.3/89.8/96.3/81.3/91.2/99.1/92.7/86.7).

## Session continuation: rungs nuevos, cola de delay y EG (2026-10-23)

- `ladder/probe_build.py` genera los rungs nuevos (`r4b`, `r5p8`, `r5m32`, `r5p63`, `r5k`) con su exe ABD parcheado y su home aislado; `ladder/probe_render.py` renderiza y compara, con modo `--tail` (216 000 frames) y note-off a los 24 000 frames; `ladder/delay_tail.py` analiza la cola. El comparador acepta un frame opcional de note-off (sin argumento, comportamiento idéntico al anterior).
- **R4b (FEG rediseñado)**: sim 99.2%; el rung separa FEG (ya no es idéntico a R1). El emu abre mucho más en el ataque (centroide 7916 Hz → 1135 Hz en ~170 ms) que el ABD (656 → 364 Hz).
- **LFO→Pitch (r5p8/r5m32/r5p63/r5k)**: se midió la frecuencia instantánea con autocorrelación. +8: ±0.2 st ambos. -32: ±4.9 st ambos. +63: ±24 st ambos. **Escala y signo correctos**; el 86.8% original viene de la fase: con key sync el emu arranca en +4.4 st bajando y el ABD en ~0 subiendo, y el semiciclo dura ~171 ms (emu) contra ~320 ms (ABD) para LFO knob 48.
- **R2 (ENV)**: ambos llegan a la meseta en 20-30 ms; no hay desajuste de tiempos de EG. Cociente meseta/pico 0.52 (emu) vs 0.88 (ABD), pero el ABD está cerca de fondo de escala por el offset de ~23 dB, así que su cociente está comprimido. Queda como tema de nivel de sustain + calibración de nivel.
- **R8 (DELAY)**: captura de 4.5 s con note-off a 0.5 s. El emu decae de forma continua (-7 dB a 0.55 s, -57 dB a 1.5 s); el ABD da ecos discretos cada ~0.79 s (-4/-8/-12 dB) con silencio digital entre ellos. Control con R1 (delay apagado): silencio tras el note-off, así que los bursts son del delay. El ABD mapea knob 40 a 786 ms por la curva pow del registro; el emu se comporta como un delay mucho más corto y modulado.
- Verificación: suite completa 568/568 (exit 0) y A/B idéntico al baseline (91.3/89.8/96.3/81.3/91.2/99.1/92.7/86.7).

## Session continuation: barrido multi-knob de velocidad LFO (2026-10-23)

- Añadidos `ladder/lfo_rate_sweep.py` y `ladder/lfo_rate_analyze.py` para barrer siete valores de LFO1 Speed usando el rung R5K, capturas pareadas emu/ABD y autocorrelación de f0. El script conserva homes/emulador aislados, parchea solo el byte Speed y limita stdout del emulador a DEVNULL (el tracing podía crear archivos de varios GB).
- Render/captura final: knobs 32/48/64/80/96/112/127, C4, key-sync Voice, profundidad +32, 15 s por bin desde onset; comparador A/B generado para cada knob. Resultados en `ladder/lfo_rate_sweep/rate_results.csv`.
- El ABD sigue `kRomLfo1IncrementQ23` en 7/7 (diferencia máxima <0.2%). Rates emu medidos: 0.834/2.748/4.999/12.971/32.686/18.117/12.038 Hz; ROM: 0.195/0.790/2.403/5.802/11.982/22.161/36.610 Hz. La divergencia no es un multiplicador: cambia de signo y el emu deja de ser monótono en los extremos.
- Estimador: autocorrelación parabólica, ventana de 1024 muestras, salto 120; autoprueba sintética 3.200 Hz → 3.1973 Hz. Limitación: knob 32 ABD solo aporta 2.90 ciclos y la prominencia llega al piso/techo numérico 160 dB; los valores extremos requieren verificación directa del acumulador/fase DSP y posible aliasing antes de concluir el mapeo del emulador. No modificar la curva ABD, ya que coincide con ROM.
- Incidente de ejecución: el primer barrido expiró a 600 s mientras el emulador generaba 4.5 GB de tracing; se aisló/eliminó solo ese log parcial y se corrigió el script para descartar stdout. La nota se alargó para subir la resolución del punto lento. Sin cambios en código de producto.
- Verificación específica: `py_compile` de ambos scripts y `lfo_rate_analyze.py --self-test` pasan; barrido final (7×2 capturas) termina con exit 0.

## Session continuation: ley de cutoff resuelta y aplicada (2026-10-23)

- Volcado de la X: del DSP (`cutoff/dsp_coeff_probe.py`, `dspdump_k*.txt`): el word de cutoff que recibe el DSP es `kRomMasterPitchQ23[128 + knob]` **exacto** en 7 knobs, en las 4 voces e independiente de la resonancia. La tabla que leía el motor (`kRomFilterCutoffQ23[47+knob]`, lineal a 20 kHz) no es la del VCF: de ahí el error con cambio de signo (−8.8 semitonos en los medios, +20.5 en los altos).
- Ancla en Hz por autoscilación del filtro en el emulador (`cutoff/selfosc.py` + `selfosc_spectrum.py`, reso 127, nota liberada): 207.5 / 2090 / 9348 / 13696 / 18186 Hz en los knobs 20/60/90/100/127, cada uno un pico aislado.
- Ajuste (`cutoff/law_fit.py`): **`c = min(2w, 0x770000/2^23)`, `fc = (48000/π)·asin(c)`**, error ≤ 0.28 % en los cinco knobs (las leyes `atan`/`tan` fallan hasta 26 % arriba). El techo `0x770000` del microcódigo aparece solo, en el knob 127; se alcanza en el knob 109 → meseta de 18,236 Hz.
- La ley no depende de la resonancia (`selfosc_reso.py`): a knob 60 el anillo es 2090 Hz con reso 127 y 2093 Hz con reso 100.
- Aplicado en `getHardwareFilterCutoffHz` + LUT compartida (`ABDShared/.../lut_korg_ms2000_dsp56362_vcf.h`, columna `mu` regenerada) + tests. El harness del filtro anilla ahora a ≤0.27 % del emulador (antes 41.4/417.1/4204.5/20000 Hz en 20/60/100/127).
- Validación: rung R3 (filtro aislado, reso 100) 33.4% → **76.0 %**; A/B de los 8 presets 91.0% → 86.4 %, con la caída concentrada en A10 (96.3→78.9) y B01 (81.3→57.3), los dos presets con cutoff base 0 y EG1 Int alto (el barrido de EG1 ya no está tapado por un cutoff base artificialmente bajo). Suite 570/570, calibración 32/32.
- Docs corregidos: `MS2000_FIRMWARE_ROM_MAP.md`, `MS2000_Firmware_DSP_Reference.md`, `firmware_disassembly/01`, `04`. Detalle completo en `cutoff/FINDINGS.md`.

### Test Results
| Test | Expected | Actual | Status |
|------|----------|--------|--------|
| Ladder isolation (8 rungs, emu vs ABD) | Each rung changes exactly one subsystem on both sides | R1/R2/R3/R5/R6/R7/R8 react per rung; R4 (FEG) identical to R1 on both sides (rung design flaw, +32 over already-saturated cutoff) | PARTIAL (7/8 conclusive) |
| Ladder program injection | Audio must equal the injected program | Edit-buffer dump changes timbre, note-off envelope and md5 vs the previously selected program (R5 pilot 86.8%) | PASS |
| Isolated emulator boot and MIDI acceptance | Firmware starts, receives PC + C4/100 after SCI1 is enabled | `[MIDI-IN]` logs confirm all bytes received; non-zero audio in all eight WAVs | PASS |
| Reference file validation | Eight 24,000-frame finite, audible float32 stereo inputs | All eight satisfy format, exact size 192,004 bytes, finite samples and peak > -60 dBFS | PASS |
| Existing A/B comparator | Metrics for all eight actual matching factory programs | Eight presets analyzed and `abd_preset_*.bin` outputs generated | PASS (comparison executed; metrics are not assertion thresholds) |
| ROM isolation | No source ROM writes | Original source hashes unchanged; temp flash user slots match original factory preload | PASS |
| Rebuild current CalibrationTests project | Current sources compile into comparator executable | Attempted MSBuild; blocked because VS v145 `Microsoft.VCToolsVersion.VC.14.51.props` is missing. Existing built comparator executable was used. | BLOCKED |

### Errors / Limitations
| Issue | Resolution / status |
|-------|------------|
| Early MIDI probe happened before firmware SCI1 receiver was enabled and was lost | Scheduled all requested MIDI after 4.0s boot time; emulator logs show receipt |
| PC selects user memory rather than factory preload directly | Seeded only the isolated copy's writable USER bank with exact factory bytes |
| Existing comparator is diagnostic, not pass/fail | Report similarities and metrics without claiming parity thresholds passed |
| ABD float processing produced peaks above 0 dBFS on several presets | Preserved and reported as diagnostic output; no limiting or normalization added |
| Current CalibrationTests rebuild cannot resolve missing VS component props | Use existing binary for execution and report rebuild blocker |
| Ladder R4 (FEG) not separable: EG1 +32 on an already-open cutoff moves nothing on either side | Rung must be redesigned (lower base cutoff, wider EG1 depth) before FEG can be measured |
| A single cutoff scale factor (3.6x) fitted acoustically | Rejected by the 8-preset A/B (mean 91.0% -> 86.0%) and by the knob-dependent sign of the error; engine left on the raw ROM table |
| R5 read as a patch-matrix sign/scale error | Re-measured with instantaneous f0 at +8/-32/+63 and with key sync: depth and sign match on both sides; the gap is the LFO rate/phase |
| R2 read as an envelope timing error | Both sides reach the sustain plateau in 20-30 ms; residual is sustain level, contaminated by the ~23 dB level offset |
| R8 read as a feedback/envelope difference | Tail structure differs: ABD = discrete echoes every 0.79 s with digital silence between; emu = continuous decay. Delay time curve + diffusion are the suspects |
| Constant ~23 dB level offset between emu and ABD captures | Emu reference-chain gain; reported as pipeline calibration issue, not a timbre difference in R1 |
