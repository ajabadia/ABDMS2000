# Mapa de Menús, Parámetros y Valores del Korg MS2000 (Árbol Completo)

Este documento contiene la estructura oficial de páginas de edición extraída del manual técnico y la implementación MIDI del **Korg MS2000 / MS2000R / microKORG**. Está estructurado para mapear directamente el árbol de datos (`juce::AudioProcessorValueTreeState`) del plugin y sincronizar con SysEx y contratos WebUI.

---

## PARTE 1: VOZ Y SINTETIZADOR

### 1. GLOBAL / VOICE (Configuración de la Arquitectura de Voz)
* **Voice Assign (Págs. de edición global del parche):** Estructura cómo se comportan las voces y los timbres.
  * `MONO` / `POLY` / `UNISON`
* **Trigger Mode (Comportamiento del teclado):**
  * `SINGLE` (La envolvente no se redispara en legato) / `MULTI` (Se redispara siempre en cada pulsación)
* **Voice Mode (Modo de capas/timbres):**
  * `SINGLE` (1 Timbre, 4 voces) / `SPLIT` (2 Timbres divididos en el teclado) / `DUAL` (2 Timbres encapados, 2 voces)
* **Unison Detune (Solo visible si Voice Assign está en UNISON):**
  * `0` a `99` (Cents de desafinación entre las voces apiladas)
* **Unison Spread (Solo visible si Voice Assign está en UNISON):**
  * `0` a `127` (Apertura estéreo de las voces apiladas)

### 2. PITCH (Afinación y Glide por Timbre)
* **Transpose:** `-24` a `+24` (Desplazamiento por semitonos)
* **Tune:** `-50` a `+50` (Afinación fina en cents)
* **Portamento (Glide):** `0` a `127` (Tiempo de transición de tono)
* **Vibrato Intensity (Modulación rápida de pitch por LFO2):** `-63` a `+63` (Mapeado SysEx `0` a `127`, centro en `64`)
* **Pitch Bend Range:** `-12` a `+12` (Rango de la rueda de pitch bend en semitonos)

### 3. OSC 1 (Oscilador Principal)
* **Wave (Algoritmo base):**
  * `SAW` / `PULSE` / `TRIANGLE` / `SINE` / `VOX` / `DWGS` / `NOISE` / `AUDIO IN`
* **OSC1 Control 1 (Función cambia según la onda elegida):** `0` a `127`
  * *Si es SAW/TRIANGLE:* Modula la forma de onda (Waveform Shaping).
  * *Si es PULSE:* Controla el ancho de pulso base (Pulse Width).
  * *Si es SINE:* Ajusta la profundidad de la modulación cruzada (Cross Mod Depth).
  * *Si es VOX:* Modula el carácter de la formante vocal.
* **OSC1 Control 2 (Sub-parámetro de modulación o selección):**
  * *Si la onda es DWGS:* Selecciona la forma de onda digital entre `0` y `63` (64 ondas DWGS).
  * *Si la onda es PULSE/SAW/TRIANGLE:* Permite asignar una fuente para modular automáticamente el Control 1 (`LFO1`, `LFO2`, `EG1`).

### 4. OSC 2 (Oscilador Secundario)
* **Wave:** `SAW` / `SQUARE` / `TRIANGLE`
* **Modulation Mode (Interacción con OSC1):**
  * `OFF` / `RING` (Modulación en anillo) / `SYNC` (Sincronización dura) / `RING+SYNC`
* **Semitone (Afinación basta):** `-24` a `+24`
* **Tune (Afinación fina):** `-50` a `+50`

### 5. MIXER (Mezclador de Señal)
* **OSC1 Level:** `0` a `127`
* **OSC2 Level:** `0` a `127`
* **Noise Level / Audio In Level:** `0` a `127`

---

## PARTE 2: FILTRO, AMPLIFICADOR Y ENVOLVENTES

### 6. VCF (Filtro Multimodo)
* **Type (Tipo de filtro):**
  * `24LPF` (Paso bajo de 4 polos, -24dB/oct)
  * `12LPF` (Paso bajo de 2 polos, -12dB/oct)
  * `12BPF` (Paso banda de 2 polos, -12dB/oct)
  * `12HPF` (Paso alto de 2 polos, -12dB/oct)
