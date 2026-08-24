# ABDMS2000 — Virtual Korg MS2000 & microKORG (con Modo Avanzado)

## 1. Visión General del Proyecto

**ABDMS2000** es un sintetizador virtual híbrido que emula tanto el **Korg MS2000** como el **microKORG**, además de ofrecer un **Modo Avanzado (ABD Ultra)**. Integra cuatro roles funcionales:

1. **Controlador MIDI del hardware** — enviar CCs/SysEx al MS2000 y al microKORG físico
2. **Receptor de control hardware** — el hardware físico controla el software de forma bidireccional
3. **Gestor universal de bancos de sonidos** — enviar/recibir/organizar/filtrar bancos desde/hacia el hardware y en local
4. **Motor de síntesis propio multi-modo** — reproducir sonidos con un engine VA idéntico al hardware y expandido en modo avanzado

---

### 1.1 Arquitectura Tri-Modo en un Mismo Plugin

El plugin permite alternar instantáneamente entre 3 perfiles operativos desde un selector central en la barra superior (`[ MS2000 | microKORG | ADVANCED ]`):

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    ABDMS2000 — Selector de Modos                            │
├──────────────────────┬──────────────────────┬───────────────────────────────┤
│    1. Modo MS2000    │   2. Modo microKORG  │       3. Modo Avanzado        │
│  (Réplica Analógica) │   (Matriz Clásica)   │       (ABD Ultra Engine)      │
├──────────────────────┼──────────────────────┼───────────────────────────────┤
│ • Panel azul clásico │ • Panel beige/madera │ • UI moderna expandida        │
│ • Knobs dedicados    │ • Display LED 3 dig. │ • Visualizadores ADSR/LFO     │
│ • Display LCD        │ • 2 Diales matricial │ • Mod Matrix visual libre     │
│ • 16 botones de paso │ • 5 Knobs asignables │ • Polifonía hasta 16/32 voces │
│ • Mod Sequence 3x16  │ • 8 Géneros músical. │ • Reverb Shimmer + Delay Tape │
│ • SysEx ModelID: 0x58│ • SysEx ModelID: 0x5E│ • 4 LFOs + 4 Envelopes        │
│ • Bancos A1 - H16    │ • Bancos A/B x Gen.  │ • Step Seq polifónico 64 pas. │
└──────────────────────┴──────────────────────┴───────────────────────────────┘
```

#### Modo 1: Korg MS2000 (Fidelidad Hardware)
- **UI:** Panel azul metálico con todos los potenciómetros físicos accesibles directamente, emulación de pantalla LCD de 2 líneas y 16 botones de paso iluminados.
- **Funcionalidad:** Acceso al Secuenciador de Modulación (3 pistas × 16 pasos con selector Step/Smooth y Slew Limiter).
- **MIDI/SysEx:** Model ID `0x58`, mapeo nativo de CCs y volcados de 128 patches organizados en 8 bancos (A a H) de 16 programas.

#### Modo 2: microKORG (Matriz Clásica)
- **UI:** Panel retro (madera/beige/negro), display LED de 3 dígitos de 7 segmentos, 2 diales rotativos de selección matricial (`Edit Select 1` y `Edit Select 2`) y 5 potenciómetros rotativos de edición asignables dinámicamente a los parámetros de la fila activa. Selector maestro de 8 géneros musicales.
- **Funcionalidad:** Mismo motor DSP compartido pero con flujo de trabajo por matriz del microKORG.
- **MIDI/SysEx:** Model ID `0x5E`, compatibilidad con parches `.syx` del microKORG y estructura de memoria (Side A/B × 8 géneros × 8 programas).

#### Modo 3: Modo Avanzado (ABD Ultra / Extended Mode)
- **UI:** Interfaz moderna de alta densidad (similar a la experiencia avanzada de *ABDEep*, *ABDJuno* y *ABDCZ101*), con osciloscopio en tiempo real, visualizadores interactivos de curvas de envolvente y LFOs, y analizador de espectro.
- **Capacidades Expandidas:**
  - **Polifonía extendida:** Opciones de 4, 8, 16 o 32 voces (superando el límite de 4 voces del hardware).
  - **Cadena de Efectos Avanzada:** Se añade Reverb estéreo de alta calidad (Hall, Room, Shimmer, Plate), Chorus analógico BBD de 6 fases, y Delay analógico con emulación de cinta (Tape Flutter & Saturation).
  - **Modulaciones Ampliadas:** 4 LFOs sincronizables, 4 generadores de envolvente DAHDSR y Matriz de Modulación de 8 ranuras con multiplicadores libres.
  - **Secuenciador Polifónico Extendido:** Hasta 64 pasos con Parameter Locks (p-locks) por paso.

---

### 1.2 Plataformas Target

| Plataforma | Formato | Motor UI |
|---|---|---|
| Windows | Standalone + VST3 | WebView2 |
| macOS | Standalone + VST3 | WKWebView |
| Web | PWA (WASM AudioWorklet) | HTML/CSS/JS nativo |
| Raspberry Pi | (futuro) Standalone | Headless (sin UI, control externo) |

### 1.3 Stack Tecnológico

- **C++ Engine**: JUCE 8, C++20 (Single Source of Truth para todos los modos)
- **UI**: WebView2 (Vanilla HTML/CSS/JS con Sistema de Tokens CSS para tematización de modos)
- **Build**: CMake (sin dependencias de Projucer)
- **WASM**: Emscripten → AudioWorklet con HEAP optimizado
- **Tests**: Vitest (UI y Contratos) + JUCE UnitTest (C++)

---

## 2. Lecciones Aprendidas de Proyectos Anteriores

### 2.1 Problemas detectados en ABDCZ101

| Problema | Impacto | Solución para ABDMS2000 |
|---|---|---|
| `PluginEditor.cpp` / `PluginProcessor.cpp` en la raíz de `Source/` | Rompe la separación de capas; el plugin wrapper se mezcla con el core DSP | Mover a `Source/Plugin/` como en ABDEep |
| Wasm bridge fragmentado en múltiples archivos ad-hoc (`WasmBank.cpp`, `WasmEnvelopes.cpp`, `WasmMacros.cpp`, `WasmParams.cpp`, `WasmPresets.cpp`) | Difícil de mantener, sin patrón claro | Usar patrón de ABDEep: `WasmBridge.h/cpp` único con exports bien organizados, pero dividido por dominio si crece |
| `BankManager.cpp` sin header propio (`BankManager.h`) | Compilación frágil, forward declarations manuales | Siempre par `.h` + `.cpp` por componente |
| Factory presets como código C++ compilado (`factory_data_bank_0.cpp`, etc.) | Tiempos de compilación altos, difícil de actualizar | Presets como JSON/binario cargado en runtime, con fallback embedded |
| `ParameterRegistry.gen.h/.cpp` generado pero sin schema canónico claro | Desincronización WebUI ↔ C++ | Un solo `parameters_spec.json` como fuente canónica |
| Sin carpeta `Plugin/` explícita | Mezcla de responsabilidades en raíz de Source | Subcarpeta `Plugin/` dedicada |

### 2.2 Mejoras adoptadas de ABDEep

| Patrón | Descripción |
|---|---|
| `Source/Plugin/` separado | PluginEditor, PluginProcessor y BridgeActions en su propia carpeta |
| `BridgeActions` dividido por dominio | `_Calibration.cpp`, `_File.cpp`, `_MIDI.cpp`, `_Params.cpp`, `_State.cpp` — escalable |
| FX como clases individuales | Cada efecto en su propio `.h/.cpp` — fácil de añadir/quitar |
| `Source/Tools/` para utilidades de desarrollo | Benchmarks, CalibrationLab, UnitTests separados del producto |
| `WebUI/resources/parameters_spec.json` | Spec canónica de parámetros fuera del código |
| Schemas JSON versionados | `schemas/*.schema.v1.0.0.json` para contratos |

### 2.3 Problemas a evitar de ABDEep

| Problema | Solución para ABDMS2000 |
|---|---|
| `WasmBridge.cpp` monolítico (un solo archivo gigante) | Dividir en `WasmBridge_DSP.cpp`, `WasmBridge_MIDI.cpp`, `WasmBridge_State.cpp` |
| WebUI `src/` sin separación clara contracts vs store vs bridge | Añadir `src/store/`, `src/bridge/`, `src/contracts/` bien diferenciados |
| Tests WebUI numerosos pero sin categorización | Organizar en `tests/unit/`, `tests/integration/`, `tests/contract/` |
| Demasiados archivos `bundle_code/` de depuración acumulados | Añadir a `.gitignore` desde el inicio |

---

## 3. Arquitectura del Sistema

### 3.1 Diagrama de Capas

```
┌─────────────────────────────────────────────────────────────────┐
│                        WebUI (WebView2)                         │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌───────────────────┐  │
│  │ UI Layer │ │  Store   │ │  Bridge  │ │  Engine (WASM)    │  │
│  │ (panels, │ │ (state,  │ │ (IPC ↔   │ │  (AudioWorklet    │  │
│  │  knobs,  │ │  params) │ │  C++)    │ │   solo en web)    │  │
│  │  LCD)    │ │          │ │          │ │                   │  │
│  └──────────┘ └──────────┘ └──────────┘ └───────────────────┘  │
├─────────────────────────────────────────────────────────────────┤
│               C++ Plugin Layer (JUCE 8)                         │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ PluginEditor (WebView host)  │  BridgeActions (IPC ↔ JS)  │ │
│  ├────────────────────────────────────────────────────────────┤ │
│  │                    PluginProcessor                         │ │
│  │         (AudioProcessor, MIDI routing, state)              │ │
│  └────────────────────────────────────────────────────────────┘ │
├─────────────────────────────────────────────────────────────────┤
│                    Core Engine (C++)                             │
│  ┌─────────┐ ┌──────────┐ ┌───────────┐ ┌──────────────────┐  │
│  │ Synth   │ │  Voice   │ │  MIDI     │ │   State /        │  │
│  │ Engine  │ │ Manager  │ │ Processor │ │   Bank Manager   │  │
│  └─────────┘ └──────────┘ └───────────┘ └──────────────────┘  │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                       DSP Layer                          │   │
│  │  Oscillators │ Filters │ Envelopes │ LFOs │ FX │ Vocoder│   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
```

### 3.2 Flujo de Datos Bidireccional Hardware ↔ Software

```
MS2000 Hardware                    ABDMS2000 Software
     │                                    │
     │──── CC/SysEx (knob turn) ─────────►│ MIDIProcessor → Store → UI update
     │                                    │
     │◄─── CC/SysEx (UI knob turn) ──────│ UI → Store → MIDIProcessor → MIDI Out
     │                                    │
     │──── Program Dump Request ─────────►│ SysExManager → parse → BankManager
     │                                    │
     │◄─── Program Dump Send ────────────│ BankManager → SysExManager → MIDI Out
     │                                    │
     │──── All Data Dump ────────────────►│ SysExManager → BankManager (128 progs)
     │                                    │
     │◄─── All Data Dump ───────────────│ BankManager → SysExManager → MIDI Out
```

---

## 4. Estructura de Archivos Propuesta

```
ABDMS2000/
├── CMakeLists.txt                      # Build principal
├── build.bat / build.sh                # Scripts de build
├── package.json                        # NPM (Vitest, scripts)
├── vitest.config.js
├── README.md
├── .gitignore
│
├── GITS/                               # JUCE 8 como submodule
│   └── JUCE/
│
├── DOCS/                               # Documentación del proyecto
│   ├── KORG MS2000 MIDI CCs and NRPNs.html
│   ├── MS2000_SysEx_Spec.md
│   ├── ARCHITECTURE.md
│   ├── CHANGELOG.md
│   └── ...
│
├── schemas/                            # Contratos JSON versionados
│   ├── parameters-spec.schema.v1.json
│   ├── program-dump.schema.v1.json
│   ├── bank-file.schema.v1.json
│   └── bridge-messages.schema.v1.json
│
├── Scripts/                            # Generadores y herramientas
│   ├── registry_generator.js           # Genera .gen.h/.cpp y .gen.js
│   ├── build_webui.js                  # Empaqueta WebUI para BinaryData
│   └── extract_midi_spec.js            # Parsea HTML spec → JSON
│
├── Source/
│   ├── Core/                           # Motor puro, sin dependencia de UI
│   │   ├── SynthEngine.h / .cpp        # Orquestador principal del DSP
│   │   ├── Voice.h / .cpp              # Voz individual (2 OSC + Filter + Envs + LFOs)
│   │   ├── VoiceManager.h / .cpp       # Polifonía, asignación, unísono
│   │   ├── AudioThreadSnapshot.h       # Struct plano para snapshot lock-free
│   │   ├── HardwareConstants.h         # Constantes MS2000 (num params, ranges)
│   │   └── BuildVersion.h              # Versión de build auto-generada
│   │
│   ├── DSP/                            # Bloques DSP individuales
│   │   ├── Oscillators/
│   │   │   ├── VAOscillator.h / .cpp   # Virtual Analog (Saw, Sq, Tri, Sin)
│   │   │   ├── DWGSOscillator.h / .cpp # Digital waveforms (DWGS 1-64)
│   │   │   ├── NoiseGenerator.h / .cpp # White/Pink noise
│   │   │   ├── AudioInput.h / .cpp     # External audio input (Vocoder)
│   │   │   └── OscMixer.h / .cpp       # Ring Mod, Sync, mezcla OSC1+OSC2
│   │   │
│   │   ├── Filters/
│   │   │   ├── MultiModeFilter.h / .cpp  # LP 12/24, HP 12/24, BP, BR
│   │   │   ├── FilterModel.h             # Enum de tipos + interface
│   │   │   └── TPTOnePole.h / .cpp       # Bloque TPT/ZDF base reutilizable
│   │   │
│   │   ├── Envelopes/
│   │   │   ├── ADSREnvelope.h / .cpp   # ADSR estándar MS2000
│   │   │   └── EnvelopeParams.h        # Struct de parámetros
│   │   │
│   │   ├── Modulation/
│   │   │   ├── LFO.h / .cpp            # LFO 1 y 2 (key sync, tempo sync)
│   │   │   ├── VirtualPatch.h / .cpp   # Mod Matrix del MS2000 (4 slots)
│   │   │   ├── ModSequence.h / .cpp    # Secuenciador de modulación (3 pistas)
│   │   │   └── ModSources.h            # Enum fuentes/destinos modulación
│   │   │
│   │   ├── Vocoder/
│   │   │   ├── VocoderEngine.h / .cpp  # Motor vocoder 16 bandas
│   │   │   ├── BandpassBank.h / .cpp   # Banco filtros análisis/síntesis
│   │   │   └── EnvelopeFollower.h/.cpp # Detector envolvente por banda
│   │   │
│   │   ├── FX/
│   │   │   ├── FXSlot.h / .cpp         # Contenedor genérico de efecto
│   │   │   ├── FXModulation.h / .cpp   # Chorus / Flanger / Phaser / Ensemble
│   │   │   ├── FXDelay.h / .cpp        # Stereo Delay, Cross Delay
│   │   │   ├── FXEqualizer.h / .cpp    # EQ del MS2000
│   │   │   ├── FXDistortion.h / .cpp   # Asymmetric Distortion
│   │   │   └── FXFactory.cpp           # Factory method para crear FX
│   │   │
│   │   └── Arpeggiator/
│   │       ├── Arpeggiator.h / .cpp    # Motor de arpeggiador (inyecta MidiBuffer)
│   │       └── ArpPatterns.h           # Patrones predefinidos
│   │
│   ├── MIDI/                           # Todo lo relacionado con MIDI
│   │   ├── MIDIProcessor.h / .cpp      # Recepción/envío CCs, notes, pitchbend
│   │   ├── MIDIMap.h / .cpp            # Mapa CC ↔ parámetro (spec MS2000)
│   │   ├── SysExManager.h / .cpp       # Encode/decode SysEx del MS2000
│   │   ├── SysExCodec.h / .cpp         # Codec Korg-specific (7-bit packing)
│   │   ├── MIDILearn.h / .cpp          # Asignación dinámica de CCs
│   │   └── HardwareLink.h / .cpp       # Gestión conexión bidireccional HW
│   │
│   ├── State/                          # Gestión de estado y presets
│   │   ├── ParameterIDs.h              # IDs canónicos (enum/constexpr)
│   │   ├── ParameterRegistry.gen.h     # [GENERADO] Lookup table
│   │   ├── ParameterRegistry.gen.cpp   # [GENERADO]
│   │   ├── Parameters.h / .cpp         # APVTS setup y ranges
│   │   ├── ProgramState.h / .cpp       # Estado de un programa individual
│   │   ├── BankManager.h / .cpp        # Gestión de bancos (128 programs)
│   │   ├── ModelContracts.h / .cpp     # Adaptadores de modelo (MS2000, microKORG, etc.)
│   │   ├── PresetSerializer.h / .cpp   # Serialización programa ↔ JSON/bin
│   │   └── FactoryPresets.h / .cpp     # Presets de fábrica embebidos
│   │
│   ├── Plugin/                         # JUCE plugin wrappers
│   │   ├── PluginProcessor.h / .cpp    # AudioProcessor principal
│   │   ├── PluginEditor.h / .cpp       # Editor con WebView2
│   │   ├── PluginEditor_ResourceProvider.h / .cpp
│   │   ├── BridgeActions.h / .cpp      # Dispatcher IPC principal
│   │   ├── BridgeActions_Params.cpp    # Acciones de parámetros
│   │   ├── BridgeActions_MIDI.cpp      # Acciones MIDI (envío CC, SysEx)
│   │   ├── BridgeActions_State.cpp     # Acciones estado (load/save)
│   │   ├── BridgeActions_Bank.cpp      # Acciones banco (import/export)
│   │   └── BridgeActions_File.cpp      # Acciones de archivo
│   │
│   ├── Standalone/                     # App standalone extras
│   │   ├── StandaloneApp.cpp
│   │   └── HeadlessRpcServer.h / .cpp
│   │
│   ├── Wasm/                           # Compilación WASM (Emscripten)
│   │   ├── WasmBridge.h               # Interface exportada a JS
│   │   ├── WasmBridge_DSP.cpp         # Exports procesado audio
│   │   ├── WasmBridge_MIDI.cpp        # Exports procesado MIDI
│   │   ├── WasmBridge_State.cpp       # Exports gestión estado/presets
│   │   └── WasmBridge_Bank.cpp        # Exports gestión bancos
│   │
│   ├── Utils/                          # Utilidades compartidas
│   │   ├── CircularBuffer.h
│   │   ├── DSPHelpers.h
│   │   ├── SmoothedValue.h
│   │   ├── LockFreeQueue.h
│   │   └── StringHelpers.h
│   │
│   └── Tests/                          # Tests C++
│       ├── SynthEngineTests.cpp
│       ├── SysExRoundTripTest.cpp
│       ├── VocoderTests.cpp
│       ├── FilterTests.cpp
│       └── Mocks/
│
├── WebUI/
│   ├── index.html
│   ├── manifest.json
│   │
│   ├── src/
│   │   ├── app.js                      # Bootstrap principal
│   │   │
│   │   ├── bridge/                     # Comunicación C++ / WASM
│   │   │   ├── bridgeCore.js           # Detecta WebView vs WASM
│   │   │   ├── bridgeWebView.js        # IPC vía window.__JUCE__
│   │   │   ├── bridgeWasm.js           # Llamadas directas WASM
│   │   │   └── bridgeMessages.js       # Tipos y constantes
│   │   │
│   │   ├── store/                      # Estado centralizado
│   │   │   ├── parameterStore.js
│   │   │   ├── bankStore.js
│   │   │   ├── midiStore.js
│   │   │   ├── uiStore.js
│   │   │   └── normalizer.js
│   │   │
│   │   ├── contracts/                  # Datos generados y constantes
│   │   │   ├── registry.gen.js         # [GENERADO]
│   │   │   ├── midiMap.js
│   │   │   ├── sysExCodec.js
│   │   │   ├── modelContracts.js       # Contratos de modelo JS
│   │   │   ├── themes.js
│   │   │   ├── shortcuts.js
│   │   │   └── factoryPresets.js
│   │   │
│   │   ├── engine/                     # Motor WASM (solo web)
│   │   │   ├── ms2000AudioEngine.js
│   │   │   └── ms2000Worklet.js
│   │   │
│   │   ├── ui/                         # Componentes de interfaz
│   │   │   ├── panels/
│   │   │   │   ├── panelOsc.js
│   │   │   │   ├── panelFilter.js
│   │   │   │   ├── panelAmp.js
│   │   │   │   ├── panelEnvelopes.js
│   │   │   │   ├── panelLFO.js
│   │   │   │   ├── panelModPatch.js
│   │   │   │   ├── panelArp.js
│   │   │   │   ├── panelFX.js
│   │   │   │   ├── panelVocoder.js
│   │   │   │   └── panelTimbre.js
│   │   │   │
│   │   │   ├── controls/
│   │   │   │   ├── knob.js
│   │   │   │   ├── slider.js
│   │   │   │   ├── selector.js
│   │   │   │   ├── toggleSwitch.js
│   │   │   │   ├── ledIndicator.js
│   │   │   │   └── filmstrips.js
│   │   │   │
│   │   │   ├── lcd/
│   │   │   │   ├── lcdPanel.js
│   │   │   │   ├── lcdGraphics.js
│   │   │   │   ├── lcdProgramMode.js
│   │   │   │   ├── lcdEditMode.js
│   │   │   │   └── lcdGlobalMode.js
│   │   │   │
│   │   │   ├── bankManager.js
│   │   │   ├── sysexInspector.js       # Pantalla dedicada SysEx Hex Inspector
│   │   │   ├── keyboard.js             # Teclado con Octave LEDs y ABDEep lightbar
│   │   │   ├── oscilloscope.js
│   │   │   ├── navbar.js
│   │   │   ├── midiSystem.js
│   │   │   ├── modMatrix.js
│   │   │   └── nameEditor.js
│   │   │
│   │   ├── styles/
│   │   │   ├── base.css
│   │   │   ├── themes.css              # Tokens CSS para Skins (MS2000, microKORG, etc.)
│   │   │   ├── main.css
│   │   │   ├── panels.css
│   │   │   ├── controls.css
│   │   │   ├── lcd.css
│   │   │   ├── keyboard.css
│   │   │   ├── bankManager.css
│   │   │   ├── sysexInspector.css
│   │   │   ├── midi.css
│   │   │   ├── overlays.css
│   │   │   └── menu.css
│   │   │
│   │   └── assets/
│   │       ├── knobs/
│   │       ├── sliders/
│   │       └── icons/
│   │
│   ├── wasm/
│   │   └── ms2000_dsp.js
│   │
│   ├── tests/
│   │   ├── setup.js
│   │   ├── unit/
│   │   │   ├── parameterStore.test.js
│   │   │   ├── normalizer.test.js
│   │   │   ├── sysExCodec.test.js
│   │   │   └── midiMap.test.js
│   │   ├── integration/
│   │   │   ├── bridgeWasm.test.js
│   │   │   ├── bankManager.test.js
│   │   │   └── patchFlow.test.js
│   │   └── contract/
│   │       ├── registryGen.test.js
│   │       └── sysExRoundtrip.test.js
│   │
│   └── public/
│       └── presets/
│
└── wasm/
    ├── CMakeLists.txt
    └── build_wasm.bat / .sh
```

---

## 5. Especificación DSP del MS2000 (Basado en Ingeniería Inversa)

La arquitectura sigue una emulación estricta basada en el comportamiento medido del MS2000. El flujo de audio es estrictamente lineal de izquierda a derecha.

### 5.1 Arquitectura de Síntesis

```
┌─────────┐     ┌─────────┐
│  OSC 1  │────►│         │     ┌────────┐     ┌────────────┐     ┌─────────┐
│(VA/DWGS)│     │  MIXER  │────►│ FILTER │────►│ DISTORTION │────►│   AMP   │──► FX ──► OUT
│  OSC 2  │────►│(+Ring/  │     │(Multi- │     │ (Soft-Clip │     │(EG2+VCA)│
│  (VA)   │     │ Sync)   │     │ mode)  │     │ asimétrico)│     └─────────┘
└─────────┘     └─────────┘     └────────┘     └────────────┘
                                     ▲                ▲                ▲
                                  ┌──┴──┐             │            ┌───┴───┐
                                  │ EG1 │             │            │  EG2  │
                                  │(Flt)│             │            │ (Amp) │
                                  └─────┘             │            └───────┘
                                     ▲                │                ▲
                              ┌──────┴────────────────┴────────────────┴──────┐
                              │    LFO 1    │    LFO 2    │  Mod Sequence     │
                              └─────────────┴─────────────┴───────────────────┘
                              ┌───────────────────────────────────────────────┐
                              │            Virtual Patch (4 slots)            │
                              └───────────────────────────────────────────────┘
```

### 5.2 Osciladores (71 formas de onda en total)

El motor dispone de 71 formas de onda en total (68 en OSC 1 + 3 en OSC 2):

**OSC 1 (68 formas de onda / 8 algoritmos principales):**
- Waveforms VA (Analógicas): Saw, Pulse (con PWM), Triangle. Usarán PolyBLEP para anti-aliasing.
- Sine (con Cross Modulación / FM por OSC2).
- VoxWave (basado en formantes).
- DWGS (64 formas digitales). Se implementará cargando un buffer `std::vector<float>` de 2048 × 64 = 131,072 muestras para latencia cero usando interpolación lineal o cúbica.
- Noise (generador estocástico con seed seguro) y Audio In (entrada externa).

**OSC 2 (3 formas de onda analógicas acopladas):**
- Waveforms: Saw, Square, Triangle.
- Modulación: Ring Modulation (`OSC1 * OSC2`), Hard Sync (Reset fase OSC2 al completar ciclo OSC1), Cross Mod (Modula frecuencia de OSC1).
- Pitch: Coarse (-24...+24), Fine (-50...+50 cents).

### 5.3 Mezclador y Distorsión

- **Mixer:** Suma directa de OSC1, OSC2 y Noise.
- **Distorsión (Clave secreta):** Ocurre *después* del filtro. El algoritmo es una saturación asimétrica no lineal que responde al volumen del Mixer. A mayor volumen de osciladores, mayor la destrucción. Se usará una aproximación hiperbólica o polinómica diferencial por semiciclo para recrear armónicos pares.

### 5.4 Filtro Multimodo

- **Tipos:** LP 24dB, LP 12dB, BP 12dB, HP 12dB.
- **Cutoff:** Logarítmico exponencial (`minHz * std::pow(maxHz / minHz, norm)`), mapeando 20Hz a 20kHz.
- **Comportamiento analógico:** A mayor Resonancia, se atenúa la entrada (reducción de graves).
- Auto-oscila puramente a partir de valor ~105.
- Tracking e intensidad de modulación (EG1) aplicados dinámicamente.

### 5.5 Envolventes (EG1 y EG2)

- **Curvas NO lineales:** Attack (Exponencial invertida), Decay/Release (Exponenciales puras).
- **Rango:** Attack (1ms a 11s), Decay/Release (2ms a 20s).
- **Mapeo:** La transformación del valor SysEx (0-127) usará una curvatura de ~2.5 (`std::pow(norm, 2.5f)`) para emular la precisión del potenciómetro físico.

### 5.6 LFOs (1 y 2)

- **Comportamiento Libre:** Curva logarítmica de 0.01 Hz (100 segundos) a 20.0 Hz, usando un exponente de 1.25 para concentrar el control entre 0.1 y 5 Hz.
- **Comportamiento Tempo Sync:** Mapea los valores 0-127 a índices de subdivisión de tiempo musical (1/1 hasta 1/32).

### 5.7 Secuenciador de Modulación (Mod Sequence)

- 3 pistas (A, B, C) x 16 pasos. Sincronizadas a subdivisiones musicales (TEMPO_SYNC).
- **Modos de reproducción:** Forward, Backward, Bounce, Random.
- **Transición STEP:** Cambio abrupto por paso.
- **Transición SMOOTH:** Interpolación lineal pasada por un Slew Limiter (Filtro paso-bajo a ~10ms) para emular inercia analógica sin clics digitales.

### 5.8 Arpegiador (Arpeggiator)

- Procesa notas en tiempo real. En lugar de procesarlo en bloque DSP de audio, se inyectará en la cola de notas (`juce::MidiBuffer`) evaluando la posición `juce::AudioPlayHead`.
- Tipos: Up, Down, Alt 1, Alt 2, Random, Trigger (Acordes completos punteados).

### 5.9 Vocoder (16 bandas)

- 16 filtros paso-banda de análisis (Audio In) y síntesis (Osciladores).
- Implementará **Formant Shift** (+2 a -2).
- Requiere circuito **Vocal Distortion / HPF Gate** para extraer transitorios / consonantes de la voz e inyectar ruido blanco (imprescindible para inteligibilidad).

### 5.10 Timbres y Modos de Programa

- **Single**: 1 timbre, 4 voces máx.
- **Layer**: 2 timbres superpuestos, 2 voces cada uno.
- **Split**: 2 timbres divididos por punto de split.
- **Voice Assign:** Mono, Poly, Unison.
- **Unison:** Clonará 4 fases, desfasadas según `Unison Detune` (0-99 cents) y distribuidas estéreo en `Unison Spread`.

### 5.11 Efectos

- **EQ:** Low/High Shelving paramétrico.
- **Mod FX:** Chorus/Flanger (Delay corto + LFO), Ensemble (Múltiples líneas de retardo desfasadas estilo String Machine), Phaser (Cancelación de fase).
- **Delay:** Stereo, Cross (Ping-Pong), L/R Delay (Desfase estéreo).

---

## 6. Especificación MIDI del MS2000

### 6.1 Control Changes (CCs)

Basado en el documento `KORG MS2000 MIDI CCs and NRPNs.html`:

| Sección | CCs Principales |
|---|---|
| **Pitch** | Portamento (CC 5, 65, 84) |
| **OSC 1** | Wave (CC 77), Control 1/2 (CC 14, 15) |
| **OSC 2** | Wave (CC 78), Semitone (CC 16), Tune (CC 17) |
| **Mixer** | OSC 1 Level (CC 20), OSC 2 Level (CC 21), Noise (CC 22) |
| **Filter** | Type (CC 83), Cutoff (CC 74), Resonance (CC 71), EG Int (CC 79), KBD Track (CC 80) |
| **Amp** | Level (CC 7), Pan (CC 10), Distortion (CC 81), KBD Track (CC 82) |
| **EG 1** | A (CC 23), D (CC 24), S (CC 25), R (CC 26) |
| **EG 2** | A (CC 73), D (CC 75), S (CC 27), R (CC 72) |
| **LFO 1** | Wave (CC 86), Freq (CC 76), Key Sync (CC 85) |
| **LFO 2** | Wave (CC 88), Freq (CC 87), Key Sync (CC 90) |
| **Patch 1–4** | Source/Dest/Intensity (CCs 28-31, 33-36, 37-40) |
| **Arpeggiator** | Tempo (CC 41-42), On/Off (CC 89), Range, etc. |
| **Mod FX** | LFO Speed (CC 12), Depth (CC 93) |
| **Delay FX** | Time (CC 13), Depth (CC 94) |
| **Vocoder** | HPF Level (CC 18), Threshold (CC 19), Formant (CC 83), Filter Mod (CC 79) |

### 6.2 SysEx (Program Dump)

- **Manufacturer ID**: `0x42` (Korg)
- **Device ID**: `0x3n` (n = MIDI channel)
- **Model ID**: `0x58` (MS2000 / MS2000R)
- **Program Data Dump Request**: `F0 42 3n 58 10 F7`
- **Program Data Dump**: `F0 42 3n 58 40 <data> F7`
- **All Data Dump Request**: `F0 42 3n 58 0E F7`
- **Write Completed**: `F0 42 3n 58 23 F7`
- **Write Error**: `F0 42 3n 58 24 F7`

Se desarrollará un `MS2000SysExParser` basado en el Manual de Implementación Korg y referencias Open Source (ReMS2000) para decodificar los bloques de datos empaquetados en 7-bits a variables normalizadas (0.0 a 1.0).

---

## 7. Arquitectura del Gestor Universal de Bancos (ABD Universal Bank Manager)

Para evitar reescribir la lógica de gestión de bancos en cada sintetizador (como ocurrió en JUNIO601, ABDEep y ABDCZ101), se diseña un **módulo universal agnóstico y extensible** capaz de adaptarse a cualquier sintetizador de la familia ABD mediante **Contratos de Modelo (Model Adapters)** y **Tokens de Diseño CSS**.

---

### 7.1 Principios de Diseño del Gestor Universal

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    WebUI: Componente Universal <bank-manager>               │
│  (Búsqueda, Filtros, Drag & Drop, Tagging, Rating, Comparador Diff, Paginación)│
├─────────────────────────────────────────────────────────────────────────────┤
│         Capa de Adaptadores por Modelo (Contracts & Model Adapters)         │
│   ┌───────────────┐ ┌───────────────┐ ┌───────────────┐ ┌───────────────┐   │
│   │ MS2000 Adapter│ │microKORG Adpt │ │ CZ-101 Adapter│ │JUNO-60 Adapter│   │
│   │ (128 / A1-H16)│ │(128 / A-B Gen)│ │(64 / Cart-Int)│ │(128 / 1-8 1-8)│   │
│   └───────┬───────┘ └───────┬───────┘ └───────┬───────┘ └───────┬───────┘   │
├───────────┼─────────────────┼─────────────────┼─────────────────┼───────────┤
│           ▼                 ▼                 ▼                 ▼           │
│   [ SysEx Codec ]   [ SysEx Codec ]   [ SysEx Codec ]   [ SysEx Codec ]     │
│   (Pack 7->8 bit)   (Pack 7->8 bit)   (CZ 4-bit nibble) (Roland Checksum)   │
├─────────────────────────────────────────────────────────────────────────────┤
│           Capa de Almacenamiento y Persistencia Universal (C++ / JS)         │
│  • Base de datos local de bancos infinitos (.json canónico y .syx crudo)    │
│  • Indexación en memoria: búsqueda por nombre, autor, categoría, favoritos  │
│  • Protocolo de Handshake MIDI SysEx unificado (Dump Request / Progress / ACK)│
└─────────────────────────────────────────────────────────────────────────────┘
```

---

### 7.2 El Contrato de Modelo (`ISynthModelContract`)

Cada sintetizador implementa un adaptador descriptivo que define sus reglas de negocio sin tocar el código central del Bank Manager:

```javascript
// Contrato de Modelo implementado por cada sinte
export const MS2000ModelContract = {
  modelId: "korg-ms2000",
  modelName: "Korg MS2000 / MS2000R",
  manufacturer: "Korg",
  
  // Capacidad y direccionamiento
  bankCapacity: 128,
  banksCount: 8,              // Bancos A a H
  programsPerBank: 16,        // 1 a 16
  getProgramAddress: (index) => {
    const bankLetter = String.fromCharCode(65 + Math.floor(index / 16));
    const progNum = (index % 16) + 1;
    return `${bankLetter}.${progNum < 10 ? '0' : ''}${progNum}`; // "A.01" a "H.16"
  },

  // Capacidades de Hardware
  hardwareCapabilities: {
    supportsProgramDump: true,
    supportsAllDataDump: true,
    supportsRealtimeParamChanges: true,
    dumpPayloadSize: 284,       // Bytes por programa desempaquetado
  },

  // Handshake y SysEx
  sysEx: {
    modelIdByte: 0x58,
    manufacturerId: 0x42,
    buildDumpRequest: (channel) => [0xF0, 0x42, 0x30 + channel, 0x58, 0x10, 0xF7],
    buildAllDumpRequest: (channel) => [0xF0, 0x42, 0x30 + channel, 0x58, 0x0E, 0xF7],
    validateSysEx: (bytes) => bytes[1] === 0x42 && bytes[3] === 0x58
  }
};
```

---

### 7.3 Tematización Adaptativa mediante Tokens CSS

El componente visual del Gestor de Bancos no tiene colores fijos; consume variables CSS semánticas que se inyectan automáticamente según el sintetizador anfitrión:

```css
/* Tokens semánticos del Bank Manager Universal */
:root {
  /* Variables base inyectadas por el Theme Provider */
  --bank-bg-primary: #12141a;
  --bank-bg-secondary: #1a1d26;
  --bank-bg-surface: #222634;
  --bank-accent: #00d4ff;             /* Azul Cyan MS2000 / Dorado microKORG / etc. */
  --bank-accent-hover: #33ddff;
  --bank-accent-rgb: 0, 212, 255;
  --bank-text-main: #f0f4f8;
  --bank-text-muted: #8a99ad;
  --bank-border: rgba(255, 255, 255, 0.08);
  --bank-border-active: var(--bank-accent);
  --bank-diff-added: #00e676;
  --bank-diff-changed: #ffab00;
  --bank-diff-removed: #ff1744;
}

