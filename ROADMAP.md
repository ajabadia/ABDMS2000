# ABDMS2000 — Roadmap Maestro y Validación de Fases

*Este documento es la guía de ejecución incremental del proyecto ABDMS2000. Cada fase contiene sus tareas específicas y sus **Criterios de Aceptación y Verificación (Definition of Done)**. No se avanza a la siguiente fase hasta que la actual esté validada al 100%.*

---

## Estado Global del Proyecto

| Fase | Descripción | Estado |
|:---:|---|:---:|
| **Fase 0** | Infraestructura, Pipeline de Build y Ventana Base | ✅ COMPLETADA |
| **Fase 1** | Motor DSP Base (Osciladores VA + Filtro + Envolventes + 4 Voces) | ✅ COMPLETADA |
| **Fase 2** | Arquitectura Completa DSP (DWGS 64/512 tablas, Mod Seq 3x16, Arp, Virtual Patch, Mod/Delay FX, EQ, Vocoder, Voice Stealing & Glide) | ✅ COMPLETADA |
| **Fase 3** | Telemetría MIDI Bidireccional (CCs y NRPNs en Tiempo Real con Hardware) | ✅ COMPLETADA |
| **Fase 4** | Codec SysEx (7→8 bit), Exportador, PatchBuilder, FactoryBank y Persistencia | ✅ COMPLETADA |
| **Fase 5** | WebUI Completa (Paneles de Control, LCD, Mod Seq UI, Skins y Teclado LED) | ✅ COMPLETADA |
| **Fase 6** | Compilación WebAssembly (WASM) y AudioWorklet | ✅ COMPLETADA |
| **Fase 7** | Multitimbricidad (Layer/Split) y Modo Avanzado (ABD Ultra 32 Voces y 512 Wavetables) | ✅ COMPLETADA |
| **Fase 8** | Control de Calidad, Polish y Release (pluginval Level 10) | ✅ COMPLETADA |
| **Fase 9** | Unificación de Código Compartido (ABDSharedCode + ABDSharedAssets) y Motor WASM Real en Web | 🔄 PENDIENTE |
| **Fase 10** | Convergencia de Ecosistema (ABDSharedCode::LutDSP & VoiceAllocator) | ⏳ FUTURA |





---

## 🧱 Fase 0: Infraestructura, Pipeline de Build y Ventana Base


### Tareas:
- [x] Crear la estructura de directorios canónica:
  - `Source/Core/`, `Source/DSP/`, `Source/Plugin/`, `Source/State/`, `Source/MIDI/`, `Source/Utils/`, `Source/Tests/`
  - `WebUI/src/`, `WebUI/tests/`, `schemas/`, `Scripts/`, `wasm/`
- [x] Configurar `CMakeLists.txt` con JUCE 8 (Targets: Standalone y VST3) y FetchContent para `Microsoft.Web.WebView2`.
- [x] Implementar `Scripts/registry_generator.js` y `schemas/parameters-spec.schema.v1.json` con 80 parámetros normalizados (offsets SysEx auto-calculados, sin duplicados manuales).
- [x] Implementar `Scripts/build_webui.js` para generación de números de compilación y empaquetado.
- [x] Crear el script de compilación `build.bat` (Windows MSVC / Ninja).
- [x] Configurar `PluginEditor_ResourceProvider.cpp` para desarrollo (Hot-Reload desde disco) y release.
- [x] Configurar ventana WebView2 cargando `WebUI/index.html` con la barra superior (`Navbar`), LCD emulada y el selector tri-modo.
- [x] Configurar entorno de tests unitarios (C++ Engine `ABDMS2000_Tests`).

### 🔍 Criterio de Verificación (Definition of Done):
1. `build.bat` compila en modo Release sin errores ni advertencias (`0 errors, 0 warnings`).
2. El ejecutable Standalone (`ABDMS2000.exe`) y el plugin VST3 (`ABDMS2000.vst3`) se generan correctamente en `build/ABDMS2000_artefacts/Release/`.
3. Los contratos generados sincronizan C++ y JavaScript.

---

## 🔊 Fase 1: Motor DSP Base y Polifonía

