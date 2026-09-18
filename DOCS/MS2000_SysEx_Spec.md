# Korg MS2000 — Especificación de Mensajes SysEx y Offsets Hexadecimales

Este documento es la referencia canónica del **formato del equipo real** para ABDMS2000.
Sustituye al mapa anterior, que describía los parámetros del *motor* del plugin
(entonces un bloque de 128 B; hoy 384 B, ver §5) como si fueran los del MS2000. El mapa de aquí está contrastado con un dump real
(patch `INIT Program`) y con el panel ReMS2000 (`DOCS/ReMS2000-main/ReMS2000.panel`,
usado por la comunidad contra hardware de verdad).

Implementación: `Source/MIDI/MS2000HardwareProgram.h` (modelo + mapeo al motor) y
`Source/MIDI/SysExManager.cpp` (entrada/salida MIDI).

> [!IMPORTANT]
> Hay **dos formatos** y no se mezclan:
> * **MS2000 real (Korg, 254 B)**: lo que sale y entra por MIDI de un MS2000 físico.
>   Cabecera `F0 42 3n 58`, programa de 254 B.
> * **Preset del plugin (ABDSynths, 384 B)**: el bloque propio del motor, sistema
>   `native` del contrato del host. Cabecera `F0 7D 0A` (fabricante `0x7D`,
>   modelo `0x0A`). **No** se le pone cabecera de Korg: un MS2000 real lo leería mal.
>   Ver `Source/MIDI/ABDSynthsSysEx.h` y §5. **Spec completa del formato propio:**
>   `DOCS/ABDSynths_SysEx_Spec.md`.

## 1. Estructura de Cabecera SysEx (MS2000 real)

```
F0 42 3n 58 [Comando] [Datos...] F7
```

- **`F0`**: Inicio de SysEx (Exclusive Status).
- **`42`**: Korg Manufacturer ID.
- **`3n`**: Device ID y Canal MIDI (donde `n` es el canal de `0x0` a `0xF`, o sea canal MIDI 1 → `0x30`).
  - **La lectura desplazada** (`n = canal − 1`) es la de los dos extremos desde **2026-09-15**: el
    plugin (`MS2000HardwareProgram::korgChannelByte`) y el ABD Bank Manager (`korgChannelByte` en
    `Source/Contracts/Models/korg-ms2000.ts`, compartido con el Prophecy). Antes el Banco emitía
    `0x30 | canal` (canal 1 → `0x31`), **un desplazado** — no coincidía con los 23 de 24 volcados
    reales de fábrica de Korg del repo (`ABDBankManager/fixtures/sysex/`, todos `F0 42 30`) ni podía
    direccionar el canal 16 (`0x30 | 16` se colapsa en `0x30`, porque el bit 4 ya está puesto en la
    base). El único fixture con otro byte (`0x3E`) solo encaja como canal 15.
  - ⚠️ *Pendiente de confirmar con el equipo* (item 4 del `ROADMAP.md`): con el MS2000 en canal 1,
    mandarle `F0 42 30 58 10 F7` y `F0 42 31 58 10 F7` y ver cuál contesta. Si contestara al `0x31`,
    la corrección se revierte en un punto único por lado (`KORG_CHANNEL_ZERO_BASED` en el contrato y
    `korgChannelByte` en el C++) y su test (`WebUI/tests/unit/korgChannelConvention.test.js`) fija el
    comportamiento actual. El C++ ya cubre el eco del byte del emisor para esa misma petición
    (`[Test 22]`: `F0 42 30 58 10 F7` → respuesta en `0x30`), y desde el mismo día hay un E2E que ata
    los dos repos: `[Test 25]` **no escribe la fórmula** — repetirla es justo como se cuela el
    desplazamiento —, sino que consume las tramas que **emite el contrato del Banco**, compiladas a
    `Source/MIDI/KorgChannel.gen.h` por `Scripts/generate_korg_channel.js` (paso 1 de `build.bat`,
    igual que `HostModelId.gen.h`), y comprueba que el plugin reconoce cada una en su canal y
    contesta en el **mismo** byte: los 16 canales y las dos peticiones (`0x10` y `0x0E`). Si algún
    lado se vuelve a desplazar, ese test cae.
