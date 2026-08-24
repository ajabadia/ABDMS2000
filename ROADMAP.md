# ABDMS2000 — Roadmap Maestro y Validación de Fases

*Este documento es la guía de ejecución incremental del proyecto ABDMS2000. Cada fase contiene sus tareas específicas y sus **Criterios de Aceptación y Verificación (Definition of Done)**. No se avanza a la siguiente fase hasta que la actual esté validada al 100%.*

---

## Estado Global del Proyecto

| Fase | Descripción | Estado |
|:---:|---|:---:|
| **Fase 0** | Infraestructura, Pipeline de Build y Ventana Base | ⏳ PENDIENTE |
| **Fase 1** | Motor DSP Base (Osciladores VA + Filtro + Envolventes + 4 Voces) | ⏳ PENDIENTE |
| **Fase 2** | Telemetría MIDI Bidireccional (CCs en Tiempo Real con Hardware) | ⏳ PENDIENTE |
| **Fase 3** | Codec SysEx (7→8 bit) y Gestor Universal de Bancos | ⏳ PENDIENTE |
| **Fase 4** | WebUI Completa, Pantalla LCD, Skins y Teclado LED | ⏳ PENDIENTE |
| **Fase 5** | DSP Avanzado (DWGS 64 ondas, Virtual Patch, Mod Seq, Efectos) | ⏳ PENDIENTE |
| **Fase 6** | Vocoder de 16 Bandas (Audio In, Formant Shift, HPF Gate) | ⏳ PENDIENTE |
| **Fase 7** | Compilación WebAssembly (WASM) y AudioWorklet | ⏳ PENDIENTE |
| **Fase 8** | Multitimbricidad (Layer/Split) y Modo Avanzado (ABD Ultra) | ⏳ PENDIENTE |
| **Fase 9** | Control de Calidad, Polish y Release (pluginval Level 10) | ⏳ PENDIENTE |

---

## 🧱 Fase 0: Infraestructura, Pipeline de Build y Ventana Base

### Tareas:
- [ ] Crear la estructura de directorios canónica:
  - `Source/Core/`, `Source/DSP/`, `Source/Plugin/`, `Source/State/`, `Source/MIDI/`, `Source/Utils/`, `Source/Tests/`
  - `WebUI/src/`, `WebUI/tests/`, `schemas/`, `Scripts/`, `wasm/`
- [ ] Configurar `CMakeLists.txt` con JUCE 8 (Targets: Standalone y VST3).
- [ ] Implementar `Scripts/registry_generator.js` y `schemas/parameters-spec.schema.v1.json` con los primeros parámetros.
- [ ] Implementar `Scripts/build_webui.js` para empaquetado de assets en `BinaryData`.
- [ ] Crear el script de compilación `build.bat` (Windows MSVC / Ninja).
- [ ] Configurar `PluginEditor_ResourceProvider.cpp` para desarrollo (Hot-Reload desde disco) y release (BinaryData).
- [ ] Configurar ventana WebView2 básica cargando `WebUI/index.html` con la barra superior (`Navbar`) y el selector tri-modo.
- [ ] Configurar entorno de tests unitarios (Vitest para JS y JUCE UnitTest para C++).

### 🔍 Criterio de Verificación (Definition of Done):
1. `build.bat` compila en modo Release sin errores ni advertencias (`0 errors, 0 warnings`).
2. El ejecutable Standalone (`ABDMS2000.exe`) y el plugin VST3 (`ABDMS2000.vst3`) se abren en Windows / DAW mostrando la barra superior y el selector de modos.
3. Los tests básicos de Vitest pasan en verde: `npm test`.

---

## 🔊 Fase 1: Motor DSP Base y Polifonía

### Tareas:
- [ ] Implementar osciladores analógicos virtuales con PolyBLEP:
  - `Source/DSP/Oscillators/VAOscillator.h/.cpp` (Saw, Square/Pulse con PWM, Triangle, Sine).
