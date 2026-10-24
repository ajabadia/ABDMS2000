# Hallazgos de la escalera de aislamiento (2026-10-22)

## Contexto y método

- 8 programas de diagnóstico (254 B) generados desde `init_base.bin` (plantilla INIT exacta de `MS2000HardwareProgram.h`), cada uno aísla **un** subsistema (OSC / ENV / FIL / FEG / LFO / LAYER / MFX / DELAY).
- Lado emu: el rung se inyecta por **Program Data Dump SysEx orden-Korg al edit buffer** (`F0 42 30 58 40 [291 B] F7`, vía `MS2K_MIDIIN`), desbloqueado en la fase previa. PC#2 + Note On C4/100 en t≈4.2 s, Note Off ~0.5 s.
- Lado ABD: `CalibrationTests_diag_rN.exe` con las 128 posiciones de ambos bancos parcheadas con el rung (cualquier índice seleccionado es el rung).
- Comparador: `CalibrationTests` con crops de 24 000 frames desde onset; WAVs en `ladder/wave_r{1..8}.wav`, logs en `ladder/compare_r{1..8}.log`.
- Scripts: `ladder/ladder_build.py` (genera rungs + exe parcheado + ROMs aislados), `ladder/ladder_render.py` (orquesta emu → wave → compare).

## Advertencia clave sobre la métrica

El comparador **no normaliza nivel**: el lado emu sale ~23 dB más bajo de forma sistemática (RMS -29..-41 vs -1..-11 en ABD). Ese offset constante es un tema de *escala de referencia / headroom* del pipeline del emu, **no** una diferencia de timbre. La "Similitud Espectral" se ve distorsionada por esto en los rungs con envolvente (el emu, más bajo, entra/sale de la ventana de medición distinto). Interpretar similitud **conjuntamente con bandas/centroides**, no sola.

## Resultados por rung

| Rung | Escenario | Sim. | Emu RMS/Centroid | ABD RMS/Centroid | Diagnóstico |
|---|---|---:|---|---|---|
| R1 | OSC (saw, filtro abierto, amp plano) | 100.0% | -30.2 / 327 Hz | -6.9 / 337 Hz | **TIMBRE IGUAL**. Diferencia de banda baja 0.1% y centroid 10 Hz = ruido de medición. La forma espectral coincide. |
| R2 | ENV (EG2 amp con attack/decay/sustain) | 100.0% | -37.5, slope **-1.4 dB** | -10.8, slope **-0.1 dB** | El emu muestra la envolvente decayendo dentro de la ventana; el ABD la muestra casi plana. La escala temporal / curva de EG2 difiere (decay 10 emu ≈ 0.5 s vs plana en ABD). |
| R3 | FIL (cutoff 40 + reso 100) | **33.4%** | 520 Hz, 1.7/98.3 LM | 225 Hz, **33.4/66.6 B** | **EL GAP REAL #1.** El filtro resonante del emu se comporta como LPF clásico (energía en LM, auténtico peak de resonancia); el ABD desplaza el espectro a banda baja. Con cutoff bajo, el filtro del ABD no reproduce el carácter resonante del hardware. Medido después: a knob 40 el pico de resonancia del emu está en ~523 Hz y el del ABD en ~262 Hz (~1 octava), pero a knob 127 el emu queda ~8-10 dB más oscuro que el ABD: el error cambia de signo, no es un factor constante. Ver `../cutoff/FINDINGS.md`. |
| R4 | FEG (EG1 → cutoff +32) | 100.0% | = R1 | = R1 | **RUNG MAL DISEÑADO, no concluyente**: EG1 int=+32 sobre un cutoff ya saturado a 127 no mueve el filtro en ninguno de los dos lados. Rediseñar: cutoff base medio (~80) + EG1 int negativo o decay largo para ver barrido. |
| R5 | LFO (LFO1 → Pitch, +32) | **86.8%** | 318 Hz, 1.6% trig | 401 Hz, **31.5% bass** | **EL GAP REAL #2.** El emu tira el tono hacia abajo (mucho bass en el vibrato); el ABD lo lleva arriba (más LM/HM). La escala y signo del patch se midieron después y coinciden; el principal diferencial es rate/fase. |
| R6 | LAYER (T2 pan Derecha, T1 pan Izquierda) | 100.0% | Width **50.8%** | Width **0.9%** | El emu hace stereo spread real al panner los dos timbres; el ABD apenas esparce (pendiente de panning de cada timbre casi idéntica). El "Spread" / panning del Layer no funciona igual. |
| R7 | MFX (chorus depth 80) | 99.8% | slope **+5.8 dB**, width 66.1% | slope -0.2 dB, width 70.1% | El chorus del emu **pumpa** (el poking del delay modulado produce cambio de nivel visible); el chorus del ABD es plano. Depth/LFO interno del chorus distinto. Width incrementa igual en ambos (correcto). |
| R8 | DELAY (depth 80, time 40) | 99.8% | slope **+3.1 dB**, width 0 | slope -0.0 dB, width 0 | El delay del emu acumula energía envolvente (+3.1) dentro de la ventana; el del ABD se mantiene plano en la misma ventana. Delay de menor feedback o feed más corto en ABD. Width 0 en ambos (ambos delays mono/center absorbidos = ok). |