/* Modo MS2000: Paleta Azul Metálico */
[data-theme="ms2000"] {
  --bank-accent: #1e88e5;
  --bank-bg-primary: #0a1118;
}

/* Modo microKORG: Paleta Retro Madera / Beige */
[data-theme="microkorg"] {
  --bank-accent: #d4af37;
  --bank-bg-primary: #1c1814;
}

/* Modo Advanced: Paleta Cyberpunk / Neón */
[data-theme="advanced"] {
  --bank-accent: #ff007f;
  --bank-bg-primary: #08080c;
}
```

---

### 7.4 Funcionalidades del Gestor Universal

1. **Biblioteca Infinita Local:**
   - Base de datos local persistente en disco (carpeta `Presets/Banks/` en standalone/plugin e `IndexedDB` en Web/WASM).
   - Almacena ilimitados archivos `.syx` originales y `.json` canónicos.
2. **Motor de Búsqueda y Filtrado Instantáneo:**
   - Búsqueda por texto libre en tiempo real (nombre de parche, autor, comentarios).
   - Filtrado por categorías (Bass, Lead, Pad, Arp, FX, Vocoder, Percussion, etc.).
   - Sistema de favoritos (estrellas) y etiquetas de usuario personalizadas (tags).
3. **Gestión de Bancos Visual y Drag & Drop:**
   - Reordenar patches dentro de un banco o entre bancos abiertos arrastrando y soltando.
   - Operaciones en lote: clonar, limpiar, inicializar, renombrar.
4. **Comparador Visual de Parches (Diff Viewer):**
   - Compara en tiempo real los valores del parche actualmente en el motor/hardware con el parche seleccionado en la lista antes de sobrescribirlo, destacando parámetros modificados.
5. **Comunicación Bidireccional con Barra de Progreso:**
   - Diálogo de recepción/envío con barra de progreso, detección de timeout y verificación de checksum/integridad.
   - Envío de un solo patch (`Send to Edit Buffer`) o volcado completo del banco (`Burn Bank to Hardware`).

---

### 7.5 Esquema Canónico del Archivo de Banco Universal (`schemas/bank-file.schema.v1.json`)

```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "title": "ABDUniversalBank",
  "type": "object",
  "required": ["schemaVersion", "format", "targetModel", "name", "programs"],
  "properties": {
    "schemaVersion": { "type": "string", "enum": ["1.0.0"] },
    "format": { "type": "string", "enum": ["abd-universal-bank"] },
    "targetModel": { "type": "string" },
    "name": { "type": "string" },
    "author": { "type": "string" },
    "created": { "type": "string", "format": "date-time" },
    "tags": { "type": "array", "items": { "type": "string" } },
    "programs": {
      "type": "array",
      "items": {
        "type": "object",
        "required": ["slot", "name", "parameters"],
        "properties": {
          "slot": { "type": "integer" },
          "name": { "type": "string" },
          "category": { "type": "string" },
          "isFavorite": { "type": "boolean" },
          "rawSysExHex": { "type": "string" },
          "parameters": { "type": "object" }
        }
      }
    }
  }
}
```

---

## 8. Contratos de Datos, Generación de Registros y Vistas Especializadas de UI

### 8.1 Fuente Canónica: `schemas/parameters-spec.schema.v1.json`

```json
{
  "schemaVersion": "1.0.0",
  "parameters": [
    {
      "id": "osc1Wave",
      "name": "OSC1 Waveform",
      "group": "OSC1",
      "cc": 77,
      "min": 0,
      "max": 127,
      "default": 0,
      "type": "choice",
      "choices": ["Saw", "Square", "Triangle", "Sine", "Vox Wave", "DWGS", "Noise", "Audio In"],
      "sysexOffset": null
    }
  ]
}
```

### 8.2 Artefactos Generados

Desde `parameters-spec.schema.v1.json`, el script `Scripts/registry_generator.js` genera:

| Artefacto | Destino | Uso |
|---|---|---|
| `ParameterRegistry.gen.h` | `Source/State/` | Lookup C++ de parámetros |
| `ParameterRegistry.gen.cpp` | `Source/State/` | Implementación |
| `registry.gen.js` | `WebUI/src/contracts/` | Registro JS para UI y WASM |
| `parameter-registry.data.json` | `schemas/` | Validación y documentación |

### 8.3 Bridge IPC (WebView ↔ C++)

El bridge usa el sistema `window.__JUCE__.backend.emitEvent()` nativo de JUCE 8 WebView:

```
WebUI → C++:  { action: "setParam", paramId: "osc1Wave", value: 0.5 }
C++ → WebUI:  { action: "paramChanged", paramId: "osc1Wave", value: 0.5 }
WebUI → C++:  { action: "requestProgramDump" }
C++ → WebUI:  { action: "programDumpReceived", data: {...} }
```

### 8.4 Pantalla LCD Emulada del Hardware (Navegación y Menús Oficiales)

El plugin incorpora un componente de visualización fiel a la pantalla del hardware original:
- **Modo MS2000 (LCD de 2 líneas × 16 caracteres):**
  - Réplica exacta de la tipografía de matriz de puntos (usando `REGISTER.TTF`).
  - Navegación por páginas y menús originales del manual:
    - `1A VOICE NAME`: Edición del nombre del patch (hasta 12 caracteres).
    - `2A GLOBAL`: Memory Protect (On/Off), Master Tune, Transpose, Velocity Curve.
    - `3A MIDI`: MIDI Channel, Local Control, Clock (Internal/Auto/External).
    - `4D MIDI FILTER`: Habilitar/Deshabilitar recepción y transmisión de SysEx, CCs y Program Changes.
    - `VOCODER EDIT`: Ganancia y paneo de las 16 bandas individuales, HPF Level, Gate Threshold.
    - `TIMBRE UTILITY`: Copiar y transferir datos entre Timbre A y Timbre B.
- **Modo microKORG (Display LED de 3 dígitos de 7 segmentos):**
  - Muestra el número de programa (ej. `A11` a `B88`) y valores numéricos directos de edición al mover knobs.
- **Modo Avanzado (Pantalla Expandida):**
  - Menús gráficos táctiles para ruteos complejos, osciloscopio en vivo y asignaciones de macro.

### 8.5 Pantalla / Inspector Dedicado de SysEx (Hex Debugger & Patch Creator)

Al igual que en *ABDCZ101*, *ABDEep* y *ABDJuno*, el plugin incluye una vista dedicada para inspección y edición SysEx:
- **Visor Hexadecimal en Tiempo Real:** Muestra el volcado binario completo del patch actualmente cargado (284 bytes desempaquetados o 324 bytes empaquetados Korg).
- **Coloreado Sintáctico por Secciones:** Cabecera (F0 42 3n 58), Global, Timbre 1 (Osc, Flt, Amp, Envs, LFOs, Patch), Timbre 2, Arp, y Secuencias Mod A/B/C.
- **Botón "Copiar SysEx al Portapapeles":** Exporta la cadena hexadecimal en formato texto limpio listo para compartir o respaldar.
- **Creación de Parches pegando SysEx:** El usuario puede pegar cualquier cadena de texto hexadecimal desde el portapapeles; el parser valida la cabecera y el checksum, cargándolo instantáneamente en el motor.
- **Editor Manual de Bytes:** Permite modificar bytes individuales directamente en la matriz Hex con actualización sonora en tiempo real.

### 8.6 Teclado MIDI Virtual, Ruedas de Expresión y Sistema de Luces LED (Estilo ABDEep)

Componente interactivo inferior para interpretación, monitoreo visual y feedback de estado:
- **Ruedas de Expresión Físicas (Pitch Bend y Mod Wheel):**
  - Reutilización directa de los assets gráficos y filmstrips renderizados de *ABDCZ101* (`WebUI/src/assets/filmstrips/`).
  - Pitch Bend con retorno automático por resorte al soltar y rango de semitonos configurable.
  - Mod Wheel con memoria de posición y vinculación nativa al MIDI CC#01.
- **Control de Transposición de Octavas con Indicadores LED Inteligentes:**
  - Botones dedicados `[ Oct - ]` y `[ Oct + ]` para desplazar el rango del teclado (rango de -3 a +3 octavas).
  - **Comportamiento de LEDs de Estado de Octava:**
    - **Octava Central (0 / Base):** LEDs apagados.
    - **Octava ±1:** LED correspondiente en modo **parpadeo suave**.
    - **Octava ±2 o superior:** LED correspondiente en modo **luz fija continua**.
- **Barra de Luces LED sobre el Teclado y Lógica Visual (Heredada de ABDEep):**
  - Cada tecla cuenta con un indicador LED individual superior con brillo dinámico.
  - **Lógica de Iluminación y Animaciones de Estado:**
    - **Interpretación en Tiempo Real (Note On / Off):** Iluminación instantánea de los LEDs correspondientes a las notas tocadas (vía ratón, QWERTY, DAW o hardware MIDI externo), con intensidad proporcional a la velocidad.
    - **Cambio de Patch (Patch Change):** Animación de barrido en ola luminosa (*Sweep Wave*) de izquierda a derecha confirmando la carga del nuevo sonido.
    - **Cambio de Banco (Bank Change):** Pulso/destello global (*Ripple Pulse*) que se expande desde el centro hacia ambos extremos.
    - **Botón Panic / All Sound Off:** Destello de corte rápido (*Flash & Cut*) que apaga inmediatamente todos los LEDs y silencia las voces activas.
    - **Arpegiador / Step Sequencer Activo:** Los LEDs parpadean secuencialmente al ritmo del reloj siguiendo las notas disparadas por el arpegiador o los pasos del Mod Sequencer.
- **Mapeo de Teclado QWERTY del Ordenador:**
  - Permite tocar notas polifónicas directamente con el teclado del PC (filas A-S-D-F y W-E-T-Y).

### 8.7 Funcionalidades Avanzadas de Experiencia de Usuario (Heredadas de la Suite ABD)

Funciones de alto nivel adoptadas de *ABDEep*, *ABDJuno* y *ABDCZ101*:
1. **Buffer de Comparación A/B (Compare Switch):** Permite alternar instantáneamente entre el estado original del patch cargado y las modificaciones en curso para evaluar mejoras tímbricas.
2. **Randomizador Inteligente con Candados (Smart Randomizer):** Genera sonidos aleatorios pero con protecciones configurables (*Lock Pitch*, *Lock Envelopes*, *Lock FX*) para asegurar resultados musicales.
3. **Pila de Historial Deshacer / Rehacer (Undo / Redo Stack):** Soporte global para `Ctrl + Z` y `Ctrl + Y` en cualquier cambio de parámetro.
4. **Macros de Rendimiento Rápido (Quick Performance Macros 1-4):** 4 potenciómetros macro para control simultáneo de brillo, espacio, movimiento y calidez.
5. **Zoom de Interfaz de Alta Densidad:** Selector en la barra de menú para escalar el viewport al **75%, 100%, 125% o 150%** con nitidez vectorial en monitores 4K y pantallas pequeñas.
6. **Gestor Universal de MIDI Learn:** Clic derecho sobre cualquier control → asignación inmediata de CC hardware con persistencia en `%APPDATA%/ABDSynths/ABDMS2000/midi_bindings.json`.
7. **Conmutador de Fidelidad de Hardware:**
   - **Modo Authentic 2000s:** Emula el filtrado DAC, ruido de fondo analógico y drift térmico de los osciladores del MS2000 original.
   - **Modo Pristine Digital:** Procesamiento en coma flotante de 64 bits ultra limpio con cero ruido.
### 8.8 Sistema de Skins / Temas Visuales Específicos por Modelo (Patrón JUNIO601)

Siguiendo la arquitectura multi-skin que implementamos con éxito en *JUNIO601* (Juno-60 vs Juno-106):
- **Skin 1 — Korg MS2000 Vintage Blue (Skin por defecto en Modo MS2000):**
  - **Chasis y Textura:** Panel superior en tono azul metálico característico del MS2000 con acabado de pintura en polvo y serigrafía blanca/azul cobalto.
  - **Potenciómetros y Botones:** Knobs grises estriados vintage (filmstrips renderizados con sombras dinámicas), conmutadores de palanca metálicos y los 16 botones de pasos alargados con retroiluminación LED roja/verde.
  - **Display:** Pantalla LCD de 2 líneas retroiluminada en tono verde-lima clásico con tipografía matricial de puntos `REGISTER.TTF`.
- **Skin 2 — microKORG Retro Beige & Wood (Skin por defecto en Modo microKORG):**
  - **Chasis y Madera:** Paneles laterales de madera oscura clásica y chasis central en negro mate con serigrafía retro en tonos beige/dorado.
  - **Matriz y Diales:** Panel con la matriz completa de parámetros visible, diales giratorios gruesos estilo baquelita para selección de género musical y diales `Edit Select 1 / 2`.
  - **Potenciómetros de Edición:** 5 perillas plateadas/negras de control directo de la fila activa.
  - **Display:** Display numérico LED de 3 dígitos de 7 segmentos en rojo brillante de alta visibilidad.
- **Skin 3 — ABD Ultra Dark Neon (Skin por defecto en Modo Avanzado):**
  - **Chasis:** Interfaz moderna de inspiración analógica oscura de alto contraste.
  - **Acentos:** Colores Cyberpunk Neón (Cyan `#00d4ff` y Magenta `#ff007f`), visualizadores de osciloscopio en tiempo real, curvas de envolvente con nodos arrastrables y medidores de nivel dinámicos por voz.