- [ ] Implementar filtro multimodo básico:
  - `Source/DSP/Filters/MultiModeFilter.h/.cpp` (LPF 24dB, LPF 12dB, BPF 12dB, HPF 12dB).
  - Curva de frecuencia logarítmica (20Hz a 20kHz, centro en ~1kHz).
  - Compensación de pérdida de graves según resonancia analógica.
- [ ] Implementar generador de envolventes ADSR exponencial:
  - `Source/DSP/Envelopes/ADSREnvelope.h/.cpp` (Attack exponencial inverso 1ms-11s, Decay/Release exponencial 2ms-20s).
- [ ] Implementar `Voice.h/.cpp` y `VoiceManager.h/.cpp` (4 voces polifónicas con robo de voz inteligente).
- [ ] Mapear parámetros del APVTS en C++ a los módulos DSP.

### 🔍 Criterio de Verificación (Definition of Done):
1. Tocar notas desde teclado MIDI o teclado QWERTY en Standalone y escuchar un sonido analógico virtual limpio, rico y sin clics.
2. Comprobar que el corte de filtro y la resonancia responden con calidez analógica.
3. Las 4 voces de polifonía se asignan correctamente sin cortes abruptos ni fugas de memoria.

---

## 🎛️ Fase 2: Telemetría MIDI Bidireccional en Tiempo Real

### Tareas:
- [ ] Implementar `Source/MIDI/MIDIProcessor.h/.cpp` (Gestión de Notes, PitchBend, ModWheel y CCs).
- [ ] Implementar `Source/MIDI/MIDIMap.h/.cpp` con la tabla oficial completa de CCs del MS2000 (`DOCS/KORG MS2000 MIDI CCs and NRPNs.html`).
- [ ] Implementar `Source/MIDI/HardwareLink.h/.cpp` para detección y selección de puertos MIDI del MS2000 físico.
- [ ] Implementar envío bidireccional: UI → MIDI Out y MIDI In → UI / APVTS.