- **`58`**: Model ID del MS2000 / MS2000R (el microKORG lo comparte).
- **`[Comando]`**: el tipo de operación.
- **`[Datos]`**: el bloque de información, codificado en 7 bits.
- **`F7`**: Fin de Exclusive.

## 2. Comandos Principales (Sub-status)

| Código Hex | Nombre del Comando | Descripción |
|---|---|---|
| **`41`** | **Parameter Change** | Modifica un solo parámetro en tiempo real: `F0 42 3n 58 41 [Offset Low] [Offset High] [Value] F7`. |
| **`10`** | **Program Data Dump Request** | El software pide al hardware el parche activo. |
| **`40`** | **Program Data Dump** | Volcado de un parche: **254 B** empaquetados. |
| **`0E`** | **All Data Dump Request** | Petición de volcado completo de la memoria. |
| **`4C`** | **All Data Dump** | Volcado de los programas de la memoria (254 B cada uno). |
| **`23`** | **Write Completed** | ACK. |
| **`24`** | **Write Error** | NACK. |

### 2.1 Empaquetado 7→8 (Korg) y tamaños

- Cada grupo de **7 bytes reales** viaja como **1 byte de MSBs + 7 bytes de datos**.
- El **último grupo parcial NO se rellena**: 254 B → **291 B** de payload y una trama de
  **297 bytes** (`F0 42 3n 58 40` + 291 + `F7`).
- Un decodificador que exija payloads múltiplos de 8 (o que descarte el último grupo
  parcial) **pierde los 2 últimos bytes** de cada programa. Tolerante en ambos lados:
  el codec de C++ (`ABDSharedCode/HardwareDrivers/SysExCodec.h`) y el de TS
  (`ABDBankManager/Source/Contracts/SysEx/codec.ts`; el contrato Korg usa
  `pack8to7NoPad`/`unpack7to8Tolerant`, que no descarta el grupo parcial) decodifican el
  grupo parcial desde 2026-09-13.

## 3. Mapa del programa real (254 B, **unpacked**)

Layout global:

| Offset | Bytes | Contenido |
|---|---|---|
| `0x00` | 12 | **Nombre del programa** (ASCII, relleno con espacios). Va aquí, no en `0x1C`. |
| `0x0C` | 4 | Sin uso |
| `0x10` | 1 | Timbre Voice (bits 6,7) + **Voice Mode** (bits 4,5: 0 Single, 1 Split, 2 Layer, **3 Vocoder**) |
| `0x11` | 1 | Scale Key (bits 4..7) + Scale Type (bits 0..3) |
| `0x12` | 1 | Split Point |
| `0x13` | 1 | Delay FX: bit 7 tempo sync, bits 0..3 time base |
| `0x14` | 1 | Delay Time |
| `0x15` | 1 | Delay Depth / Feedback |
| `0x16` | 1 | Delay Type (0 Stereo, 1 Cross, 2 L/R) |
| `0x17` | 1 | Mod FX Speed |
| `0x18` | 1 | Mod FX Depth |
| `0x19` | 1 | Mod FX Type (0 Chorus/Flanger, 1 Ensemble, 2 Phaser) |
| `0x1A` | 1 | EQ High Freq (**0..29**) |
| `0x1B` | 1 | EQ High Gain (**+64**, 64 = 0 dB) |
| `0x1C` | 1 | EQ Low Freq (0..29) |
| `0x1D` | 1 | EQ Low Gain (+64) |
| `0x1E` | 2 | Arp Tempo MSB + LSB (20..300 BPM) |
| `0x20` | 1 | Arp: bit 7 On/Off, bit 6 Latch, bits 4,5 Target, bit 0 Key Sync |
| `0x21` | 1 | Arp: bits 0..3 Type (0 Up … 5 Trigger), bits 4..7 Range (0..3 = 1..4 octavas) |
| `0x22` | 1 | Arp Gate (0..100 %) |
| `0x23` | 1 | Arp Resolution (0..5 = 1/24, 1/16, 1/12, 1/8, 1/6, 1/4) |
| `0x24` | 1 | Arp Swing (0..127 = 0..+100 %, 128..255 = -100..-1 %) |
| `0x25` | 1 | Sin uso |
| `0x26` | **108** | **TIMBRE 1** |
| `0x92` | **108** | **TIMBRE 2** (mismo layout) |