- **Conmutación en Tiempo Real y Desacoplamiento:**
  - El usuario puede vincular la Skin automáticamente al Modo de Síntesis seleccionado o desacoplarla para usar la Skin que prefiera independientemente del modo de motor activo.

### 8.9 Panel Deslizante Lateral para Parámetros Avanzados / LCD (Side Drawer)

Al igual que en *ABDEep* y *ABDCZ101*, aquellos controles que en el hardware físico solo eran accesibles navegando por menús LCD se exponen directamente en la interfaz gráfica:
### 8.13 Drag & Drop Directo de Archivos `.syx` y `.json`

Para maximizar la agilidad del flujo de trabajo:
- **Arrastre Global:** El usuario puede arrastrar cualquier archivo `.syx` (SysEx original de Korg MS2000/microKORG) o `.json` (banco canónico de ABD) desde el explorador de archivos directamente sobre cualquier zona de la interfaz gráfica.
- **Detección Automática:** El sistema analiza la cabecera del archivo:
  - Si es un volcado de un solo programa (284 bytes / sub-status `0x40`), lo carga inmediatamente en el búfer de edición activo.
  - Si es un volcado de banco completo (128 patches / sub-status `0x4C`), abre el diálogo del Gestor de Bancos preguntando si desea reemplazar el banco actual o abrirlo en una nueva pestaña.
