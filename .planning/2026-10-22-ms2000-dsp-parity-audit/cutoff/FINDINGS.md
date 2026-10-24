# Calibración del cutoff del VCF contra ms2000r-emu (2026-10-22 / 23)

**Resultado: la ley está resuelta y aplicada.** El firmware no envía el cutoff desde
`kRomFilterCutoffQ23` (0x26154): envía `kRomMasterPitchQ23[128 + knob]` y el DSP56362 lo
convierte en un coeficiente tipo Chamberlin `c = min(2w, 0x770000/2^23)`.
`getHardwareFilterCutoffHz` usa ahora `fc = (48000/π)·asin(c)`.

Error de la ley frente a la autoscilación medida en el emulador: **≤ 0.28 %** en los knobs
20/60/90/100/127. El filtro del ABD, con la ley nueva, anilla a **≤ 0.27 %** de esas mismas
frecuencias (medido con `abd_filter_harness --ir`).

## Cómo se llegó aquí

1. **Volcado de X: del DSP** (`dspdump_kNNN.txt`, `dsp_coeff_probe.py`): el word que el H8S
   deja en `X:0x4B0 + 0x0A·i` (4 voces) para el cutoff es **exactamente**
   `kRomMasterPitchQ23[128 + knob]`, bit a bit, en los 7 knobs medidos (0/20/40/60/80/100/127),
   idéntico en las 4 voces e **independiente de la resonancia** (0x4B1 sí cambia con ella).
   La tabla que el ABD leía (`kRomFilterCutoffQ23[47 + knob]`, escalada lineal a 20 kHz) no es
   la del camino del VCF: por eso el error cambiaba de signo con el knob (de −8.8 semitonos en
   los medios a +20.5 en los altos).
2. **Ancla en Hz por autoscilación** (`selfosc.py`, `selfosc_spectrum.py`): con reso 127 y la
   nota liberada solo puede anillar el filtro. Frecuencias dominantes de la cola:

   | knob | word (kRomMasterPitchQ23[128+k]) | anillo |
   |---:|---:|---:|
   | 20 | 0x00DECF | 207.5 Hz |
   | 60 | 0x08BD2C | 2090 Hz |
   | 90 | 0x24CEE7 | 9348 Hz |
   | 100 | 0x320E47 | 13696 Hz |
   | 127 | 0x4D1EB7 | 18186 Hz |

   En cada uno hay un pico aislado (los vecinos, 30-100 dB abajo) y la fundamental de C4
   (261.6 Hz) aparece aparte: es el filtro, no la nota.
3. **Ajuste de la ley** (`law_fit.py`): con `w = word/2^23`,

   | ley | knob 20 | 60 | 90 | 100 | 127 |
   |---|---:|---:|---:|---:|---:|
   | **A: `asin(min(2w, 0.9297))`** | +0.14 % | +0.13 % | +0.15 % | +0.18 % | +0.28 % |
   | B: `atan(2w)` (TPT con g=2w) | +0.13 % | −0.79 % | −14.7 % | −26.0 % | −26.2 % |
   | C: `2·atan(w)` (g = tan(πf/2fs)) | +0.13 % | −0.33 % | −8.5 % | −16.8 % | −8.9 % |

   El error de A es sistemáticamente +0.15 % (el anillo cae 0.15 % bajo la esquina nominal,
   normal en un resonador poco amortiguado): la ley no tiene sesgo de forma.
   `2w / sin(πf/fs)` = 1.0013 constante en los cuatro primeros knobs, y en el 127 el
   coeficiente implícito es 0.9285 ≈ **`0x770000`/2^23 = 0.9297**, el techo que el microcódigo
   aplica en P:$0760..$0766 (`#>$77` en Q7, documentado en `MS2000_Firmware_DSP_Reference.md`).
   El techo se alcanza en el **knob 109**: de ahí arriba el cutoff es plano en 18,236 Hz.
4. **La ley no depende de la resonancia** (`selfosc_reso.py`): a knob 60, el anillo es 2090 Hz
   con reso 127 y **2093 Hz** con reso 100. A knob 20 con reso 100/60 el filtro ya no
   autooscila (el pico dominante pasa a ser la nota, 261.6 Hz), así que no hay anillo que medir;
   no hay indicio de compensación de cutoff por resonancia.
5. **El ABD con la ley nueva** (`abd_filter_harness --ir`, reso 1.0): anilla en
   207.8 / 2092.7 / 9362.0 / 13721.4 / 18236.4 Hz, es decir +0.16 / +0.13 / +0.15 / +0.18 /
   +0.27 % sobre el emulador. Antes de la corrección el mismo harness daba
   41.4 / 417.1 / — / 4204.5 / 20000 Hz.

## Validación acústica