### 3.1 Bloque de timbre (108 B)

| Byte | Contenido |
|---|---|
| 0 | MIDI channel (-1 = GLOBAL, se lee como `0xFF`) |
| 1 | bits 6,7 Assign (Mono/Poly/Unison) · bit 5 EG2 reset · bit 4 EG1 reset · bit 3 Trigger · bits 0,1 Key Priority |
| 2 | Unison Detune (0..99 cents) |
| 3 | Tune (**+64** = 0 cents) |
| 4 | Bend Range (+64) |
| 5 | Transpose (+64) |
| 6 | Vibrato Intensity (+64) |
| 7 | OSC1 Wave (0 Saw, 1 Pulse, 2 Tri, 3 Sine, 4 Vox, 5 DWGS, 6 Noise, 7 Audio In) |
| 8 | OSC1 Control 1 |
| 9 | OSC1 Control 2 |
| 10 | OSC1 DWGS Wave (0..63, cuando Wave = DWGS) |
| 11 | Sin uso |
| 12 | bits 4,5 OSC2 Mod (0 Off, 1 Ring, 2 Sync, 3 Ring+Sync) · bits 0,1 OSC2 Wave |
| 13 | OSC2 Semitone (+64) |
| 14 | OSC2 Tune (+64) |
| 15 | Portamento Time (bits 0..6) |
| 16 | Mixer OSC1 Level |
| 17 | Mixer OSC2 Level |
| 18 | Mixer Noise Level *(en modo vocoder: HPF Level)* |
| 19 | Filter Type (0 24dB LPF, 1 12dB LPF, 2 12dB BPF, 3 12dB HPF) |
| 20 | Cutoff (0..127) |
| 21 | Resonance (0..127; desde ~105 autoscila) |
| 22 | Filter EG1 Intensity (+64) *(en vocoder: Filter Shift 0..4 = 0, +1, +2, -1, -2)* |
| 23 | Filter Velocity Sense (+64) *(en vocoder: Gate Sense)* |
| 24 | Filter Keyboard Track (+64) *(en vocoder: Threshold)* |
| 25 | Amp Level |
| 26 | Amp Pan (+64 = centro) |
| 27 | bit 6 Amp SW (0 EG2, 1 Gate) · bit 0 Distortion On/Off |
| 28 | Amp Velocity Sense (+64) *(en vocoder: Direct Level)* |
| 29 | Amp Keyboard Track (+64) |
| 30..33 | EG1 Attack, Decay, Sustain, Release |
| 34..37 | EG2 Attack, Decay, Sustain, Release |
| 38 | LFO1: bits 4,5 Key Sync (0 Off, 1 Timbre, 2 Voice) · bits 0,1 Wave |
| 39 | LFO1 Frequency |
| 40 | LFO1: bit 7 Tempo Sync · bits 0..4 Sync Note |
| 41 | LFO2 (igual que 38; onda 2 = Sine en vez de Tri) |
| 42 | LFO2 Frequency |
| 43 | LFO2: bit 7 Tempo Sync · bits 0..4 Sync Note |
| 44 | Virtual Patch 1: bits 4..7 Destination · bits 0..3 Source |
| 45 | Virtual Patch 1 Intensity (+64) |
| 46 / 48 / 50 | Virtual Patch 2 / 3 / 4 (mismo empaquetado) |
| 47 / 49 / 51 | Intensidad de Patch 2 / 3 / 4 (+64) |
| 52 | Mod Seq: bit 7 On/Off · bit 6 Run Mode · bits 0..4 Resolution |
| 53 | Mod Seq: bits 4..7 Last Step (0..15) · bits 2,3 Type · bits 0,1 Key Sync |
| 54 | Seq 1 Knob (destino, 0..30) |
| 55 | Seq 1 Motion (bit 0: 0 Smooth, 1 Step) |
| 56..71 | Seq 1 — 16 pasos (+64) |
| 72..89 | Seq 2 (Knob, Motion, 16 pasos) |
| 90..107 | Seq 3 (Knob, Motion, 16 pasos) |