- **Implementación:** `juce::FileDragAndDropTarget` en C++ y evento `dragover` / `drop` en JS con decodificación de `FileReader`.

### 8.14 Gestor de Atajos de Teclado Globales (`shortcuts.js`)

Atajos de teclado unificados para agilizar la producción:
- `Espacio`: Inicia / Detiene la reproducción del Arpegiador o del Secuenciador de Modulación.
- `Flechas Arriba / Abajo`: Navega al programa siguiente / anterior dentro del banco.
- `Flechas Izquierda / Derecha`: Navega al banco siguiente / anterior.
- `Ctrl + O` / `Cmd + O`: Abre el diálogo para cargar un banco o archivo `.syx`.
- `Ctrl + S` / `Cmd + S`: Guarda el banco actual.
- `Ctrl + Z` / `Ctrl + Y`: Deshacer / Rehacer cambios de parámetros.
- `Escape`: Cierra cualquier menú desplegable, Drawer lateral o diálogo modal abierto.

### 8.15 Normalización Simétrica y Curvas de Skew (`normalizer.js` ↔ `Parameters.cpp`)

Para asegurar paridad matemática 1:1 entre el movimiento de los potenciómetros en pantalla y el valor real procesado por el DSP:
- **Contrato Canónico:** En `parameters-spec.schema.v1.json`, cada parámetro declara su tipo de escala (`linear`, `skew`, `exponential`, `discrete`, `logarithmic`).
- **Factor de Skew:** Para parámetros de tiempo (ADSR) o frecuencia (LFO/Cutoff), se especifica el punto medio exacto (ej. skew `0.35` para concentrar el recorrido en valores bajos).
- **Consistencia C++ / JS:** `normalizer.js` y `Parameters.cpp` implementan exactamente la misma fórmula de normalización (`0.0f a 1.0f`) y desnormalización (`min a max`), evitando desajustes perceptuales.