* **Cutoff (Frecuencia de corte):** `0` a `127` (Mapeo logarítmico calibrado 20Hz a 20kHz).
* **Resonance (Resonancia):** `0` a `127` (Entra en auto-oscilación analógica a partir de ~105).
* **EG1 Intensity (Profundidad de la envolvente 1 hacia el filtro):** `-63` a `+63` (Mapeado SysEx: `0` a `127`, neutro en `64`).
* **KBD Track (Seguimiento de teclado):** `-63` a `+63` (Controla cuánto abre o cierra el filtro según la altura de la nota).

### 7. VCA / AMP (Sección de Amplificación y Distorsión)
* **Amp Level (Volumen del timbre):** `0` a `127`
* **Panpot (Posición estéreo):** `L64` (Todo a la izquierda) a `CNT` (Centro, valor `64`) a `R63` (Todo a la derecha, valor `127`).
* **Distortion (Conmutador de saturación analógica):** `OFF` / `ON`
* **KBD Track (Seguimiento de volumen por teclado):** `-63` a `+63`

### 8. EG1 & EG2 (Generadores de Envolvente)
*Ambas envolventes (EG1: Filtro/Pitch, EG2: Amplitud) comparten los mismos parámetros y curvas exponenciales calibradas:*
* **Attack (Tiempo de ataque):** `0` a `127` (Curva exponencial invertida, 1ms a 11s).
* **Decay (Tiempo de decaimiento):** `0` a `127` (Curva exponencial, 2ms a 20s).
* **Sustain (Nivel de sostenido):** `0` a `127` (Nivel lineal 0% a 100%).
* **Release (Tiempo de relajación):** `0` a `127` (Curva exponencial, 2ms a 20s).

### 9. LFO 1 & LFO 2 (Osciladores de Baja Frecuencia)
* **LFO1 Wave:** `SAW` / `SQUARE` / `TRIANGLE` / `S&H` (Sample & Hold)
* **LFO2 Wave:** `SAW` / `SQUARE` / `SINE` / `S&H`
* **Tempo Sync:** `OFF` (Frecuencia libre en Hz) / `ON` (Sincronizado al reloj del DAW / BPM).
* **LFO Speed / Sync Frequency:**
  * *Si Tempo Sync está en OFF:* `0` a `127` (Mapea logarítmicamente de 0.01 Hz a 20 Hz).
  * *Si Tempo Sync está en ON:* Subdivisiones rítmicas: `1/1`, `3/4`, `1/2`, `1/4`, `1/8`, `1/16`, `1/32`, etc.

### Virtual Patch (4 Ranuras de Modulación Matricial)
* **PATCH [1–4] SOURCE (Orígenes):** `EG1`, `EG2`, `LFO1`, `LFO2`, `VELOCITY` (Velocidad de pulsación), `KBD_TRK` (Seguimiento de nota), `PITCH_BEND`, `MOD_WHEEL`.
* **PATCH [1–4] DESTINATION (Destinos):** `PITCH`, `OSC2_PITCH`, `OSC1_CTRL1`, `CUTOFF`, `AMP`, `PAN`, `LFO2_FREQ`.
* **PATCH [1–4] INTENSITY (Intensidad de modulación bipolar):** `-63` a `+63` (Mapeado SysEx: `0` a `127`, neutro en `64`).

---

## PARTE 3: EFECTOS (FX), ECUALIZADOR (EQ) Y VOCODER

### 10. CHORUS/MOD FX (Efectos de Modulación)
* **Mod FX Type (Tipo de efecto):**
  * `CHORUS/FLANGER`
  * `ENSEMBLE`
  * `PHASER`
* **LFO Speed (Velocidad del LFO del efecto):** `0` a `127`
* **Effect Depth (Profundidad / Intensidad):** `0` a `127`
* **Feedback (Solo operativo si el tipo es Phaser o Flanger):** `0` a `127`

### 11. DELAY FX (Efectos de Retardo Maestro)
* **Delay Type (Tipo de Delay):**
  * `STEREO DELAY` (Retardo estéreo paralelo)
  * `CROSS DELAY` (Efecto ping-pong alternado)
  * `L/R DELAY` (Tiempos independientes izquierda/derecha)
