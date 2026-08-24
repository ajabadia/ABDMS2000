# Especificación de Maquetación de Interfaz y Panel Frontal (Korg MS2000)

*Este documento contiene la arquitectura visual, distribución física del hardware original, mapa de controles y especificación de diseño para el editor/emulador ABDMS2000.*

---

## 1. Diseño Estructural del Panel Frontal (ASCII Wireframe)

```text
=======================================================================================================================
||                                              KORG MS2000 HYBRID EDITOR / VST                                      ||
=======================================================================================================================
||                                                                                                                   ||
||  [1. MAIN DISPLAYS & PERFORMANCE GLOBAL SECTIONS]                                                                 ||
||  +-------------------------------+   +-------------------------------------------------------------------------+  ||
||  | KORG MS2000 V2.00             |   | SYSEX MONITOR (HEX RAW DATA OUT)                                        |  ||
||  | Prog: A11 [Init Synth       ] |   | F0 42 30 58 41 00 01 4C 02 7F 3F 00 44 12 0A 34 5F 62 00 12 43 ... F7   |  ||
||  +-------------------------------+   +-------------------------------------------------------------------------+  ||
||    (LCD DISPLAY 16x2 MATRIX)           (ADDITIONAL EDITOR SCREEN: BYTE / BIT REAL-TIME TELEMETRY TRACKER)         ||
||                                                                                                                   ||
||    [VOLUME]   [PORTAMENTO]    [GLOBAL]  [VOICE]   [PITCH]   [OSC1]   [OSC2]   [MIXER]   [VCF]   [AMP]   [EG1/2]     ||
||      (O)          (O)           [_]       [_]       [_]       [_]      [_]      [_]     [_]     [_]     [_]       ||
||                                    (PAGINATION / MENU EDIT SELECT BUTTONS: CLICK TO SHOW MENU TREE IN LCD)        ||
||                                                                                                                   ||
||===================================================================================================================||
||  [2. SYNTHESIZER ENGINE CONTROL PANEL - PHYSICAL HARDWARE MIRROR]                                                 ||
||                                                                                                                   ||
||   -- OSCILLATOR 1 --               -- OSCILLATOR 2 --                -- MIXER --                                  ||
||   [WAVE SELECT]                    [WAVE]      [MOD MODE]            [OSC1 LVL] [OSC2 LVL] [NOISE LVL]            ||
||   (Saw/Pulse/DWGS...)              (Saw/Sq/Tri)(Off/Sync/Ring)          (O)        (O)        (O)                 ||
||                                                                                                                   ||
||   [CONTROL 1]    [CONTROL 2]       [SEMITONE]  [TUNE]                                                             ||
||       (O)            (O)               (O)       (O)                                                              ||
||                                                                                                                   ||
||   -- VCF (FILTER) --               -- AMP / VCA --                   -- APREGGIATOR / STEP SEQ --                 ||
||   [TYPE] (24LPF/12LPF/BPF/HPF)     [LEVEL]     [PANPOT]              [SWITCH]    [TYPE]       [GATE]              ||
||   [CUTOFF]   [RESONANCE]           (O)         (O)                   [On/Off]    (Up/Down...)  (O)                ||
||       (O)        (O)                                                                                              ||
||                                    [DISTORTION]                                                                   ||
||   [EG1 INT]  [KBD TRACK]           [On/Off]                          (O) [LATCH] (O) [KEY SYNC]                   ||
||       (O)        (O)                                                                                              ||
||                                                                                                                   ||
||   -- ENVELOPE GENERATORS --        -- LFO SECTION --                 -- VIRTUAL PATCH (MATRIX MULTI-SLOT) --      ||
||   [EG1: FILTER]                    [LFO 1]     [SPEED]               [PATCH 1]  Src: [LFO1 ] -> Dest: [CUTOFF] (O) ||
||   [A]     [D]     [S]     [R]      (Tri/S&H...)  (O)   [TEMPO SYNC]  [PATCH 2]  Src: [EG2  ] -> Dest: [AMP   ] (O) ||
||   (O)     (O)     (O)     (O)                          [On/Off]      [PATCH 3]  Src: [VEL  ] -> Dest: [O1CTRL1] (O) ||
||   [EG2: AMPLIFIER]                 [LFO 2]     [SPEED]               [PATCH 4]  Src: [M.WHL] -> Dest: [PITCH  ] (O) ||
||   [A]     [D]     [S]     [R]      (Sine/Sq...)  (O)   [TEMPO SYNC]                                             ||
||   (O)     (O)     (O)     (O)                          [On/Off]                                                   ||
||                                                                                                                   ||
||   -- EFFECTS SECTION (FX) --                                                                                      ||
||   [MOD FX] (Chorus/Phaser/Ensemble)  [SPEED] (O)  [DEPTH] (O)  [FEEDBACK] (O)                                   ||
||   [DELAY]  (Stereo/Cross/LR Delay)   [TIME]  (O)  [DEPTH] (O)  [FEEDBACK] (O)   [BPM SYNC] [On/Off]               ||
||                                                                                                                   ||
||===================================================================================================================||
||  [3. 16-STEP MODULATION SEQUENCER VIEW]                                                                           ||
||                                                                                                                   ||
||   SEQ A: [CUTOFF]  Motion: [SMOOTH]  [01] [02] [03] [04] [05] [06] [07] [08] [09] [10] [11] [12] [13] [14] [15] [16]  ||
||   SEQ B: [PITCH ]  Motion: [STEP  ]  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)  (O)   ||
||   SEQ C: [PAN   ]  Motion: [SMOOTH]                                                                               ||
||                                                                                                                   ||
||===================================================================================================================||
||  [4. CONTROLLERS & INTERACTIVE PIANO KEYBOARD COMPONENT]                                                          ||
||                                                                                                                   ||
||   +-------+  +-------+   [OCTAVE]  |  | | | |  |  | | | | | |  |  | | | |  |  | | | | | |  |  | | | |  |  | | | | |  ||
||   |       |  |       |   [ + ]     |  | | | |  |  | | | | | |  |  | | | |  |  | | | | | |  |  | | | |  |  | | | | |  ||
||   |  (A)  |  |       |   [ - ]     |  |_| |_|  |  |_| |_| |_|  |  |_| |_|  |  |_| |_| |_|  |  |_| |_|  |  |_| |_| |_|  ||
||   |       |  |  (A)  |             |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |  ||
||   |  (V)  |  |       |   Active:   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |   |  ||
||   |       |  |  (V)  |   [ C3-C5 ] | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ | _ |  ||
||   +-------+  +-------+             |____________________________________________________________________________|  ||
||   PITCH BEND  MOD WHEEL                                                                                           ||
=======================================================================================================================
```