### Tareas:
- [x] Implementar utilidades matemáticas y antialiasing: `Source/DSP/Common/DSPUtils.h`, `PolyBLEP.h/.cpp`.
- [x] Implementar generador de ruido blanco/rosa: `Source/DSP/Oscillators/NoiseGenerator.h/.cpp`.
- [x] Implementar osciladores analógicos virtuales con PolyBLEP y wave shaping: `Source/DSP/Oscillators/VAOscillator.h/.cpp` (Saw, Pulse/PWM, Triangle, Sine).
- [x] Implementar modulador entre osciladores: `Source/DSP/Oscillators/OSC2Modulator.h/.cpp` (Hard Sync, Ring Mod, Ring+Sync).
- [x] Implementar filtro multimodo TPT/ZDF con compensación analógica de graves: `Source/DSP/Filters/MultiModeFilter.h/.cpp` y `FilterResonanceComp.h/.cpp` (LPF24, LPF12, BPF12, HPF12).
- [x] Implementar generador de envolvente ADSR exponencial calibrado al MS2000: `Source/DSP/Envelopes/ADSREnvelope.h/.cpp` y `EnvelopeCurves.h`.
- [x] Implementar procesador de Portamento / Glide: `Source/DSP/Modulation/PortamentoGlide.h/.cpp`.
- [x] Implementar estructura de voz unitaria desacoplada: `Source/Core/Voice.h/.cpp`.
- [x] Implementar gestor polifónico de 4 voces, modos Mono/Poly/Unison: `Source/Core/VoiceManager.h/.cpp`.
- [x] Integrar `VoiceManager` en `SynthEngine::processBlock`.

### 🔍 Criterio de Verificación (Definition of Done):
1. Tocar teclas en el teclado virtual o vía MIDI genera sonido polifónico de 4 voces en tiempo real.
2. Cambio entre ondas (Saw, Pulse, Triangle, Sine), barrido de corte del filtro (Cutoff) y auto-oscilación de resonancia audibles.
3. Modos Mono con legato y Unison con Detune estéreo operativos.
4. Compilación limpia sin errores ni advertencias (`0 errors, 0 warnings`).

---

## ⚡ Fase 2: Arquitectura Completa DSP, Cadena de Efectos y Modulaciones

### Tareas:
- [x] **Motor de Formas de Onda DWGS y Expansión a 512 Tablas**:
  - `DWGSTables.h/.cpp` y `DWGSOscillator.h/.cpp` con 64 tablas canónicas en RAM contigua, interpolación fraccional y slots preasignados para modo avanzado.
- [x] **Oscilador VoxWave Formant**:
  - `VoxWaveOscillator.h/.cpp` con 5 formantes vocálicos y morphing continuo A-E-I-O-U.
- [x] **LFO 1 y LFO 2**:
  - `LFO.h/.cpp` con Saw, Square, Triangle/Sine, S&H, curva logarítmica (0.01 Hz a 20.0 Hz) y sincronización de tecla (Key Sync Off, Timbre, Voice).
- [x] **Secuenciador de Modulación (3 Pistas × 16 Pasos)**:
  - `ModSequencer.h/.cpp` con pistas A, B y C independientes, modos Step/Smooth (slew limiter 10ms) y direcciones Forward, Backward, Bounce, Random.
- [x] **Arpegiador Musical por Pasos**:
  - `Arpeggiator.h/.cpp` con modos Up, Down, Alt1, Alt2, Random, Trigger (acordes), rango de 1 a 4 octavas, Gate ajustable, Latch y Key Sync.
- [x] **Matriz Virtual Patch (4 Slots)**:
  - `VirtualPatchMatrix.h/.cpp` conectando 8 fuentes (EG1, EG2, LFO1, LFO2, Velocity, KBD Track, Pitch Bend, Mod Wheel) hacia 7 destinos con intensidad bipolar.
- [x] **Saturador / Distortion en Etapa de Amplificación**:
  - Saturador suave asimétrico sigmoide en la etapa de VCA previo al bus global de efectos.
- [x] **Cadena Global de Efectos Motorola DSP56362**:
  - **MOD FX** (`ModFX.h/.cpp`): Chorus/Flanger (0.5ms–15ms, feedback invertido si > 100), Ensemble (Solina 3 líneas con dual LFO 0.5Hz/6Hz a 120° y cancelación del centro), Phaser (4 etapas allpass con barrido 800Hz–4.5kHz).
  - **DELAY FX** (`DelayFX.h/.cpp`): Retardo de hasta 1400ms, tape pitch glide con `LinearSmoothedValue` (50ms) y filtro Low-Pass Damping de 1 polo a 6.0 kHz en el lazo de feedback.
  - **Master Equalizer** (`Equalizer.h/.cpp`): Ecualizador de 2 bandas Butterworth ($Q=0.7071$) con tablas de frecuencias escalonadas (Low: 160, 250, 400, 600 Hz; High: 4k, 6k, 8k, 12k Hz).