### 🔍 Criterio de Verificación (Definition of Done):
1. Al mover un potenciómetro en el MS2000 físico (ej. Cutoff CC#74), el parámetro correspondiente se actualiza inmediatamente en el software.
2. Al mover un control en la interfaz del plugin, el MS2000 físico recibe el CC y actualiza su parámetro.

---

## 💾 Fase 3: Codec SysEx y Gestor Universal de Bancos

### Tareas:
- [ ] Implementar `Source/MIDI/SysExCodec.h/.cpp`:
  - Algoritmo de desempaquetado/empaquetado Korg 7-bit a 8-bit (validado con `DOCS/ReMS2000_Lua_Analysis.md`).
- [ ] Implementar adaptador de modelo Korg (`Source/State/ModelContracts.h/.cpp` y `WebUI/src/contracts/modelContracts.js`).
- [ ] Programar peticiones y recepción de volcados:
  - `Program Data Dump Request (0x10)` y `All Data Dump Request (0x0E)`.
- [ ] Desarrollar el componente visual `<bank-manager>` con búsqueda, filtrado por categorías, favoritos y Drag & Drop.
- [ ] Implementar pantalla del **Inspector SysEx** (`sysexInspector.js`):
  - Visor Hexadecimal en tiempo real con coloreado de secciones.
  - Botón de copiado de texto Hex al portapapeles.
  - Creación de parches pegando cadenas Hex.
  - Editor manual de bytes.

### 🔍 Criterio de Verificación (Definition of Done):
1. Solicitar un *Program Dump* al MS2000 físico y comprobar que los 284 bytes se decodifican y rellenan todos los controles de la UI con exactitud.
2. Exportar un banco a `.syx` e importarlo en el hardware o en el emulador sin pérdida de datos.
3. Pegar una cadena SysEx Hex en el inspector y verificar que carga el sonido inmediatamente.

---

## 🎨 Fase 4: WebUI Completa, Pantalla LCD, Skins y Teclado LED

### Tareas:
- [ ] Ensamblar todos los paneles de control en WebUI:
  - `panelOsc.js`, `panelFilter.js`, `panelAmp.js`, `panelEnvelopes.js`, `panelLFO.js`, `panelModPatch.js`, `panelArp.js`, `panelFX.js`.
- [ ] Implementar la pantalla LCD emulada del hardware:
  - Réplica de matriz de puntos (`REGISTER.TTF`) con menús `1A VOICE NAME`, `2A GLOBAL`, `3A MIDI`, `4D MIDI FILTER`, `VOCODER EDIT`.
- [ ] Implementar el **Side Drawer** (Panel lateral izquierdo) para acceso directo a parámetros avanzados/LCD.
- [ ] Implementar el sistema Multi-Skin con tokens CSS (`themes.css`):
  - **Skin MS2000** (Azul metálico).
  - **Skin microKORG** (Madera y beige).
  - **Skin ABD Ultra** (Dark Neon).
- [ ] Desarrollar el teclado virtual (`keyboard.js`):
  - Ruedas de Pitch Bend y Mod Wheel (assets CZ-101).
  - Indicadores LED de transposición de octavas (Apagado / Parpadeo / Fijo).
  - Barra de luces LED con animaciones de *ABDEep* (Note On, Sweep Wave en patch change, Ripple Pulse en bank change, Flash Cut en Panic).

### 🔍 Criterio de Verificación (Definition of Done):
1. La interfaz se renderiza nítida y fluida a 60 FPS.
2. La conmutación de skins se produce en caliente y adapta todos los colores y texturas.
3. El Drawer lateral se abre/cierra de forma suave mostrando los parámetros avanzados contextuales.
4. Las luces LED del teclado ejecutan sus animaciones correctamente.

---

## ⚡ Fase 5: DSP Avanzado y Modulaciones

### Tareas:
- [ ] Implementar oscilador DWGS (`DWGSOscillator.h/.cpp`):
  - Cargar las 64 ondas digitales de ciclo único (2048 muestras cada una) en un buffer contiguo (`131,072` floats) con latencia cero.
- [ ] Implementar modulaciones complejas de OSC2:
  - Ring Modulation, Hard Sync y Cross Modulation (FM).
- [ ] Implementar la Matriz de Modulación Virtual (`VirtualPatch.h/.cpp`):
  - 4 ranuras libres con escalas calibradas (±2 octavas en Pitch, ±5 octavas en Cutoff, 100% en PWM/VCA/Pan).
- [ ] Implementar los LFOs (`LFO.h/.cpp`):
  - Curva logarítmica de 0.01Hz a 20Hz (exponente 1.25) y subdivisiones rítmicas con `Tempo Sync`.
- [ ] Implementar la etapa de **Distorsión Asimétrica** (`FXDistortion.h/.cpp`):
  - Saturación no lineal dependiente del nivel de entrada del Mixer colocada después del filtro.
- [ ] Implementar la sección de Efectos:
  - EQ paramétrico de 2 bandas (`FXEqualizer.h/.cpp`).
  - Mod FX: Chorus, Ensemble (String Machine 3 fases), Phaser (`FXModulation.h/.cpp`).
  - Delay: Stereo, Cross Delay Ping-Pong, L/R Delay (`FXDelay.h/.cpp`).
- [ ] Implementar el Secuenciador de Modulación (`ModSequence.h/.cpp`):
  - 3 pistas × 16 pasos con modos Step y Smooth (Slew Limiter de 10ms).
- [ ] Implementar el Arpegiador por pasos (`Arpeggiator.h/.cpp` con inyección en `juce::MidiBuffer`).

### 🔍 Criterio de Verificación (Definition of Done):
1. Los patches de fábrica con modulaciones complejas, secuencias de movimiento y efectos suenan idénticos al hardware real en comparación A/B directa.
2. La distorsión asimétrica reacciona al volumen del mixer generando armónicos pares analógicos.

---

## 🎙️ Fase 6: Vocoder de 16 Bandas

### Tareas:
- [ ] Implementar `VocoderEngine.h/.cpp`:
  - Banco de 16 filtros paso-banda de análisis (Audio In) y síntesis (Osciladores).
- [ ] Implementar detectores de envolvente (`EnvelopeFollower.h/.cpp`) por banda con ganancia y panorama individual.
- [ ] Implementar **Formant Shift** (+2 a -2) para desplazamiento de frecuencias.
- [ ] Implementar circuito **Vocal Distortion / HPF Gate** con inyección de ruido blanco para consonantes inteligibles (S, P, T).

### 🔍 Criterio de Verificación (Definition of Done):
1. Inyectar voz por la entrada de micrófono del DAW y tocar un acorde: las palabras son perfectamente inteligibles y claras.
2. El Formant Shift modifica el timbre de voz (masculina ↔ femenina) de forma suave.

---

## 🌐 Fase 7: Compilación WebAssembly (WASM) y AudioWorklet

### Tareas:
- [ ] Crear `wasm/CMakeLists.txt` y `wasm/build_wasm.bat` con Emscripten.
- [ ] Integrar `wasm/juce_shim/wasm_link_stubs.cpp` y `wasm_compat.h` para aislar el core DSP de dependencias de escritorio.
- [ ] Exportar binario con `-s SINGLE_FILE=1 -s MALLOC=emmalloc`.
- [ ] Implementar `ms2000Worklet.js` con vistas cacheadas zero-copy de `HEAPF32.subarray()`.
- [ ] Adaptar el Gestor de Bancos a `IndexedDB` en navegador.
- [ ] Configurar Progressive Web App (PWA) con `manifest.json`.

### 🔍 Criterio de Verificación (Definition of Done):
1. `build_wasm.bat` genera `ms2000_dsp.js` (< 2MB).
2. La versión Web se ejecuta en Chrome/Edge sin caídas de audio, con baja latencia y con la misma fidelidad sonora que el binario nativo.

---

## 🚀 Fase 8: Multitimbricidad y Modo Avanzado (ABD Ultra)

### Tareas:
- [ ] Implementar soporte multitímbrico de 2 capas:
  - Modo Single (4 voces).
  - Modo Layer (2 timbres superpuestos, 2 voces cada uno).
  - Modo Split (división por punto de teclado).
- [ ] Activar el **Modo Avanzado (ABD Ultra Engine)**:
  - Polifonía expandible a 8, 16 o 32 voces.
  - Reverb Shimmer / Hall / Room de alta fidelidad.
  - Tape Delay analógico con saturación y flutter.
  - 4 LFOs sincronizables y 4 envolventes DAHDSR.
  - Secuenciador polifónico extendido de 64 pasos con *Parameter Locks*.

### 🔍 Criterio de Verificación (Definition of Done):
1. Conmutar en tiempo real entre Modo MS2000 clásico y Modo Avanzado sin cortes ni reinicios del motor de audio.
2. Los efectos modernos expandidos suenan con calidad de estudio sin sobrecargar la CPU.

---

## 🏆 Fase 9: Control de Calidad, Polish y Release

### Tareas:
- [ ] Ejecutar suite completa de tests de rigor con `pluginval`:
  - `pluginval --strictness-level 10 --validate "ABDMS2000.vst3"`.
- [ ] Implementar sistema de **MIDI Learn Universal** con persistencia en JSON.
- [ ] Implementar **Smart Randomizer** con candados (*Lock Pitch, Lock Envs, Lock FX*).
- [ ] Implementar **Compare Buffer A/B** y botón de **Pánico MIDI [ ! ]**.
- [ ] Incorporar banco oficial completo de fábrica (MS2000 y microKORG).
- [ ] Configurar build para macOS (WKWebView / Universal Binary).
- [ ] Generar documentación de usuario final y empaquetado de distribución.

### 🔍 Criterio de Verificación (Definition of Done):
1. `pluginval` pasa al 100% en Nivel 10 sin un solo fallo.
2. Cero fugas de memoria al instanciar y destruir múltiples copias del plugin en DAWs principales (Ableton Live, Reaper, Cubase, Logic Pro).
3. Transiciones de presets completamente silenciosas y musicales (crossfade de 5ms).

---

*Documento creado: 2026-08-24*  
*Proyecto: ABDMS2000 by ajabadia*
