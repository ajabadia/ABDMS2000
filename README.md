# ABDMS2000 — Virtual Korg MS2000 & microKORG (con Modo Avanzado)

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)
[![JUCE 8](https://img.shields.io/badge/JUCE-8.0-orange.svg)](https://juce.com/)
[![WebUI](https://img.shields.io/badge/UI-WebView2%20%2F%20HTML%2FCSS%2FJS-brightgreen.svg)]()
[![WASM](https://img.shields.io/badge/WASM-AudioWorklet-purple.svg)]()
[![License](https://img.shields.io/badge/License-Proprietary-red.svg)]()

**ABDMS2000** es un sintetizador virtual analógico híbrido y editor/gestor integral basado en la arquitectura de síntesis del **Korg MS2000** y **microKORG**, incorporando además un **Modo Avanzado (ABD Ultra Engine)**.

---

## 🌟 Características Principales

1. **Arquitectura Tri-Modo conmutable:**
   - **Modo Korg MS2000:** Réplica fiel con panel analógico azul, potenciómetros dedicados, pantalla LCD de 2 líneas (menús oficiales 1A-4D) y Secuenciador de Modulación de 3 pistas × 16 pasos.
   - **Modo microKORG:** Panel retro con matriz gráfica, diales de género musical, perillas de edición dinámica y display LED de 3 dígitos.
   - **Modo Avanzado (ABD Ultra):** Polifonía de hasta 32 voces, Reverb Shimmer, Tape Delay analógico, 4 LFOs, 4 Envolventes DAHDSR y Secuenciador polifónico de 64 pasos.
2. **Motor de Síntesis Fiel al Hardware:**
   - 2 Osciladores (68 ondas en OSC 1 incluyendo las 64 tablas DWGS + 3 en OSC 2 + Noise + Audio In).
   - Ring Modulation, Hard Sync y Cross Mod (FM).
   - Filtro Multimodo (LP24, LP12, BP12, HP12) con modelado no lineal y compensación de ganancia analógica.
   - Distorsión asimétrica post-filtro dependiente del nivel de mezcla.
   - Vocoder de 16 bandas con Formant Shift y HPF Gate (detector de transitorios para consonantes).
3. **Telemetría y Control MIDI / SysEx Bidireccional:**
   - Control en tiempo real del hardware físico vía CCs y NRPNs.
   - Petición y recepción de volcados (*Program Dump* y *All Data Dump*) mediante códec Korg 7-to-8 bits.
   - Pantalla dedicada **SysEx Inspector** con visor/editor Hexadecimal y copiado/pegado.
4. **Gestor Universal de Bancos (ABD Universal Bank Manager):**
   - Sistema de adaptadores de modelo (`ISynthModelContract`) y tokens de diseño CSS.
   - Almacenamiento infinito, búsqueda instantánea, categorización, tags y comparador visual A/B (Diff Viewer).
5. **Teclado Virtual Interactivo (Estilo ABDEep):**
   - Ruedas físicas de Pitch Bend y Mod Wheel.
   - Indicadores LED de transposición de octavas (Apagado / Parpadeo / Fijo).
   - Barra de luces LED con animaciones de cambio de patch (*Sweep Wave*), cambio de banco (*Ripple Pulse*) y pánico (*Flash Cut*).
6. **Multiplataforma:**
   - Standalone y VST3 para Windows (WebView2) y macOS (WKWebView).
   - Versión Web PWA compilada a WebAssembly (AudioWorklet) con IndexedDB.
   - Modo Headless / Servidor RPC para Raspberry Pi.

---

## 📁 Arquitectura General del Repositorio

```text
ABDMS2000/
├── CMakeLists.txt                    # Configuración principal de build (JUCE 8 + C++20)
├── build.bat                         # Script de compilación Standalone + VST3
├── ROADMAP.md                        # Seguimiento y validación de fases 0 a 9
├── implementation_plan.md            # Especificación técnica exhaustiva
├── DOCS/                             # Especificaciones de ingeniería inversa y SysEx
├── schemas/                          # Contratos JSON canónicos
├── Scripts/                          # Generadores de código y empaquetadores
│
├── Source/
│   ├── Core/                         # Motor de Síntesis y Telemetría RT
│   │   ├── SynthEngine.h / .cpp      # Orquestador del hilo de audio y cadena DSP
│   │   ├── Voice.h / .cpp            # Voz individual analógico-virtual
│   │   ├── VoiceManager.h / .cpp     # Asignador de polifonía (4 voces) y robo inteligente
│   │   ├── VoiceParameters.h         # Estructura POD de parámetros por bloque
│   │   └── AudioThreadSnapshot.h     # Estructura POD Lock-Free para telemetría a 60 FPS
│   │
│   ├── DSP/                          # Algoritmos DSP y Procesamiento de Señal
│   │   ├── Oscillators/              # PolyBLEP (Saw/Pulse/Tri/Sine), DWGS (64 ondas), VoxWave
│   │   ├── Filters/                  # VCF Multimodo (24dB LPF, 12dB LPF/BPF/HPF) ZDF/TPT
│   │   ├── Envelopes/                # Generadores ADSR exponenciales analógicos
│   │   ├── LFO/                      # LFOs 1 & 2 sincronizados por fase métrica al DAW
│   │   ├── Modulation/               # Virtual Patch (4 slots), ModSequencer (3x16), Arp, Portamento
│   │   ├── FX/                       # ModFX (Chorus/Ensemble/Phaser), DelayFX, Equalizer
│   │   └── Vocoder/                  # Vocoder 16 bandas unrolled, Follower y Bus HPF Sibilancia
│   │
│   ├── MIDI/                         # Telemetría MIDI y Sistema Exclusivo (SysEx)
│   │   ├── MIDIMap.h / .cpp          # Tabla de 74 Control Changes canónicos
│   │   ├── NRPNParser.h / .cpp       # Intérprete y serializador de parámetros de 14 bits
│   │   ├── MIDITelemetryManager.h    # Filtro anti-eco y gestor bidireccional MIDI
│   │   ├── SysExCodec.h / .cpp       # Algoritmo de empaquetado/desempaquetado 7 <-> 8 bits
│   │   ├── MS2000ProgramData.h       # Mapa binario estructurado de 256 bytes de memoria
│   │   ├── SysExManager.h / .cpp     # Gestor de volcados (.syx / .mid), banco 128 programas
│   │   └── MS2000SysExExporter.h     # Exportador de archivos .syx para hardware físico
│   │
│   ├── State/                        # Estado, Parches y Presets
│   │   ├── ParameterRegistry.gen.h   # Árbol canónico APVTS generado automáticamente
│   │   ├── MS2000PatchBuilder.h      # Inicializador "Init Synth" y Randomizador Acotado
│   │   ├── MS2000FactoryBank.h       # Banco de presets de fábrica grabado en binario
│   │   └── LCDMenuFormatter.h        # Motor de texto para pantalla LCD 16x2
│   │
│   ├── Plugin/                       # Wrappers de Plugin e Interfaz Gráfica
│   │   ├── PluginProcessor.h / .cpp  # Implementación AudioProcessor y getState/setState
│   │   ├── PluginEditor.h / .cpp     # Contenedor WebView2 y componentes nativos
│   │   ├── PluginEditor_ResourceProvider.h / .cpp  # Servidor de recursos embebidos y disco
│   │   └── BridgeActions.h / .cpp    # Handlers IPC entre JavaScript y C++
│   │
│   └── Tests/                        # Pruebas Unitarias de Rendimiento y Paridad
│       └── DSPCoreTests.cpp          # Suite de verificación DSP, MIDI y códec SysEx
│
├── WebUI/                            # Interfaz de Usuario Gráfica (HTML5, CSS3, JS)
│   ├── index.html                    # Layout principal y modales
│   ├── src/                          # Módulos JS (app.js, bridge.js, panel*.js, bankManager.js)
│   └── styles/                       # CSS Tokens y temas (themes.css, main.css)
│
└── wasm/                             # Pipeline de compilación WebAssembly (AudioWorklet)
```

---

## 🛠️ Firmas Técnicas y Contratos de Métodos Clave

### 1. Sistema de Control de Voces y Afinación
* **`VoiceManager::noteOn(int midiNote, float velocity)`**:
  Registra eventos de nota presionada. En modo `Mono/Unison`, implementa prioridad de última nota. En modo `Poly`, administra la asignación a las 4 voces analógico-virtuales con algoritmo de ladrón de voces estricto (*Priority 1: Release más tenue; Priority 2: FIFO en Sustain; Priority 3: Re-trigger de misma nota sin clics*).
* **`PortamentoGlide::process(double targetFreq) -> double`**:
  Implementa el *Slew Limiter* exponencial muestra a muestra con coeficiente $\alpha = \exp(-1.0 / (T \times f_s))$. Si `noteCounter == 1`, salta de frecuencia instantáneamente para ejecutar *Fingered/Legato Glide*.

---

### 2. Motor DSP del Vocoder Polifónico Multimodo
* **`Vocoder16Band::process(float modSample, float carrierSample, float& outLeft, float& outRight)`**:
  Ejecuta la modulación cruzada de las 16 bandas fijas mediante **Loop Unrolling** total sin condiciones de salto (*zero branch overhead*). Procesa el bus paralelo de sibilancia a $8.0\text{ kHz}$ con inyección de ruido blanco o bypass de audio de entrada según `hpfGate`.

---

### 3. Matriz de Modulación Virtual (*Virtual Patch*)
* **`ModMatrix::process(size_t slotIndex, float sourceValue) -> float`**:
  Calcula el aporte de modulación de los 4 *slots* asignables:
  * `VCF Cutoff`: Escala de 5 Octavas exponenciales.
  * `Pitch / OSC2 Tune`: Escala rígida de $\pm 24$ semitonos (2 octavas).

---

### 4. Conectividad Externa y Serialización (SysEx / Persistencia)
* **`SysExCodec::unpack7to8(...)` & `SysExCodec::pack8to7(...)`**:
  Codificación/decodificación sin pérdidas de bloques de 7 bytes de 8 bits a 8 bytes de 7 bits con recolección de MSB.
* **`ABDMS2000AudioProcessor::getStateInformation(...) / setStateInformation(...)`**:
  Serializa el árbol de parámetros `APVTS` y metadatos persistentes (`currentProgramIndex`, `currentProgramName` de 12 caracteres) en flujo XML binario comprimido para guardado/restauración de sesiones en el DAW con protección anti-clics vía `LinearSmoothedValue`.

---

### 5. Generador Estocástico Inteligente
* **`MS2000PatchBuilder::buildMusicalRandomPatch(juce::AudioProcessorValueTreeState& apvts)`**:
  Algoritmo estocástico acotado por zonas de síntesis que previene sonidos nulos, asegurando presencia en osciladores ($80 - 127$), filtro en zona dulce ($40 - 127$), y acoplamiento dinámico de envolventes contra silencios.

---

## 🚦 Protocolo de Integración Continua y Buenas Prácticas

1. **Protección Real-Time en Audio Thread**: Prohibida la asignación dinámica de memoria (`new`/`malloc`), redimensionamiento de contenedores o llamadas bloqueantes en `processBlock` y métodos de audio DSP.
2. **Compilación en Release para Profiling**: Para medir el rendimiento real de los LFOs sincronizados y las cadenas del vocoder, compilar en modo **Release** (`-O3` / `/O2`) aprovechando vectorización SIMD / AVX.
3. **Desacoplo de Componentes Gráficos**: Todo componente al que se le asigne un `LookAndFeel` debe llamar estrictamente a `setLookAndFeel(nullptr)` en el destructor de su contenedor para evitar fugas de memoria o punteros colgantes (*Dangling Pointers*).

---

## 🛠️ Compilación

### Requisitos
- **Windows:** Visual Studio 2022 (MSVC C++20) o Clang + CMake 3.22+
- **Node.js:** v18+ (para generadores y tests de UI)
- **Emscripten SDK:** (opcional, para compilar la versión WASM)

### Construcción
```bash
# Compilación completa (Genera Standalone y VST3 en build/)
build.bat
```

---

*Desarrollado por ajabadia — Suite de Sintetizadores ABD.*