## Conclusión: prioridad de paridad para ABDMS2000

1. **Filtro resonante con cutoff bajo (R3, 33.4%)** — el gap más tangible y audible. El error de cutoff cambia de signo con el knob, así que no hay factor escalar: curva dependiente del knob y carácter del resonador. La ley de cutoff ya se midió del DSP y se aplicó; el detalle/impacto están en `../cutoff/FINDINGS.md`.
2. **Velocidad/fase del LFO (R5)** — el patch pitch tiene profundidad/signo correctos, pero la medición multi-knob encontró una divergencia no monótona en el emulador. ABD coincide con su tabla ROM, mientras el emu mide muy por encima en knobs bajos/medios y por debajo al final de escala.
3. **Curva de tiempo del delay y difusión (R8)** — ABD da ecos discretos a 0.79 s sin smear; el emu, cola continua. Localizar la tabla citada `0x28356` y comprender la modulación antes de cambiar la curva.
4. **Barrido EG1→cutoff (R4b)** — el ABD abre menos que el emu en el ataque (centroide 656 vs 7916 Hz en el onset); confirmar con un rung de span corto.
5. **Spread de Layer (R6)** — panning de timbres apenas se nota en ABD vs 50% en emu.
6. **Nivel de sustain / calibración de nivel (R2)** — tiempos EG coinciden (meseta en 20-30 ms); queda nivel de sustain y offset ~23 dB entre pipelines.

## Continuación 2026-10-23: rungs nuevos, cola de delay y EG

Scripts nuevos: `probe_build.py` (rungs `r4b`, `r5p8`, `r5m32`, `r5p63`, `r5k`), `probe_render.py` (render/comparación y modo `--tail`) y `delay_tail.py` (cola post Note Off).

### R4 rediseñado (FEG) — `r4b`

Programa: cutoff base 80, reso 40, EG1 int +32, decay 30, sustain 0 (antes el rung R4 abría +32 sobre un cutoff saturado).

| lado | Sim. | Centroid | Bandas B/LM/HM/H | EnvSlope |
|---|---:|---:|---|---:|
| emu | 99.2% | 385 Hz | 10.4 / 85.3 / 3.4 / 0.8 % | +1.0 dB |
| ABD | — | 306 Hz | 11.1 / 87.8 / 1.1 / 0.0 % | -0.4 dB |

El rung separa FEG de R1. En ventanas de 21 ms el emu abre en ataque (centroide 7916 Hz, bajando a 1135 Hz en ~170 ms) frente a ABD (656 → 364 Hz). El centroide de sierra no es proporcional al cutoff, pero la diferencia de apertura es clara.

### LFO → Pitch: profundidad/signo coinciden; rate ABD/ROM vs emulador difiere

La frecuencia instantánea se estimó de f0 por autocorrelación con interpolación parabólica; el pitch se convierte a cents y un pico espectral estima el LFO. Rungs `r5p8`, `r5m32`, `r5p63`, `r5k`: +8 da ±0.2 st en ambos; -32 recorre -4.9..+4.8 st en emu y -4.9..-0.2 en ABD; +63 llega a ±24 st en ambos. La matriz patch `kRomPatchPitchQ23` no invierte signo ni escala.

Con key sync, el emu arranca en +4.4 st bajando, ABD en ~0 subiendo. El barrido de speed está en `lfo_rate_sweep/rate_results.csv`; usa el mismo rung de key-sync, siete knobs y captura de 15 s desde onset. ROM Hz = `kRomLfo1IncrementQ23[knob] * 48000 / 2^24`.

