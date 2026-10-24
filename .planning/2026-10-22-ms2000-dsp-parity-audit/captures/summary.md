# MS2000 DSP A/B: presets con igualdad binaria confirmada

Fecha: 2026-10-22. Emulador headless a 48 kHz; MIDI: C4 (60), velocity 100. La lista del comparador se eligio tras comparar los bytes originales del banco USER del ROM con los bytes PRELOAD, no por nombre ni por programa de usuario supuesto.

## Verificacion de las fuentes

- `Source/State/MS2000FactoryPreloadData.h` coincide exactamente, 254/254 bytes en las 128 posiciones, con el banco PRELOAD del ROM original (`0x0B0000..0x0B7EFF`).
- El banco USER del ROM original (`0x0A0400..0x0A82FF`) tiene **100/128** programas identicos, en la misma posicion, al PRELOAD; **28/128** fueron modificados por el propietario del sintetizador.
- Cada uno de los ocho pares comparados pertenece a las 100 coincidencias exactas. USER, PRELOAD y la entrada de ABDMS2000 coinciden byte a byte para estos indices: **A03 (#02) Evolution, A06 (#05) Zoop Mania, A10 (#09) GatesOfHell, B01 (#16) Lazy Pitch, B05 (#20) Uni Synth, B07 (#22) Deep Bell, B08 (#23) Synth Clav, B09 (#24) Line Bass**.
- El emulador se volvio a ejecutar desde copias aisladas que conservan el USER bank original del ROM. No se reemplazo USER con PRELOAD en estos renders corregidos. El SHA-256 de la flash fuente antes y despues coincide; ver [manifest de ROM](source-rom-sha256.txt) y [auditoria USER/PRELOAD](rom-user-factory-audit.txt).

Los 28 programas USER distintos de PRELOAD son A01 Stab Saw, A02 Synth Lana, A04 Boost Bass, A05 Dirty Sync, A07 Ice Field, A08 Lounge Organ, A09 MG Bass, A11 PWM Strings, A12 Turn Wheel, A13 Synth Tp, A14 DWGS WaveSeq, A15 Drive Bass, A16 Surrounded, B02 Stairs Pad, B03 Silk Pad, B04 Zap Bass, B12 Far Horizon, C05 Healing, C07 Candy Box, C11 Royal Pad, C14 IZDISA-WS, D02 Water Edge, D03 Reactor Pad, D09 Fat Bass, D10 Flashlight, E08 Reed Piano, F05 Killa Lead y F08 Cutting Arpg.

## Capturas y comparacion

Se generaron WAVs completos de 6 s por programa; el comparador utiliza un recorte de 24.000 frames desde el primer sample sobre -60 dBFS. Cada `.bin` usa uint32 LE con el numero de frames y luego L/R planar float32 LE; mide 192.004 bytes. Los 8 WAV completos, 8 referencias de emulador y 8 renders ABD quedan en esta carpeta. `comparison.log` guarda la salida completa del comparador existente.

| Preset | Similitud espectral | RMS emulador | Pico emulador |
|---|---:|---:|---:|
| A03 Evolution | 91,3 % | -37,9 dB | -25,3 dB |
| A06 Zoop Mania | 89,8 % | -12,8 dB | -5,1 dB |
| A10 GatesOfHell | 96,3 % | -25,7 dB | -11,4 dB |
| B01 Lazy Pitch | 81,3 % | -32,3 dB | -11,9 dB |
| B05 Uni Synth | 91,2 % | -23,6 dB | -8,4 dB |
| B07 Deep Bell | 99,1 % | -22,6 dB | -15,3 dB |
| B08 Synth Clav | 92,7 % | -25,5 dB | -10,8 dB |
| B09 Line Bass | 86,7 % | -16,3 dB | -8,8 dB |

La similitud es un indicador descriptivo; el comparador no define un umbral pass/fail. Algunas salidas de ABD exceden 0 dBFS en float, y se conservaron sin normalizacion. Intentar reconstruir `CalibrationTests.vcxproj` fallo por falta de `Microsoft.VCToolsVersion.VC.14.51.props`; la comparacion se hizo con el ejecutable existente `build/Release/CalibrationTests.exe`.
