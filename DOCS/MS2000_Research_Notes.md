# Investigación Completa del Korg MS2000 — Arquitectura, DSP e Ingeniería Inversa

*Este documento recopila y destila toda la investigación realizada sobre el comportamiento documentado y no documentado del Korg MS2000, incluyendo datos de ingeniería inversa de la comunidad open-source y hardware hacking. Es la referencia canónica para el desarrollo del ABDMS2000.*

---

## Índice

1. [Visión General y Viabilidad](#1-visión-general-y-viabilidad)
2. [Recursos Disponibles en Internet](#2-recursos-disponibles-en-internet)
3. [Estrategia de Desarrollo: Editor Híbrido](#3-estrategia-de-desarrollo-editor-híbrido)
4. [Arquitectura Completa del MS2000](#4-arquitectura-completa-del-ms2000)
5. [Osciladores (71 formas de onda)](#5-osciladores-71-formas-de-onda)
6. [DWGS Wavetables (64 ondas digitales)](#6-dwgs-wavetables-64-ondas-digitales)
7. [Filtro Multimodo](#7-filtro-multimodo)
8. [Envolventes (EG1 y EG2)](#8-envolventes-eg1-y-eg2)
9. [LFOs (1 y 2)](#9-lfos-1-y-2)
10. [Distorsión (Amp Distortion)](#10-distorsión-amp-distortion)
11. [Secuenciador de Modulación (Mod Sequence)](#11-secuenciador-de-modulación-mod-sequence)
12. [Arpegiador / Step Sequencer](#12-arpegiador--step-sequencer)
13. [Virtual Patch (Matriz de Modulación)](#13-virtual-patch-matriz-de-modulación)
14. [Efectos (EQ, Mod FX, Delay)](#14-efectos-eq-mod-fx-delay)
15. [Componentes Extra (Vocoder, Voice Assign, AMP, Portamento)](#15-componentes-extra)
16. [Frecuencias Exactas del EQ y Escalas del Virtual Patch](#16-frecuencias-exactas-del-eq-y-escalas-del-virtual-patch)
17. [Flujo de Señal Completo y Orden de Procesamiento](#17-flujo-de-señal-completo-y-orden-de-procesamiento)
18. [Estructura de Clases Recomendada para JUCE](#18-estructura-de-clases-recomendada-para-juce)

---

## 1. Visión General y Viabilidad

Existe suficiente información en internet sobre la arquitectura y el comportamiento del Korg MS2000 como para desarrollar una emulación virtual analógica utilizando JUCE. [[Wikipedia: Korg MS2000](https://en.wikipedia.org/wiki/Korg_MS2000)]

### 1.1 Qué información está disponible

- **Manuales y diagramas de bloques:** El MS2000 Service Manual detalla la lista de componentes, esquemas de circuitería analógica de apoyo y la estructura general del sintetizador. [[Elektrotanya: Service Manual](https://elektrotanya.com/korg_ms2000.pdf/download.html)]
- **Implementación MIDI y SysEx:** Los documentos oficiales de Korg describen todos los mensajes de control, parámetros y tablas de ondas (DWGS). [[Korg Downloads](https://www.korg.com/us/support/download/product/1/154/)]
- **Especificaciones del motor:** Se conocen los algoritmos de osciladores, los tipos de filtro, la matriz de modulación (Virtual Patch) y el funcionamiento del secuenciador de modulación. [[ManualsLib](https://www.manualslib.com/manual/1072845/Korg-Ms2000.html), [Sound On Sound Review](https://www.soundonsound.com/reviews/korg-ms2000), [GitHub Topic](https://github.com/topics/korg-ms2000)]

### 1.2 Qué limitaciones existen

- **Código del chip DSP original:** No existe documentación pública ni el código fuente del chip de procesamiento de señal patentado (era Korg Prophecy/Z1). [[dtech.lv: Korg DSPs](https://www.dtech.lv/techarticles_korg_dsps.html)]
- **Enfoque de modelado:** No se puede hacer una emulación a nivel de componentes exactos, sino una recreación basada en modelado virtual analógico del comportamiento sonoro.
- **Funciones de transferencia (curvas):** Korg **nunca** publica las ecuaciones de transferencia de sus chips DSP en los manuales de usuario ni de servicio. El manual dice "LFO Frequency: 0...127" pero no menciona ni los Hertz límites ni el tipo de curva.

### 1.3 Estimación de fidelidad alcanzable

Con la información disponible (manuales, ingeniería inversa comunitaria, mediciones de caja negra), se puede aproximar al **90-95% del comportamiento** del hardware. El último 5% (curvas exactas de respuesta) se resuelve mediante un proceso de **"caja negra auditiva"** usando el hardware físico y analizadores de espectro por software (ej. Voxengo SPAN).

---

## 2. Recursos Disponibles en Internet

### 2.1 Mensajería SysEx, MIDI y Mapeo de Parámetros

| Recurso | Descripción | URL |
|---|---|---|
| **MS2000 MIDI Implementation Manual** | Documento maestro. Contiene las tablas completas de mapeo de parámetros (Parameter Change Data Format), indicando qué posición de byte y bit en una cadena SysEx corresponde a cada elemento. | [Korg MIDI Impl.](https://www.korg.com/us/support/download/manual/1/154/2706/) |
| **ReMS2000 (GitHub)** | Editor MIDI open-source para el MS2000. Mina de oro para decodificar archivos `.syx` e interactuar con modos globales, vocoder y secuenciadores de modulación. | [github.com/inteyes/ReMS2000](https://github.com/inteyes/ReMS2000) |
| **GitHub Topic: korg-ms2000** | Agrupa proyectos específicos con implementaciones concretas para manejar mensajes MIDI System Exclusive. | [github.com/topics/korg-ms2000](https://github.com/topics/korg-ms2000) |
| **MIDI Guide: MS2000 CCs** | Tabla completa de MIDI CCs y NRPNs con valores. | [midi.guide/d/korg/ms2000](https://midi.guide/d/korg/ms2000/) |
| **Spectrasonics OmnisphereHW** | Referencia de implementación de CCs del MS2000 como controlador. | [Spectrasonics: MS2000](https://support.spectrasonics.net/manual/OmnisphereHW/2.5/en/topic/korg-ms-2000-overview) |
| **AURA Plugins MS2000 Editor** | Editor comercial cuyo comportamiento fue referencia para las curvas exponenciales. | [AURA Plugins Docs](https://docs.auraplugins.com/kb/user-guide/korg-ms2kxr-getting-started/) |
| **Renoise Forum (Guru Tool)** | Discusión técnica sobre decodificación de SysEx de Korg. | [forum.renoise.com](https://forum.renoise.com/t/new-tool-2-8-guru/35934?page=13) |
| **Sequencer.de** | Comunidad alemana con información sobre errores comunes de SysEx en Korg. | [sequencer.de](https://www.sequencer.de/synthesizer/threads/fehlermeldung-microkorg-didnt-respond.73929/) |

### 2.2 Formas de Onda Digitales (DWGS)

| Recurso | Descripción | URL |
|---|---|---|
| **Foros de Renoise** | Pack gratuito de osciladores del Korg MS2000 en formato `.wav`, cortados a un único ciclo. Incluye las 64 ondas DWGS. | [Renoise: MS2000 WAV Packs](https://forum.renoise.com/t/korg-ms-2000-xrni-wav-packs/37785) |

### 2.3 Circuitería y Estructura de Bloques

| Recurso | Descripción | URL |
|---|---|---|
| **MS2000 Service Manual** | Lista de componentes y Block Diagram (flujo de señal). Verifica el ruteo de Noise, efectos y Vocoder. | [Elektrotanya](https://elektrotanya.com/korg_ms2000.pdf/download.html) |
| **ManualsLib** | Owner's Manual escaneado con diagramas de panel y flujo de señal. | [ManualsLib](https://www.manualslib.com/manual/871131/Korg-Ms2000.html) |

### 2.4 Datos de Audio de Referencia (Calibración)

| Recurso | Descripción | URL |
|---|---|---|
| **Korg Factory Presets** | Volcados de datos estándar con los parches de fábrica. Necesarios si la batería interna ha fallado. | [Korg: Factory Data](https://www.korg.com/us/support/download/software/1/154/3406/) |
| **Autodafe.net** | Lista de Program Change del MS2000. | [Autodafe](https://www.autodafe.net/synth-manuals/209-korg-ms2000-ms2000r-program-change-list.html) |

### 2.5 Reviews y Análisis Técnicos

| Recurso | URL |
|---|---|
| Sound On Sound Review (2000) | [soundonsound.com](https://www.soundonsound.com/reviews/korg-ms2000) |
| Vintage Synth Explorer | [vintagesynth.com](https://www.vintagesynth.com/korg/ms2000) |
| SynthMania | [synthmania.com](https://www.synthmania.com/ms2000.htm) |
| Daily Analog Review | [dailyanalog.com](https://dailyanalog.com/synths/korg/korg-ms2000-review/) |
| Synth.Market Specs | [synth.market](https://synth.market/en/catalogue/korg/ms2000/) |
| KVR Audio Forums | [kvraudio.com](https://www.kvraudio.com/forum/viewtopic.php?t=553145) |
| Vintage Synth Forum | [vintagesynth.com forum](https://forum.vintagesynth.com/viewtopic.php?t=77496) |
| microKORG Manual (Italiano, comparte arquitectura) | [Scribd](https://it.scribd.com/doc/50402759/400-manuale-microkorg-ita) |

---

## 3. Estrategia de Desarrollo: Editor Híbrido

Los editores híbridos (VST/Plugin que actúan como "clon de audio" y espejo MIDI) son la mejor solución para integrar hardware antiguo sin saturar las entradas de la tarjeta de sonido.

### 3.1 El problema de los intentos anteriores

El problema en anteriores intentos de emulación radica en que los fabricantes casi nunca documentan las **funciones de transferencia** (las curvas). El manual dice que el Decay va de 0 a 127, pero no especifica si 64 son 500ms o 2 segundos, ni si la curva es exponencial, logarítmica o lineal. Si mapeas el valor 64 de forma lineal, el emulador asigna 5.5 segundos al ataque, mientras que el hardware real ya iba por los 0.6 segundos.

### 3.2 Flujo de datos del editor híbrido

```
[ Hardware Korg MS2000 ]
         │
         ▼ (Envía MIDI CC / SysEx físico al mover knobs)
[ Plugin JUCE (Editor) ] ──► Mapea números (0-127) a variables internas.
         │
         ▼ (Aplica fórmulas de curvas calibradas de oído/espectro)
[ Motor DSP de JUCE ]  ────► Genera el audio emulado en tiempo real dentro del DAW.
```

### 3.3 Protocolo de calibración de curvas (sin osciloscopio)

Para resolver las curvas de los parámetros que no están documentados oficialmente:

1. **Tiempos de envolvente:** Poner un parche básico (sierra limpia, filtro abierto), Attack=127, grabar nota larga. Medir en DAW cuántos segundos tarda en llegar al pico → límite superior. Repetir para Decay y Release.
2. **Curva del filtro (Cutoff):** Grabar el MS2000 auto-oscilando (Resonancia al máximo, osciladores apagados) y mover el Cutoff a valores fijos (0, 32, 64, 96, 127). Usar Voxengo SPAN para ver qué frecuencia Hz genera el filtro en cada punto. Poner esos 5 puntos en una función de interpolación. [[ManualOnline: MS2000 p.42](https://auto.manualsonline.com/manuals/mfg/korg/ms2000_ms2000r.html?p=42)]
3. **Exponente de curva:** Modificar el exponente entre 2 y 4 para emparejar el comportamiento del hardware "de oído".

### 3.4 Arquitectura recomendada en JUCE

- Crear una clase `MS2000SysExParser` encargada exclusivamente de escuchar los búferes de entrada MIDI de JUCE (`juce::MidiBuffer`).
- Usar los datos extraídos de ReMS2000 y el MIDI manual de Korg para convertir esos bytes crudos en **valores normalizados** (float de 0.0f a 1.0f).
- Conectar esos valores flotantes al `juce::AudioProcessorValueTreeState` para que controlen simultáneamente la interfaz gráfica y los algoritmos DSP.

---

## 4. Arquitectura Completa del MS2000

### 4.1 Listado de Componentes (Módulos Internos por Voz)

Cada una de las 4 voces del MS2000 (o 2 en modo Dual/Split) contiene los siguientes bloques individuales en su DSP:

| Módulo | Descripción |
|---|---|
| **OSC 1** | Oscilador principal con 8 algoritmos de onda (Saw, Pulse, Triangle, Sine, Vox, DWGS×64, Noise, Audio In) |
| **OSC 2** | Oscilador secundario con 3 ondas analógicas (Saw, Square, Triangle) y 3 modos de interacción (Ring, Sync, Cross Mod) |
| **Audio In (External)** | Entrada física estéreo que pasa por un preamplificador digital hacia el mezclador |
| **Mixer** | Tres canales independientes con controles de volumen para OSC 1, OSC 2 y Noise (o entrada de Audio) |
| **VCF (Filtro Multimodo)** | Filtro resonante con 4 pendientes configurables (24dB LPF, 12dB LPF, 12dB BPF, 12dB HPF) |
| **Amp Distortion** | Etapa de saturación asimétrica no lineal que se activa por conmutación (On/Off) |
| **VCA (Amplificador)** | Control de ganancia dinámica, panorama (Pan) y Keyboard Track |
| **EG 1** | Generador ADSR exponencial — ruteo fijo a Cutoff del filtro |
| **EG 2** | Generador ADSR exponencial — ruteo fijo a VCA |
| **LFO 1** | 4 ondas (Saw, Square, Triangle, S&H). Key Sync y Tempo Sync |
| **LFO 2** | 4 ondas (Saw, Square, Sine, S&H adaptado). Key Sync y Tempo Sync |
| **Virtual Patch (Matriz)** | 4 ranuras (Slots) de direccionamiento libre Source → Destination |
| **Mod Sequence** | 3 pistas independientes de secuenciador por pasos (A, B, C) de hasta 16 pasos |
| **Arpeggiator** | Patrones de arpegio (Up, Down, Alt, Random, Trigger) con inyección en MidiBuffer |
| **Portamento / Glide** | Módulo de tiempo que ralentiza la transición de tono entre notas |
| **Voice Assign** | Mono, Poly, Unison (con Detune 0-99 cents y Spread estéreo) |

### 4.2 Componentes Globales (fuera de la voz)

| Módulo | Descripción |
|---|---|
| **Vocoder (16 bandas)** | Emulación del Korg VC-10. Audio In 2 como modulador, osciladores como portadora. Ganancia y pan por banda. Formant Shift y HPF Gate. [[Sound On Sound](https://www.soundonsound.com/reviews/korg-ms2000)] |
| **Envelope Follower** | Analiza amplitud de audio externo (Audio In 1) y la convierte en voltaje de control para Virtual Patch |
| **EQ (2 bandas)** | Low Shelf + High Shelf, procesado global al final de la cadena de voz |
| **Mod FX** | Chorus/Flanger, Ensemble, Phaser |
| **Delay FX** | Stereo Delay, Cross Delay (Ping-Pong), L/R Delay |
| **Timbre A / Timbre B** | Modo multitímbrico de 2 partes (Single, Layer, Split) |

### 4.3 Interconexiones de Control Fijas (Ruteo por Defecto)

Incluso con la matriz de modulación vacía, el MS2000 tiene cables "virtuales" soldados internamente:

| Origen | Destino | Descripción |
|---|---|---|
| Teclado (Pitch) | OSC 1 y OSC 2 | Controla frecuencia fundamental según nota MIDI |
| Teclado (Velocity) | EG 1 y EG 2 | La velocidad altera la intensidad máxima de las envolventes |
| EG 1 | Filtro (VCF Cutoff) | Ruteada fijamente vía potenciómetro "EG1 Intensity" |
| EG 2 | VCA (Volumen) | Envolvente que abre y cierra el amplificador principal |
| LFO 1 y LFO 2 | Pitch, Cutoff | Preconectados a destinos del panel, regulables con potenciómetros dedicados |

---

## 5. Osciladores (71 formas de onda)

El Korg MS2000 tiene un total de **71 formas de onda** diferentes accesibles directamente desde sus osciladores principales.

### 5.1 Oscilador 1 (OSC1) — 68 formas de onda, 8 algoritmos

El potenciómetro físico "OSC1 Wave" selecciona uno de los 8 modos principales mediante SysEx o MIDI CC. En JUCE, el OSC1 debe implementar un despachador (switch-case):

```cpp
enum class OSC1Algorithm {
    Saw = 0,        // Diente de sierra (PolyBLEP anti-aliasing)
    Pulse_PWM,      // Pulso con modulación de ancho (PolyBLEP/BLIT)
    Triangle,       // Triangular (integración de cuadrada)
    Sine_CrossMod,  // Senoidal pura o FM por OSC2
    VoxWave,        // Tablas con picos de formantes vocales (A-E-I-O-U)
    DWGS,           // 64 ondas digitales (índice 0-63 en Control 2)
    Noise,          // Generador estocástico (blanco/rosa)
    AudioIn         // Buffer de entrada de audio externo
};
```

**Desglose de las 68 formas:**
- 3 ondas analógicas VA: Saw, Pulse (con PWM), Triangle
- 1 onda matemática especial: Sine (se convierte en FM/Cross Mod al activar el control secundario)
- 64 ondas digitales DWGS: Heredadas de los Korg DW-6000 y DW-8000 (Bell, Organ, Guitar, Brass, Strings, Vocal, Clav, Endless, etc.)
- Los modos "Noise" y "Audio In" **no** utilizan formas de onda pregrabadas

**Implementación de cada modo en JUCE:**

1. **Saw:** `juce::dsp::Oscillator<float>` con anti-aliasing (band-limited) o PolyBLEP.
2. **Pulse / PWM:** Combinación de dos rampas invertidas o algoritmo PolyBLEP/BLIT. El ancho de pulso se controla dinámicamente mediante OSC1 Control 1 y el LFO.
3. **Triangle:** Integración de una onda cuadrada o aproximación armónica suave.
4. **Sine / Cross Modulation:** Senoidal limpia cuya fase o frecuencia instantánea se modula con la salida directa de OSC2: `frecuencia_efectiva = f1 + (salida_OSC2 × profundidad_mod)`
5. **Vox Wave:** Formas de onda basadas en formantes con tablas de ondas con picos fijos vocales.
6. **DWGS (64 ondas):** Tabla contigua de 64 ondas de ciclo único (2048 muestras cada una). El parámetro OSC1 Control 2 selecciona el índice (0 a 63).
7. **Noise:** Generador estocástico procesado con filtro paso bajo suave. **IMPORTANTE:** No usar `juce::Random` constructor por defecto en AudioWorklet/hilo de audio (no RT-safe). Usar seed explícito: `juce::Random noiseGen(12345)` o LCG inline.
8. **Audio In:** Conecta directamente el `juce::AudioBuffer<float>& buffer` del DAW para enrutar audio externo a los filtros.

**Parámetros SysEx:**
- `OSC1_WAVE`: Menú de 8 posiciones. Cuando seleccionas DWGS, se habilita un sub-parámetro de 0 a 63.
- `OSC1_CTRL1`: Ancho de pulso (Pulse), profundidad FM (Sine), formante (Vox), etc.
- `OSC1_CTRL2`: Índice DWGS (0-63) cuando Wave=DWGS.

### 5.2 Oscilador 2 (OSC2) — 3 formas de onda

El OSC2 opera como fuente de tono **y** como operador de modulación hacia OSC1:

- **Saw** (Diente de sierra)
- **Square** (Cuadrada / Pulso fijo al 50%)
- **Triangle** (Triangular)

**Pitch:**
- Coarse: -24 a +24 semitonos
- Fine: -50 a +50 cents
- Fórmula: `osc2Freq = baseFreq * std::pow(2.0f, (semitones + (cents / 100.0f)) / 12.0f)`

### 5.3 Algoritmos de Modulación de OSC2

```cpp
enum class OSC2ModulationMode {
    Off = 0,
    RingMod,    // Modulación en anillo
    Sync,       // Sincronización dura (Hard Sync)
    CrossMod    // Modulación de frecuencia hacia OSC1
};
```

- **Ring Modulation:** Multiplicación simple en el dominio del tiempo: `Salida = OSC1_sample * OSC2_sample`
- **Hard Sync:** Cuando la fase acumulada de OSC1 completa un ciclo (2π), fuerza a cero la fase de OSC2:
  ```cpp
  if (osc1PhaseWrapped) { osc2Phase = 0.0f; }
  ```
- **Cross Mod:** Modula la frecuencia instantánea de OSC1 con la salida de OSC2.

### 5.4 Mixer

Las tres fuentes se suman antes de entrar al filtrado:

```
Señal Mezclada = (OSC1 × Vol1) + (OSC2 × Vol2) + (Noise × VolNoise)
```

**Peculiaridad:** Si Ring Mod o Cross Mod están activos, la señal del OSC1 se altera **antes** de sumarse al bus del mezclador. La cantidad de distorsión (sección Amp Distortion) depende directamente del volumen total de este mixer.

---

## 6. DWGS Wavetables (64 ondas digitales)

### 6.1 Origen

Las 64 formas de onda DWGS (Digital Waveform Generator System) están heredadas del sintetizador clásico Korg DW-8000. No es necesario extraerlas de la ROM mediante ingeniería inversa; la comunidad ya lo hizo y están disponibles como archivos `.wav` de ciclo único (single-cycle waveforms) en los [Foros de Renoise](https://forum.renoise.com/t/korg-ms-2000-xrni-wav-packs/37785). [[MIDI Guide](https://midi.guide/d/korg/ms2000/)]

### 6.2 Estrategia de Almacenamiento (La Gran Tabla)

En lugar de crear 64 objetos de tabla de ondas separados, se crea un **array contiguo en memoria**. Cada forma de onda de un solo ciclo tiene una resolución fija de 2048 muestras por ciclo (alta fidelidad sin aliasing).

- **Total:** 2048 × 64 = **131,072 muestras** = menos de 1 MB de RAM
- El índice SysEx (0-63) sirve como multiplicador para calcular el offset en el vector: `tableOffset = currentWaveIndex * tableSize`

### 6.3 Código de Referencia

```cpp
// DWGSOscillator — Gestiona las 64 ondas DWGS en un bloque contiguo
class DWGSOscillator {
public:
    void loadDWGSTables(const float* audioData, int samplesPerCycle, int numTables);
    void prepareToPlay(double sampleRate);
    void setFrequency(float frequency);
    void setWaveformIndex(int index);  // 0 a 63
    float getNextSample() noexcept;

private:
    std::vector<float> wavetableData;  // 131,072 floats contiguos
    int tableSize = 2048;
    int totalWaves = 64;
    int currentWaveIndex = 0;
    double currentIndex = 0.0;         // Posición de lectura dentro del ciclo
    double phaseIncrement = 0.0;       // Velocidad de lectura según frecuencia
    double currentSampleRate = 44100.0;
};
```

**Lectura con interpolación lineal (o cúbica para agudos):**
```cpp
float DWGSOscillator::getNextSample() noexcept {
    int tableOffset = currentWaveIndex * tableSize;
    int indexLower = static_cast<int>(currentIndex);
    int indexUpper = (indexLower + 1) % tableSize;
    float fraction = static_cast<float>(currentIndex - indexLower);

    float sampleLower = wavetableData[tableOffset + indexLower];
    float sampleUpper = wavetableData[tableOffset + indexUpper];
    float interpolated = sampleLower + fraction * (sampleUpper - sampleLower);

    currentIndex += phaseIncrement;
    if (currentIndex >= tableSize) currentIndex -= tableSize;

    return interpolated;
}
```

### 6.4 Integración con el Editor MIDI

Cuando el plugin recibe el CC/SysEx de selección de onda de OSC1:
1. El `APVTS` captura el cambio (entero de 0 a 127).
2. Si el valor es < 4, activa las ondas analógicas clásicas (Saw, Pulse, etc.).
3. Si entra en el rango DWGS, llama a `setWaveformIndex(valor - desfase)`.
4. El motor DSP cambia instantáneamente el bloque de memoria → **latencia cero**.

---

## 7. Filtro Multimodo

El filtro es el componente más crítico para la identidad sonora del MS2000. Si falla aquí, la emulación sonará a "plugin digital genérico" y no al carácter agresivo, arenoso y a veces estridente que define al MS2000. [[Vintage Synth Forum](https://forum.vintagesynth.com/viewtopic.php?t=77496), [SynthMania](https://www.synthmania.com/ms2000.htm), [Daily Analog](https://dailyanalog.com/synths/korg/korg-ms2000-review/)]

### 7.1 Modos de Filtro y Datos SysEx

El tipo de filtro se controla con un único byte, dividido en 4 bloques: [[MIDI Guide](https://midi.guide/d/korg/ms2000/)]

| Rango Hex | Rango Dec | Tipo | Pendiente |
|---|---|---|---|
| 0x00 - 0x1F | 0–31 | **24dB LPF** | Paso bajo de 4 polos (estilo Moog, pegada fuerte) |
| 0x20 - 0x3F | 32–63 | **12dB LPF** | Paso bajo de 2 polos (más brillante y agresivo) |
| 0x40 - 0x5F | 64–95 | **12dB BPF** | Paso banda de 2 polos (texturas nasales) |
| 0x60 - 0x7F | 96–127 | **12dB HPF** | Paso alto de 2 polos (barrer graves) |

### 7.2 Conversión del Cutoff (Frecuencia de Corte)

El chip DSP de Korg **no** mapea el potenciómetro de 0 a 127 de forma lineal en Hertz. Sigue una **escala logarítmica exponencial** basada en semitonos para dar máxima resolución al oído humano en la zona de medios y graves.

| Valor Dec | Frecuencia Aproximada | Nota |
|---|---|---|
| 0 | ~20 Hz | Mínimo |
| 64 | ~1,000 Hz | **Punto medio del potenciómetro** (no 10kHz) |
| 127 | ~20,000 Hz (Nyquist) | Máximo |

**Fórmula de mapeo para JUCE:**
```cpp
float convertSysExToCutoffHz(uint8_t sysexByte) {
    float norm = juce::jlimit(0, 127, static_cast<int>(sysexByte)) / 127.0f;
    const float minHz = 20.0f;
    const float maxHz = 20000.0f;
    return minHz * std::pow(maxHz / minHz, norm);
}
```

### 7.3 La Resonancia y la Pérdida de Graves (El Secreto)

Este es el secreto mejor guardado de la ingeniería inversa del MS2000 y el microKORG: [[Vintage Synth Forum](https://forum.vintagesynth.com/viewtopic.php?t=77496)]

- **Pérdida de ganancia:** Al subir la Resonancia, el MS2000 **sacrifica volumen de las frecuencias graves** drásticamente para dar paso al pico de resonancia estridente. En los filtros digitales estándar de JUCE (`juce::dsp::LadderFilter`), la amplitud general se mantiene estable — hay que compensar manualmente.
- **Auto-oscilación:** A partir del valor decimal **~105**, el filtro entra en auto-oscilación pura, generando una onda senoidal por sí mismo, incluso con los osciladores apagados. [[SynthMania](https://www.synthmania.com/ms2000.htm), [Reddit: Ice Field Clone](https://www.reddit.com/r/synthrecipes/comments/k7t3yl/ms2000_ice_field_clone/)]

**Emulación en JUCE:**
```cpp
// A mayor resonancia, reducir la ganancia de entrada para "adelgazar" los graves
filter.setResonance(resParam);
float gainCompensation = 1.0f - (resParam * 0.5f);
float inputSampleProcessed = inputSample * gainCompensation;
```

### 7.4 Modulación del Filtro (Intensity y Tracking)

- **EG1 Intensity:** Va de -63 a +63 (SysEx 0 a 127, donde 64 es cero). Determina cuántas octavas se desplaza el corte siguiendo la EG1. [[MIDI Guide](https://midi.guide/d/korg/ms2000/), [Daily Analog](https://dailyanalog.com/synths/korg/korg-ms2000-review/)]
- **Keyboard Tracking (KBD):** Va de -63 a +63. En +32 (100% Tracking real), el corte se abre en perfecta sincronía armónica con la nota tocada. Si el filtro auto-oscila, se puede "tocar" el filtro como un oscilador perfectamente afinado.

**Ecuación final del Cutoff por muestra:**
```cpp
float baseCutoffHz = convertSysExToCutoffHz(sysexCutoffValue);

// Modulaciones en escala de semitonos/octavas
float egModulation = eg1CurrentValue * eg1IntensityParam;
float kbdModulation = (noteNumber - 60) * (kbdTrackParam / 64.0f);

float finalCutoffHz = baseCutoffHz * std::pow(2.0f, (egModulation + kbdModulation) / 12.0f);
finalCutoffHz = juce::jlimit(20.0f, 20000.0f, finalCutoffHz);
filter.setCutoffFrequencyHz(finalCutoffHz);
```

---

## 8. Envolventes (EG1 y EG2)

Korg documenta rigurosamente en qué bits viajan los datos, pero mantiene en absoluto secreto las fórmulas de conversión a milisegundos y la curvatura. [[Synth.Market](https://synth.market/en/catalogue/korg/ms2000/), [Sound On Sound: More About Envelopes](https://www.soundonsound.com/techniques/more-about-envelopes)]

### 8.1 Comportamiento de las Curvas

El MS2000 **no utiliza envolventes lineales**. El motor DSP imita la carga y descarga de un condensador eléctrico: [[KVR Audio](https://www.kvraudio.com/forum/viewtopic.php?t=553145)]

- **Attack:** Curva **exponencial invertida** (comienza muy rápido y se frena al llegar al pico).
- **Decay y Release:** Curvas **exponenciales puras** (caen en picado al principio y se suavizan al final).

> **Consecuencia:** Si usas rampas lineales en tu código, los sonidos tipo pluck o bajos cortados sonarán artificiales y sin fuerza.

### 8.2 Tiempos Reales Medidos (Ingeniería Inversa)

Midiendo las envolventes a través de un DAW, la comunidad determinó los tiempos reales:

| Valor Hex | Valor Dec | Tiempo Attack | Tiempo Decay/Release |
|---|---|---|---|
| `0x00` | 0 | ~1.0 ms (transitorio instantáneo) | ~2.0 ms |
| `0x10` | 16 | ~15 ms | ~25 ms |
| `0x20` | 32 | ~80 ms | ~150 ms |
| `0x40` | 64 | ~650 ms (punto medio) | ~1.2 segundos |
| `0x60` | 96 | ~3.2 segundos | ~5.5 segundos |
| `0x7F` | 127 | ~11.0 segundos (máximo) | ~20.0 segundos (máximo) |

### 8.3 Fórmula de Mapeo SysEx → Tiempo Real

```cpp
float convertSysExToEnvelopeTime(uint8_t sysexByte, bool isAttackStage) {
    float norm = juce::jlimit(0, 127, static_cast<int>(sysexByte)) / 127.0f;

    float minTime = isAttackStage ? 0.001f : 0.002f;  // 1ms o 2ms
    float maxTime = isAttackStage ? 11.0f  : 20.0f;   // 11s o 20s

    // Exponente ~2.5 para concentrar la resolución en valores bajos
    float curvedNorm = std::pow(norm, 2.5f);

    return minTime + (maxTime - minTime) * curvedNorm;
}
```

### 8.4 Coeficiente de Decay por Muestra

Para avanzar el nivel de decaimiento muestra a muestra de forma exponencial sin consumir CPU:

```cpp
// Factor de multiplicación constante por muestra (descarga del condensador)
double calculateCoef(double timeInSeconds, double sampleRate) {
    return std::exp(-1.0 / (timeInSeconds * sampleRate));
}

// En el bucle de audio:
currentLevel *= decayCoef;  // Cae exponencialmente idéntico al MS2000
```

### 8.5 Origen de los datos (Documentado vs Deducido)

| Dato | Origen |
|---|---|
| Qué byte del SysEx corresponde a EG1 Attack, etc. | **Documentado oficialmente** (MIDI Implementation Manual) |
| Rango bruto: 1 byte (7 bits), 0x00 a 0x7F | **Documentado oficialmente** |
| Tiempos en milisegundos (1ms a 11s, 2ms a 20s) | **Ingeniería inversa** (mediciones DAW) |
| Forma de la curva (exponencial) | **Ingeniería inversa** (analógico clásico) |
| Exponente de curvatura (~2.5) | **Deducido** (calibración de oído, comunidad open-source) |

---

## 9. LFOs (1 y 2)

### 9.1 Formas de Onda

| LFO | Ondas Disponibles |
|---|---|
| **LFO 1** | Saw, Square, Triangle, Sample & Hold |
| **LFO 2** | Saw, Square, Sine, Sample & Hold (adaptado) |

### 9.2 Comportamiento Libre (TEMPO_SYNC = OFF)

Korg solo documenta "LFO Frequency: 0...127". La comunidad midió el comportamiento conectando el hardware y cronometrando ciclos: [[AURA Plugins Docs](https://docs.auraplugins.com/kb/user-guide/korg-ms2kxr-getting-started/)]

- **Rango:** 0.01 Hz (un ciclo cada 100 segundos) hasta 20.0 Hz
- **Curva:** Logarítmica exponencial con exponente ~1.25 para concentrar la resolución en el rango 0.1 Hz a 5 Hz (si fuera lineal, 64 equivaldría a 10 Hz, imposibilitando ajustar velocidades lentas)

| Valor Hex | Valor Dec | Frecuencia (Hz) | Tiempo de 1 Ciclo |
|---|---|---|---|
| `0x00` | 0 | 0.010 Hz (mínimo absoluto) | ~100 seg |
| `0x10` | 16 | 0.035 Hz | ~28.5 seg |
| `0x20` | 32 | 0.100 Hz | ~10 seg |
| `0x40` | 64 | 0.750 Hz (punto medio en panel) | ~1.33 seg |
| `0x60` | 96 | 4.200 Hz (vibrato típico) | ~0.24 seg |
| `0x70` | 112 | 10.00 Hz | ~0.10 seg |
| `0x7F` | 127 | 20.00 Hz (máximo en panel) | ~0.05 seg (50 ms) |

**Fórmula Exponencial Continua:**

```
f(Hz) = f_min × (f_max / f_min) ^ (x ^ p)
```
Donde: `f_min = 0.01 Hz`, `f_max = 20.0 Hz`, `p ≈ 1.25`, `x = SysExVal / 127.0`

**Implementación en C++:**
```cpp
float convertSysExToLFOFreq(uint8_t sysexByte) {
    int rawValue = juce::jlimit(0, 127, static_cast<int>(sysexByte));
    float norm = rawValue / 127.0f;
    const float minHz = 0.01f;
    const float maxHz = 20.0f;
    return minHz * std::pow(maxHz / minHz, std::pow(norm, 1.25f));
}
```

### 9.3 Comportamiento Sincronizado (TEMPO_SYNC = ON)

El byte 0x00..0x7F mapea a subdivisiones rítmicas relativas al `AudioPlayHead` del DAW:

```cpp
enum class LFOSyncDivision {
    Whole_1_1 = 0,
    ThreeFourths_3_4,
    Half_1_2,
    Triplet_1_2T,
    Quarter_1_4,
    Triplet_1_4T,
    Eighth_1_8,
    Triplet_1_8T,
    Sixteenth_1_16,
    ThirtySecond_1_32
};
```

### 9.4 Origen de los datos

| Dato | Origen |
|---|---|
| Ubicación del byte en SysEx | **Documentado oficialmente** |
| Rango bruto: 0x00 a 0x7F | **Documentado oficialmente** |
| Tabla de subdivisiones Tempo Sync | **Documentado en manual de usuario** |
| Rango en Hz (0.01 a 20 Hz) | **Ingeniería inversa** (mediciones con osciloscopio/DAW) |
| Constante exponencial (1.25) | **Deducido** (mediciones en puntos clave 16, 32, 64, 96) |

---

## 10. Distorsión (Amp Distortion)

No existe **ningún tipo** de documentación técnica oficial, código o esquema matemático público sobre el algoritmo de distorsión. Korg jamás liberó esa información; al estar en el chip DSP propietario (era Prophecy/Z1), sigue siendo una caja negra a nivel de firmware. [[dtech.lv](https://www.dtech.lv/techarticles_korg_dsps.html), [Daily Analog](https://dailyanalog.com/synths/korg/korg-ms2000-review/)]

Sin embargo, la comunidad de ingeniería inversa ha descifrado por completo el comportamiento. [[Reddit: MS2000 Tips](https://www.reddit.com/r/synthesizers/comments/572m80/just_got_a_korg_ms2000_any_tipstricksadvice/), [MIDI Guide](https://midi.guide/d/korg/ms2000/), [SynthMania](https://www.synthmania.com/ms2000.htm)]

### 10.1 Comportamiento Oculto

- El parámetro `Amp Distortion` solo tiene **dos estados: ON u OFF** (SysEx/CC: 0-63=Off, 64-127=On). **No hay** control de "Gain" o "Drive".
- La clave: la cantidad de distorsión está directamente vinculada al **volumen de los osciladores en el Mixer**. [[Reddit](https://www.reddit.com/r/synthesizers/comments/572m80/just_got_a_korg_ms2000_any_tipstricksadvice/)]
  - OSC1, OSC2 y Noise al máximo (127) → la señal entra "caliente" → la distorsión destruye la señal con sonido agresivo y roto.
  - Mixer a valores cercanos a 40-50 → la distorsión apenas añade coloración o armónicos suaves (overdrive sutil).

### 10.2 Tipo de Distorsión

**Saturación Asimétrica de Onda de Modelado Analógico (Asymmetric Waveshaping):**

1. Amplifica masivamente la señal de entrada mediante una ganancia fija (boost interno).
2. Aplica una función de transferencia **no lineal** (tanh o polinómica) para Soft Clipping.
3. Modifica ligeramente el eje vertical para que el **semiciclo positivo se deforme diferente al negativo** → genera armónicos pares → la distorsión suena "musical", "gorda" y con textura analógica.

### 10.3 Implementación en JUCE

```cpp
float processMS2000Distortion(float inputSample, bool isDistortionButtonOn) {
    if (!isDistortionButtonOn) return inputSample;

    // 1. Boost de entrada (el MS2000 satura más cuanto más fuerte es la señal)
    float x = inputSample * 2.5f;

    // 2. Saturación Asimétrica (ingeniería inversa de Soft-Clipping)
    float saturatedSample;
    if (x > 0.0f) {
        // Semiciclo positivo: curva más suave (válvula idealizada)
        saturatedSample = x / (1.0f + std::abs(x));
    } else {
        // Semiciclo negativo: curva ligeramente más abrupta (armónicos pares)
        saturatedSample = x / (1.1f + std::abs(x));
    }

    // 3. Atenuación de salida para evitar clipping digital
    return saturatedSample * 0.4f;
}
```

### 10.4 Posición en la Cadena de Audio

La distorsión va **inmediatamente después del filtro** y **antes del amplificador** (EG2/Volumen):

```
[ Mixer ] → [ Filtro ] → [ DISTORSIÓN ] → [ Amplificador (EG2) ] → Salida
```

Por eso, si el filtro resuena mucho, la distorsión se vuelve extremadamente agresiva.

---

## 11. Secuenciador de Modulación (Mod Sequence)

El Mod Sequence es, junto con los filtros, el corazón de la identidad del Korg MS2000. Es el elemento que lo diferenciaba de su hermano menor, el microKORG.

### 11.1 Arquitectura

- **3 pistas independientes** (Secuencias A, B y C)
- **16 pasos** cada una (Steps). Cada paso almacena un valor de 7 bits (0-127), centrado en 64 para bipolares.
- **Modos de reproducción:** Forward, Backward, Bounce, Random
- **Resolución temporal:** Sincronizado al BPM en subdivisiones (1/1 hasta 1/16 o fusas)

### 11.2 Los Dos Modos de Transición

- **STEP:** El valor cambia instantáneamente al inicio de cada paso (escalón cuadrado puro). Suena como un arpegiador por bloques marcados.
- **SMOOTH:** Interpolación lineal continua entre el paso actual y el siguiente, **pasada por un filtro paso-bajo (Slew Limiter, ~10ms)** para suavizar las esquinas del cambio. Evita clics digitales y hace que los barridos de filtro suenen orgánicos, como si movieras un potenciómetro real con la mano.

### 11.3 Datos SysEx (Documentado Oficialmente)

```cpp
struct ModSequenceTrack {
    uint8_t steps[16] = { 64 };  // Punto medio (0 modulación)
    uint8_t targetParameter = 0;  // Índice del destino
    bool isSmooth = false;        // false=Step, true=Smooth
    int numSteps = 16;            // Longitud (1 a 16)
};
ModSequenceTrack seqA, seqB, seqC;
```

### 11.4 Implementación del Motor

```cpp
class MS2000ModSequencer {
public:
    void prepare(double sampleRate) {
        currentSampleRate = sampleRate;
        smoothedValue.reset(sampleRate, 0.010);  // Slew Limiter de 10ms
    }

    float processSample(const juce::AudioPlayHead::PositionInfo& posInfo,
                        const ModSequenceTrack& track)
    {
        if (!posInfo.getBpm().hasValue() || !posInfo.getPpqPosition().hasValue())
            return 0.0f;

        double ppq = *posInfo.getPpqPosition();
        double stepsPerQuarter = 4.0;  // Semicorcheas
        double exactStepPosition = ppq * stepsPerQuarter;
        int currentStep = static_cast<int>(exactStepPosition) % track.numSteps;
        int nextStep = (currentStep + 1) % track.numSteps;

        float currentStepVal = (track.steps[currentStep] - 64) / 63.0f;
        float nextStepVal = (track.steps[nextStep] - 64) / 63.0f;

        if (track.isSmooth) {
            float stepFraction = static_cast<float>(
                exactStepPosition - std::floor(exactStepPosition));
            float linearInterp = currentStepVal +
                stepFraction * (nextStepVal - currentStepVal);
            smoothedValue.setTargetValue(linearInterp);
            return smoothedValue.getNextValue();
        } else {
            return currentStepVal;
        }
    }

private:
    double currentSampleRate = 44100.0;
    juce::LinearSmoothedValue<float> smoothedValue;  // Emulador de Slew Limiter
};
```

---

## 12. Arpegiador / Step Sequencer

El MS2000 gestiona **dos secuenciadores en paralelo** que corren al mismo ritmo pero hacen tareas distintas:

| Característica | Mod Sequence | Arpeggiator |
|---|---|---|
| **Qué graba** | Datos de control (knobs, voltajes) | Notas musicales (pitches, silencios) |
| **Destino** | Filtros, LFOs, PWM, parámetros internos | Osciladores (melodías directas) |
| **Polifonía** | Monofónico por pista | Polifónico (acordes por paso) |
| **Longitud** | 16 pasos | Hasta 8 pasos (Step Base) o patrones de arpegio |

### 12.1 Modos del Arpegiador

- **Up:** Notas ascendentes
- **Down:** Notas descendentes
- **Alt 1 / Alt 2:** Alternando arriba/abajo
- **Random:** Orden aleatorio
- **Trigger (Paso a Paso):** Dispara el acorde completo al unísono en los pasos que estén iluminados en el panel frontal. Si tocas un acorde, el MS2000 **no** arpegia las notas una a una.

### 12.2 Parámetros SysEx

- `ARPEGGIATOR_SWITCH`: 0x00=Off, 0x7F=On
- `ARPEGGIATOR_RANGE`: 1, 2, 3 o 4 octavas
- `ARPEGGIATOR_TYPE`: Selector del algoritmo (Up, Down, Alt1, Alt2, Random, Trigger)
- `ARPEGGIATOR_GATE`: Duración de las notas (0%=transitorio hasta 100%=ligado)

### 12.3 Implementación en JUCE (MidiBuffer)

El arpegiador genera **mensajes MIDI puros**, no señales de audio. Se debe inyectar la lógica en el `juce::MidiBuffer` dentro del `processBlock`, leyendo la posición temporal desde el `juce::AudioPlayHead`:

```cpp
void MS2000Processor::processArpeggiator(
    juce::MidiBuffer& midiMessages,
    const juce::AudioPlayHead::PositionInfo& posInfo)
{
    if (!arpeggiatorEnabled) return;

    double ppq = *posInfo.getPpqPosition();

    if (isNewStepTrigger(ppq)) {
        sendMidiNoteOffEvents(midiMessages);
        auto nextNotes = calculateNextArpeggioStep();
        for (auto note : nextNotes) {
            midiMessages.addEvent(
                juce::MidiMessage::noteOn(1, note, 0.8f),
                currentSampleOffset);
        }
    }
}
```

---

## 13. Virtual Patch (Matriz de Modulación)

El MS2000 permite realizar **4 interconexiones personalizadas** (ruteos de modulación) entre orígenes y destinos.

### 13.1 Orígenes de Modulación (Sources)

| Source | Descripción |
|---|---|
| EG 1 | Envolvente del filtro |
| EG 2 | Envolvente del amplificador |
| LFO 1 | Oscilador de baja frecuencia 1 |
| LFO 2 | Oscilador de baja frecuencia 2 |
| Velocity | Velocidad del teclado |
| KBD Track | Posición de la nota en el teclado |
| Pitch Bend | Rueda de afinación |
| Mod Wheel | MIDI CC#01 / Rueda de modulación |

### 13.2 Destinos de Modulación (Destinations)

| Destination | Descripción |
|---|---|
| Pitch | Desplaza la afinación de ambos osciladores a la vez |
| OSC 2 Pitch | Modula solo la afinación de OSC2 |
| OSC 1 Control 1 | Modula el parámetro especial de OSC1 (PWM, formante, FM depth) |
| VCF Cutoff | Abre o cierra la frecuencia de corte del filtro |
| VCA Amplification | Modula el volumen (trémolo) |
| Pan | Mueve el sonido de izquierda a derecha en el estéreo |

### 13.3 Intensidad

El potenciómetro de intensidad va de **-63 a +63** (SysEx/MIDI CC de 0 a 127, donde 64 es cero modulación).

**Implementación en JUCE:**
```cpp
// Recolectar valores de moduladores antes de procesar audio
float lfo1Val = lfo1.getNextSample();
float eg1Val  = eg1.getNextSample();

// Procesar la matriz virtual (Slot 1: LFO1 -> Cutoff)
float cutoffModulation = 0.0f;
if (patch1Source == Source::LFO1 && patch1Destination == Destination::Cutoff) {
    cutoffModulation += lfo1Val * patch1Intensity;
}

// Inyectar en el componente final
float finalCutoff = baseCutoffHz + (cutoffModulation * rangoEnOctavas);
filtro.setCutoffFrequencyHz(finalCutoff);
```

---

## 14. Efectos (EQ, Mod FX, Delay)

El MS2000 cuenta con **tres bloques de efectos en serie** al final de la cadena de audio. **No tiene reverberación (reverb)**, pero sus algoritmos de delay y modulación definen la profundidad espacial. [[Sound On Sound](https://www.soundonsound.com/reviews/korg-ms2000), [SynthMania](https://www.synthmania.com/ms2000.htm)]

```
[ Salida VCA ] → [ 1. EQ ] → [ 2. Mod FX ] → [ 3. Delay FX ] → Salida Principal
```

### 14.1 Bloque 1: Ecualizador (EQ)

EQ paramétrico/shelving de 2 bandas (Graves y Agudos). Su rango permite moldear la respuesta final antes de los efectos de tiempo.

**Para JUCE:** Usar `juce::dsp::IIR::Filter` configurada como `makeLowShelf` y `makeHighShelf`.

*(Ver Sección 16 para las frecuencias exactas.)*

### 14.2 Bloque 2: Efectos de Modulación (Mod FX)

Tres algoritmos basados en líneas de retardo corto con LFO interno: [[Vintage Synth Explorer](https://www.vintagesynth.com/korg/ms2000), [ManualsLib](https://www.manualslib.com/manual/1072845/Korg-Ms2000.html)]

| Algoritmo | Descripción |
|---|---|
| **Chorus / Flanger** | Duplicado de señal con retraso de tiempo modulado. Ensancha el sonido estéreo o crea barrido metálico "jet". |
| **Ensemble** | Múltiples líneas de modulación desfasadas (estilo String Machine de los 70). Grosor coral masivo para pads. |
| **Phaser** | Modula la fase creando cancelaciones armónicas en movimiento. Texturas vintage psicodélicas. |

**Para JUCE:** Usar `juce::dsp::Chorus` y `juce::dsp::Phaser`. Para el Ensemble, configurar un Chorus a 3 voces con retardo base específico.

### 14.3 Bloque 3: Delay FX

Tres tipos de Delay con sincronización al reloj MIDI del DAW (BPM Sync): [[ManualsLib](https://www.manualslib.com/manual/1072845/Korg-Ms2000.html)]

| Tipo | Descripción |
|---|---|
| **Stereo Delay** | Retraso estéreo convencional (L y R en paralelo) |
| **Cross Delay (Ping-Pong)** | Repeticiones que rebotan L ↔ R |
| **L/R Delay** | Desfase de tiempo entre canales L y R (ensancha la imagen estéreo) |

**Para JUCE:** Crear estructura con `juce::dsp::DelayLine<float>` configurando un búfer circular estéreo con lectura cruzada para el modo Cross Delay.

---

## 15. Componentes Extra

### 15.1 Vocoder (16 bandas)

Emulación digital del clásico Korg VC-10. [[Sound On Sound](https://www.soundonsound.com/reviews/korg-ms2000)]

- **16 filtros paso-banda** de análisis (Audio In 2 como modulador) y síntesis (osciladores como portadora)
- Ganancia y panorama ajustables por banda individualmente
- **Formant Shift:** Desplaza las frecuencias de los filtros de análisis arriba o abajo (transformar voz masculina ↔ femenina, cambiar "tamaño de garganta virtual")
- **Vocal Distortion / HPF Gate:** Circuito **detector de transitorios** que extrae las consonantes de la voz (S, P, T) y las mezcla con ruido blanco puro para asegurar la **inteligibilidad** del vocoder. **Sin este componente, las palabras sonarán borrosas.**

### 15.2 Envelope Follower

Inyectando audio por Audio In 1, analiza la amplitud de la señal entrante en tiempo real y la convierte en un **voltaje de control dinámico** asignable como fuente en el Virtual Patch.

### 15.3 Portamento / Glide

Módulo de tiempo que ralentiza la transición de tono entre notas en modo monofónico o al unísono.

### 15.4 AMP Section (Amplificador + Panpot)

Ubicado entre la distorsión y los efectos. Tres parámetros independientes: [[Vintage Synth Explorer](https://www.vintagesynth.com/korg/ms2000)]

| Parámetro | Rango | Descripción |
|---|---|---|
| **Amp Level** | 0-127 | Volumen maestro del timbre |
| **Panpot** | 0-127 (64=centro) | Posición estéreo. Destino muy común del Virtual Patch con LFO |
| **KBD Track del VCA** | -63 a +63 | Modifica volumen según altura de nota. Valores negativos → notas agudas suenan más flojas (imitar pianos/acústicos) |

### 15.5 Voice Assign

El comportamiento de las 4 voces depende de este bloque:

| Modo | Descripción |
|---|---|
| **Mono** | Monofónico (1 sola nota a la vez) |
| **Poly** | Polifónico de 4 voces |
| **Unison** | Apila las 4 voces digitales sobre una sola nota |

**Algoritmo de Unison (Detune & Spread):**
- **Unison Detune:** Desafina las 4 voces entre sí (0-99 cents) para sonido super grueso (bajos y leads masivos).
- **Unison Spread:** Distribuye las 4 voces en el espectro estéreo (2 voces a la izquierda, 2 a la derecha).

### 15.6 Timbres y Modos de Programa

A nivel de código, el MS2000 clona su motor en dos capas independientes: [[Wikipedia](https://en.wikipedia.org/wiki/Korg_MS2000), [Sound On Sound](https://www.soundonsound.com/reviews/korg-ms2000)]

| Modo | Descripción | Voces |
|---|---|---|
| **Single** | 1 timbre | 4 voces máx |
| **Layer (Dual)** | 2 timbres superpuestos | 2 voces cada uno |
| **Split** | 2 timbres divididos por punto de split | 2 voces cada uno |

---

## 16. Frecuencias Exactas del EQ y Escalas del Virtual Patch

Al no figurar en los manuales estándar, estos datos proceden de la ingeniería inversa aplicada al DSP original y a su gemelo de código, el microKORG.

### 16.1 Frecuencias del Ecualizador

El MS2000 implementa un ecualizador shelving de 2 bandas con rangos de corte configurables por software. Ambas bandas se comportan como filtros tipo Low Shelf y High Shelf (1er/2º orden), procesados de forma global al final de la cadena de voz, inmediatamente antes de los procesadores de efectos estéreo. [[Synth.Market](https://synth.market/en/catalogue/korg/ms2000/)]

**Fuente 1 (microKORG Manual, arquitectura compartida):** Rangos continuos [[Scribd: microKORG Manual](https://it.scribd.com/doc/50402759/400-manuale-microkorg-ita), [Scribd: MicroKORG Manual EN](https://www.scribd.com/document/41097209/MicroKORG-Manual)]

| Banda | Ganancia | Rango de Frecuencia |
|---|---|---|
| **Low EQ** | ±12 dB (0-127, donde 64 = 0dB) | **40 Hz a 1.00 kHz** (mapeado de 0 a 127) |
| **High EQ** | ±12 dB (0-127, donde 64 = 0dB) | **1.00 kHz a 18.0 kHz** (mapeado de 0 a 127) |

**Fuente 2 (Ingeniería inversa del DSP):** Valores fijos por bloques [[YouTube: MS2000 Pad Tutorial](https://www.youtube.com/watch?v=SbtBBgn3-ZU&t=1159), [Sound On Sound](https://www.soundonsound.com/reviews/korg-ms2000)]

**Low EQ Freq (4 valores fijos indexados por bloques SysEx):**
| Rango SysEx | Frecuencia |
|---|---|
| 0x00 - 0x1F (0-31) | 160 Hz |
| 0x20 - 0x3F (32-63) | 250 Hz (por defecto) |
| 0x40 - 0x5F (64-95) | 400 Hz |
| 0x60 - 0x7F (96-127) | 600 Hz |

**High EQ Freq (4 valores fijos):**
| Rango SysEx | Frecuencia |
|---|---|
| 0x00 - 0x1F (0-31) | 4.0 kHz |
| 0x20 - 0x3F (32-63) | 6.0 kHz |
| 0x40 - 0x5F (64-95) | 8.0 kHz (por defecto) |
| 0x60 - 0x7F (96-127) | 12.0 kHz |

> **Nota:** Las dos fuentes difieren ligeramente. Es posible que el MS2000 use los 4 valores fijos mientras que el microKORG use el rango continuo, o viceversa. Ambos datos se conservan aquí para validar contra el hardware durante el desarrollo.

**Para JUCE:** Implementar `juce::dsp::IIR::Filter` como `makeLowShelf` / `makeHighShelf`. Mapear frecuencia según la fuente que mejor coincida con el hardware medido.

### 16.2 Escala Máxima del Virtual Patch (Intensidad)

La escala de afectación real cuando la intensidad (±63) está al extremo: [[YouTube: MS2000 Tutorial](https://www.youtube.com/watch?v=7fLA01y1tu4&t=33)]

| Destino | Escala Máxima (±63) | Fórmula / Detalle |
|---|---|---|
| **PITCH (OSC1+2)** | ±2 Octavas (±24 semitonos) | Cada paso ≈ 0.38 semitonos (38.1 cents). `Mod = (Intensity / 63.0) * 24.0` |
| **OSC2 PITCH** | ±2 Octavas (±24 semitonos) | Igual que Pitch |
| **VCF CUTOFF** | ±4 a 5 Octavas | Si filtro base a 500 Hz, modulación máxima abre hasta 16 kHz (`500 * 2^5`) |
| **OSC1 CTRL 1** | 100% del rango del parámetro | Barre el PWM de 0% a ~100%, o la totalidad del índice FM/Vox |
| **AMP LEVEL (VCA)** | 100% de la amplitud | Permite cierres totales de compuerta (tremolo/gating) |
| **PANPOT** | 100% L a 100% R | Si el Pan base está centrado, un LFO forzará la señal de un extremo al otro |

---

## 17. Flujo de Señal Completo y Orden de Procesamiento

El flujo de audio en el MS2000 es **estrictamente lineal** de izquierda a derecha. Es fundamental respetar este orden en `processBlock`:

```
┌─────────┐     ┌─────────┐
│  OSC 1  │────►│         │     ┌────────┐     ┌────────────┐     ┌─────────┐
│(VA/DWGS)│     │  MIXER  │────►│ FILTER │────►│ DISTORTION │────►│   AMP   │
│  OSC 2  │────►│(+Ring/  │     │(Multi- │     │ (Soft-Clip │     │(EG2+VCA)│
│  (VA)   │     │ Sync)   │     │ mode)  │     │ asimétrico)│     └────┬────┘
└─────────┘     └─────────┘     └────────┘     └────────────┘          │
                                     ▲                                  ▼
                                  ┌──┴──┐                          ┌────────┐
                                  │ EG1 │                          │  EQ    │
                                  │(Flt)│                          └────┬───┘
                                  └─────┘                               │
                                     ▲                              ┌───▼───┐
                              ┌──────┴─────────────────────┐        │Mod FX │
                              │  LFO1  │  LFO2  │ ModSeq   │        └───┬───┘
                              └────────┴────────┴──────────┘            │
                              ┌────────────────────────────┐        ┌───▼───┐
                              │   Virtual Patch (4 slots)  │        │ Delay │
                              └────────────────────────────┘        └───┬───┘
                                                                        │
                                                                        ▼
                                                                   [ SALIDA ]
```

---

## 18. Estructura de Clases Recomendada para JUCE

```
MS2000AudioProcessor (Clase Principal)
│
├── MS2000SysExParser     (Decodifica los bytes físicos del hardware)
│
├── MS2000Arpeggiator     (Gestiona notas rítmicas en el MidiBuffer)
│
└── MS2000SynthVoice      (Instanciada 4 veces para la polifonía)
    ├── MS2000OscillatorBank  (OSC1 con 71 ondas, OSC2, CrossMod y Mixer)
    ├── MS2000Filter          (LPF/BPF/HPF con compensación de graves por Resonancia)
    ├── MS2000Distortion      (Algoritmo asimétrico no lineal)
    ├── MS2000Amp             (Volume, Pan y KBD Track)
    ├── MS2000ModSequencer    (3 pistas de 16 pasos con Slew Limiter para Smooth)
    ├── MS2000EnvelopeGenerator (EG1 y EG2 exponenciales)
    └── MS2000LFO             (LFO1 y LFO2 libres o sincronizados)
```

---

*Última actualización: 2026-08-24*
*Fuentes principales: Korg MIDI Implementation Manual, ReMS2000 (GitHub), AURA Plugins, SynthMania, Sound On Sound, Vintage Synth Forum, Renoise Forums, microKORG Manual (Scribd)*