### 8.16 Instantánea de Audio Lock-Free (`AudioThreadSnapshot.h`) y Congelación de Osciloscopio

- **Estructura POD Lock-Free:** `AudioThreadSnapshot` transporta a 60 FPS desde el hilo de audio hacia la UI los datos visuales esenciales (niveles de VU estéreo, voces activas, fases de LFO/ADSR y buffer circular de 512 muestras de audio).
- **Cero Bloqueos RT:** La transferencia se realiza mediante doble buffer con intercambio de punteros atómicos (`std::atomic<Snapshot*>`), garantizando que la UI jamás interrumpa el procesamiento de audio.
- **Función Freeze en Osciloscopio:** El osciloscopio en WebUI incluye un botón `[ Freeze / Pausa ]` para detener el renderizado y permitir inspeccionar con calma la forma de onda generada.

---

## 9. Pipeline de Compilación, Guía Maestra WASM y Empaquetado de Assets

### 9.1 Guía Maestra de Compilación WASM (Estándar ABDEep)

Siguiendo las reglas de oro de `guia_maestra_wasm_juce.md` y `README_WASM_COMPILATION.md`:

1. **Aislamiento Total del Core DSP:**
   - La carpeta `Source/DSP/` y `Source/Core/` no tiene dependencias de clases gráficas de JUCE (`juce_gui_basics`, `juce_gui_extra`, etc.) ni de hilos de sistema operativo.
   - En WASM, se incluye `wasm/juce_shim/wasm_link_stubs.cpp` y `wasm/juce_shim/wasm_compat.h` para proveer stubs mínimos de compatibilidad sin inflar el binario.