> [!NOTE]
> **En modo Vocoder** (`0x10` bits 4,5 = 3) los bytes 15..29 del timbre 1 son parámetros
> del vocoder (HPF Level/Gate/Threshold, Filter Shift, Direct Level) y **46..61 / 62..77
> son el nivel y el pan de las 16 bandas**. El mismo bloque, dos lecturas.

### 3.2 Cómo se mapea al motor

- **Los dos timbres son el mismo vocabulario.** El programa guarda dos bloques de 108 B
  idénticos en estructura (`0x26` y `0x92`), así que el motor declara el espejo del Timbre 2
  con el prefijo `t2` (`osc1Wave` → `t2Osc1Wave`) y la **misma tabla** de campos se lee dos
  veces, cambiando solo la base (`108 × índice`). Un `Field` se clasifica como global o de
  timbre con `isTimbreField()`.
- **Mod Sequence = 3 filas por timbre**, con **31 destinos reales**
  (`NONE, PITCH, STEP LENGTH, PORTAMENTO, OSC1 CTRL1, OSC1 CTRL2, OSC2 SEMI, OSC2 TUNE,
  OSC1/OSC2/NOISE LEVEL, CUTOFF, RESONANCE, EG1 INT, KBD TRK, AMP LEVEL, PANPOT,
  EG1/EG2 ATTACK-DECAY-SUSTAIN-RELEASE, LFO1/LFO2 FREQ, PATCH1..4 INT`) en su orden exacto:
  el índice del parámetro `seq{N}Dest` es el byte del equipo. El rango de cada paso depende
  del destino (±6 para `STEP LENGTH`, ±24 para `PITCH`/`OSC2 SEMI`, ±63 para el resto) y se
  guarda como byte `64 ± 63`.
- **El bit de movimiento va invertido**: el equipo guarda `0 = Smooth, 1 = Step` y el motor
  numera `Step = 0, Smooth = 1` (`Xform::SeqMotionInv`). El último paso se guarda como
  `valor − 1` (`Xform::SeqLastStep`) y el par Scale Key/Type, el Timbre Voice y las voces
  comparten byte, así que se escriben por rango de bits.

## 4. Qué hace ABDMS2000 con esto

- **Entrada** (`SysExManager::parseSysEx`): una trama `0x40`/`0x4C` cuyo payload da 254 B
  (o múltiplo) se aplica al motor con `MS2000HardwareProgram::applyToAPVTS()` y sus bytes
  se guardan; el bloque nativo del plugin (384 B, v2; o 128 B, v1) es otra cosa.
- **Salida hacia el equipo** (`createHardwareProgramDump`): parte de los bytes que
  vinieron del hardware y vuelca encima **lo que el motor modela de los dos timbres**
  (osciladores, mezcla, filtro con su velocidad, VCA con la suya, EG, LFO, patch, los dos
  mod sequences, escala/split y el reparto de voces), así que lo que no modela vuelve
  **intacto**.