* **Tempo Sync:** 
  * `OFF` (Tiempo libre manual)
  * `ON` (Sincronizado al reloj del sistema o reloj MIDI del DAW)
* **Delay Time / Sync Time:**
  * *Si Tempo Sync está en OFF:* `0` a `127` (Mapeado de ~1ms a 1400ms).
  * *Si Tempo Sync está en ON:* Valores rítmicos indexados (`1/32`, `1/16`, `1/16P` [Puntillo], `1/8T` [Tresillo], `1/8`, `1/4`, `1/2`, `1/1`, etc.).
* **Delay Depth (Volumen / Mix de las repeticiones):** `0` a `127`
* **Feedback (Cantidad de repeticiones / Retroalimentación):** `0` a `127`

### 12. EQUALIZER (EQ Paramétrico Global del Timbre)
* **Low EQ Freq (Frecuencia de corte de graves):**
  * `160Hz`, `250Hz`, `400Hz`, `600Hz`
* **Low EQ Gain (Ganancia de graves):**
  * `-12` a `+12` (Mapeado SysEx: `0` a `127`, neutro en `64`).
* **High EQ Freq (Frecuencia de corte de agudos):**
  * `4.0kHz`, `6.0kHz`, `8.0kHz`, `12.0kHz`
* **High EQ Gain (Ganancia de agudos):**
  * `-12` a `+12` (Mapeado SysEx: `0` a `127`, neutro en `64`).

### 13. VOCODER (Filtros y Modos Especiales)
*Los siguientes parámetros solo se activan cuando el modo global del sintetizador se conmuta de 'Synth' a 'Vocoder':*
* **Vocoder Select (Estructura de enrutamiento):** `INTERNAL` / `EXTERNAL`
* **Formant Shift (Desplazamiento armónico de formantes):** 
  * `-2`, `-1`, `0`, `+1`, `+2`
* **VCO Level / Carrier In (Nivel de la portadora interna):** `0` a `127`
* **Audio In Level / Modulator In (Nivel del micrófono/entrada analógica):** `0` a `127`
* **Gate Sense (Umbral de la puerta de ruido del modulador):** `0` a `127`
* **HPF Level (Nivel del filtro paso alto para consonantes sibilantes):** `0` a `127`
* **HPF Gate (Modo de disparo de las consonantes):** `DISABLE` / `ENABLE`

---

## PARTE 4: SECUENCIADORES Y AJUSTES GLOBALES

### 14. MOD SEQUENCE (Secuenciador de Modulación)
*El MS2000 tiene 3 pistas de modulación paralelas (A, B, C) de hasta 16 pasos individuales cada una:*
* **Sequence Control (Comportamiento global):**
  * **Sequence A/B/C Param (Destino):** `PITCH`, `OSC2_PITCH`, `OSC1_CTRL1`, `CUTOFF`, `AMP`, `PAN`, `LFO2_FREQ`, etc.
  * **Sequence A/B/C Motion (Tipo de transición):** `STEP` (Salto directo por escalones) / `SMOOTH` (Rampa interpolada).
  * **Sequence Length (Longitud de la secuencia):** `1` a `16` pasos.
  * **Sequence Mode (Dirección de lectura):** `FORWARD`, `BACKWARD`, `BOUNCE`, `RANDOM`.
* **Step Values (Valores por paso para cada secuencia A, B y C):**
  * **Step 1 al Step 16:** `0` a `127` (Cada paso almacena de forma independiente un entero de 7 bits).

### 15. GLOBAL SETTINGS (Ajustes de Sistema)
*Estos parámetros afectan al comportamiento de todo el dispositivo y no se guardan en el parche, sino en la memoria interna del sistema:*
* **Master Tune (Afinación global de referencia):** `430.0Hz` a `450.0Hz` (Por defecto en `440.0Hz`).
* **MIDI Clock (Sincronización de reloj):** `INTERNAL`, `EXTERNAL_MIDI`, `AUTO`
* **MIDI Local Control (Desacoplar teclado físico del motor interno):** `OFF` / `ON`
* **Memory Protect (Protección contra escritura de parches):** `OFF` / `ON`
