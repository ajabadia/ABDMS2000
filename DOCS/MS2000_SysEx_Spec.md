# Korg MS2000 - Especificación de Mensajes SysEx y Offsets Hexadecimales

Este documento detalla la estructura y el mapeo de memoria de los mensajes de Sistema Exclusivo (SysEx) del Korg MS2000, fundamental para desarrollar el `MS2000SysExParser` en JUCE.

## 1. Estructura de Cabecera SysEx

Todos los mensajes SysEx del MS2000 comienzan y terminan con la siguiente secuencia de bytes:

```
F0 42 3n 58 [Comando] [Datos...] F7
```

- **`F0`**: Inicio de SysEx (Exclusive Status).
- **`42`**: Korg Manufacturer ID.
- **`3n`**: Device ID y Canal MIDI (donde `n` es el canal de 0x0 a 0xF).
- **`58`**: Model ID del MS2000 / MS2000R.
- **`[Comando]`**: El tipo de operación que se está realizando.
- **`[Datos]`**: El bloque de información (parámetros o volcados), codificado siempre en 7-bits.
- **`F7`**: Fin de SysEx (End Of Exclusive).

## 2. Comandos Principales (Sub-status)

| Código Hex | Nombre del Comando | Descripción |
|---|---|---|
| **`41`** | **Parameter Change** | Modifica el valor de un solo parámetro en tiempo real. Formato: `F0 42 3n 58 41 [Offset Low] [Offset High] [Value] F7`. |
| **`10`** | **Program Data Dump Request** | Petición desde el software para que el hardware envíe el parche activo. |
| **`40`** | **Program Data Dump** | Volcado de un solo parche. Almacena todos los offsets de memoria de forma contigua. Los datos a partir de aquí requieren desempaquetado de 7 a 8 bits. |
| **`0E`** | **All Data Dump Request** | Petición de volcado completo de la memoria. |
| **`4C`** | **All Data Dump** | Volcado de los 128 parches del hardware. |
| **`23`** | **Write Completed** | Confirmación (ACK) de que los datos fueron recibidos y guardados con éxito. |
| **`24`** | **Write Error** | Notificación (NACK) de error al escribir datos en memoria. |

---

## 3. Mapa de Offsets (Timbre 1 / Program Parameters)

Este mapa define la dirección de memoria interna para el **Timbre 1** y los parámetros globales. Cada parámetro acepta valores decimales de 0 a 127 (7 bits). El Timbre 2 tiene una estructura idéntica pero sumando un desplazamiento (offset) base a estos valores.

### 3.1 Parámetros Globales (Voice & Arpeggiator)
| Offset Hex | Parámetro | Rango / Valores (Dec) |
|---|---|---|
| `0x00` | Voice Assign Mode | 0=Mono, 1=Poly, 2=Unison |
| `0x01` | Arpeggiator Switch | 0=Off, 127=On |
| `0x02` | Arp Type | 0=Up, 1=Down, 2=Alt1, 3=Alt2, 4=Random, 5=Trigger |
| `0x03` | Arp Range | 0=1 Octava, 1=2 Octavas, 2=3 Octavas, 3=4 Octavas |
| `0x04` | Arp Gate | 0 a 127 (0% a 100% de duración de paso) |
| `0x08` | Unison Detune | 0 a 99 (cents de desafinación) |

### 3.2 Timbre 1: Osciladores y Mezclador
| Offset Hex | Parámetro | Rango / Valores (Dec) |
|---|---|---|
| `0x0E` | OSC1 Waveform | 0=Saw, 1=Pulse, 2=Tri, 3=Sine, 4=Vox, 5=DWGS, 6=Noise, 7=AudioIn |
| `0x0F` | OSC1 Control 1 | 0 a 127 (Ej: Ancho de pulso o parámetro principal) |
| `0x10` | OSC1 Control 2 | 0 a 127 (Si Wave=DWGS, selecciona tabla de 0 a 63) |
| `0x11` | OSC2 Waveform | 0=Saw, 1=Square, 2=Triangle |
| `0x12` | OSC2 Modulation | 0=Off, 1=Ring, 2=Sync, 3=Cross Mod |
| `0x14` | OSC2 Semitone | -24 a +24 (Offset de semitonos) |
| `0x15` | OSC2 Tune | -50 a +50 (Cents) |
| `0x18` | Mixer: OSC1 Level | 0 a 127 |
| `0x19` | Mixer: OSC2 Level | 0 a 127 |
| `0x1A` | Mixer: Noise Level | 0 a 127 |