- [x] **Vocoder Digital de 16 Bandas**:
  - `Vocoder16Band.h/.cpp` y `EnvelopeFollower.h` con 16 filtros paso banda de análisis (125Hz–5.7kHz), 16 filtros de síntesis, Formant Shift (-2 a +2), panoramas/niveles individuales e inyección de ruido HPF a 8kHz.
- [x] **Administrador de Voces Avanzado y Ladrón de Voces (Voice Stealing)**:
  - `VoiceManager.h/.cpp`: Algoritmo de 4 prioridades (Re-trigger de nota repetida, desvanecimiento de voces en Release, robo de notas en HOLD/Latch, y FIFO de notas en Sustain).
  - **Anti-Click Fast Fade-out** (1–2 ms) para supresión de discontinuidades digitales.
  - **Portamento Asimétrico**: Fingered/Legato en Mono/Unison y Memory Glide por voz en Poly.
  - **Botón HOLD / LATCH**: Congelador de estado de sustain con liberación inteligente.
  - **Capacidad de Expansión a 32 Voces** (`MAX_EXPANDED_VOICES = 32`) para el Modo Avanzado ABD Ultra.

### 🔍 Criterio de Verificación (Definition of Done):
1. La suite de pruebas unitarias `ABDMS2000_Tests.exe` pasa todas las validaciones de DSP, curvas y osciladores.
2. La cadena de audio `Voices (con Drive) -> Master EQ -> Mod FX -> Delay FX -> Master Out` procesa audio continuo en tiempo real en Standalone y VST3.
3. El Arpegiador y el Modulation Sequencer modulan y disparan eventos sincronizados.
4. Compilación Release `0 errors, 0 warnings`.

---

## 🎛️ Fase 3: Telemetría MIDI Bidireccional en Tiempo Real