- **Respuesta al cable** (`SysExParseResult::reply`):
  - una petición `0x10` se contesta con el **programa real** de 254 B del canal pedido
    (`createHardwareProgramDump` → `F0 42 3n 58 40`, 297 B);
  - una petición `0x0E` (memoria completa) se contesta con la memoria entera en formato del
    equipo (`buildHardwareBankDumpResponse` → `F0 42 3n 58 4C ... F7`), de dos fuentes:
    - si el plugin tiene cargado un banco **real** (algún All Data Dump `0x4C` que le llegó del
      equipo), sale ese: plazas tal como vinieron, **solo la activa refrescada desde el motor**
      (lo que se está oyendo viaja, pero editar un programa no reescribe los otros 127);
    - si nunca llegó memoria del equipo, el plugin hace de equipo con **la suya**: convierte sus
      128 presets nativos a programas reales de 254 B (`MS2000HardwareProgram::fromNativeProgram`).
      Es una conversión **aproximada y declarada como tal**: lo que el motor no modela sale con
      los valores del INIT Program del equipo, y falta verificar contra un MS2000 físico que
      acepte el volcado (ver `ABDSynths_SysEx_Spec.md` §6.4 y el roadmap);
  - una escritura `0x40`/`0x4C` recibe su acuse — `F0 42 3n 58 23 F7` si se guardó,
    `... 24 F7` si no — con la cabecera del equipo y el canal del emisor, igual que el MS2000
    físico. Un acuse recibido (`0x23`/`0x24`) se reconoce (`WriteCompleted` / `WriteError`) y
    **no se contesta**.
- **Empaquetado del All Data Dump**: el 7→8 de Korg se aplica al **flujo completo** (los
  `N × 254` B concatenados), no a cada programa por separado — 254 no es múltiplo de 7, así que
  empaquetar cada programa y concatenar deja los bytes de MSBs en otro sitio y el receptor
  descodifica otra cosa. Con los 128 programas del equipo son 32 512 B → 37 157 B de payload y
  **37 163 B** de trama. ⚠️ Es la única afirmación de este documento **sin verificar contra el
  equipo**: hay que grabar un `0x4C` real y comparar (ver el roadmap de ABDMS2000).
- **Lo que el motor no puede aplicar** está declarado en `MS2000HardwareProgram::unmodelled()`
  y se conserva byte a byte; no se finge que se aplicó. Hoy es: MIDI channel por timbre,
  Tune, Bend Range, Transpose y Vibrato; EG1/EG2 Reset, Trigger Mode y Key Priority; el
  Amp SW (EG2/Gate); el swing/target/key sync del arpegiador; el tempo sync y el time base
  del delay; el Mod FX Feedback y los on/off de Mod FX y Delay; el modo 1Shot/Loop del mod
  sequence y su destino `OSC1 CTRL2`; los 30 pasos reales del EQ (el motor tiene 4); la
  afinación de las escalas (se conservan, pero el motor suena en temperamento igual) y el
  4º modo del OSC2 (Ring+Sync ≠ CrossMod del motor).

## 5. Bloque nativo del plugin (sistema `native`)

`MS2000ProgramData` es la memoria del **plugin**, no del equipo: 12 B de nombre, un byte de
voz (`0x0C`) y, desde `0x0E`, **un byte por cada parámetro marcado `sysex != false`** en
`schemas/parameters-spec.schema.v1.json` (hoy 253, el último en `0x10A`).

| Versión | Tamaño | Qué cabe |
|---|---|---|
| v1 | 128 B | Timbre 1 + voz. Los 30 parámetros `sysex: false` (vocoder entero, mod seq, `unisonSpread`, `masterVolume`…) y el Timbre 2 entero no cabían |
| **v2** | **384 B** | Timbre 1 **y 2**, sus dos velocidades, escala/split/reparto de voces y los 3 × 16 pasos de cada secuenciador |

La v2 **empieza igual** que la v1 y es múltiplo de 128, así que un bloque v1 se sigue
leyendo (el resto queda en sus defaults) y la detección de tamaño del `SysExManager` no
cambia. El bloque es **opaco** para el ABD Bank Manager: viaja en base64 y el tamaño no se
declara en el contrato, solo lo conoce el host.