### 3.3 Timbre 1: Filtro, Amplificador y Distorsión
| Offset Hex | Parámetro | Rango / Valores (Dec) |
|---|---|---|
| `0x1B` | Filter Type | 0-31=24dB LPF, 32-63=12dB LPF, 64-95=12dB BPF, 96-127=12dB HPF |
| `0x1C` | Cutoff Frequency | 0 a 127 |
| `0x1D` | Resonance | 0 a 127 (Atenúa graves. >105 autooscila) |
| `0x1E` | EG1 Intensity | -63 a +63 (Ruteo directo EG1 -> Cutoff) |
| `0x1F` | Keyboard Track | -63 a +63 |
| `0x20` | Amp Level | 0 a 127 |
| `0x21` | Panpot | 0 a 127 (64 = Centro) |
| `0x22` | Amp Distortion | 0-63=Off, 64-127=On |
| `0x23` | Amp KBD Track | -63 a +63 (Atenuación de volumen por altura de nota) |

### 3.4 Timbre 1: Envolventes (EG) y LFOs
| Offset Hex | Parámetro | Rango / Valores (Dec) |
|---|---|---|
| `0x24` | EG1 Attack | 0 a 127 |
| `0x25` | EG1 Decay | 0 a 127 |
| `0x26` | EG1 Sustain | 0 a 127 |
| `0x27` | EG1 Release | 0 a 127 |
| `0x28` | EG2 Attack | 0 a 127 |
| `0x29` | EG2 Decay | 0 a 127 |
| `0x2A` | EG2 Sustain | 0 a 127 |
| `0x2B` | EG2 Release | 0 a 127 |
| `0x2C` | LFO1 Waveform | 0=Saw, 1=Square, 2=Triangle, 3=S&H |
| `0x2D` | LFO1 Frequency | 0 a 127 (Curva exponencial si Tempo Sync=Off) |
| `0x2E` | LFO1 Key Sync | 0=Off, 127=On, (Ruteos intermedios) |
| `0x2F` | LFO2 Waveform | 0=Saw, 1=Square, 2=Sine, 3=S&H |
| `0x30` | LFO2 Frequency | 0 a 127 |
| `0x31` | LFO2 Key Sync | 0=Off, 127=On |

### 3.5 Timbre 1: Virtual Patch (Matriz de Modulación)
| Offset Hex | Parámetro | Rango / Valores (Dec) |
|---|---|---|
| `0x32` | Patch 1: Source | (EG1, EG2, LFO1, LFO2, Vel, KBD, ModWhl, PitchBend) |
| `0x33` | Patch 1: Destination | (Pitch, Osc2Pitch, Osc1Ctrl1, Cutoff, Amp, Pan) |
| `0x34` | Patch 1: Intensity | -63 a +63 |
| `0x35` a `0x37` | Patch 2 | Source, Dest, Intensity |
| `0x38` a `0x3A` | Patch 3 | Source, Dest, Intensity |
| `0x3B` a `0x3D` | Patch 4 | Source, Dest, Intensity |

### 3.6 Secuenciador de Modulación
Los datos globales de las secuencias se definen por offsets consecutivos.

| Offset Hex | Parámetro | Descripción |
|---|---|---|
| `0x3E` | Mod Seq A: Destination | Selección del parámetro objetivo |
| `0x3F` | Mod Seq A: Mode | 0=Step, 1=Smooth |
| `0x40` | Mod Seq B: Destination | Selección del parámetro objetivo |
| `0x41` | Mod Seq B: Mode | 0=Step, 1=Smooth |
| `0x42` | Mod Seq C: Destination | Selección del parámetro objetivo |
| `0x43` | Mod Seq C: Mode | 0=Step, 1=Smooth |

*Nota sobre datos de secuencia:* Los valores exactos de los 16 pasos para las pistas A, B y C se almacenan en un bloque masivo de datos al final del Program Dump que requiere desempaquetado de bloques de 7-bits a 8-bits. 

---

## 4. Estrategia de Conversión de Datos SysEx (7-bit a 8-bit)

Al recibir un volcado de programa (Program Dump `40`), el MS2000 no envía los datos crudos, sino empaquetados. Como el protocolo MIDI solo permite 7 bits de datos por byte (el 8º bit es de estado), los bytes de 8-bits se dividen:
- Cada grupo de 7 bytes reales de 8-bits se transmite utilizando 8 bytes MIDI.
- El primer byte contiene los "MSB" (Most Significant Bits) de los siguientes 7 bytes.
- En la implementación de C++, el `MS2000SysExCodec` deberá decodificar esto desempaquetando los bits de estado para obtener la matriz completa de datos.
