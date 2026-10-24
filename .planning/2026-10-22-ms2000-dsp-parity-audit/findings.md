# Hallazgos: coincidencia USER/PRELOAD y comparación A/B

## Verificación byte a byte del ROM original
- El payload USER ocupa `0x0A0400..0x0A82FF`; PRELOAD ocupa `0x0B0000..0x0B7EFF`. Cada posición contiene 254 bytes.
- Los 128 payloads de `Source/State/MS2000FactoryPreloadData.h` coinciden exactamente con los payloads PRELOAD del `flash.bin` original.
- En el USER bank original, 100/128 programas coinciden exactamente con el payload PRELOAD de la misma posición; 28/128 difieren porque el dueño anterior los modificó.
- Los ocho pares usados para A/B —A03 (#02), A06 (#05), A10 (#09), B01 (#16), B05 (#20), B07 (#22), B08 (#23) y B09 (#24)— fueron comprobados como igualdad exacta de 254/254 bytes entre USER original, PRELOAD y la entrada de la factory bank de ABDMS2000.
- El inventario completo de los 28 slots USER distintos está en `captures/rom-user-factory-audit.txt`.

## Captura y emulación
- Se usó `ms2000_emulator.exe` headless, con las ROMs en una carpeta temporal aislada. En las capturas corregidas no se sobrescribió el USER bank: cada preset se seleccionó por su MIDI program index normal sobre el USER bank original.
- MIDI: PC en t=4.0 s; Note On C4/velocidad 100 en t=4.2 s; Note Off en t=4.7 s. La recepción de SCI1 fue confirmada por logs.
- Se conservaron WAV completos a 48 kHz/32-bit stereo y se generaron crops de 24.000 frames desde el onset sobre -60 dBFS para el comparador.
- Los source ROM hashes before/after concuerdan para flash; boot ROM y DSP ROM fueron copiados y usados sin modificación. El manifiesto está en `captures/source-rom-sha256.txt`.

## Resultados A/B
- Similitud espectral: A03 91.3%, A06 89.8%, A10 96.3%, B01 81.3%, B05 91.2%, B07 99.1%, B08 92.7%, B09 86.7%.
- Valores diagnósticos, no test de pass/fail; no hay umbrales declarados. Algunas salidas float ABD exceden 0 dBFS y permanecen sin normalizar.
- Intentar reconstruir el comparador con `CalibrationTests.vcxproj` fue bloqueado por falta de Visual Studio v145 `Microsoft.VCToolsVersion.VC.14.51.props`; se ejecutó el comparador desde el binario existente.
- Informe completo: `captures/summary.md`; transcript: `captures/comparison.log`.

## Escalera de aislamiento (fase 2)

- Inyección de programa **por SysEx Program Data Dump orden-Korg al edit buffer** (`F0 42 30 58 40 …`) confirmada como el canal fiable del emulador; el parcheo del flash no se propaga a la voz en marcha, el dump al edit buffer sí (audio que ES el programa).
- Escalera de 8 rungs de aislamiento (OSC/ENV/FIL/FEG/LFO/LAYER/MFX/DELAY) renderizada 8/8 y comparada contra ABD; resultados completos en `ladder/FINDINGS.md`.
- Gaps reales detectados (por prioridad): ~~**(1) filtro resonante con cutoff bajo** (R3, sim 33.4%)~~ -> resuelto el 2026-10-23 (R3 76.0%), **(2) velocidad/fase del LFO** (el patch LFO→Pitch tiene escala y signo correctos: ±4.9 st a +32 y ±24 st a +63 en ambos lados; a knob 48 el emu va ~2.9 Hz y el ABD 0.79 Hz), **(3) curva de tiempo y difusión del delay** (ABD: ecos discretos a 0.79 s con silencio entre ellos; emu: cola continua), **(4) barrido EG1→cutoff corto en ABD** (R4b: centroide 656→364 Hz vs 7916→1135 Hz del emu), **(5) spread de layer** (R6 width 50.8% vs 0.9%).
- Corrección a R2: no es un desajuste de la escala temporal de EG — ambos llegan a la meseta en 20-30 ms. Lo que difiere es el nivel de sustain, y la medición del ABD está contaminada por el offset de nivel (pico cerca de fondo de escala).
- R4 (FEG) no concluyente: rung mal diseñado, EG1 +32 sobre cutoff ya saturado no mueve nada en ningún lado; rediseñar antes de medir.
- Investigación del gap #1 (cutoff del VCF): el ajuste acústico que apuntaba a multiplicar la tabla ROM por 3.6x fue **rechazado** (media 91.0% -> 86.0%). El error cambiaba de signo con el knob porque la tabla era la equivocada.
- **Gap #1 resuelto (2026-10-23).** El firmware no envía el cutoff desde `kRomFilterCutoffQ23` (0x26154): volcando la X: del DSP se ve que `X:0x4B0 = kRomMasterPitchQ23[128 + knob]` exacto en 7 knobs, en las 4 voces y sin depender de la resonancia. La ley del DSP, ajustada contra la autoscilación medida en el emulador (knobs 20/60/90/100/127), es `c = min(2w, 0x770000/2^23)` con `fc = (48000/pi)*asin(c)` — error **<= 0.28 %** en los cinco, y el techo `0x770000` del microcódigo aparece solo en el knob 127 (plano desde el 109, 18,236 Hz). Aplicado en `getHardwareFilterCutoffHz`; el filtro del ABD anilla ahora a **<= 0.27 %** del emulador y el rung R3 pasa de 33.4% a **76.0%**. La media de los 8 presets baja (91.0% -> 86.4%) pero la caída se concentra en A10 y B01, los dos presets con cutoff base 0 y EG1 Int alto, donde el barrido de EG1 (gap #4) queda ahora sin el colchón del cutoff artificialmente bajo. Detalle, artefactos y lo que queda abierto en `cutoff/FINDINGS.md`.
- El offset de nivel emu-vs-ABD es constante ~23 dB (escala de referencia del pipeline del emu); distorsiona la métrica de similitud pero no implica timbre distinto en R1 (bandas y centroid casi idénticos).