| knob | ROM/ABD Hz | emu Hz | emu/ROM |
|---:|---:|---:|---:|
| 32 | 0.1946 | 0.8335 | 4.28x |
| 48 | 0.7896 | 2.7476 | 3.48x |
| 64 | 2.4033 | 4.9985 | 2.08x |
| 80 | 5.8022 | 12.9708 | 2.24x |
| 96 | 11.9820 | 32.6862 | 2.73x |
| 112 | 22.1615 | 18.1172 | 0.82x |
| 127 | 36.6096 | 12.0384 | 0.33x |

ABD concuerda con la ROM en los siete puntos (error máximo <0.2%). El emulador no sigue un factor constante ni una curva monótona con el knob: el error cambia de signo. No hay fundamento para cambiar la curva ABD usando un multiplicador global; la siguiente investigación adecuada es inspeccionar el acumulador/fase del LFO DSP y confirmar los posibles artefactos de estimación/aliasing en rates altos.

Limitaciones: el estimador sintético inyectado a 3.200 Hz midió 3.1973 Hz. La captura almacenada es de 15 s; knob 32 ABD solo contiene 2.90 ciclos, el punto de menor resolución. La prominencia del pico se satura numéricamente a 160 dB y no representa incertidumbre. A rates altos el pitch-span medido se comprime por aliasing/clipping de la excursión, así que no expresa profundidad real.

### R2 (ENV): el problema no es la escala temporal

Envolvente RMS en ventanas de 5 ms en `cmp_r2`: ambos lados llegan a meseta en ~20-30 ms. Cociente meseta/pico: emu 0.52 (-5.7 dB), ABD 0.88 (-1.1 dB); el dato ABD puede estar comprimido por la diferencia de nivel ~23 dB y saturación. R2 no muestra un gap de tiempo EG; queda nivel sustain/calibración.

### R8 (DELAY): la cola post note-off no se parece

Captura de 4.5 s con Note Off a 0.5 s (`cmp_r8t`, time 40/depth 80).

| lado | cola tras el Note Off |
|---|---|
| emu | decaimiento continuo y monótono: -7 dB a 0.55 s, -13 a 0.65, -27 a 0.95, -57 a 1.5 s |
| ABD | ecos discretos cada ~0.79 s (0.75, 1.55, 2.35 s), silencio digital entre ellos (-4/-8/-12 dB por repetición) |

El control R1 queda en silencio tras Note Off: los bursts vienen del delay. ABD mapea knob 40 a 786 ms (`delayTime`, pow(t,0.5) en 1..1400 ms); el emu muestra una cola continua más corta/modulada. Los docs citan tabla ROM en `0x28356`, aún por localizar. El delay ABD tampoco reproduce el smear del emu.

## Siguientes pasos propuestos (revisados)

1. ~~Rediseñar R4 FEG~~ — hecho (`r4b`), el barrido de EG1 del ABD es más corto.
2. ~~Curva de cutoff por knob~~ — la ley ya fue leída del DSP y corregida; ver `../cutoff/FINDINGS.md` para impacto en R3 y presets.
3. ~~LFO1→Pitch con varias intensidades y signos~~ — profundidad/signo coinciden. El rate ABD sigue ROM, pero el emu diverge de manera no monótona; inspeccionar fase/contador del DSP y verificar alias en los extremos.
4. ~~Cola delay post Note Off~~ — diferencia confirmada; falta localizar tabla de tiempo y entender difusión/modulación antes de corregirla.

## Artefactos de medición LFO

- CSV: `ladder/lfo_rate_sweep/rate_results.csv` (7 knob values, dos lados).
- Capturas `.bin` de 15 s, WAVs, logs de comparador y script: `ladder/lfo_rate_sweep/` y `ladder/lfo_rate_sweep.py`.
- El emulador genera gigabytes de stdout de tracing en headless; el script lo descarta. El note-on se mantiene hasta 22.5 s para ampliar la ventana útil; los archivos bin contienen 15 s desde el onset.

## Artefactos

- Rungs: `ladder/rung_r{1..8}.bin` (254 B, desde init_base.bin).
- Exe ABD: `build/Release/CalibrationTests_diag_r{1..8}.exe` (128 slots × 2 bancos por rung).
- Homes aislados emu: `ladder/emu_home_r{1..8}/`.
- WAVs: `ladder/wave_r{1..8}.wav`; comparadores: `ladder/compare_r{1..8}.log`; orquestación: `ladder/ladder_full.log`.
- Los stdout del emulador (`emu_r{1..8}.log`) llegaron a ~2.2 GB cada uno (~17 GB); se recortaron y se separaron las líneas MIDI a `emu_midi_r{1..8}.log`.