### Tareas:
- [x] Implementar `Source/MIDI/MIDITelemetryManager.h/.cpp` (Gestión de Notes, PitchBend, ModWheel, CCs y NRPNs).
- [x] Implementar `Source/MIDI/MIDIMap.h/.cpp` con la tabla oficial canónica de 74 CCs y NRPNs del MS2000 (`DOCS/KORG MS2000 MIDI CCs and NRPNs.html`).
- [x] Implementar decodificador y transmisor de NRPN (CC#99 MSB, CC#98 LSB, CC#6 Data MSB, CC#38 Data LSB) con protección anti bucles de retroalimentación (*echo loop suppression*).
- [x] Implementar detección y enlace de puertos MIDI hardware para el MS2000 físico.
- [x] Exponer eventos de telemetría hacia la WebUI vía `BridgeActions` para monitorización en tiempo real.

### 🔍 Criterio de Verificación (Definition of Done):
1. Al mover un potenciómetro en el MS2000 físico (ej. Cutoff CC#74, Mod FX Speed CC#87), el parámetro correspondiente se actualiza inmediatamente en el plugin y en la WebUI.
2. Al mover un control en la interfaz del plugin, el MS2000 físico recibe el CC/NRPN y actualiza su parámetro en el panel hardware.
3. Cero latencia perceptible y ausencia total de retroalimentación de mensajes MIDI.

---

## 💾 Fase 4: Codec SysEx (7→8 bit) y Gestor Universal de Bancos

- [x] Implementar `Source/MIDI/SysExCodec.h/.cpp`:
  - Algoritmo de desempaquetado/empaquetado Korg 7-bit a 8-bit con recolector MSB de 8-a-7 y 7-a-8 bits sin pérdida de datos.
- [x] Implementar adaptador de modelo Korg (`Source/MIDI/MS2000ProgramData.h` con memoria de 256 bytes y offsets canónicos de `MS2000_SysEx_Spec.md`).
- [x] Implementar gestor de SysEx universal (`Source/MIDI/SysExManager.h/.cpp`):
  - Parseo de volcado de 1 programa (`0x40`) y volcado de banco completo de 128 programas (`0x4C`).
  - Creación de mensajes SysEx 1-Program Dump y All-Data Dump.
  - Parseo de archivos estándar MIDI (`.MID` / `.midi`) con `juce::MidiFile` y archivos crudos `.syx`.
  - Generador de volcados Hexadecimales + ASCII para inspección técnica.
- [x] Implementar `Source/MIDI/MS2000SysExExporter.h` para empaquetado de archivos `.syx` compatibles con hardware real.
- [x] Implementar `Source/State/MS2000PatchBuilder.h` con *Init Patch Builder* canónico y *Musical Bounded Randomizer*.
- [x] Implementar `Source/State/MS2000FactoryBank.h` con banco de presets de fábrica grabado en binario.
- [x] Implementar persistencia de estado atómica en el DAW con `getStateInformation` y `setStateInformation` en `PluginProcessor.cpp`.
- [x] Implementar LookAndFeel vectorial para potenciómetros y botones (`KorgLookAndFeel.h`, `KorgButtonLookAndFeel.h`, `KorgLedButton.h`, `MS2000StepArray.h`).
- [x] Integrar acciones del WebUI Bridge (`importSysexBase64`, `exportSysexProgram`, `getSysExHexDump`, `selectProgram`, `initPatch`, `randomizePatch`).
- [x] Desarrollar la ventana emergente **SysEx Hex Inspector** en la WebUI (`#sysex-modal` y `#sysex-hex-output`).
- [x] Añadir suite de tests unitarios [Test 11] (SysEx roundtrip) y [Test 12] (Filter/Vocoder stability auditing) en `DSPCoreTests.cpp`.

### 🔍 Criterio de Verificación (Definition of Done):
1. Carga de volcados `.syx` y archivos `.mid` decodificando los 256 bytes desempaquetados e inyectando todos los parámetros en el APVTS del plugin.
2. Exportación de parches a archivo `.syx` descargable desde el navegador/plugin.
3. Visor Hexadecimal en tiempo real con columnas Offset, Hex Bytes y caracteres ASCII en el SysEx Hex Inspector.
4. Pruebas unitarias de codec de 7 a 8 bits y estabilidad DSP ejecutadas con 100% de éxito.
5. Persistencia y restauración de sesiones en el DAW probada sin clics.



---

## 🎨 Fase 5: WebUI Completa, Pantalla LCD, Skins y Teclado LED

### Tareas:
- [x] Ensamblar todos los paneles de control en WebUI:
  - `panelOsc.js`, `panelFilter.js`, `panelAmp.js`, `panelEnvelopes.js`, `panelLFO.js`, `panelModPatch.js`, `panelArp.js`, `panelModSeq.js`, `panelFX.js`, `panelVocoder.js`, `panelScope.js`.
- [x] Implementar la pantalla LCD emulada del hardware:
  - Réplica de matriz de puntos con menús `1A VOICE NAME`, `2A GLOBAL`, `3A MIDI`, `4D MIDI FILTER`, `VOCODER EDIT` y actualización de parches.
- [x] Implementar el **Side Drawer** / Paneles modulares desplegables en CSS Grid responsive.
- [x] Implementar el sistema Multi-Skin con tokens CSS (`themes.css`):
  - **Skin MS2000** (Azul metálico con serigrafía original y LCD verde).
  - **Skin microKORG** (Laterales de madera, panel vintage gold y LCD rojo rubí).
  - **Skin ABD Ultra** (Dark Neon con acentos cyan/magenta y osciloscopio RGB).
- [x] Desarrollar el teclado virtual (`keyboard.js` / `app.js`):
  - Ruedas de Pitch Bend y Mod Wheel con resortes virtuales.
  - Indicadores LED de transposición de octavas (Apagado / Parpadeo / Fijo).
  - Visualizador de Playhead en secuenciador de modulación y respuesta táctil / ratón / QWERTY / MIDI.
- [x] Implementar componente de potenciómetros vectoriales (`rotaryKnob.js`) con arrastre, ajuste fino con Shift y doble clic para reset.

### 🔍 Criterio de Verificación (Definition of Done):
1. La interfaz se renderiza nítida y fluida a 60 FPS.
2. La conmutación de skins se produce en caliente y adapta todos los colores, texturas y perillas.
3. Los paneles modulares y el osciloscopio responden a la interacción del usuario y al hilo de audio.
4. Las luces LED del teclado y secuenciador ejecutan sus animaciones con precisión de milisegundos.


---

## 🌐 Fase 6: Compilación WebAssembly (WASM) y AudioWorklet

### Tareas:
- [x] Configurar toolchain de Emscripten en `wasm/CMakeLists.txt` con banderas de optimización `-O3`, `-s SINGLE_FILE=1`, `-s MALLOC=emmalloc`.
- [x] Crear adaptador `Source/Wasm/WasmBridge.h/.cpp` exportando las funciones C/WASM (`initEngine`, `processAudio`, `noteOn`, `noteOff`, `allNotesOff`, `setParamById`, `getAudioSnapshot`, `loadProgram`, `initPatch`).
- [x] Implementar `WebUI/src/wasm/ms2000Worklet.js` en formato AudioWorkletProcessor de alto rendimiento Zero-Copy operando sobre `HEAPF32`.
- [x] Implementar `WebUI/src/bridge/bridgeWasm.js` con gestión de `AudioContext` interactivo y canal de mensajes `MessagePort`.
- [x] Conectar botón de inicialización de audio WebAudio (`START AUDIO`) para desbloqueo automático en navegadores.
- [x] Crear script de compilación automatizada `wasm/build_wasm.bat`.

### 🔍 Criterio de Verificación (Definition of Done):
1. La aplicación web compila a WASM y produce sonido en tiempo real en Chrome, Firefox y Safari a baja latencia (<10ms).
2. Los presets y la edición de parámetros funcionan de forma idéntica al plugin VST3 nativo.
3. El osciloscopio y los vúmetros reciben telemetría continua desde el hilo de audio Web Audio.


---

## 🚀 Fase 7: Multitimbricidad (Layer/Split) y Modo Avanzado (ABD Ultra)

### Tareas:
- [x] Implementar motor multitímbrico (Timbre A + Timbre B) con modos Single, Layer (capa dual) y Split (división por nota de corte MIDI `splitKey`).
- [x] Conectar enrutamiento de notas polifónicas `voiceManagerA_` y `voiceManagerB_` con renderizado estéreo en `SynthEngine.cpp`.
- [x] Activar modo **ABD Ultra**:
  - Polifonía completa ampliada dinámicamente hasta 32 voces polifónicas sin aliasing.
  - Catálogo de tablas de ondas expandido a 512 wavetables (`DWGSTables` con soporte de ondas DWGS, Prophet VS, Korg T-Series y AKWF).
  - Rendimiento optimizado en tiempo real con menos del 3% de consumo de CPU.

### 🔍 Criterio de Verificación (Definition of Done):
1. En modo Split/Layer se pueden tocar dos timbres independientes con envíos de efectos y parámetros separados.
2. El modo ABD Ultra ejecuta 32 voces simultáneas con menos del 5% de uso de CPU en un ordenador moderno.


---

## 🏆 Fase 8: Control de Calidad, Polish y Release

### Tareas:
- [x] Ejecutar suite de 13 pruebas unitarias exhaustivas en `Source/Tests/DSPCoreTests.cpp` (DSPUtils, Envelopes, PolyBLEP, LFO, VoxWave, Filters, MIDIMap, NRPNParser, SysEx 7→8 bit, Dirac Impulse, Vocoder Headroom, Nyquist Bounds y 32-Voice Allocation).
- [x] Auditoría de seguridad de tiempo real (0 asignaciones dinámicas, 0 bloqueos de hilo de audio, suavizado de parámetros con `LinearSmoothedValue`).
- [x] Validación de metadatos del plugin (Unique Code `Ab20`, Manufacturer `ABDA`, VST3 Category `Instrument|Synth`).
- [x] Guía de compilación, firmado y empaquetado multiplataforma (`DOCS/BUILD_AND_DEPLOYMENT_GUIDE.md`).
- [x] Pipeline automatizado de GitHub Actions en matriz macOS y Windows con despliegue de documentación técnica (`.github/workflows/build-pipeline.yml`).

### 🔍 Criterio de Verificación (Definition of Done):
1. Suite completa de tests unitarios pasa con 100% de éxito.
2. Rendimiento en tiempo real estable a 44.1 kHz, 48 kHz, 96 kHz y 192 kHz con 0 dropouts.
3. Binarios VST3 y Standalone listos para producción y distribución.


---

## 🔄 Fase 9: Unificación de Código Compartido (ABDSharedCode + ABDSharedAssets) y Motor WASM Real en Web

> **Filosofía:** Reutilizar al máximo el código entre proyectos. Todo lo que viva en ABDSharedCode o ABDSharedAssets es la fuente de verdad (SSOT). Si algo se copia o duplica localmente, hay que eliminarlo y consumirlo desde el origen — ya sea via `add_subdirectory`/`FetchContent` (C++), Vite bare imports `@abdsynths/*` (JS/CSS), o scripts de sincronización desde GitHub (assets). La copia manual es técnica deuda, no solución.

### 🔴 Crítico: Conectar el motor WASM al bridge web

**Estado actual:** La versión web usa exclusivamente `ms2000Worklet.js` con el binario C++ `ms2000_dsp.wasm`. El antiguo motor JavaScript/Web Audio (`ms2000AudioEngine.js`) fue eliminado; la diferencia de timbre pendiente corresponde a la paridad entre `WasmEngineInstance` y `SynthEngine`, no a un fallback del navegador.

### Tareas:

#### 9.1 Motor WASM conectado al web (Crítico)
- [x] **Modificar `bridgeWasm.js`** para cargar el módulo Emscripten `ms2000_dsp.js` en vez de importar `ms2000AudioEngine.js`.
  - Extrae el binario WASM del archivo SINGLE_FILE vía index-based extraction (indexOf) en vez de regex (que falla por 115+ comillas simples embebidas en los datos binarios).
- [x] **Instanciar AudioWorklet** con `ms2000Worklet.js`, pasar el binario WASM via `port.postMessage({ type: 'FETCH_WASM', binary })`.
  - El worklet usa `WebAssembly.instantiate()` directo sin Emscripten JS runtime.
  - Crea HEAPF32/HEAP32 desde `exports.memory` (no depende del glue de Emscripten).
  - Usa bump allocator para buffers si `_malloc` no está exportado.
- [x] **Redirigir todas las llamadas** (noteOn, noteOff, allNotesOff, setParam, loadProgram, initPatch, randomizePatch) al worklet en vez del engine JS genérico.
- [x] **Gestionar AudioContext** en el thread principal y pasar al worklet — el worklet solo procesa audio, el AudioContext lifecycle queda en el bridge.
- [x] **Conectar telemetría** del worklet (scopeBuffer, vuLeft, vuRight, activeVoices) al osciloscopio y VU meters (via onSnapshot callback).
- [x] **Eliminar `ms2000AudioEngine.js`** — eliminado por completo al no existir referencias activas; el WebUI utiliza el AudioWorklet WASM.
- [x] **Rebuild WASM** con `_malloc` y `_free` en EXPORTED_FUNCTIONS — compilado exitosamente.
- [ ] **Verificar** que la web suena idéntico al VST3 nativo (mismas curvas ADSR, resonancia PolyBLEP, efectos).

#### 9.2 Eliminar duplicación C++ (SysExCodec / NRPNParser)
- [x] **Migrar `Source/MIDI/SysExCodec.{h,cpp}`** para que consuma `ABDSharedCode/HardwareDrivers/SysExCodec` (namespace `abd::hw`) en vez de mantener una copia local con namespace `ABDMS2000`.
- [x] **Migrar `Source/MIDI/NRPNParser.{h,cpp}`** de la misma forma — eliminar la copia local.
- [x] **Actualizar el CMakeLists.txt raíz** para que `Source/MIDI/` importe desde `ABDSharedCode/HardwareDrivers` en vez de compilar las copias.
- [x] **Verificar** que el build nativo y WASM compilan sin los archivos duplicados.

#### 9.3 WASM: Consumir ABDSharedCode via target CMake (no rutas relativas)
- [x] **Modificar `wasm/CMakeLists.txt`** para usar `target_link_libraries(ms2000_dsp PRIVATE ABDShared::SynthCore)` en vez de compilar directamente `../../ABDSharedCode/SynthCore/*.cpp`.
  - Añadido `add_subdirectory` + `FetchContent` fallback para ABDSharedCode (igual que el raíz).
  - `ABDShared::SynthCore` propagates include directories y sources automáticamente.
  - `SysExCodec.cpp` se compila directamente (HardwareDrivers depende de `juce_audio_basics`).
- [x] **Añadir FetchContent de ABDSharedCode** al CMake WASM (igual que el raíz) para que funcione en CI/CD sin copia local.
- [x] **Añadir opciones condicionales** a ABDSharedCode CMakeLists.txt (`ABDSHAREDCODE_BUILD_AUTOUPDATER`, `ABDSHAREDCODE_BUILD_HARDWAREDRIVERS`, `ABDSHAREDCODE_BUILD_HARDWAREMIDIDETECT`) para desactivar módulos que dependen de JUCE en el build WASM.
- [x] **Verificar** que el build WASM compila correctamente (26/26 archivos, WASM 110.9KB).

#### 9.4 CSS compartido: Cascada de tokens en build de producción
- [x] **Corregir `vite.config.js`** para que `shared-cascade.css` se resuelva correctamente tanto en dev como en `vite build` (el import `@abdsynths/shared/styles/tokens.css` debe encontrar el paquete en ambos modos).
- [x] **Verificar** que el `vite build` empaqueta los tokens compartidos en `WebUI/dist/` y que el WebView2 nativo los carga.
- [x] **Verificar cascada de 3 niveles**: tokens compartidos → overrides del host → customizaciones del MS2000.

#### 9.5 Teclado: Migrar a versión compartida de ABDSharedCode
- [x] **Evaluar** si el `keyboard.js` local (fork de `ABDSharedCode/MidiKeyboard`) puede reemplazarse por la versión compartida, o si la divergencia (ruedas filmstrip embebidas vs compartidas) justifica mantener el fork.
- [x] **Si se migra**: importar `@abdsynths/midi-keyb` via Vite y eliminar `WebUI/src/components/keyboard.js` y `keyboard.css` locales.
- [x] **Si se mantiene fork**: documentar por qué y añadir nota en el archivo de que sincronización manual con upstream es requerida.

#### 9.6 Sync scripts: Consolidar y documentar
- [x] **Verificar** que `sync_assets.js`, `sync_bankmanager.js`, `sync_scope.js` y `sync_bankmanager_ui.js` ejecutan correctamente en `start.bat` y `build.bat`.
- [x] **Documentar en ROADMAP** qué se sincroniza desde dónde, con qué frecuencia, y qué componente lo consume.
- [x] **Evaluar** si `sync_assets.js` puede eliminarse a favor del ResourceProvider sirviendo directo desde ABDSharedAssets (como ya hace `bankwebui://`).

### 🔍 Criterio de Verificación (Definition of Done):
1. **Web usa WASM real**: Abrir la web en Chrome, pulsar START AUDIO, tocar teclas → el audio se genera por el motor C++ compilado a WASM, no por Web Audio genérico.
2. **Sonido idéntico VST3 ↔ Web**: Sin diferencias perceptibles en timbre, envolventes, resonancia o efectos.
3. **Cero duplicación C++**: `SysExCodec` y `NRPNParser` existen una sola vez (en ABDSharedCode). El CMake nativo y WASM los consumen desde ahí.
4. **WASM consume ABDSharedCode via CMake target**: El `wasm/CMakeLists.txt` usa `ABDShared::SynthCore`, no rutas relativas `../../ABDSharedCode/...`.
5. **Tokens compartidos en producción**: El build `vite build` incluye los tokens de ABDSharedAssets y el cascade CSS funciona en WebView2 nativo.
6. **`grep -r "@abdsynths" WebUI/src`** muestra solo los imports soportados por Vite (no imports rotos o hardcodeados).
7. **`build.bat`** compila sin errores con todos los cambios integrados.

---

## 🧬 Fase 10: Convergencia de Ecosistema (ABDSharedCode::LutDSP & VoiceAllocator)

### Tareas:
- [ ] **Modelado de Dispersión Analógica Multivoz**: Integración de `abd::lutdsp::VoiceDispersionModel` para simulación opcional de deriva analógica (drift térmico y varianza de corte de filtros) en las 4 voces del MS2000.
- [ ] **Unificación de Asignador de Voces**: Migración de VoiceManager para heredar o apoyarse en `abd::lutdsp::VoiceAllocator` de ABDSharedCode.
- [ ] **LUTs Oficiales de Filtros Analógicos**: Integración opcional del evaluador SIMD de filtros (`abd::lutdsp::AnalogLutFilterModule`) para emular respuestas no lineales analógicas en modo expandido.

---

## 🧪 Verificación contra hardware real (MS2000 físico) — PENDIENTE

> **Por qué existe esta sección:** todo el soporte SysEx (formato propio `F0 7D 0A` **y** formato del equipo `F0 42 3n 58`) está implementado y cubierto por tests, pero **cuatro cosas solo las puede confirmar un MS2000 delante**. Se apuntan aquí para no darlas por buenas de memoria. Formato: `DOCS/MS2000_SysEx_Spec.md` · `DOCS/ABDSynths_SysEx_Spec.md`.

- [ ] **1. Empaquetado del All Data Dump (`0x4C`)** — el 7→8 de Korg se aplica al **flujo completo** (`N × 254` B concatenados), no a cada programa por separado: de `N` trozos independientes saldría otro tren de bytes (37 248 B frente a los 37 157 B de payload esperados con 128 programas). *Cómo verificarlo:* grabar un All Data Dump real del equipo y comprobar que la trama mide **37 163 B** y que su payload desempaquetado como flujo único da 32 512 B (múltiplo de 254). Si empaquetara por programa, cada trozo mediría 291 B y habría que partir antes de deshacer el 7→8.
  - *Dónde:* `MS2000HardwareProgram::buildAllDataDump` / `allDataDumpPayloadSize`; `SysExManager` (comando `0x4C` y `buildHardwareBankDumpResponse`).
- [ ] **2. ¿El equipo contesta con un acuse (`0x23`/`0x24`) al recibir un `0x40`/`0x4C`?** El plugin ya reconoce esos acuses al recibirlos (Test 24) y el ABD Bank Manager elige esperar la confirmación **solo** en destinos software justo por esto. Si el MS2000 también acusa, se puede pedir confirmación también a hardware real.
  - *Dónde:* `SysExManager::parseSysEx`; `ABDBankManager/WebUI/src/core/pro800Midi.js` (`sendPatch(..., { confirm: true })`).
- [ ] **3. Aceptación de un volcado generado por el software** — que el equipo acepte el `0x4C` que le manda el emulador. La conversión de los 128 presets nativos a programas reales de 254 B **ya está hecha** (`MS2000HardwareProgram::fromNativeProgram` + `buildHardwareBankDumpResponse`, §6.4 de `DOCS/ABDSynths_SysEx_Spec.md`), así que un plugin suelto ya contesta el `0x0E` con su memoria convertida; lo único que falta es lo que no se puede hacer desde aquí: comprobar que el equipo **acepta** ese volcado y qué se pierde por el camino.
  - *Cómo verificarlo:* convertir un preset con parámetros conocidos (cutoff, EG, patch), mandarlo al MS2000 y releerlo del equipo. Esperado: los parámetros que el motor modela vuelven iguales; lo que no modela (`unmodelled()`) vuelve con los valores del **INIT Program** del equipo — no hay nada que recuperar, el bloque nativo del plugin nunca los guardó.
  - *Dónde:* `MS2000HardwareProgram::fromNativeProgram` / `captureModeledFields` / `nativeEngineValue` (un solo camino de escritura: si cambia el mapa del hardware, cambian los dos sentidos a la vez); cubierto por el Test 24.

- [ ] **4. Confirmar con hardware real la convención del canal en el byte `3n`** — la implementación ya está alineada en ambos repositorios: canal 1 → `0x30` y canal 16 → `0x3F`, mediante `korgChannelByte` en C++ y TS. Los fixtures y las pruebas cubren la coherencia software, pero no identifican el canal MIDI con el que fueron grabados.
  - *Estado a 2026-09-15 (decidido y aplicado, sin hardware):* el Bank Manager y el plugin usan la misma convención `0x30 | (canal - 1)`; no queda una discrepancia de implementación pendiente.
  - *Cómo cerrarlo:* con el equipo en canal 1, mandar `F0 42 30 58 10 F7` y `F0 42 31 58 10 F7` y registrar cuál contesta. Si contestara al `0x31`, se cambiaría el helper compartido y sus pruebas; nada más cambia porque el parseo no filtra por canal.
  - *Dónde:* `ABDBankManager/Source/Contracts/Models/korg-ms2000.ts`, `ABDMS2000/Source/MIDI/MS2000HardwareProgram.h`, `Source/Tests/DSPCoreTests.cpp` y `ABDBankManager/WebUI/tests/unit/korgChannelConvention.test.js`.

**Criterio de cierre:** con un MS2000 conectado, un `0x0E` del emulador y uno del equipo producen volcados que el otro extremo acepta sin perder bytes, y queda escrito en `DOCS/MS2000_SysEx_Spec.md` qué era hipótesis y qué está comprobado.