---

## 2. Diagramas de Flujo de Señal Oficiales (Hardware Service Manual)

### 2.1 Diagrama de Bloques del Sintetizador (Synthesizer Block Diagram)

```
[AUDIO IN] ──> [OSC 1] ──┐
                 │       ├─> [MIXER] ──> [FILTER] ──> [AMP / VCA] ──> [PAN] ──> [+] ──> [MOD FX] ──> [DELAY FX] ──> [EQ] ──> L/R OUT
[OSC 2] ─────────┘       │                 │              │                      ▲
[NOISE] ─────────────────┘             (EG1/KBD)      (EG2/DIST)                 │
                                           ▲              ▲                      │
                                           └──────┬───────┘                      │
                                                  │                              │
                                         [VIRTUAL PATCH MATRIX]                  │
                                         (EG1, EG2, LFO1, LFO2, MIDI 1/2)        │
                                                  │                              │
                                         [MOD SEQUENCE (1, 2, 3)]                │
                                                                                 │
                                         [TIMBRE 2 (Layer / Split)] ─────────────┘
```

### 2.2 Diagrama de Bloques del Vocoder de 16 Bandas (Vocoder Block Diagram)

```
                       ┌───────────────────────────────────────────────────────────┐
                       │                   VOCODER SECTION (16 CH)                 │
                       │                                                           │
[VOICE (Mod In)] ──────┼──> [16ch ANALYSIS BPF] ──> [16ch ENVELOPE FOLLOWERS]      │
                       │                                   │ (EF SENSE)            │
                       │                                   ▼                       │
[CARRIER (OSC1/Noise)] ┼──> [MIXER] ──> [DIST] ──> [16ch SYNTHESIS BPF] ─> [16x VCA] ─> [16x PAN] ──> [+] ──> [MOD FX] ──> [DELAY] ──> [EQ] ──> OUT
                       │                                   ▲                                             ▲
                       │                              (FORMANT SHIFT)                                    │
                       └─────────────────────────────────────────────────────────────────────────────────┼─┐
                                                                                                         │ │
[DIRECT LEVEL (Dry Modulator)] ──────────────────────────────────────────────────────────────────────────┘ │
[HPF NOISE / GATE (Unvoiced Consonants Detector)] ───────────────────────────────────────────────────────────┘
```

---

## 3. Estado de la Investigación

### 3.1 Formas de Onda Digitales DWGS (64 ondas)
* **Origen:** Korg DW-8000 / DW-6000.
* **Resolución:** Tablas de ciclo único de 2048 muestras cada una.
* **Estructura contigua:** Array de $64 \times 2048 = 131.072$ floats contiguos en memoria (~512 KB de RAM), garantizando cambio de onda en tiempo real con cero latencia y cero llamadas al sistema de archivos.

### 3.2 Vocoder de 16 Bandas
* **Frecuencias de las 16 bandas:** Distribución logarítmica inspirada en el clásico Korg VC-10 (desde 100 Hz hasta 7.5 kHz).
* **Formant Shift:** Desplaza las 16 frecuencias centrales de los filtros de síntesis hacia arriba o hacia abajo ($-2, -1, 0, +1, +2$).
* **HPF Gate & Level:** Extrae las sibilantes (sonidos sordos como "S", "T", "CH") de la voz mediante un filtro paso alto y las inyecta directamente con ruido blanco para máxima inteligibilidad vocal.