2. **Optimización de Memoria y Flags de Emscripten:**
   - `-s SINGLE_FILE=1`: Embebe el binario `.wasm` como Base64 dentro del pegamento JS `ms2000_dsp.js`.
   - `-s MALLOC=emmalloc`: Usa el asignador ultra-ligero para tiempo real.
   - `-s "EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue','HEAPF32']"`: Exporta acceso directo a memoria.
   - `-s ALLOW_MEMORY_GROWTH=1` con memoria inicial fija de 32MB.
3. **Zero-Copy en AudioWorklet (`ms2000Worklet.js`):**
   - Cachear las vistas de `HEAPF32.subarray()` durante la inicialización (`init`), **nunca crear TypedArrays dentro de `process()`**.
   - Los punteros de entrada y salida de audio se reutilizan en cada bloque de 128 muestras.
4. **Seguridad de Ruido en Web:**
   - Jamás invocar `juce::Random` sin semilla fija en el worklet; se usa generador LCG determinista o semilla explícita `juce::Random(12345)`.

### 9.2 Modo Headless / Servidor RPC para Raspberry Pi (`HeadlessRpcServer`)

- **Objetivo:** Permitir ejecutar el motor en dispositivos embebidos (ej. Raspberry Pi 4/5) sin entorno gráfico X11/Wayland.
- **Implementación:** `Source/Standalone/HeadlessRpcServer.h / .cpp` inicializa el motor de audio ALSA/JACK y abre un servidor ligero de sockets/IPC que acepta comandos JSON para cambio de presets y parámetros.