| prueba | antes | después |
|---|---:|---:|
| Escalera R3 (filtro aislado, cutoff 40, reso 100) | 33.4 % | **76.0 %** |
| R3 bandas B/LM | 33.4/66.6 | 9.4/90.6 (emu 1.7/98.3) |
| R3 centroide | 225 Hz | 327 Hz (emu 520 Hz) |
| 8 presets de fábrica, media | 91.0 % | 86.4 % |

El rung R3 —el único que aísla el filtro— mejora 2.3× y sus bandas se acercan a las del emu.
La media de los 8 presets **baja**, pero la caída se concentra en dos presets cuyo cutoff base
es **0** con EG1 Int alto:

| preset | slot | tipo | cutoff | reso | EG1 int | similitud antes → después |
|---|---:|---:|---:|---:|---:|---:|
| A10 GatesOfHell | 9 | LPF12 | 0 | 97 | 77 | 96.3 % → 78.9 % |
| B01 Lazy Pitch | 16 | HPF12 | 0 | 60 | 118 | 81.3 % → 57.3 % |

En ambos el corte instantáneo lo manda EG1, no el knob: con el cutoff base correcto (65.4 Hz
en vez de 17.7 Hz) el barrido de EG1 arranca 1.9 octavas más arriba y llega antes al techo, así
que el filtro pasa más tiempo abierto que en el emulador. Eso es el gap **EG1→cutoff** ya
identificado en la escalera ("R4b: barrido EG1→cutoff corto en ABD"), que hasta ahora quedaba
enmascarado por un cutoff base artificialmente bajo. El resto de los presets no empeora (A06
+0.2, B05 igual, B07 +0.1, B08 +2.8, B09 +1.6).

Además, el A/B completo no es un juez fino del filtro: el ABD entrega ~23 dB por encima del
emu (A10 con RMS +1.7 dB y pico +7.6 dB), lo que satura el `softClip` interno de los
integradores del SVF y colorea el espectro. En R3 ese offset es de 17 dB y aun así la métrica
responde al cambio de cutoff, que es lo que se buscaba.

## Qué cambió

- `Source/Core/MS2000FirmwareTables.h`: `getHardwareFilterCutoffHz` lee
  `kRomMasterPitchQ23[128 + knob]` y aplica `c = min(2w, 0x770000/2^23)`,
  `fc = (48000/π)·asin(c)`. Se añadió `kHardwareFilterMaxCoefficient` y `kQ23FractionOne`.
- `ABDShared/Code/LutDSP/models/lut_korg_ms2000_dsp56362_vcf.h`: columna `mu` regenerada con
  la ley nueva (8 puntos × 4 tiers; los dos últimos quedan planos en 18,236 Hz). **El generador
  de esa LUT vive en ABDAudioLab: hay que aplicarle la misma fórmula o la próxima regeneración
  revierte el archivo.**
- Tests: `DSPCoreTests.cpp` (valores 65.40 Hz / 18,236 Hz / +1 octava = 1.911× / meseta desde
  el knob 109) y `CalibrationTests.cpp` (base de key-track 0.45 y comentarios; Cal 2.1 y Cal 8.1
  renombrados). Suite completa: **570/570**, calibración: **32/32**.

## Lo que queda abierto

1. **EG1→cutoff**: con el cutoff base correcto, la profundidad/velocidad del barrido de EG1 en
   el ABD ya no está tapada. Es el siguiente gap a medir (rung R4 rediseñado, sin saturar el
   techo).
2. **Offset de nivel de ~23 dB** del ABD frente al emu: contamina el A/B y el `softClip` del
   filtro. Merece su propia entrada.
3. **Topología**: el hardware usa `c = sin(πf/fs)` y el ABD `g = tan(πf/fs)`. Con el cutoff en
   Hz ya igualado, la esquina coincide; la forma cerca de Nyquist (18 kHz) y la profundidad del
   pico siguen siendo distintas. Cambiar el coeficiente a `sin` es una decisión aparte
   (afecta autooscilación y Cal 8.3).

## Artefactos

- Ley y ajuste: `law_fit.py`, `curve_compare.py`/`.csv`/`.txt`, `dsp_coeff_probe.py`,
  `dspdump_k*.txt`, `coeff_writes_k*.txt`.
- Autoscilación: `selfosc.py`, `selfosc_spectrum.py`, `selfosc_analyze.py`, `selfosc_reso.py`,
  `selfosc_k*.wav`, `resosc_k*_r*.wav`.
- Filtro del ABD: `abd_filter_harness.cpp`, `abd_ring_analyze.py`, `abd_ring/`, `abd_ring2/`.
- A/B: `ab_asin/`, `ab_asin.log`, `../ladder/compare_r3_asin.log`, `ab_final.log` (baseline),
  `ab_after_cutoff.log` (intento 3.6×, rechazado).
- Suite: `tests_asin2.log` (calibración), `tests_asin_full2.log` (570/570).