### 9.3 Scripts de Compilación (`.bat`)

| Script | Propósito | Comandos Clave |
|---|---|---|
| `build.bat` | Compila Standalone y VST3 para Windows (MSVC / Ninja) | `node Scripts/registry_generator.js`<br>`node Scripts/build_webui.js`<br>`cmake -B build -S . -DCMAKE_BUILD_TYPE=Release`<br>`cmake --build build --config Release` |
| `wasm/build_wasm.bat` | Compila el motor DSP en WebAssembly (AudioWorklet) | `emcmake cmake -B build_wasm -S wasm`<br>`cmake --build build_wasm --config Release`<br>`-s SINGLE_FILE=1 -s MALLOC=emmalloc` |

### 9.4 Empaquetado de Assets y Estilos (Single Source of Truth)

- **Modo Desarrollo (Hot Reload):** El `PluginEditor_ResourceProvider.cpp` sirve los archivos directamente desde la carpeta `WebUI/` en el disco duro, permitiendo editar HTML/CSS/JS y ver cambios en tiempo real sin recompilar C++.
- **Modo Release (Producción):** El script `Scripts/build_webui.js` minifica y empaqueta la carpeta `WebUI/` en un archivo binario `.cpp` embebido mediante JUCE `BinaryData`. El plugin resultante es **100% autónomo (un solo archivo `.vst3` o `.exe`)** sin dependencias externas.

### 9.5 Rutas de Almacenamiento de Bancos y Parches

| Entorno | Ruta de Almacenamiento | Formatos Soportados |
|---|---|---|
| **Windows** | `%APPDATA%\ABDSynths\ABDMS2000\Banks\` | `.syx` (Korg estándar), `.json` (ABD canónico), `.prg` |
| **macOS** | `~/Library/Application Support/ABDSynths/ABDMS2000/Banks/` | `.syx`, `.json`, `.prg` |
| **Web / PWA** | `IndexedDB` (Base de datos local en navegador) | `.json`, `.syx` |
| **Fallback de Fábrica** | Embebido en C++ (`Source/State/FactoryPresets.cpp`) y `WebUI/public/presets/` | Carga automática en memoria al iniciar si no hay bancos en disco |

---

## 10. Plan de Testing y Lista de Control de Calidad (Plugin Quality Checklist)

### 10.1 Checklist de Calidad Profesional (Estándar ABDEep)

1. **Real-Time Safety Estricta:**
   - Cero bloqueos (`mutex`), cero asignaciones dinámicas (`malloc` / `new`), cero llamadas a disco o red y cero logs (`std::cout`, `DBG`) en el hilo de audio (`processBlock`).
2. **Transiciones Suaves de Parches (De-zippering / Anti-Click):**
   - Rampa de crossfade suave de 5ms al cambiar de programa para evitar clics audibles al conmutar formas de onda o estados de filtro.
3. **Persistencia Total del Estado (APVTS):**
   - El estado del plugin se restaura de forma idéntica en cualquier DAW tras guardar y reabrir el proyecto.
4. **Validación Exhaustiva con `pluginval`:**
   - Pase obligatorio con Nivel de Rigor 10 (`pluginval --strictness-level 10`) antes de cualquier release.
5. **Cierre Limpio:**
   - Cero fugas de memoria o hilos colgados al cerrar la UI o destruir la instancia del plugin.

### 10.2 C++ Tests

| Test Suite | Qué Valida |
|---|---|
| `SynthEngineTests` | Procesado de audio, silencio en init, note on/off |
| `SysExRoundTripTest` | Encode → Decode SysEx = datos idénticos |
| `FilterTests` | Respuesta logarítmica de frecuencia y compensación de ganancia |
| `VocoderTests` | Análisis/síntesis de bandas, envelope followers y gates |
| `OscillatorTests` | Wavetables DWGS, anti-aliasing PolyBLEP |
| `ArpeggiatorTests` | Patrones inyectados al MidiBuffer |

### 10.3 WebUI Tests (Vitest)

| Categoría | Tests |
|---|---|
| **Unit** | `parameterStore`, `normalizer`, `sysExCodec`, `midiMap` |
| **Integration** | `bridgeWasm`, `bankManager`, `patchFlow` |
| **Contract** | `registryGen` (gen.js = gen.h), `sysExRoundtrip` |

### 10.4 Parity Tests

- Comparar output del motor C++ nativo vs WASM con los mismos inputs
- Golden master: grabar audio de referencia y comparar bit-exacto

---

## 11. Fases de Desarrollo Propuestas

### Fase 0 — Infraestructura (Base)
- [ ] Crear repositorio y estructura de directorios
- [ ] Configurar CMakeLists.txt (JUCE 8 submodule, targets Standalone + VST3)
- [ ] Configurar WebView2 básico (index.html con selector Tri-Modo)
- [ ] Script `registry_generator.js` y `build_webui.js` funcionales
- [ ] CI básico (build Windows + tests)
- [ ] `parameters-spec.schema.v1.json` con los primeros parámetros

### Fase 1 — Motor DSP Básico
- [ ] Osciladores VA con PolyBLEP (Saw, Square, Triangle, Sine)
- [ ] Filtro multimode (LP24 + HP12 mínimo) con comportamiento analógico
- [ ] ADSR Envelopes (EG1 + EG2) exponenciales
- [ ] VoiceManager (4 voces polyphonic)
- [ ] Audio output funcional en standalone

### Fase 2 — MIDI Bidireccional
- [ ] MIDIProcessor: recibir notas + CCs del MS2000
- [ ] MIDIMap completo (todos los CCs del spec)
- [ ] Envío de CCs al hardware
- [ ] HardwareLink: detección y selección de puerto MIDI

### Fase 3 — SysEx y Gestión Universal de Bancos
- [ ] SysExCodec: encode/decode Korg 7-bit a 8-bit (validado con ReMS2000)
- [ ] Model Adapters (MS2000 `0x58` y microKORG `0x5E`)
- [ ] Program Dump Request/Receive
- [ ] All Data Dump Request/Receive
- [ ] BankManager Universal (búsqueda, filtros, drag & drop, diff viewer)
- [ ] Pantalla SysEx Inspector con copiado/pegado de Hex

### Fase 4 — WebUI Completa y Tematización
- [ ] Panel de osciladores con knobs funcionales
- [ ] Panel de filtro con compensación visual
- [ ] Panel de envelopes (visualización gráfica)
- [ ] LCD Hardware Emulado (Menús oficiales 1A-4D)
- [ ] Skin MS2000 (Azul) + Skin microKORG (Madera/Beige) vía Tokens CSS
- [ ] Teclado virtual y controles de expresión

### Fase 5 — DSP Avanzado
- [ ] DWGS wavetables (64 formas de onda cargadas en memoria)
- [ ] Ring Mod + Sync + Cross Mod
- [ ] Virtual Patch (Mod Matrix) con escalas verificadas (±2 oct pitch, ±5 oct cutoff)
- [ ] LFO logarítmico con tempo sync
- [ ] Arpeggiador (MidiBuffer inject)
- [ ] Efectos (EQ, Mod FX, Delay)
- [ ] Mod Sequence (Step + Smooth con Slew Limiter de 10ms)

### Fase 6 — Vocoder
- [ ] 16-band analysis filters
- [ ] 16-band synthesis filters
- [ ] Envelope followers
- [ ] Formant shift y HPF Gate (transient detector con ruido blanco)
- [ ] Audio input routing

### Fase 7 — WASM / Web
- [ ] Compilación WASM del motor (`build_wasm.bat`)
- [ ] AudioWorklet integration
- [ ] Bridge WASM ↔ WebUI
- [ ] PWA funcional con IndexedDB para bancos

### Fase 8 — Timbres Multi y Modo Avanzado
- [ ] Soporte Layer (2 timbres) y Split
- [ ] Voice Assign: Unison mode con Detune y Spread
- [ ] Modo Avanzado (Polifonía 32 voces, Reverb Shimmer, Tape Delay, 4 LFOs, Mod Seq 64 pasos)

### Fase 9 — Polish y Release
- [ ] MIDI Learn
- [ ] Presets de fábrica completos (MS2000 + microKORG)
- [ ] Manual de usuario integrado
- [ ] Build macOS
- [ ] pluginval validation

---

## 12. Índice Canónico de Documentación Técnica (`DOCS/`)

Todos los detalles de bajo nivel, análisis de código e investigaciones están organizados en el directorio `DOCS/`:

| Documento | Ubicación | Contenido Principal |
|---|---|---|
| **Research Notes Maestras** | [`DOCS/MS2000_Research_Notes.md`](file:///d:/desarrollos/ABDSynths/ABDMS2000/DOCS/MS2000_Research_Notes.md) | 18 secciones completas: curvas de transferencia, fórmulas matemáticas C++, comparativas de frecuencia, comportamiento de filtros y envolventes. |
| **Especificación SysEx** | [`DOCS/MS2000_SysEx_Spec.md`](file:///d:/desarrollos/ABDSynths/ABDMS2000/DOCS/MS2000_SysEx_Spec.md) | Mapa de memoria hexadecimal de parámetros (Offsets 0x00 a 0x43), sub-comandos Korg y protocolo de comunicación. |
| **Análisis Lua ReMS2000** | [`DOCS/ReMS2000_Lua_Analysis.md`](file:///d:/desarrollos/ABDSynths/ABDMS2000/DOCS/ReMS2000_Lua_Analysis.md) | Código fuente y algoritmos extraídos del panel Ctrlr ReMS2000: empaquetado/desempaquetado 7-to-8 bits, gestión de volcados y menús LCD. |
| **MIDI CCs y NRPNs** | `DOCS/KORG MS2000 MIDI CCs and NRPNs.html` | Tabla de mapeo físico oficial de controladores continuos y NRPNs. |
| **Max4Live Devices** | `DOCS/MAX/max4live-main/` | Mapeos `.amxd` y scripts `nrpn.js` para MS-2000 y microKORG. |
| **Manuales Oficiales** | `DOCS/manuals/` | Manual de servicio con esquemáticos y manual de instrucciones del usuario. |
| **Assets Gráficos y Fuentes** | `DOCS/ReMS2000-main/resources/` | Tipografías originales (`REGISTER.TTF`, `BebasNeue`, `RobotoCondensed`) y plantillas gráficas. |
