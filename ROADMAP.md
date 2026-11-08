# ABDMS2000 — Hoja de Ruta y Plan de Evolución

## 🛡️ Plan de Modernización y Cumplimiento Normativo 2026 (Auditoría 2026-11)
> **Estado:** 🟡 En Progreso  
> **Objetivo:** Alinear ABDMS2000 con los estándares consolidados de 2026 (ADR-001 a ADR-005, juce-audio-hybrid-plugin y SECURITY_STANDARDS).

### 📋 Fases del Plan de Adaptación

| Fase | Tarea / Subsistema | Estado | Descripción Técnica y Criterio de Aceptación |
| :--- | :--- | :---: | :--- |
| **MS2K.1** | **Aislamiento UDF por PID en WebView2** | ✅ Completado | Incorporado Process ID (::GetCurrentProcessId()) y soporte de WEBVIEW2_USER_DATA_FOLDER en getAndPrepareWebView2UserDataFolder(). Permite instancias múltiples simultáneas sin interferencia en DAWs multipista. |
| **MS2K.2** | **Sanitización en Frontera Nativa IPC** | ✅ Completado | Blindado BridgeActions::handleJsEvent con validaciones std::isfinite y acotación segura en setParam, setTempoBPM y notas MIDI entrantes. Protege al APVTS y motor DSP contra inyecciones maliciosas de NaN/Infinity. |
| **MS2K.3** | **Reducción de Vocoder a 8 Bandas (microKORG)** | ✅ Completado | Añadido set8BandMode() a MS2KVocoder16Band con asignación alternada de bandas e incremento de ganancia compensatoria (+6 dB). Cableado dinámicamente según synthMode == 1 tanto en C++ nativo (SynthEngine.cpp) como en WebAssembly (WasmBridge.cpp). Verificado en DSPCoreTests (591/591 tests pasando). |
| **MS2K.4** | **Canal de Telemetría Optimizado (Scope Taps)** | ✅ Conforme ADR-003 P8 | Auditado según ADR-003 §2.5: el pipeline opera 100% lock-free en el hilo de audio vía ScopeTap SPSC y colecta tramas solo con panel activo; la migración a binario crudo queda sujeta a métricas de profiling para evitar sobre-ingeniería especulativa. |

---

## 2026-10-11 — Arpegiador microKORG: Máscara Rítmica de 8 Pasos y Motor Extendido

Implementación completa de la característica distintiva del hardware **microKORG** (máscara rítmica de 8 pasos del arpegiador) y su extensión al perfil **ADVANCED**:
1. **Contrato de Parámetros**: 9 nuevos parámetros registrados (`arpLastStep` de 1 a 8, `arpStep1` a `arpStep8` booleanos) con `"sysex": false`, blindando la memoria de 384 bytes de `MS2000ProgramData` y preservando intactos los 253 offsets SysEx del MS2000.
2. **Motor C++ y WASM**:
   - `Arpeggiator.h/.cpp`: estado `stepMask_[8]`, `lastStep_`, `currentRhythmStep_` y `stepMaskEnabled_`. Los pasos marcados como silencio (rest) avanzan el reloj rítmico pero no disparan notas de voz.
   - `SynthEngine.cpp` y `WasmBridge.cpp`: activación automática cuando `synthMode != 0` (microKORG o Advanced).
3. **WebUI Interactiva**:
   - Drawer de Arpegiador (`renderArpDrawer`): sección *Step Rhythm Mask* con LEDs luminosos, estados PLAY/REST, longitud dinámica (`arpLastStep`), botones de preset rápido (`ALL ON`, `ODD`, `EVEN`, `INVERT`) y detección contextual de perfil.
   - Sincronización en tiempo real con la tarjeta del dashboard (`updateArpCard`) reflejando `[activeSteps/lastStep Steps]`.
4. **Batería de Pruebas**:
   - Vitest: **203/203 tests en verde** (19 suites).
   - Pruebas C++ nativas (`ABDMS2000_Tests`): **361/361 tests en verde** (0 fallos).
   - Verificación de registro (`check:registry`): **3/3 artefactos al día**.

---

## 2026-09-27 — el contrato de foco/inert de los overlays (overlayFocus compartido)

El slideDrawer y los modales (About, Diagnostico, BankManager) entran en el MISMO
contrato de foco que el cajon compartido `createDrawer` de la familia
(ABDSharedAssets/components/overlayFocus.js + su test): cerrados van `inert` +
`aria-hidden` (el contenido sigue en el DOM, fuera del orden de tabulacion), al abrir
el foco entra en el primer control del cuerpo, Tab no se escapa con el overlay
abierto y al cerrar vuelve al disparador. En el slideDrawer, cuyo contenido se
reconstruye por apertura, el `inert` de cierre evita el modo clasico: los controles
del render ANTERIOR quedaban tabulables dentro de un dialogo aria-modal oculto. El
Escape del About/Diagnostico/BankManager queda centralizado (el del host delega en
los cierres del contrato; doble cierre = no-op). Suite: vitest **134/134**.

---

## 🧮 Phase DSP: Abstracción de Esquema - Validación y Terminado

> **Objetivo**: Completar el 1% restante, validar fórmulas DSP y asegurar calidad del código schema-driven.  
> **Estado Inicial**: 🟡 99% Schema completado, 1% edge case por definir, tests básicos creados  
> **Fecha Inicio**: %%DATE%%  
> **Relacionado con Fases**: Phase 2 (Arquitectura Completa DSP) - validación final

### 📋 Tareas Pendientes y Cerrados

| # | Tarea | Estado | Hecho | Notas |
|---|-------|--------|-------|-------|
| DSP.1 | ✅ Completer 3 parámetros edge case sin `wire` en schema JSON | ✅ Done | `t2Seq3Step1` y 2 parámetros más | Added `wire`, `wireCodec`, `curve`, `domainMin/Max` a `parameters-spec.schema.v1.json` |
| DSP.2 | ✅ Verificar que envelopas ya tienen campos schema completos | ✅ Done | `env1Attack`, `env1Release`, `env2Attack`, `env2Decay` | Ya tenían `wire`, `curve`, `domainMin/Max` definidos en schema |
| DSP.3 | ✅ Crear y ejecutar tests básicos de fórmulas DSP | ✅ Done | `tests/test_conversion.py` con 4 test suites | 45 evaluaciones de fórmulas Lineal/Exp/Pow todas pasadas |
| DSP.4 | ✅ Mejorar `Conversion.gen.h` - seguridad log inverse | ✅ Done | Añadida validación `ratio` antes de `std::log()` | Previene `nan` en conversión Exp inverse |
| DSP.5 | ✅ Añadir validaciones en `registry_core.js` | ✅ Done | Validación de curve (0-3), wire, wireCodec, domainMin<=domainMax | 4 nuevas validaciones en el generador |
| DSP.6 | ✅ Migrar parámetros Voice.cpp a `normalizedToDomain()` | ✅ Done | Migración de `filterCutoff` (curva Exp analógica $632Hz$ en centro) y `filterResonance` (curva Linear) a `conversion::normalizedToDomain()` con guardas de extremos exactos | Parámetros críticos del filtro migrados y testeados (393/393 C++) |
| DSP.7 | ✅ Clamping domainMin/domainMax en conversiones | ✅ Done | `Conversion.gen.h`: entradas clampadas a [0, 1] y salidas clampadas a [domainMin, domainMax] | Previene overflow y valores fuera de dominio |
| DSP.8 | ✅ Estandarizar tipos de registro a canónicos | ✅ Done | `registry_core.js`: wire, wireCodec, curve y smoothing normalizados | Generador valida 3/3 artefactos en verde |
| DSP.9 | ✅ Tests de inversas y round-trips | ✅ Done | `test_conversion.py` y `DSPCoreTests.cpp`: round-trips Linear/Exp/Pow/Lut | Tolerancia de precisión verificada |
| DSP.10 | ✅ Tests de valores extremos en todos los parámetros | ✅ Done | 292 parámetros probados en 7 extremos (-1.0, 0.0, 0.5, 1.0, 2.0, NaN, Inf) | 2.044 evaluaciones sin NaN ni Inf en salida |

### 📊 Progreso por Fase DSP

| Categoría | Total | Completado | Pendiente | % |
|-----------|-------|------------|-----------|---|
| Schema JSON parameters | 292 | 292 (100%) | 0 (0%) | ✅ |
| Funciones de conversión validadas | 4 (Linear/Exp/Pow/Lut) | 4 (100%) | 0 (0%) | ✅ |
| Tests de fórmula DSP | 6 test suites | 6 (100%) | 0 (0%) | ✅ |
| Clamping de dominio y normalizado | 292 params | 292 (100%) | 0 (0%) | ✅ |
| Validaciones registry_generator | 6 | 6 (100%) | 0 (0%) | ✅ |

### 📅 Roadmap de Iteraciones DSP

**Iteración 1** (Completada): 
- [x] DSP.1 - Completar parámetros edge case en schema JSON
- [x] DSP.2 - Verificar campos schema de envelopas
- [x] DSP.3 - Crear tests básicos de fórmulas DSP
- [x] DSP.4 - Mejorar seguridad log inverse en Conversion.gen.h
- [x] DSP.5 - Añadir validaciones registry_core.js
- [x] DSP.6 - Migrar parámetros Voice.cpp a `normalizedToDomain()`

**Iteración 2** (Completada):
- [x] DSP.7 - Añadir clamping domainMin/domainMax a conversiones
- [x] DSP.8 - Standardizar tipos wire/wireCodec/curve a strings
- [x] DSP.9 - Expandir tests: Lut curve, inverse round-trips
- [x] DSP.10 - Testear los 292 parámetros en valores extremos (2.044 evaluaciones)

**Iteración 3** (Completada):
- [x] DSP.11 - Extender metadatos de dominio canónicos a EQ y ModFX (`eqLowFreq`, `eqLowGain`, `eqHighFreq`, `eqHighGain`, `modFxSpeed`, `modFxDepth`, `modFxFeedback`)
- [x] DSP.12 - Migrar parámetros de EQ (`eqLowGain`, `eqHighGain`) y ModFX (`modFxDepth`) en `SynthEngine.cpp` a `conversion::normalizedToDomain()`
- [x] DSP.13 - Tests de paridad para `modFxSpeed` (Exp $[0.02\text{ Hz} \dots 15.0\text{ Hz}]$) y `eqLowGain` ($\pm 12\text{ dB}$) en `DSPCoreTests.cpp` (395 tests C++)

**Iteración 4** (Completada):
- [x] DSP.14 - Metadatos canónicos de dominio para bloques Mixer y Amp (`mixOsc1Level`, `mixOsc2Level`, `mixNoiseLevel`, `ampLevel`, `ampPan`, y espejos de Timbre 2 `t2*` con `wire: None`)
- [x] DSP.15 - Migración en `SynthEngine.cpp` de niveles de mezclador (`osc1Level`, `osc2Level`, `noiseLevel`) y `ampLevel` a `conversion::normalizedToDomain()`
- [x] DSP.16 - Tests de paridad para Mixer (Linear $[0.0 \dots 1.0]$), Amp Level ($[0.0 \dots 1.0]$) y Amp Pan (Bipolar Linear $[-1.0 \dots +1.0]$) en `DSPCoreTests.cpp` (Case 29)

**Iteración 5** (Completada):
- [x] DSP.17 - Metadatos canónicos de dominio para bloques de Envolvente EG1 y EG2 (`eg1Attack`, `eg1Decay`, `eg1Sustain`, `eg1Release`, `eg2Attack`, `eg2Decay`, `eg2Sustain`, `eg2Release`, y espejos de Timbre 2 `t2*` con `wire: None`)
- [x] DSP.18 - Migración en `SynthEngine.cpp` de todos los parámetros de envolvente (`vp.eg1*` y `vp.eg2*`) a `conversion::normalizedToDomain()`
- [x] DSP.19 - Tests de paridad para EG1 y EG2 (Linear $[0.0 \dots 1.0]$) en `DSPCoreTests.cpp` (Case 29)

**Iteración 6** (Completada):
- [x] DSP.20 - Metadatos canónicos de dominio para Controles OSC1 y Portamento (`portamentoTime`, `osc1Ctrl1`, `osc1Ctrl2`, y Timbre 2 `t2*` con Linear $[0.0 \dots 1.0]$)
- [x] DSP.21 - Metadatos canónicos de dominio para Modulaciones y Sensibilidades Bipolares (`filterEg1Int`, `filterKeyTrack`, `filterVelo`, `ampKeyTrack`, `ampVelo`, y Timbre 2 `t2*` con Bipolar Linear $[-1.0 \dots +1.0]$)
- [x] DSP.22 - Migración en `SynthEngine.cpp` y `Voice.cpp` (`glide_.setGlideTime`) a llamadas canónicas `conversion::normalizedToDomain()`
- [x] DSP.23 - Tests de paridad en `DSPCoreTests.cpp` (Case 29) para Portamento (Linear $0..1$), OSC1 Ctrl1 (Linear $0..1$), Filter EG1 Int (Bipolar $-1..+1$), Filter Key Track (Bipolar $-1..+1$) y Amp Velo (Bipolar $-1..+1$)

**Iteración 7** (Completada):
- [x] DSP.24 - Metadatos canónicos de dominio para Intensidades de Virtual Patch (`patch1Intensity` a `patch4Intensity`, y Timbre 2 `t2Patch*Intensity` con Bipolar Linear $[-1.0 \dots +1.0]$)
- [x] DSP.25 - Metadatos canónicos de dominio para Delay FX (`delayDepth`, `delayFeedback` con Linear $[0.0 \dots 1.0]$) y Arpeggiator Gate (`arpGate` con Linear $[0.0 \dots 1.0]$)
- [x] DSP.26 - Migración en `SynthEngine.cpp` de `patchSlots[s].intensity`, `delayDepth`, `delayFeedback` y `arpGate` a `conversion::normalizedToDomain()`
- [x] DSP.27 - Tests de paridad en `DSPCoreTests.cpp` (Case 29, sección 2h) para `patch1Intensity` (Bipolar $-1..+1$), `delayDepth` ($0..1$), `delayFeedback` ($0..1$) y `arpGate` ($0..1$)

---

## 📌 Tareas Futuras y Validación Cruzada (Backlog)

- [x] **Opción C: Consolidar en otro sintetizador (ABDEep):** Completada la auditoría y alineación de ABDEep:
  - Implementado validador SSOT con gate `--check` (`npm run check:registry`, 43 tests pasando sin escrituras espurias).
  - Verificado que ABDEep ya consume las primitivas canónicas compartidas (`abd::dsp::JunoVCF_ZDF` / `JunoHPFContinuous`, `abd::synth::VoiceAllocator`, `Arpeggiator`, `LfoAnalog`, `EnvelopeAnalog`, `DriftEngine`).
  - Establecido el límite estricto de identidad: no se altera la presentación visual de la WebUI ni se introducen filtros ajenos a los 12 canales DCO/VCF del DeepMind 12.
- [x] **Revisión Synth Profile & Paridad microKORG vs MS2000:**
  - [x] **Sincronización Bidireccional Tema <-> Perfil:** Vinculado el `ThemeSwitcher` del navbar con `synthMode` del store y el bridge de forma bidireccional (cambiar tema visual conmuta el perfil del motor y viceversa, verificado con `synthProfileSync.test.js`).
  - [x] **Impacto Real en DSP / WASM:** `WasmBridge.cpp` ahora soporta y despacha `synthMode` (4 voces vs 32 expandidas), `voiceMode`, `unisonDetune` y `unisonSpread` a `VoiceManager`.
  - [x] **Vocoder (8 canales microKORG vs 16 bandas MS2000):** Actualización dinámica de card `#card-vocoder` (`8-CHANNEL VOCODER` / `8 CHANNELS` / `ON (8 Channels Active)` vs `16-BAND VOCODER` / `16 BANDS` / `ON (16 Bands Active)`) y consola gráfica del drawer de vocoder adaptada dinámicamente según el perfil activo (muestra 8 faders duales `CH 1`..`CH 8` sincronizando pares de bandas [1,2]..[15,16] en microKORG, y 16 bandas discretas `B1`..`B16` en MS2000/Advanced, con refresco reactivo al cambiar de perfil).
  - [x] **Desactivación del Mod Sequencer en microKORG:** En perfil microKORG, el Mod Sequencer queda completamente inhibido en DSP nativo (`SynthEngine.cpp`) y WASM (`WasmBridge.cpp`). En WebUI, la tarjeta muestra badge `ARPEGGIATOR`, el botón `MOD SEQ` queda atenuado como no disponible, el resumen indica `N/A (microKORG)` y el drawer muestra panel informativo con conmutador directo a MS2000.
  - [x] **Matriz de Modulación Virtual:**
    - Modo Estándar (Hardware MS2000 / microKORG): **4 slots** físicos (`Patch 1`..`Patch 4`) confirmados en WebUI y hardware.
    - Modo Extendido (Advanced): Diseñado para **8 slots** (`kNumVirtualPatchSlotsAdvanced = 8` en `HardwareConstants.h`).
    - Suite de pruebas de ABDMS2000: **201/201 tests pasando** en 19 suites de Vitest.
  - **Matriz de Diferencias MS2000 vs microKORG:**
    1. *Mod Sequencer:* Exclusivo de MS2000 (3 pistas x 16 pasos); microKORG carece de él (inhibido en perfil microKORG).
    2. *Step Arpeggiator:* microKORG incorpora máscara rítmica de 8 pasos on/off; MS2000 opera por tipos/resolución estándar.
    3. *Estructura de Bancos/SysEx:* microKORG organiza 128 programas en matriz de géneros (A/B x 8) vs MS2000 (Bancos A-H x 16).
    4. *Control Físico:* microKORG usa matriz matricial de 5 encoders; MS2000 es knob-per-function.

---

## 🔀 Bifurcación SysEx & Paridad Hardware: microKORG vs MS2000 (Roadmap de Implementación)

> **Objetivo**: Separar la serialización, volcados SysEx (`.syx`), contratos de datos y comandos MIDI entre el Korg MS2000 y el microKORG para alcanzar paridad 1:1 con el hardware físico original de cada máquina.

### 📋 Diagnóstico de Arquitectura Actual
- Actualmente `ABDMS2000` y `ABDBankManager` tratan el volcado de programa como un único contrato compartido (254 bytes desempaquetados / 297 bytes en cable bajo Model ID `0x58`).
- En el hardware real:
  - **MS2000**: Su volcado de 254 bytes contiene 55 bytes dedicados al Modulation Sequencer (3 pistas x 16 pasos) y Vocoder de 16 bandas discretas. No tiene máscara de arpegio.
  - **microKORG**: Su volcado físico incluye el byte `0x25` (Last Step: 1..8) y el byte de bitmask de 8 pasos (bits 0..7 para Step 1..8 on/off), prescinde del Mod Sequencer y acopla el vocoder a 8 canales. Además, el hardware expone el control de `Last Step` mediante el mensaje oficial **NRPN 0, 9**.

### 🗓️ Fases de Ejecución

#### 🔹 Fase 1: Mapeo y Detección de Dump microKORG en C++ (`MicroKorgHardwareProgram`) - [COMPLETADA ✅]
- **Archivos**: `Source/MIDI/MicroKorgHardwareProgram.h`, `Source/MIDI/SysExManager.h`, `Source/MIDI/SysExManager.cpp`.
- **Implementación**:
  - Creada la clase canónica `MicroKorgHardwareProgram` con Model ID `0x71` (`0x58` legacy).
  - Byte `0x25` mapeado a `arpLastStep` (longitud 1..8).
  - Byte `0x0C` mapeado a la máscara de bits rítmica de 8 pasos (`arpStep1`..`arpStep8`).
  - Sincronización bidireccional con APVTS (`captureFromAPVTS`, `applyToAPVTS`), neutralizando `modSeqOn` a 0 por fidelidad hardware.
  - `SysExManager.cpp`: discriminación en `parseSysEx` por Model ID `0x71` o modo activo (`synthMode_ == 1`).
  - Implementado `createMicrokorgProgramDump` y respuesta a peticiones Korg Dump Request `0x10`.
  - Añadido `[Test 28] microKORG Hardware Parity` en `DSPCoreTests.cpp` (15 nuevas aserciones verdes, total suite: 376 tests sin fallos).

#### 🔹 Fase 2: Soporte Oficial NRPN microKORG en `MIDIMap.cpp` - [COMPLETADA ✅]
- **Archivos**: `Source/MIDI/MIDIMap.cpp`.
- **Implementación**:
  - Incorporado el mapeo canónico para `NRPN 0, 9` (CC 99 = 0, CC 98 = 9, CC 6 = 1..8) asignado a `ParamIDs::arpLastStep`.
  - Sincronización automática a través de la infraestructura compartida con el arpegiador del motor y la WebUI.

#### 🔹 Fase 3: Bifurcación en `ABDBankManager` (`modelContracts.gen.js`) - [COMPLETADA ✅]
- **Archivos**: `ABDBankManager/WebUI/src/contracts/gen/modelContracts.gen.js`, `ABDBankManager/WebUI/src/contracts/gen/contractRegistry.gen.js`, `ABDSharedCode/BankManager/Contracts/Models/korg-ms2000.ts`, `ABDMS2000/Source/Core/HardwareConstants.h`.
- **Implementación**:
  - Especializado el contrato canónico `korg-microkorg` con Model ID `0x71` nativo y compatibilidad tolerante para volcados de fábrica `0x58`.
  - Regenerados los artefactos web `modelContracts.gen.js` (98.0 KB) y `contractRegistry.gen.js` (23.9 KB).
  - Detección unívoca en `sysexParser.js`: volcados con byte `0x71` detectan unívocamente `korg-microkorg`, mientras `0x58` detecta `korg-ms2000`.
  - Conversión e importación tolerante: microKORG admite tanto volcados nativos `0x71` como volcados de MS2000 (`0x58`), neutralizando el Mod Sequencer en el motor.
  - Sincronizado `WebUI/abdbank/` con el host `ABDMS2000` mediante `sync_bankmanager.js`.
  - Corregido `kMicroKorgModelId = 0x71` en `HardwareConstants.h`.
  - Batería de pruebas: **1.170/1.170 tests unitarios en verde** en `ABDBankManager`.
  - Congelado con commits de consolidación: `ABDBankManager` (`31cd46f`), `ABDSharedCode` (`d990e38`) y `ABDMS2000` (`6f32ae808`).

---

## 🛠️ 2026-10-19 — Estrategia y Plan de Calidad de Código (Auditoría Integral C++ / DSP / Wasm / WebUI)

> **Objetivo**: Corregir los 57 hallazgos detectados en la auditoría de calidad de código maximizando el impacto real en estabilidad (Audio Real-Time, concurrencia, fidelidad sonora y fluidez UI) con el **mínimo riesgo de regresión**, ordenando el trabajo de lo más práctico e inmediato a las mejoras estructurales.

### 🧭 Decisiones Estratégicas (Criterio Práctico)

1. **Priorizar "Quick Wins" funcionales antes de refactorizaciones profundas (Sprint 0)**:
   - Hay bugs audibles y controles desconectados que se corrigen en 1–5 líneas sin riesgo arquitectónico (ej. `mod.osc2PitchMod` ignorado en `Voice.cpp`, modulaciones del Mod Sequencer que quedan pegadas en `vp.seq`, `filterVelSens` vs `filterVelo`, `ampPan: 64` forzando panorama a `R64`, `!bridge.isNative` vs `!bridge.isWebView`, `</div>` extra en `renderOsc1Drawer`). Abordarlos primero limpia el terreno y entrega valor inmediato.
2. **Estrategia en dos pasos para `WasmBridge.cpp` vs `SynthEngine.cpp`**:
   - *Descartado a corto plazo*: Reescribir `SynthEngine` completo para desacoplarlo de JUCE antes de arreglar `WasmBridge` (alto riesgo de romper los 376 tests C++ actuales).
   - *Enfoque práctico*:
     1. **Corto plazo (Sprint 0 y Sprint 3)**: Reparar la compilación de `WasmBridge.cpp` (`PatchDestination` / `.destination`), el divisor erróneo `/ 127.0f` en LFO1/LFO2 y el despacho de `setTempoBPM` / `Conversion.gen.h`.
     2. **Sinergia con RT-Safety (Sprint 1)**: Al sustituir las búsquedas por string del `APVTS` en `SynthEngine` por una tabla pre-cacheada de punteros/valores indexados por enum, `SynthEngine` quedará prácticamente desacoplado de `APVTS` sin esfuerzo extra.
3. **Respeto estricto a la Fuente Única de Verdad en `BankManager` (`AGENTS.md`)**:
   - No mantener forks divergentes en `WebUI/abdbank/`.
   - Separar claramente las correcciones del contenedor modal del synth (`WebUI/src/components/bank/BankManagerModal.js` y `mountBankManager.js`) de las correcciones del contenido bancario (`WebUI/abdbank/`), aplicando un *guard* de montaje e inhibición de atajos cuando el modal está oculto (`overlay.hidden === true`) y registrando el cambio para portarlo al repositorio canónico (`ABDBankManager` / `ABDShared`).

---

### 🗓️ Plan de Ejecución por Sprints

#### 🚀 Sprint 0: Quick Wins de Alto Impacto y Contratos Rotos (Bajo Riesgo / Efecto Inmediato)
*Correcciones puntuales (1–10 líneas por hallazgo) que arreglan bugs sonoros, errores de compilación Wasm y controles rotos en la UI.*

- [x] **S0.1 — Fidelidad DSP en Matriz y Mod Sequencer**:
  - Sumar `mod.osc2PitchMod` en el cálculo de tono de OSC2 en `Source/Core/Voice.cpp` (`renderNextSample`).
  - Resetear `vp.seq = {}` al inicio de `SynthEngine::readTimbreParams()` y `applySeqModulation()` (`Source/Core/SynthEngine.cpp`) para evitar que las modulaciones del secuenciador queden "congeladas" al cambiar el destino de una pista o apagar `modSeqOn`.
- [x] **S0.2 — Desbloqueo de compilación y LFOs en `WasmBridge.cpp`**:
  - Actualizar `PatchDest` → `PatchDestination` y `.dest` → `.destination` en `Source/Wasm/WasmBridge.cpp` (líneas 251–269).
  - Pasar `value` (`0..127`) sin dividir por `127.0f` a `FirmwareTables::getHardwareLfo1FreqHz` y `getHardwareLfo2FreqHz` en `Source/Wasm/WasmBridge.cpp` (líneas 238, 247).
- [x] **S0.3 — Alineación de IDs y rangos en WebUI con `registry.gen.js`**:
  - Corregir `paramId: 'filterVelSens'` → `'filterVelo'` en `WebUI/src/app.js` (líneas 682–686).
  - Corregir `ampPan: 64` → `ampPan: 0` y `voiceMode: 2` → `1` (Poly) en `WebUI/src/contracts/factoryPresets.js`.
  - Alinear `voiceMode` en `WebUI/src/ui/lcdProgrammer.js` a los 3 estados canónicos (`0 = Mono`, `1 = Poly`, `2 = Unison`) e implementar `getCurrentProgram()`.
- [x] **S0.4 — Correcciones puntuales de UI y Bridge**:
  - Cambiar `if (!bridge.isNative)` → `if (!bridge.isWebView)` en `WebUI/src/app.js` (línea 2259) para evitar la doble randomización simultánea C++ + JS en WebView2.
  - Eliminar el `</div>` prematuro en `renderOsc1Drawer` (`WebUI/src/app.js`, línea 285) que rompía el ocultado de la sección DWGS.
  - Implementar `setTempoBPM(bpm)` en `WebUI/src/bridge/bridgeWasm.js` y conectar el knob de tempo en `WebUI/src/app.js` / `BridgeActions.cpp` / `SynthEngine.h`.
  - Conectar el handler de subida WAV en `#wt-drop-zone` (`WebUI/src/ui/wavetableBrowser.js`, línea 404), añadir el `slot: 63` faltante en `BOOTSTRAP_CATALOG` y unificar la numeración del footer.

---

#### ⚡ Sprint 1: Seguridad Real-Time (Audio Thread) y Concurrencia C++
*Eliminación de xruns/dropouts, alojamientos dinámicos por bloque y condiciones de carrera entre el hilo de UI y el hilo de Audio.*

- [x] **S1.1 — Erradicar `ABD_LOG` y bloqueos en el Audio Thread**:
  - Retiradas las llamadas a `ABD_LOG` dentro de `SynthEngine::processBlock()`, `noteOn()` y `noteOff()` (`Source/Core/SynthEngine.cpp`), eliminando el bloqueo de `std::mutex`, `std::localtime`, `std::ostringstream` y `std::cout.flush()` en tiempo real.
- [x] **S1.2 — Cero asignaciones de memoria (`juce::String` / `std::string`) en `processBlock`**:
  - Pre-cacheados todos los punteros `std::atomic<float>*` de `apvts_.getRawParameterValue(...)` (Timbre 1, Timbre 2 y pasos `seq1..3Step1..16`) durante el constructor de `SynthEngine` (`paramCache_`, `timbre2ParamCache_`, `seqParamPtrs_`).
  - Actualizado `conversion::detail::getMetaById` en `Source/State/Conversion.gen.h` para usar `std::string_view` sin construir `std::string` en el hilo de audio.
- [x] **S1.3 — Protección de buffers y pre-reserva de vectores RT**:
  - Acotada la escritura en `scopeBuf*_[s]` en `SynthEngine::processBlock()` cuando `buffer.getNumSamples() > samplesPerBlock_`.
  - Añadido `reserve()` en `VoiceManager::prepare()` (`heldNotesStack_.reserve(64)`) y `Arpeggiator::prepare()` (`heldNotes_.reserve(64)`, `generatedPattern_.reserve(256)`).
- [x] **S1.4 — Cola MIDI Lock-Free UI → Audio y Seguridad de Hilos**:
  - Añadida cola SPSC lock-free (`uiMidiQueue_` / `enqueueNoteOn` / `enqueueNoteOff` / `enqueueAllNotesOff`) en `SynthEngine` y enrutado `BridgeActions.cpp` (hilo UI) a través de ella, drenándola al inicio de `SynthEngine::processBlock()`.
  - Sincronizado el acceso multi-productor a `MIDITelemetryManager::outgoingQueue_` y `activityCallback_` mediante spinlock atómico (`std::atomic_flag`).
  - Sustituidas las capturas `[this]` crudas en `callAsync` (`PluginEditor.cpp`, `BridgeActions.cpp`) por `juce::Component::SafePointer` y `std::weak_ptr<bool>` (`aliveToken_`).

---

#### 🧩 Sprint 2: Ciclo de Vida WebUI, Fugas de Memoria y Aislamiento de `BankManagerModal`
*Eliminación de fugas en `ParamStore`, reducción a la mitad del tráfico IPC de controles y corrección del montaje DOM de BankManager.*

- [x] **S2.1 — Ciclo de vida de `BankManagerModal` y `BridgeManager`**:
  - Hecho `init()` y `setupDragDrop()` idempotentes en `WebUI/abdbank/src/app.js` para evitar la doble creación de `#drop-overlay` y listeners duplicados.
  - Inhibido `handleKeyboard` de `abdbank` cuando el modal está cerrado (`overlay.hidden === true` o `inert`) para que no secuestre atajos de teclado en el sintetizador principal.
  - Suscritos `'hostInfo'` y `'syncAllParams'` en `BridgeManager._init()` (`WebUI/abdbank/src/bridge/bridgeManager.js`).
  - Mantenida la paridad byte a byte de `WebUI/src/components/bank/` con `@abdsynths/bank-manager-ui` (`sync_bankmanager_ui.js`).
- [x] **S2.2 — Eliminar fuga de controles en `ParamStore` al abrir/cerrar `SlideDrawer`**:
  - Asignado `this.container = container` en `ABDKnob`, `ABDSegmented`, `ABDSelect`, `ABDSlider`, `ABDFilmstripFader` y `ABDToggle` (`WebUI/src/components/shared/`) para que la purga automática de nodos desconectados en `ParamStore.registerControl` funcione al re-renderizar los cajones.
- [x] **S2.3 — Eliminar tormenta de mensajes IPC duplicados/triplicados**:
  - Retirados los callbacks redundantes `onChange: (val) => bridge.setParam(...)` en `WebUI/src/app.js` en aquellos componentes que ya invocan `bridge.setParam` internamente, y eliminado el bucle `setParam` duplicado tras `setAllParams` en `LcdProgrammer.loadProgramIndex`.
- [x] **S2.4 — Robustez de Modales y Promesas del Puente**:
  - Añadido timeout de seguridad (3000 ms) y limpieza de listener a `BridgeCore.getRawProgramData()` (`WebUI/src/bridge/bridgeCore.js`).
  - Actualizado `OscilloscopeModal._ensureScope()` (`WebUI/src/components/OscilloscopeModal.js`) para conectar `this.analyser` de forma diferida si el modal se abrió antes de iniciar el `AudioContext` WASM.

---

#### 🌐 Sprint 3: Optimización DSP, Paridad Wasm Completa y Build/Tests
*Mejora de rendimiento en filtros/voces, paridad funcional del motor WebAssembly y blindaje de la suite de tests.*

- [x] **S3.1 — Optimización y refinamiento DSP**:
  - Evitado recalcular `std::tan()` en cada muestra en `MultiModeFilter::setCutoff` y `setResonance` (`Source/DSP/Filters/MultiModeFilter.cpp`) cuando el valor no varía por encima de un umbral mínimo (`1e-3f` Hz / `1e-5f`).
  - Limitada la frecuencia de corte del HPF de sibilancia en `MS2KVocoder16Band::updateHPFFilter` (`Source/DSP/Vocoder/MS2KVocoder16Band.cpp`) a `0.45f * sampleRate_` para garantizar estabilidad bajo Nyquist a cualquier frecuencia de muestreo.
- [x] **S3.2 — Paridad Funcional en `WasmBridge` / `BridgeWasm`**:
  - Conectadas las salidas de `modSeq_` (`applyModSeq`) en `WasmEngineInstance::process()` (`Source/Wasm/WasmBridge.cpp`), incluyendo soporte completo para `seqLastStep`, `seq1..3Knob`, `seq1..3Motion` y `seq1..3Step1..16`.
  - Implementado el envío y recepción de `pitchBend` y `modWheel` en el camino WASM (`WebUI/src/bridge/bridgeWasm.js` y `Source/Wasm/WasmBridge.cpp`).
  - Convertido `MS2000FactoryBank` en `static const` dentro de `WasmEngineInstance::loadFactoryPreset` para evitar alojar ~49 KB en la pila en cada cambio de programa.
- [x] **S3.3 — Higiene de CMake y Suite de Tests (C++ y Vitest)**:
  - Reemplazada la lista manual duplicada de `.cpp` en el target `ABDMS2000_Tests` (`CMakeLists.txt`) por `${MS2000_DSP_SOURCES}` (`DspSources.cmake`).
  - Protegido `WebUI/tests/unit/bankManagerModalSync.test.js` con `describe.skipIf(!existsSync(packageSrc))` cuando el repo hermano `ABDBankManager` no esté clonado en local.

---

#### 🧹 Sprint 4: Limpieza de Código Muerto, Modularización y Repositorio
*Reducción de deuda técnica, tamaño de bundle y archivos huérfanos.*

- [x] **S4.1 — Eliminación de código muerto C++ y JS**:
  - Eliminados `WebUI/src/ui/panelFactory.js` y `WebUI/src/panels/panelScope.js` junto con sus imports huérfanos (`createScopePanel`, `renderGroupPanel`, `renderAllPanels`, `createOverlayFocus`, `aboutFocusContract`) en `WebUI/src/app.js`.
  - Eliminados los shims vacíos `Source/DSP/Effects/DelayFX.cpp` y `Source/DSP/Vocoder/Vocoder16Band.cpp`.
- [x] **S4.3 — Higiene de activos y `.gitignore`**:
  - Actualizado `.gitignore` para cubrir `WebUI/src/wasm/ms2000_dsp*`.
  - Verificación integral completada: **20/20 suites Vitest (207 tests JS)**, **`npm run check:registry` OK**, y **521/521 tests C++ (`ABDMS2000_Tests.exe`) en verde**.

---

## 🎛️ 2026-10-19 — Plan Maestro de Usabilidad (UX), Bancos/Presets (`A–H`), Edición Flexible VST y Paridad Funcional MS2000

> **Objetivo**:
> 1. Resolver el acceso completo a los **8 Bancos (`A..H`) × 16 Programas (`01..16`) = 128 presets originales** desde el panel del sintetizador y agilizar su integración con `BankManager`.
> 2. Sustituir la rígida multiplexación física de las **16 teclas cuadradas inferiores** del hardware Korg MS2000 por controles visuales dedicados, directos y mucho más flexibles propios de un VST moderno.
> 3. Desbloquear el **100% de las funciones del sintetizador** (edición completa del **Timbre 2**, **Mod Sequencer de 3 pistas × 16 pasos**, **Vocoder de 16 bandas con Pan estéreo**, microafinación, curvas de velocidad y parámetros de voz/efectos del mapa SysEx de 254 bytes).
> 4. **Regla de Oro de Ordenación (Mínimo Riesgo + Cero Retrabajo)**: Ejecutar primero las capas de datos y la parametrización arquitectónica de los cajones (`activeTimbre` / no-bloqueo de UI) **antes** de poblar nuevos controles en los drawers, garantizando que **ninguna fase posterior obligue a reescribir ni tirar trabajo de las fases previas**.

---

### 🔍 1. Diagnóstico Previo: Bancos/Presets, `BankManager` y las 16 Teclas Inferiores

#### A) ¿Por qué hoy no se puede cambiar de Banco (`A..H`) ni llegar a los 128 Presets desde el Synth?
1. **En el motor C++ (`Source/State/MS2000FactoryBank.h` y `Source/MIDI/SysExManager.cpp:10-27`)**:
   - Los **128 programas de fábrica auténticos del Korg MS2000** (`A01` a `H16`, organizados en 8 bancos `A..H` de 16 presets cada uno) **sí están compilados en binario y cargados en `SysExManager::bank_[0..127]`**.
2. **En la WebUI principal (`WebUI/src/ui/lcdProgrammer.js:24, 187-222` y `WebUI/src/contracts/factoryPresets.js:5-150`)**:
   - `LcdProgrammer` inicializa `this.programs = FACTORY_PROGRAMS`, un array JS que solo contiene **8 presets mock incompletos (`A.01` a `A.08`)**.
   - Al pulsar `VALUE ▲/▼` en la página `PROG`, `loadProgramIndex(idx)` calcula `safeIdx = (idx + this.programs.length) % this.programs.length` (`módulo 8`). Como resultado, el usuario queda atrapado en un bucle `0..7` (`A.01` a `A.08`): **los programas `8..127` (de `A.09` a `H.16`, es decir, el 93.75% del banco de fábrica y todos los bancos `B` a `H`) son inaccesibles desde el panel**.
   - Además, aunque `bridge.send('selectProgram', { index: safeIdx })` carga en C++ el programa real `0..7`, `lcdProgrammer.updateDisplay()` (`líneas 255-257`) pisa el nombre real del preset (`Stab Saw`, `rikimaru`, etc.) mostrando el nombre ficticio de `FACTORY_PROGRAMS` (`A.01 Init Program`, `A.02 MS2000 Poly Lead`, etc.).
   - En modo **WASM standalone** (`Source/Wasm/WasmBridge.cpp:571-590`), `loadFactoryPreset(index)` solo lee 12 bytes crudos en lugar de decodificar el struct completo mediante `MS2000FactoryBank::toNativeProgram()`.
3. **¿Se puede gestionar hoy desde `BankManager` (`WebUI/abdbank/`)?**:
   - **El puente C++ (`Source/Plugin/BridgeActions.cpp:565-797`) sí tiene implementado el `softwareTransport`** (`bank.read`, `preset.read`, `preset.write` con `audition: true`, `bank.write`, `preset.capture`).
   - Sin embargo, hoy presenta **3 barreras de usabilidad** para cambiar de banco/preset:
     1. **Arranca desconectado y vacío**: Al abrir `BANK MGR`, su base IndexedDB local está vacía y el transporte figura como desconectado. Para ver los 128 presets del plugin, el usuario debe abrir manualmente `Destination` → elegir `Korg MS2000 (the plugin hosting this Bank Manager)` → pulsar `Connect` → pulsar `Fetch from the host synth`.
     2. **Volcado plano sin Bancos `A..H`**: `handleBankRead` (`BridgeActions.cpp:599-631`) entrega los 128 slots como una lista plana `0..127` sin agrupar ni etiquetar por bancos `A..H` (`A01..H16`).
     3. **Rol de Bibliotecario vs Selector Rápido**: `BankManager` es ideal como **Librarian** (importar/exportar `.syx`, organizar librerías de usuario, enviar a hardware MIDI real), pero abrir un modal a pantalla completa solo para pasar de `A.01` a `B.04` mientras se toca es poco ágil. El panel principal necesita su propio selector directo de **Banco `A..H` + Programa `01..16` + Popover Browser**.

#### B) Auditoría de las 16 Teclas Cuadradas Inferiores del Hardware Original y Propuesta VST Flexible
En el hardware físico del Korg MS2000 / MS2000R (esquemas `KLM-2181/82/83`, `ms2000_switch_matrix.h` y `ms2000_panel_mapping.h`), la fila inferior de **16 botones con LED (`SW 1..16`)** más sus teclas de modo (`BANK UP/DOWN`, `TIMBRE SELECT`, `EDIT`, `SEQ EDIT`, `SEQ SELECT`, `MOD SEQ REC`) estaba **multiplexada en 6 modos distintos** por limitaciones de espacio físico:

| # | Función en los 16 botones del Hardware Original | Estado Actual en `ABDMS2000` | Propuesta de Edición Flexible en el VST (Desmultiplexada) |
|---|---|---|---|
| **1** | **Selección de Programa (`01..16`) + `BANK UP/DOWN` (`A..H`)**: En modo Play, pulsar `1..16` carga inmediatamente el programa `01..16` del banco activo (`A..H`). | ❌ Ausente (solo `VALUE ▲/▼` atrapado en `A.01..A.08`). | **Selector de Bancos `[A..H]` + Browser Popover `8×16` en el Hero/LCD**: Píldoras directas `A..H` junto al LCD y, al hacer clic en la pantalla LCD o botón `BROWSE`, cuadrícula desplegable `8 Bancos × 16 Programas` con nombres reales, filtro por categoría y búsqueda rápida. |
| **2** | **Salto Rápido a las 16 Páginas de Edición (`EDIT` + `1..16`)**: Etiquetas serigrafiadas (`1: Voice`, `2: Pitch`, `3: Osc1`, `4: Osc2`, `5: Mixer`, `6: Filter`, `7: Pitch EG`, `8: Amp`, `9: Amp EG`, `10: LFO1`, `11: LFO2`, `12..15: Patch 1..4`, `16: ModSeq/Arp/FX/Global/Util`). | ⚠️ Parcial: Las 11 tarjetas del Dashboard abren drawers, pero solo desde el texto del título y bloqueando el resto de la UI con `.drawer-backdrop`. | **Dashboard de 1 Clic + Navegación Fluida**: Hacer toda la superficie libre de cada tarjeta clicable para abrir su drawer y permitir cambiar de tarjeta/drawer con **1 solo clic** (sin que el backdrop bloquee las tarjetas ni el teclado inferior). |
| **3** | **Conmutador `TIMBRE SELECT` (`Timbre 1` / `Timbre 2`) + Utilidades de Timbre (`Copy Timbre`, `Swap Timbre`, `Init`)**: Cambia qué timbre (`T1` o `T2`) editan todos los knobs del panel en modos `Split` y `Dual/Layer`. | ❌ Crítico: No existe botón `TIMBRE 1 / 2`. Los 119 parámetros `t2*` existen en C++/APVTS pero ningún control de la UI permite editarlos. | **Conmutador Global `[TIMBRE 1] [TIMBRE 2]` + Barra de Utilidades Bitímbricas**: Selector siempre visible en la barra superior y en la cabecera del `SlideDrawer` que redirige automáticamente todos los knobs/drawers al timbre activo (`T1` o `T2`), más acciones rápidas `Copy T1→T2`, `Copy T2→T1` y `Swap T1↔T2`. |
| **4** | **Edición del Mod Sequencer (`SEQ EDIT`, `SEQ SELECT 1..3`, `MOD SEQ REC` + `1..16`)**: Selecciona pista `1..3`, paso `1..16`, `Last Step`, `Motion (Step/Smooth)`, `Run Mode (1Shot/Loop)` y grabación paso a paso. | ⚠️ Muy limitado: El drawer solo muestra `SEQ 1` con 16 knobs circulares pequeños (`seq1Step1..16`); `SEQ 2` y `SEQ 3` son inaccesibles. | **Editor Gráfico Interactivo `3×16` con Dibujo a Ratón (*Paint-Drag*)**: Barras bipolares verticales para los 16 pasos (al estilo `ReMS2000`) donde puedes arrastrar el ratón de izquierda a derecha para dibujar la curva, pestañas/vista apilada para **`SEQ 1`, `SEQ 2` y `SEQ 3`**, selectores individuales de `Destination` (31 destinos) y `Motion (Step/Smooth)`, y herramientas `Copy / Paste / Clear / Invert / Smooth / Random Track`. |
| **5** | **Máscara Rítmica del Arpegiador (microKORG / Advanced, botones `1..8`)**: Activa/silencia pasos `1..8` del arpegiador y define `Last Step`. | ✅ Implementado en `renderArpDrawer` (`Step Rhythm Mask` con `ALL ON`, `ODD`, `EVEN`, `INVERT`). | Mantener la botonera interactiva en `renderArpDrawer` y completarla exponiendo `arpKeySync`, las 16 resoluciones completas (`0..15`) y sincronización visual con el reloj. |
| **6** | **Vocoder 16 Bandas (`1..16` para `Level` / `Pan` / `Vocoder Hold`) y Teclado Rack (`C..D#`)**: Selecciona banda `1..16` para ajustar nivel/pan o congela formantes (`Hold`). | ⚠️ Parcial: Hay 16 faders de nivel en el drawer, pero **faltan los 16 controles de `Pan` (`L..R`)** (que ya existen en el DSP C++) y `HPF Gate`. | **Consola Vocoder de 16 Bandas Nivel + Pan Estéreo**: Añadir en el drawer del Vocoder la fila/modo de **Panorama Estéreo por Banda (`B1..B16 Pan`)** con presets de imagen estéreo (`Center`, `Alternate L/R`, `Wide Spread`, `Random Pan`), frecuencias reales en Hz (`75 Hz .. 5 kHz`) y botón `Formant Hold`. |

---

### 🗓️ 2. Plan de Ejecución Ordenado por Sprints (Mínimo Riesgo y Cero Trabajo Desechado)

> **Por qué este orden garantiza CERO retrabajo**:
> 1. **Sprint UX-1** resuelve la capa de catálogo de 128 presets, bancos `A..H`, nombres reales en LCD, `COMPARE / WRITE` y auto-conexión de `BankManager`. Es ortogonal a los drawers de síntesis: nada de lo que se haga después lo altera.
> 2. **Sprint UX-2** establece la arquitectura de redirección bitímbrica (`activeTimbre`: `T1` vs `T2`) y el desbloqueo ergonómico de `SlideDrawer` y Dashboard **antes** de añadir nuevos controles a los drawers. Así, todos los controles que se creen o amplíen en **UX-3** y **UX-4** nacerán ya compatibles con `Timbre 1` y `Timbre 2` sin tener que tocarlos dos veces.
> 3. **Sprint UX-3** construye el nuevo editor flexible del Mod Sequencer `3×16` y expone los 36 parámetros que **ya existen** en `parameters-spec.json` y `SynthEngine.cpp` (cero cambios en binario SysEx).
> 4. **Sprint UX-4** aborda finalmente las ampliaciones de `parameters-spec.json` (`gen:registry`), la corrección de offsets SysEx del Vocoder (`+14..+45`) y los parámetros hardware restantes del MS2000.

---

#### 🟢 Sprint UX-1: Catálogo Real de 128 Presets (`A01–H16`), Selector de Bancos `A–H` y Auto-Conexión con `BankManager` (✅ COMPLETADO)
*Riesgo: Muy Bajo | Impacto: Desbloquea los 8 bancos (`A..H`) y 128 presets originales en VST y WASM sin tocar el motor DSP.*

- [x] **UX1.1 — Exponer el catálogo real de 128 programas (`A.01` a `H.16`) desde C++ y WASM**:
  - Añadir acción de puente `getPresetCatalog` (e incluir la lista compacta de 128 metadatos `{ index, bank: 'A'..'H', number: '01'..'16', code: 'A.01', name, category, timbreMode }` en el arranque) leyendo `SysExManager::getBank()` (`Source/Plugin/BridgeActions.cpp`).
  - Corregir `WasmEngineInstance::loadFactoryPreset(int index)` (`Source/Wasm/WasmBridge.cpp:571-590`) para que utilice `MS2000FactoryBank::toNativeProgram(raw, prog)` y aplique los 254 bytes reales del preset al motor WASM (en lugar de leer solo 12 bytes crudos).
  - Generar o exportar el catálogo estático de metadatos de los 128 presets de fábrica para que el modo standalone/navegador (`bridgeWasm.js`) muestre exactamente los mismos 128 nombres y bancos `A.01..H.16` que el plugin nativo.
- [x] **UX1.2 — Selector directo de Bancos `A..H`, Navegación `0..127` y Popover Browser `8×16` en el LCD**:
  - Eliminar el límite de 8 presets mock en `WebUI/src/ui/lcdProgrammer.js` (`this.programs` pasa a tener los 128 slots `A.01..H.16` sincronizados con el backend).
  - Añadir en el módulo LCD / Hero Bar controles directos de **Banco `[A][B][C][D][E][F][G][H]`** (`BANK -/+` o píldoras `A..H`) y **Programa `01..16`**, de modo que cambiar de banco o programa requiera 1 clic.
  - Hacer que al clicar sobre el nombre del programa en la pantalla LCD se abra un **Preset Browser Popover en cuadrícula `8 Bancos × 16 Programas`** (con pestañas `Bank A`..`Bank H`, filtro por categoría `[All, Lead, Pad, Bass, Arp/Seq, Vocoder, SFX]` y buscador por texto).
- [x] **UX1.3 — Reparar `WRITE` y `COMPARE` (A/B Snapshot) en la Barra Superior**:
  - Conectar el botón `#btn-compare` (`WebUI/src/app.js` / `lcdProgrammer.js`) con un buffer de snapshot A/B (`originalProgramSnapshot` vs `editedStateSnapshot`): al pulsar `COMPARE`, el botón se ilumina y alterna de forma no destructiva entre el sonido recién cargado del preset y la edición en curso.
  - Hacer que la confirmación `WRITE -> YES` en el LCD y el botón superior `#btn-write` guarden realmente el estado actual en el slot seleccionado de `SysExManager::bank_[idx]` (enviando acción IPC `writeProgramToSlot`) y actualicen el nombre/catálogo en memoria.
- [x] **UX1.4 — Auto-Conexión e Integración Fluida del `BankManager` Embebido**:
  - En modo embebido (`window.parent.__synthBridge`), hacer que `BankManager` (`WebUI/abdbank/src/app.js`) se auto-conecte al `softwareTransport` del sintetizador anfitrión al abrirse por primera vez y sincronice automáticamente los 128 programas organizados por bancos `A..H` (`A.01..H.16`) si el banco activo está vacío, eliminando los 4 pasos manuales de configuración.

---

#### 🟡 Sprint UX-2: Arquitectura Bitímbrica (`TIMBRE 1 / TIMBRE 2`), `SlideDrawer` No-Bloqueante y Reactividad del Dashboard (✅ COMPLETADO)
*Riesgo: Bajo | Impacto: Desbloquea los 119 parámetros ya existentes del Timbre 2 y elimina las mayores fricciones ergonómicas antes de ampliar los drawers.*

- [x] **UX2.1 — Conmutador Global `[TIMBRE 1] [TIMBRE 2]` y Helper de Direccionamiento `tParam(id)`**:
  - Añadir un selector visual **`TIMBRE 1` / `TIMBRE 2`** en la barra Hero (junto a `VOICE MODE: Single / Split / Layer / Vocoder`) y en la cabecera fija del `SlideDrawer`, con indicador de estado activo y atajo de teclado (`Tab` o `Alt+1` / `Alt+2`).
  - Implementar en `WebUI/src/app.js` y `WebUI/src/ui/lcdProgrammer.js` la función canónica `tParam(baseId, timbre = activeTimbre)` que resuelve automáticamente `'osc1Wave'` → `'osc1Wave'` (Timbre 1) o `'t2Osc1Wave'` (Timbre 2) utilizando el mapa simétrico de `registry.gen.js`.
  - Parametrizar los constructores de tarjetas del Dashboard y todos los `render*Drawer` actuales con `tParam(...)`, de modo que al seleccionar `TIMBRE 2` todos los knobs, faders, selectores, envolventes ADSR y páginas del LCD editen y reflejen en tiempo real los **119 parámetros `t2*`** ya presentes en C++/APVTS.
  - Añadir utilidades rápidas bitímbricas en el drawer de `Voice`: **`Copy Timbre 1 → 2`**, **`Copy Timbre 2 → 1`** y **`Swap Timbre 1 ↔ 2`**.
- [x] **UX2.2 — `SlideDrawer` No-Bloqueante (Tocar el Teclado y Cambiar de Tarjeta con 1 Clic)**:
  - Ajustar `.drawer-backdrop` (`WebUI/src/styles/slideDrawer.css:15-26` y `slideDrawer.js`) para que **NO cubra ni bloquee**:
    1. El teclado inferior `#piano-keyboard` y las ruedas `PITCH` / `MOD` (permitiendo tocar notas y probar modulaciones en vivo mientras se ajustan los knobs del cajón lateral).
    2. La barra superior Hero / LCD / Transporte.
    3. Las tarjetas del `.synth-dashboard`: hacer clic en otra tarjeta mientras un drawer está abierto conmuta directamente el contenido del drawer a la nueva sección en **1 solo clic** (en lugar de requerir 2 clics: cerrar + volver a abrir).
- [x] **UX2.3 — Tarjetas 100% Clicables, Sincronización Reactiva (`paramStore.onChange`) y Telemetría en LCD**:
  - Hacer que toda el área libre de cada `.dashboard-card` (excepto los knobs/selectores interactivos dentro de ella) abra su drawer principal al hacer clic, con feedback visual de tarjeta activa (`.dashboard-card.is-editing`).
  - Completar todos los listeners faltantes en `setupDashboardObservers()` (`WebUI/src/app.js:2066-2120`): `osc2Semi`, `osc2Tune`, `osc2Mod`, `portamentoTime`, `mixNoiseLevel`, `filterType`, `eg1*`, `eg2*`, `lfo1/2Freq`, `lfo1/2Sync`, `lfo1/2SyncNote`, `modFx*`, `delay*`, `eq*`, `arp*`, `modSeqOn`, `seq*` y sus equivalentes `t2*`, garantizando que las tarjetas y gráficos SVG (`#filter-svg`, `#env-svg`) se actualicen siempre al cambiar de preset, randomizar o editar desde el LCD.
  - Mostrar feedback temporal en el LCD principal (`[Sección] Nombre Parámetro: Valor Formateado`) durante 1.5 s al girar cualquier knob/slider/selector del dashboard o de cualquier drawer (centralizado en los wrappers de `WebUI/src/components/shared/`).
- [x] **UX2.4 — Mejoras Transversales en `ABDShared/Code/MidiKeyboard` (`@abdsynths/midi-keyb`) para Diseño Sonoro con Ratón**:
  - *(Resuelve el problema del cursor único de ratón de forma compartida para `ABDMS2000`, `ABDEep`, `ABDCZ101`, `ABDJUNiO601` y `ABDNeural`)*:
  - **Fix de `setSustain(false)`**: Al desactivar `SUST` (`sustainOn = false`), liberar y enviar `onNoteOff` para todas las notas retenidas en `activeKeys` que no estén físicamente pulsadas (evitando que queden colgadas hasta pulsar `PANIC`).
  - **Modo `HOLD / Latch` + `Shift+Click` por tecla**: Permitir fijar/soltar notas o acordes haciendo clic (con `HOLD` activo o mediante `Shift+Click` / clic derecho en cualquier tecla) para poder dejar sonando un acorde con el ratón y subir inmediatamente a mover los knobs del `SlideDrawer` o del Dashboard.
  - **Modo `RETRIG` (Auto-Repeat Loop)**: Añadir modo de re-disparo periódico de las notas fijadas en `HOLD` (ej. gate 60% cada ~1 s) para poder diseñar las fases de **Attack** y **Decay** de `EG1` (Filtro) y `EG2` (Amp) con una sola mano en el ratón (ya que una nota estática en Sustain no permite escuchar cambios de ataque/decaimiento).
  - **Glissando con ratón y prevención de notas colgadas (`window` `pointerup` + `pointerenter`/`pointerleave`)**: Garantizar que si el usuario pulsa una tecla y arrastra el cursor hacia arriba rumbo a un knob antes de soltar el botón del ratón, la nota se libere limpiamente y permita barridos sobre el teclado.
  - **Etiquetas visuales de teclado QWERTY (`Z..M` / `Q..P`)**: Mostrar sutilmente sobre las teclas del piano su mapeo QWERTY para que sea evidente que se puede tocar con la mano izquierda en el teclado del PC mientras se edita con el ratón.

---

#### 🟠 Sprint UX-3: Editor Flexible del Mod Sequencer `3×16` y Desbloqueo de los 36 Controles Ocultos (Categorías B y C) (✅ COMPLETADO)
*Riesgo: Bajo-Medio | Impacto: Sustituye la edición secuencial de las 16 teclas por un editor gráfico moderno y expone todos los parámetros del motor que estaban ocultos en la UI (sin cambios en el binario SysEx).*

- [x] **UX3.1 — Nuevo Editor Gráfico Flexible del Mod Sequencer (`3 Pistas × 16 Pasos`)**:
  - Rediseñar `renderModSeqDrawer` (`WebUI/src/app.js:1106-1196`) reemplazando los 16 knobs circulares de una sola pista por un **Secuenciador Gráfico Multi-Pista (`SEQ 1`, `SEQ 2`, `SEQ 3`)**:
    - Conmutador de pista activa **`[SEQ 1] [SEQ 2] [SEQ 3]`** + vista compacta simultánea de las 3 curvas.
    - **16 barras bipolares interactivas (`-64 .. 0 .. +63`) con soporte *Paint-Drag*** (arrastrar el ratón horizontalmente sobre las 16 columnas para dibujar rampas, senoides o escalones de un trazo, más doble clic para centrar a `0`).
    - Controles por pista conectados a `tParam(...)`: **`Destination` (`seq1..3Knob`, con los 31 destinos reales del MS2000)** y **`Motion` (`seq1..3Motion`: `Step` vs `Smooth`)**.
    - Controles globales de secuenciador: **`Run Mode` (`seqRunMode`: `1-Shot` / `Loop`)**, **`Key Sync` (`seqKeySync`: `Off` / `Timbre` / `Voice`)**, `Last Step (1..16)`, `Tempo Sync`, `Resolution`.
    - Barra de herramientas creativas por pista: **`Clear (0)`**, **`Linear Ramp ▲/▼`**, **`Sine/Triangle`**, **`Invert`**, **`Randomize Track`** y **`Copy / Paste Track`**.
- [x] **UX3.2 — Desbloqueo de Parámetros Huérfanos en Drawers de Voz, Pitch, Amp, LFO y Arpegiador (Categoría B)**:
  - **`Voice / Pitch Drawer`**: Añadir selectores para **`Scale Type` (`scaleType`: 9 temperamentos — Equal, Pure Major, Pure Minor, Arabic, Pythagorean, Werckmeister, Kirnberger, Slendro, Pelog)** y **`Scale Key` (`scaleKey`: `C .. B`)**.
  - **`Amp Drawer`**: Añadir el knob bipolar **`Velocity Sensitivity` (`ampVelo` / `t2AmpVelo`, `-63..+63`)** junto a `ampLevel`, `ampPan` y `ampKeyTrack`.
  - **`LFO 1 & 2 Drawer`**: Ampliar las opciones de **`lfo1SyncNote` y `lfo2SyncNote`** de 6 a las **15 divisiones rítmicas completas (`0..14`: `1/1` a `1/64`)** soportadas por `FirmwareTables::getLfoSyncPeriodSeconds`.
  - **`Arpeggiator Drawer`**: Ampliar **`arpResolution`** de 6 a las **16 resoluciones (`0..15`)** registradas en el esquema y sincronizar `arpTempo` bidireccionalmente con el reloj de transporte.
- [x] **UX3.3 — Limpieza de Controles "Placebo" y Atajos de Teclado (Categoría C)**:
  - Implementar pila histórica real de **`Undo / Redo`** (50 estados) en `ParamStore` (`WebUI/src/core/paramStore.js`) para que `Ctrl+Z` / `Ctrl+Y` funcionen de verdad al editar knobs o aplicar `Randomize`/`Init`.
  - Cambiar el atajo de `Randomize` de `Ctrl+R` a `Ctrl+Shift+R` (evitando recargar el navegador por accidente en modo WASM) y unificar la pila de cierre con `Escape` para que cierre solo el modal/drawer superior activo.
  - Sincronizar `PAGE 09: GLOBAL MIDI` del LCD (`midiCh`, `clockMode`) con el estado de configuración MIDI del host/transporte.

---

#### 🔵 Sprint UX-4: Paridad Profunda Hardware Korg MS2000 — Vocoder Estéreo/HPF y Parámetros SysEx Restantes (Categorías B.6 y D)
*Riesgo: Medio | Impacto: Alcanza la paridad funcional 1:1 con el mapa SysEx de 254 bytes del Korg MS2000 original.*

- [x] **UX4.1 — Vocoder Pan 16 Bandas, HPF Gate y Arp Key Sync**: ✅ hecho (2026-11-05)
  - **`vocoderBandPan1`..`vocoderBandPan16`**: registrados y conectados a `vocoder_.setBandPan(b, pan)` (`SynthEngine.cpp:760+`), con modo `PANPOTS (1-16)` en `renderVocoderBandsDrawer`.
    - Macros `CENTER`, `ALT L/R` y `LOW-C / HI-W` añadidas al modo `PANPOTS`, y cada banda rotula su centro aproximado (`75 Hz`..`5.0 kHz`, reparto geométrico entre los extremos documentados por Korg).
  - **`vocoderHpfGate`**: registrado, conectado a `vocoder_.setHPFGate()` y con toggle `HPF GATE (SIBILANCE)` en el drawer de Vocoder.
    - *Pendiente*: **`vocoderHpfThreshold` no existe en el contrato** (el drawer usa `vocoderThreshold` y `vocoderGateSense`), así que el desacople del umbral de gate sigue fuera.
  - **`arpKeySync`**: promovido a parámetro real (vive en `ARP_FLAGS` bit 0, por eso `sysex: false` para no desplazar el mapa de offsets), conectado a `arpeggiator_.setKeySync()` y con toggle `KEY SYNC` en `renderArpDrawer` y en `WasmBridge.cpp`.
- [x] **UX4.2 — Round-trip SysEx del Bloque Vocoder (`MS2000HardwareProgram.h`)**: ✅ hecho (2026-11-05)
  - La nota original sobre los offsets `+14..+45` era obsoleta: `vocoderFields()` mapea con `TIMBRE1_START + ti::*`, y las 16 bandas tienen su propio bucle (`forEachVocoderBand` / `forEachVocoderPan`) usado por `applyToAPVTS`, `toNativeProgram` y `captureModeledFields`.
  - `DSPCoreTests.cpp` ya comprueba las 16 bandas con **16 valores distintos** en ambos sentidos: apply → parámetros, y capture → bytes 46..61 (niveles) y 62..77 (pans).
- [x] **UX4.3 — Paridad de Parámetros por Timbre (Categoría D)**: ✅ cerrado al 100 % (2026-11-05)
  - **Completado**: `pitchTranspose`, `pitchTune`, `pitchBendRange`, `vibratoInt`, `ampGate`, `delayTempoSync`, `delaySyncNote`, `arpTarget`, **`arpSwing`** (byte global `0x24 = 36`, complemento a 2 `-100..+100 %`, conectado en `Arpeggiator::setSwing()`, `SynthEngine`, `WasmBridge`, `MS2000HardwareProgram::synthFields()` y `renderArpDrawer`), `triggerMode` (bit 3 de `ti::FLAGS`, aplicado en `VoiceManager::noteOn` para Mono/Unison), **`keyPriority`** (+ espejo `t2KeyPriority`, bits `0..1` de `ti::FLAGS`: `0 = Last`, `1 = Low`, `2 = High`, verificado empíricamente con guion de 2 notas solapadas `C4 ↔ G4` en el oráculo y conectado en `VoiceManager::pickPriorityNote()`, `SynthEngine`, `WasmBridge` y el drawer de `Voice`) y **`eg1Reset` / `eg2Reset`** (+ espejos `t2Eg1Reset` / `t2Eg2Reset`, bits 4 y 5 de `ti::FLAGS`: `0 = Continue`, `1 = Reset to Zero`, verificados en el oráculo tanto en `Multi` como en `Single` legato y conectados en `ADSREnvelope::setResetToZero()`, `Voice`, `SynthEngine`, `WasmBridge` y los dos drawers de `Envelopes`) son parámetros reales del APVTS, con control en sus drawers (`KEY PRIORITY (MONO)` y `BEND RANGE` en Voice/Pitch, `RESET (CONTINUE / FROM ZERO)` en EG1 y EG2, `AMP SWITCH` en Amp, `TEMPO SYNC` + `SYNC NOTE` en Delay, `SWING (%)` + `KEY SYNC` + `TARGET TIMBRE` en Arp).
  - Se eliminaron las **cuatro lecturas `apvts_.state.getProperty(...)` del hilo de audio** en `SynthEngine::updateParametersFromAPVTS()`: ahora usan `getParam(ParamIDs::…)` sobre el caché atómico `paramCache_`. La carga de presets escribe el parámetro real (además de la propiedad, por compatibilidad) en `MS2000HardwareProgram::applyToAPVTS` y `MS2000ProgramData::applyToAPVTS`.
  - `osc2Mod = 3` **sí** estaba implementado: `OSC2Modulator::RingSync` es alias de `CrossMod`, que aplica Ring + HardSync (`Voice.cpp:356`). Solo se corrigió la etiqueta `CrossMod` → `Ring+Sync` en el contrato y en el drawer de OSC 2.

---

## 🔗 2026-11-02 — Alineación con el Diccionario de Datos Común (`ABDShared` v2.0)

> **Especificación y Guía de Integración Canónica**:
> - [`ABDShared/Docs/specs/COMMON_DATA_DICTIONARY_AND_INTEGRATION.md`](file:///d:/desarrollos/ABDSynths/ABDShared/Docs/specs/COMMON_DATA_DICTIONARY_AND_INTEGRATION.md)
> - [`ABDShared/Contracts/parameter_dictionary.schema.json`](file:///d:/desarrollos/ABDSynths/ABDShared/Contracts/parameter_dictionary.schema.json)
> - [`ABDShared/Contracts/MidiTranslationEngine.ts`](file:///d:/desarrollos/ABDSynths/ABDShared/Contracts/MidiTranslationEngine.ts)

### Tareas de Convergencia para `ABDMS2000`
- [x] **CDD.1 — Cabecera Canónica v2.0 y Topología `multi_part` en `korg_ms2000_parameter_spec.json`**: ✅ hecho (2026-11-05)
  - Añadidos `"$schema": "./parameter_dictionary.schema.json"`, `"schemaVersion": "2.0.0"`, `"id": "korg_ms2000"`, `"displayName": "Korg MS2000"` y `"topology": "multi_part"` con `"parts": [{ "id": "t1", "name": "Timbre 1", "scope": "part", "cardinality": 1, "idPrefix": "" }, { "id": "t2", "name": "Timbre 2", "scope": "part", "cardinality": 1, "idPrefix": "t2" }]` (conservando `"synthName": "ABDMS2000"` y `"version": "1.0.0"` por retrocompatibilidad).
  - Asignado `scope` en los **341 parámetros**: **43 `global`** (`Master`, `Global`, `EQ`, `ModFX`, `DelayFX`, `Arp`, ubicados en bytes `0x10..0x24`, incluido `arpSwing`) y **298 `part`** (bloques de Timbre 1 y Timbre 2, con 126 espejos `t2*`).
- [x] **CDD.2 — Declarar Explícitamente `SymBipolar` (`wireZeroCenter: 64`) y Tablas `Lut`**: ✅ hecho (2026-11-05)
  - Migrados los **34 parámetros bipolares** centrados en `64` (`osc2Tune`, `filterEg1Int`, `filterKeyTrack`, `filterVelo`, `ampPan`, `ampKeyTrack`, `ampVelo`, `patch1..4Intensity`, `vibratoInt`, `pitchTranspose`, `pitchTune`, `pitchBendRange`, sus espejos `t2*` y el resto de parámetros con `wireCodec: "Bipolar"`) a `"curve": "SymBipolar"` con `"wireZeroCenter": 64`, aceptado en `Scripts/registry_core.js` y `Scripts/registry_generator.js`.
  - Declarados `"curve": "Lut"` + `"lutId": "ms2000_lfo1_hz"` / `"ms2000_lfo2_hz"` en `lfo1Freq`, `lfo2Freq` y sus espejos `t2*`, e implementado el `case 3` (`Lut`) en `Source/State/Conversion.gen.h` preservando la respuesta en Hz exacta.
  - Corregidos además los 3 valores preexistentes `"smoothing": "Log"` → `"Logarithmic"` (`lfo1Freq`, `lfo2Freq`, `modFxSpeed`) para cumplir sin errores (0 errores `jsonschema`) con `parameter_dictionary.schema.json`.
- [x] **CDD.3 — Empaquetado Binario SysEx (`binaryPacking`) y Roles Semánticos (`semanticRole`)**: ✅ hecho (2026-11-05)
  - Volcados programáticamente **245 descriptores `binaryPacking`** (`byteOffset`, `bitMask`, `bitShift`, `byteSize: 1`, `encoding`: `"Unsigned"` / `"Offset64"` / `"Bit"`) en `korg_ms2000_parameter_spec.json` a partir de la tabla canónica `Field` de `Source/MIDI/MS2000HardwareProgram.h` (`0x26 + ti::X` para Timbre 1 y `0x92 + ti::X` para Timbre 2).
  - Añadidos **39 `semanticRole`** en los parámetros principales y sus espejos (`MasterVolume`, `FilterCutoff`, `FilterResonance`, `FilterEnvAmount`, `AmpEnvAttack..Release`, `FilterEnvAttack..Release`, `Osc1Waveform`, `Osc2Waveform`, `Osc2DetuneCents`, `Osc2TransposeSemis`, `AmpLevel`, `AmpPan`, `Lfo1RateHz`, `Lfo2RateHz`, etc.).

---

## 🎯 2026-11-04 — Paridad sonora con el MS2000 original (modo clásico): oráculo, método y plan de calibración

> **Objetivo**: que un preset de fábrica del MS2000 (o cualquier `.syx` de usuario) suene en nuestro motor con la misma respuesta que el hardware, **sin firmware en el producto**. Ámbito: `synthMode = MS2000` (modo clásico). microKORG y ADVANCED quedan fuera de esta línea salvo que una corrección sea compartida.
>
> **Estado inicial**: el motor suena bien por sí solo, pero al cargar presets originales la diferencia es audible y sistemática. El motor se valida contra sus propias LUTs (`tests/CalibrationTests.cpp` comprueba que el EG cumple la tabla que el propio EG usa), no contra una referencia externa: por eso un error conceptual no se detecta.
>
> **Activos disponibles**: `Source/Core/MS2000FirmwareTables.h` (EG, LFO1/2, pitch, cutoff, patch, escalas), `DOCS/DSP56362_Microcode_Disassembly.asm`, `DOCS/DSP56362_XY_Memory_Dump.txt`, `DOCS/MS2000_Firmware_DSP_Reference.md`, `Source/DSP/Oscillators/MS2000HardwareDWGSData.h`, `Source/State/MS2000FactoryPreloadData.h`, `Source/DSP/Effects/MS2000HardwareEffects.h` y `ms2000r-emu` (H8S/2350 + DSP56362), que pasa a usarse como **instrumento de medida**.

### 🧭 Reglas de la línea de trabajo

1. **Nada se cambia "a oído"**: cada corrección entra con una métrica definida *antes* y un render pareado antes/después.
2. **Todo se compara a 48 kHz**, frecuencia nativa del DSP. Cualquier soporte a otras `fs` se resuelve con un escalado explícito `fs / 48000`, no implícito.
3. **El oráculo no es una dependencia**: `ms2000r-emu` es GPL-3.0 y este producto es propietario. Se ejecuta como proceso externo (headless), se guardan WAV y tablas como datos, y **nunca** se enlaza su código ni se copian sus clases. Los algoritmos se re-derivan del microcódigo documentado en `DOCS/`.
4. **La palabra Q23 manda**: cuando un parámetro tenga palabra en `X:`/`Y:`, el motor consume esa palabra (o su LUT), no una reconstrucción en Hz/normalizado. Es el criterio de fuente única de verdad de `AGENTS.md` aplicado al DSP.
5. **Cada fase cierra con una cifra**: si el cambio no reduce el error medido en el corpus, se revierte. No se acumulan correcciones sin evidencia.

### 📦 Fase P0 — Arnés de comparación (sin tocar el motor) · ~2–3 días

| # | Tarea | Entregable |
|---|-------|------------|
| P0.1 | **✅ hecho (2026-11-04)** CLI de render propio: target `ABDMS2000_Render` en `CMakeLists.txt` reutilizando `${MS2000_DSP_SOURCES}`, con host `AudioProcessor` mínimo + APVTS + `SynthEngine`: `--program N --patch OFF=VAL --note 60 --vel 100 --hold 1.5 --out BASE` → `BASE.bin` (float, sin recortar) + `BASE.wav` (int32) | `tests/RenderCLI.cpp` |
| P0.2 | **✅ hecho (2026-11-04)** Runner del oráculo: escribe el preset de fábrica en la ranura 0 del flash, dispara note-on/off por `MS2K_MIDIIN` y usa `--headless --quiet-boot --audio --power-on --render-test` | `Scripts/calibration/run_oracle.py` |
| P0.3 | Corpus: 128 presets de fábrica + 8 estímulos sintéticos derivados de `INIT Program` (una sola variable por estímulo) | `Scripts/calibration/corpus.json` |
| P0.4 | Comparador: alineación por correlación cruzada, RMS/nivel, distancia espectral por 1/3 de octava, envolvente de amplitud (A/D/R y correlación de forma), fc a −3 dB y pendiente con ruido, pico de resonancia, THD | `Scripts/calibration/compare.py` |
| P0.5 | Informe reproducible: `metrics.csv` + `report_<fecha>.md` por preset y por bloque | `DOCS/calibration/` |
| P0.6 | Protocolo escrito: cómo añadir un estímulo, cómo leer un informe y cómo declarar una diferencia aceptada | `DOCS/calibration/README.md` |

**Criterio de cierre P0**: un solo comando regenera los 128 pares (WAV nuestro + WAV oráculo) y produce el CSV; y el comparador detecta una diferencia inyectada a propósito (test del propio comparador, p. ej. un preset con el cutoff bajado 50 unidades).

**Comprobación previa (2026-11-04)** — el oráculo ya corre en esta máquina y existe un hook sin usar:

- **Binario**: `_RESOURCES/ms2000/ms2000r-emu-main/ms2000r-emu-main/build_vst/Release/ms2000_emulator.exe` (ya compilado, no hace falta build).
- **Comando verificado**: `MS2K_MIDIIN="3.0:903C64;4.5:803C00" ms2000_emulator.exe --rom <flash.bin> --headless --quiet-boot --audio --power-on --master-vol 100 --duration 6 --render-test <ruta absoluta>.wav`. Sin `--audio --power-on` los *gates* quedan a 0 y el WAV sale **mudo**; con ruta relativa el WAV acaba en el HOME del emulador, no en el cwd.
- **Salida**: WAV 48 kHz estéreo, PCM entero de 32 bits (`sampwidth=4`, no float), y **descarta el silencio inicial** (288.000 frames renderizados → 239.902 escritos), así que toda comparación se alinea por *onset*, nunca por índice absoluto. Coste ≈ 0,55× tiempo real (3,4 s de pared para 6 s de audio).
- **Hook ya existente**: `tests/CalibrationTests.cpp::compareIdenticalPresetsWithEmu` ya analiza 24 bandas de 1/3 de octava, perfil de envolvente, centroide espectral y anchura estéreo, y alinea por onset; pero lee `emu_preset_<idx>.bin` generados por `tests/generate_emu_preset_bins.py`, que hoy son **silencio**. Sustituir ese silencio por renders reales del oráculo es el primer paso con valor, no un target nuevo.
- **Puerta de aceptación de ese paso — ✅ CERRADA (2026-11-04)**: `INIT Program` (#127) escrito en la ranura 0 + C4 → **f0 = 261,62 Hz (−0,03 cents)**, pico −23,4 dBFS, RMS −28,3 dBFS. Los `959,6 Hz` medidos antes del arnés **no eran una propiedad del preset**: eran artefacto del estimador por cruces por cero.
- **⚠️ Trampa crítica en los presets de `flash.bin` (User `0x0A0400` vs Factory Preload `0x0B0000`)**: En el `flash.bin` que usa `ms2000r-emu`, el banco activo que carga el firmware al arrancar es `0x0A0400` (en el emulador el `Program Change` ni siquiera llega al firmware: ver P0.2 cerrado) (*User Flash*), donde el propietario anterior del hardware modificó **28 de los 128 presets sin cambiarles el nombre del LCD** (incluidos **13 de los 16 del Banco A**: `0, 1, 3, 4, 6, 7, 8, 10, 11, 12, 13, 14, 15`, más `17, 18, 19, 27, 36, 38, 42, 45, 49, 50, 56, 57, 71, 84, 87`). En cambio, `ABDMS2000` (`MS2000FactoryPreloadData.h`) usa los **128 presets de fábrica intactos** extraídos de `0x0B0000` (*Factory Preload ROM*). Por ejemplo, `A01 Stab Saw` (`0`) en `0x0A0400` tiene 39 bytes alterados (`OSC1` cambiado de `Saw` a `Sine/Cross`, *Mod Sequencer* activo, etc.). Los `959,6 Hz` que se atribuyeron a esto eran artefacto del estimador por cruces por cero. **Regla para el oráculo**: nunca comparar índices de `0x0A0400` crudos salvo los 100 no modificados (como `{2, 5, 9, 16, 20, 22, 23, 24}`); para el corpus completo o pruebas en GUI, usar siempre inyección de `.syx` desde `MS2000FactoryPreloadData.h` (`DOCS/FactorySysEx/`) o una imagen `flash_factory.bin` con `0x0B0000..0x0B7EFF` copiado sobre `0x0A0400..0x0A82FF` **y la Global Data de fábrica `0x0B8100` (200 B = `0xC8`, no 234) sobre `0x0A0010`** — exactamente lo que hace `GLOBAL 8: PRELOAD` en el hardware (`0x0E2CC` / `0x20FB4` / `0x228AA`, los tres con `mov.w #0x64,R2` = 100 palabras). Sin ese segundo bloque el emulador arranca con los ajustes globales del dueño anterior: de los 234 B en que difieren 13, **sólo uno es Global Data** (offset 16 = NoteRcv + bit de modo), y los 12 restantes (`0x0A00D8..0x0A00E3`) son residuo fuera del bloque copiado, que se deja intacto. Las tres rutinas de PRELOAD leen siempre `0x0B8100` (plantilla de teclado); `0x0B8200` sólo aparece en el Factory Reset de test, detrás de `btst #7,@0xFFF6C9` —el flag que distingue placa MS2000 de MS2000R— y sus diferencias son el mapa de notas de los botones del panel.

**P0.2 cerrado (2026-11-04)** — `Scripts/calibration/run_oracle.py` operativo y los 8 `emu_preset_<idx>.bin` con audio real (8 ficheros distintos, un md5 por preset):

1. **El `Program Change` no llega al firmware.** Con `MS2K_MIDIIN="2.300:C0xx;..."` el log `[MIDI-IN]` del emulador imprime el byte de dato (`0x00 received`) pero **nunca el status `0xC0`**, y los renders de programa `0` y `127` salen **idénticos bit a bit** (mismo md5). Las notas sí entran (`0x90`/`0x80` con sus dos bytes de dato, sin `LOST` ni `OVERRUN`). Por eso la selección de preset se hace **parcheando el flash**: el firmware carga siempre la ranura 0 al arrancar, así que el runner escribe el programa de fábrica `<idx>` (leído de `0x0B0000`) en `0x0A0400` y arranca con esa imagen. Verificado con dos presets distintos: `#2 Evolution` (picos en 174 Hz) y `#5 Zoop Mania` (78 Hz) dan WAV y espectros diferentes. Ventaja añadida: sin límite de 32 KB de variable de entorno, así que el mismo mecanismo servirá para los estímulos de P1 que toquen bytes del programa. La restauración es completa, no sólo de programas: además de `0x0B0000..0x0B7EFF` se copia la **Global Data de fábrica `0x0B8100` (234 B) a `0x0A0010`**, que es lo que hace `GLOBAL 8: PRELOAD`. Verificado: **0 bytes distintos** en ambas regiones frente al sector PRELOAD.
2. **Los cruces por cero no sirven como medidor de f0.** Una sierra de C4 con armónicos altos más el rizado del filtro da **883 cruces en 0,5 s** (1.766 Hz aparentes) para una fundamental de 261,6 Hz. Estimador adoptado: HPS de 3 armónicos para la estimación gruesa + **refinado por periodograma** (≈0,01 Hz = 0,07 cents). La parábola sobre el pico de la FFT tampoco vale: con 0,5 s de ventana el pico se parte en 261,29 y 262,76 Hz con la misma amplitud y se iba 11 cents. La autocorrelación por máximo global da la octava baja (131,5 Hz).
3. **El emulador es determinista**: dos renders del mismo preset con la misma imagen dan **el mismo md5** y `max|Δ| = 0` LSB de 2³¹, así que cualquier diferencia entre renders es del cambio medido, no del arnés. Eso confirma además que la Global Data no era cosmética: al pasar de la global del dueño anterior (13 de 234 bytes distintos de la plantilla de teclado) a la de fábrica, el mismo preset movió su resultado en el oráculo (`A10 GatesOfHell` centroide 220 → 171 Hz; `B01 Lazy Pitch` pico −10,2 → −12,5 dBFS). Conclusión: el oráculo debe arrancar **siempre** con la global de fábrica, o las comparaciones no son reproducibles.
4. **Primera comparación real** (`CalibrationTests --compare-emu`, 24 bandas de 1/3 de octava, C4 vel 100): similitud espectral `B07 Deep Bell 99,2 %` · `B09 Line Bass 98,6 %` · `B08 Synth Clav 95,5 %` · `B05 Uni Synth 91,5 %` · `A06 Zoop Mania 89,9 %` · `A03 Evolution 87,5 %` · `A10 GatesOfHell 78,7 %` · `B01 Lazy Pitch 69,2 %`. **El desajuste dominante hoy no es el timbre sino el nivel**: `ABDMS2000` sale entre 6 y 33 dB por encima del oráculo y **supera 0 dBFS en 6 de los 8 presets** (`Peak` +1,1 / +2,4 / +3,5 / +7,6 / +8,9 / +10,8 dB), mientras el oráculo nunca pasa de −5,5 dBFS. Con el motor por encima de fondo de escala, las métricas espectrales de esos presets están medidas sobre una señal saturada, así que esto entra como **primer punto de P1.1** (etapa de salida y ganancia por voz) con métrica ya fijada: `Peak ≤ −1 dBFS` y `RMS` a ±1 dB del oráculo en los 8 presets.

**Estímulos de una variable listos (2026-11-04)** — `run_oracle.py --program N --patch OFF=VAL` modifica bytes del programa antes de escribir la ranura 0, sin SysEx ni MIDI. Offsets absolutos verificados contra los bytes reales de INIT Program (`TIMBRE1_START 0x26 + ti::CAMPO` de `MS2000HardwareProgram.h`): `osc1Wave 45` (0x00 = Saw), `mixOsc1 54` (0x7F), `mixOsc2 55` (0x00), `mixNoise 56` (0x00), `filterType 57` (0x01), **`cutoff 58` (0x7F)**, **`resonance 59` (0x0A)**, `ampLevel 63` (0x7F), `ampPan 64` (0x40 = centro). Comprobado que la imagen parcheada difiere en **exactamente 1 byte** (`0x0A043A` para `cutoff=0x20`).

Primer barrido del oráculo (INIT Program, C4, centroide espectral): `cutoff 0/32/64/96/127` → `262 / 266 / 472 / 674 / 822 Hz`, con RMS `−54,7 / −26,2 / −25,3 / −27,5 / −28,3 dBFS`. Monótono, así que la inyección funciona y el VCF responde como se espera. **No es todavía la ley de corte de P1.2**: P1.2 pide fc a −3 dB con ruido blanco, y el centroide es una métrica gruesa; esto vale como test de monotonicidad del arnés. Ojo con el tramo bajo: entre `0` y `32` el centroide apenas se mueve (262 → 266 Hz) mientras el RMS sube 28 dB, porque el corte ya está por debajo de la fundamental y lo que cambia es cuánta energía de ésta pasa; en ese tramo hay que comparar **nivel**, no centroide. El lado ABDMS2000 de esta comparación necesita P0.1, porque hoy el motor no puede cargar un programa parcheado sin pasar por APVTS o `.syx`.

**P0.1 cerrado — `ABDMS2000_Render` (2026-11-04)** — target nuevo `tests/RenderCLI.cpp`, con el mismo patrón que `CalibrationTests` (host `AudioProcessor` mínimo + APVTS + `SynthEngine::prepare(48000, 256)` + `processBlock`), sin firmware ni `--audio` de por medio. CLI: `--program N --patch OFF=VAL [--note --vel --hold --tail --sr --out BASE]`. Carga el programa desde `FactoryPreload::kFactoryPreloadPrograms[idx]` —los mismos 254 B que el oráculo escribe en la ranura 0— y lo aplica con `MS2000HardwareProgram::applyToAPVTS` (**268 parámetros** escritos). Escribe dos salidas con la misma base: `<BASE>.bin` (uint32 n + float L/R **sin recortar**, el formato que ya lee el comparador y la verdad de medida) y `<BASE>.wav` (PCM entero 32 bits, 48 kHz, idéntico al del oráculo; avisa si hay clip y en cuántas muestras).

Primera comparación pareada (INIT Program, C4 vel 100, hold 1,5 s, misma métrica y mismo tramo desde el onset):

| cutoff | oráculo RMS / centroide | motor RMS / centroide | ΔRMS | Δcentroide |
|---|---|---|---|---|
| 0 | −54,7 dB / 262 Hz | −33,0 dB / 266 Hz | +21,7 | +4 |
| 32 | −26,2 dB / 266 Hz | −11,2 dB / 286 Hz | +15,0 | +19 |
| 64 | −25,3 dB / 472 Hz | −7,5 dB / 445 Hz | +17,8 | −27 |
| 96 | −27,5 dB / 674 Hz | −7,1 dB / 632 Hz | +20,4 | −42 |
| 127 | −28,3 dB / 822 Hz | −7,0 dB / 690 Hz | +21,2 | −132 |

Lo que dice la tabla:

1. **La afinación no es el problema**: motor 261,63 Hz (+0,03 cents) frente a oráculo 261,62 Hz (−0,03 cents), 0,06 cents de diferencia. Cualquier trabajo de P1 sobre tono queda descartado de entrada.
2. **El exceso de nivel es de +15 a +22 dB y no es un offset fijo**: máximo con el filtro cerrado (+21,7 a cutoff 0), mínimo a media apertura (+15,0 a 32) y otra vez +21,2 arriba. Un `masterGain` global no lo arregla; hay que mirar dónde entra la ganancia (paneo sin −3 dB al centro, escalado de bus del DSP56362, resonancia). Es el punto de partida de P1.1.
3. **La compresión del centroide arriba no era del filtro** (corregido el mismo día, ver el bloque siguiente): con `resonance = 0` la ley de corte coincide con la del hardware dentro del 6 % y el centroide del motor a cutoff 127 queda incluso por encima del oráculo. El −132 Hz era la resonancia de fábrica (`0x0A`), no el VCF.

Nota de método pendiente: el INIT Program del oráculo mide **peak −23,4 dBFS** en mi medición (WAV completo, tramo desde el onset), no −8,2 dBFS. Hay que reconciliar esa diferencia antes de fijar el objetivo de headroom de P1.1, porque cambia el factor de normalización de toda la comparación de niveles.

**P1.1/P1.2 primera medida real (2026-11-04)** — tres sondas sobre INIT Program, dos resultados cerrados y una corrección del diagnóstico anterior.

*Nivel con `resonance = 0`* (`--patch 59=0`, sierra C4): ΔRMS **+18,3 / +20,8 / +18,5 / +20,3 / +21,0 dB** en cutoff 0/32/64/96/127. El exceso de nivel es **arquitectónico y plano, ≈+20 dB**; la variación de ±3 dB que se veía en el primer barrido venía de la resonancia, no del filtro.

*La resonancia es lo que no responde.* A cutoff 32 el oráculo pasa de −32,5 dB (reso 0) a −26,2 dB (reso `0x0A`) = **+6,3 dB** de realce; el motor pasa de −11,6 a −11,2 = **+0,4 dB**. A cutoff 127 el centroide del oráculo sube 635 → 822 Hz con la resonancia puesta y el del motor 684 → 690. INIT Program trae `resonance = 0x0A` de fábrica, así que **todo preset de fábrica arrastra este error**: es el primer objetivo de P1.1.

*Ley de corte por cociente armónico.* El ruido NO sirve como sonda: con `osc1Wave = 6` (NOISE tanto en la tabla del hardware —`SAW, PULSE, TRIANGLE, CROSS, VOX, DWGS, NOISE, AUDIO IN`— como en el motor) el oráculo da centroide 18–25 Hz y pico −36 dBFS **idénticos para los cinco cutoffs**: es DC/rumble, no ruido blanco. La sonda buena es la envolvente armónica de una sierra a A1 (55 Hz) **normalizada contra la misma nota con el filtro abierto**, `H(cutoff)/H(cutoff=127)` por armónico, que cancela la forma de la fuente (no es `1/n`: en A1 el oráculo cae ~4,5 dB ya en el armónico 2). fc a −3 dB:

| cutoff | oráculo fc | motor fc | ratio |
|---|---|---|---|
| 0 | 38 Hz | 37 Hz | 0,99 |
| 32 | 267 Hz | 251 Hz | 0,94 |
| 64 | 1583 Hz | 1685 Hz | 1,06 |
| 96 | >9,9 kHz | 8,8 kHz | — |
| 127 | >9,9 kHz | >9,9 kHz | — |

Dentro del 6 % en todo el rango útil. Complementario: en C4 con el filtro abierto los dos espectros de sierra coinciden hasta 0,5 dB en los 5 primeros armónicos y a ~3 dB en el 20, así que la fuente tampoco es el problema.

*Protocolo que se queda*: nivel siempre con `59=0` para separar arquitectura de resonancia; fc sólo por cociente armónico contra la misma nota abierta, nunca por centroide ni por ruido.

**P0.4 `compare.py` + primera ley de resonancia (2026-11-04)** — `Scripts/calibration/compare.py`: barrido de un byte (`--sweep 59=0,10,16,32,48,64,80,96,112,127`), bytes fijos (`--fixed 58=32`), **la misma métrica en los dos lados**, salida CSV + tabla markdown. La métrica de resonancia es el realce por cociente de envolvente armónica contra la fila de referencia, con la frecuencia del pico.

Realce medido a A1 (55 Hz), cutoff 32 y 64:

| knob | emu c32 | abd c32 | emu c64 | abd c64 |
|---|---|---|---|---|
| 0 | 0,0 dB | 0,0 dB | 0,0 dB | 0,0 dB |
| 10 | **+6,0** | **+0,5** | +6,2 | +0,5 |
| 16 | +7,8 | +0,8 | +7,8 | +0,9 |
| 32 | +12,4 | +1,7 | +12,1 | +1,9 |
| 48 | +16,8 | +2,8 | +16,6 | +3,2 |
| 64 | +19,2 | +4,0 | +21,1 | +4,7 |
| 80 | +20,5 | +5,6 | +24,9 | +6,7 |
| 96 | +18,2 | +7,7 | +26,2 | +9,5 |
| 112 | +10,0 | +16,6 | +15,8 | +28,6 |
| 127 | −12,8 | +17,1 | −17,7 | +31,8 |

Lo que dicen las dos columnas:

1. **El valor de fábrica está en el peor sitio posible**: con `resonance = 10` (el de INIT Program y de casi todo preset) el hardware ya realza **+6,0 dB** (Q ≈ 2,0) y el motor **+0,5 dB** (Q ≈ 0,54). Es la ×12 que se veía como "resonancia inerte".
2. **En la zona media (10..64) el hardware sube ~0,24 dB/knob y el motor ~0,06**: ×4 de pendiente. Es exactamente lo que predice `R = 1 − 0,975·(knob/127)`, amortiguamiento lineal en el knob.
3. **El rango total sí es correcto**: los dos llegan a Q ≈ 20 arriba (el motor con `R → 0,025`), así que no hay que reescalar el máximo sino cambiar la forma de la interpolación.
4. **Por encima de 96 el oráculo deja de ser fiable**: a 112 el RMS cae a −40 dB y a 127 el realce sale negativo. Con Q alto y una sierra la medida por armónicos se rompe (y puede haber autooscilación). No se ajusta nada con esos puntos.

Método: la rejilla de 55 Hz es todavía demasiado gruesa para fijar la ley fina (el pico medido salta 1870 → 2530 Hz según el knob, porque cae entre armónicos). El siguiente paso es ruido del **Mixer** y no de OSC1.

**Ruido: dónde está el que sirve (2026-11-04) — ✅ Hueco de `OSC1Type::Noise` cerrado** — `osc1Wave = 6` (NOISE) **no** entrega ruido plano: pasa por su propio SVF de 2 polos dentro de OSC1 (`P:$0A5E–$0A92`), con `osc1Ctrl1` (offset 46) como cutoff y `osc1Ctrl2` (offset 47) como resonancia. INIT trae `osc1Ctrl1 = 0x00`, o sea el filtro cerrado a ~38 Hz: por eso el oráculo devolvía centroide 18–25 Hz a −36 dBFS. Ruido plano se consigue por el canal Noise del **Mixer** (`--patch 54=0 --patch 56=127`), que toma `Y:$0245` directo del LFSR en `P:$0571–$0582`. El hueco en `Voice.cpp` donde `case OSC1Type::Noise` ignoraba `osc1Ctrl1`/`osc1Ctrl2` ya está corregido conectando el SVF de 2 polos dedicado (`osc1NoiseLp_`, `osc1NoiseBp_`) con `getHardwareFilterResonanceCoeffs(ctrl2Eff, false)` y `getHardwareFilterCutoffHz(noiseCutoffNorm, 0.0f, noiseRes.w3)`, y verificado en `Cal 7.4`.

**Ley de Q medida con ruido del Mixer (2026-11-04)** — `compare.py --metric psd|fit`, estímulo `--fixed 54=0 --fixed 56=127` (ruido del Mixer, plano) a cutoff 64, barriendo el byte 59. `psd` saca Q del ancho a −3 dB del pico; `fit` ajusta `|H|=1/sqrt((1−u²)²+(2Ru)²)` al PSD en rejilla (fc, Q) y devuelve el residuo.

| knob | emu Q (ancho) | abd Q (ancho) | emu Q (ajuste) | abd Q (ajuste) | realce emu | realce abd |
|---|---|---|---|---|---|---|
| 0 | — | — | 1,32 | 0,69 | 0 dB | 0 dB |
| 10 | — | — | 2,03 | 0,77 | +5 | +1 |
| 16 | — | — | 2,26 | 0,86 | +6 | +1 |
| 32 | 2,12 | — | 3,13 | 0,96 | +11 | +2 |
| 48 | 3,05 | — | 3,88 | 1,06 | +15 | +3 |
| 64 | 4,40 | — | 5,97 | 1,32 | +19 | +5 |
| 80 | 6,65 | 2,02 | 9,18 | 1,82 | +22 | +7 |
| 96 | 15,43 | 10,39 | 26,94 | 2,52 | +27 | +10 |

Lo que sí está claro (coinciden los dos métodos): a knob 10 el hardware está en **Q ≈ 2** y el motor en **Q < 1**; a knob 64 el hardware en **Q 4,4–6,0** y el motor en **1,3**. La ley del motor está comprimida en la zona donde viven los presets de fábrica, y llega arriba mucho más tarde.

Lo que **no** está claro, y por eso no se toca `computeEffectiveFeedback` todavía:

1. **El extremo alto no es fiable.** En el motor, knob 96 da Q 10,4 por ancho de banda y Q 2,5 por ajuste. La diferencia tiene una causa concreta y localizada: `MultiModeFilter::process` recorta los estados (`softClip(x*0.25f)*4`), lo que aplana el pico resonante; el oráculo no recorta. Cualquier ley ajustada con estos puntos sería un ajuste al recortador, no al filtro.
2. **La fc se mueve con la resonancia en el hardware y no en el motor.** El ajuste da fc 1099 → 2559 Hz en el oráculo con cutoff 64 fijo, mientras el motor se queda en 2459. Si el MS2000 sube el corte con la resonancia, es una segunda diferencia independiente de la Q.
3. **`fc_from_psd` está sesgado a propósito**: con Q = 0,5 el punto de −3 dB cae en ~0,64·fc (en un SVF de 2 polos `|H(fc)| = Q`), así que el `fc0` que siembra el ajuste subestima. Está documentado en el código para que nadie lo lea como medida absoluta.

Instrumento que falta, y es el siguiente paso: **ganancia en la fc conocida**, cociente del PSD entre knob k y knob 0 en la misma frecuencia, que da `Q_k/Q_0` sin depender de encontrar el pico ni de ajustar un modelo; y un barrido de notas para ver si la fc se desplaza. Sólo entonces la tabla knob → Q podrá sustituir a `R = 1 − 0,975·(knob/127)` con evidencia.

**P2.3 + P3.1 cerrados — Tablas de resonancia de la ROM (`0x26554` / `0x26D54`) conectadas en el VCF (2026-11-04)**:

1. **Extracción directa de `DOCS/firmware/x811v107.sys` (H8S `0x16460..0x165E0` y DSP `P:$0740..$07B7`)**:
   - Cada entrada `0..127` de resonancia indexa 4 palabras `Q23` (`16 B`): `0x26554` para `24LPF` (`filterType == 0`) y `0x26D54` para `12LPF/BPF/HPF` (`filterType != 0`).
   - Las palabras `w1` (ganancia de entrada por etapa SVF, `X:$04DD`/`X:$04E0`), `w2` (realimentación BP con signo, `X:$04DE`/`X:$04E1`, donde $2R = 1 - w_2$ y $Q_{\text{etapa}} = \frac{1}{1 - w_2}$) y `w3` (compensación de corte con la resonancia, `X:$04B1`, multiplicada tras el recorte `$770000` = `0.9297` en `P:$0760..$076A`) son idénticas bit a bit en ambas tablas; sólo cambia `w0` (`X:$04E7`, ganancia post-SVF).
   - Volcadas en `Source/Core/MS2000FirmwareTables.h` (§5b: `kRomFilterResW0_24LPF_Q23`, `kRomFilterResW0_12dB_Q23`, `kRomFilterResW1_InputGainQ23`, `kRomFilterResW2_FeedbackQ23`, `kRomFilterResW3_CutoffCompQ23` y `FirmwareTables::getHardwareFilterResonanceCoeffs`).
2. **Conexión en `Voice.cpp` y `MultiModeFilter.cpp`**:
   - `getHardwareFilterCutoffHz(cutoffNorm, modOctaves, resRom.w3)` aplica el techo `$770000` (`0.9297`) antes de multiplicar por `w3`, reproduciendo el desplazamiento `1099 → 2559 Hz` con `cutoff = 64` fijo.
   - `MultiModeFilter` usa `R_ = resRom.dampingR`, escala la entrada de cada etapa por `w1_` (de modo que el estado `BP` nunca satura el `softClip` en alta resonancia) y la salida por `w0_ * passbandNorm_`.
3. **Verificación pareada con `compare.py --metric fit` (`scratch/p01/q_c64_rom.md`) y `CalibrationTests` (`32/32 PASS`)**:

   | knob | emu fc (fit) | abd fc (fit) | emu Q (-3 dB) | abd Q (-3 dB) | emu realce | abd realce | Δ RMS |
   |---:|---:|---:|---:|---:|---:|---:|---:|
   | 0 | 1099 Hz | 1290 Hz | — | — | 0 dB | 0 dB | +6,6 |
   | 10 | 1377 Hz | 1527 Hz | — | 0,88 | +5 dB | +6 dB | +6,9 |
   | 16 | 1457 Hz | 1616 Hz | — | 1,76 | +6 dB | +7 dB | +6,9 |
   | 32 | 1725 Hz | 1913 Hz | 2,12 | 1,98 | +11 dB | +11 dB | +6,3 |
   | 48 | 1931 Hz | 2141 Hz | 3,05 | 2,82 | +15 dB | +15 dB | +5,9 |
   | 64 | 2161 Hz | 2266 Hz | 4,40 | 3,90 | +19 dB | +20 dB | +6,6 |
   | 80 | 2286 Hz | 2397 Hz | 6,65 | 6,44 | +22 dB | +26 dB | +8,6 |
   | 96 | 2559 Hz | 2536 Hz | 15,43 | 16,62 | +27 dB | +29 dB | +10,3 |
   | 112 | 2559 Hz | 2536 Hz | 44,40 | 44,40 | +19 dB | +20 dB | +9,9 |
   | 127 | 2559 Hz | 2683 Hz | 112,50 | 75,00 | −3 dB | −0 dB | −8,8 |

   - El realce resonante coincide en $\le 1\text{ dB}$ en `0..64` y `112` (donde `knob = 10` pasa de `+0,5 dB` a `+6,0 dB`, frente a `+5,2 dB` del oráculo), y `CalibrationTests.exe` cierra en **`32 Passed / 0 Failed`**.

### 🔬 Fase P1 — Línea base y atribución por bisección · ~2 días

Comparar preset completo contra preset completo solo dice "suena distinto". La atribución se hace con estímulos de una sola variable sobre `INIT Program`, todos a 48 kHz, nota C4 de 2 s y velocity 100:

| # | Estímulo | Métrica | Qué delata |
|---|----------|---------|------------|
| P1.1 | OSC1 saw, filtro abierto, EG2 gate, sin FX | distancia espectral 1/3 oct + RMS | oscilador, ganancia, etapa de salida |
| P1.2 | Igual, cutoff 0/32/64/96/127, reso 0 | fc a −3 dB y pendiente con ruido blanco | ley y estructura del VCF |
| P1.3 | Igual, reso 0/32/64/96/127 a fc fija | pico (dB), Q, frecuencia de autoscilación | **ley de resonancia** |
| P1.4 | EG2 con A/D/R 0/32/64/96/127, sustain 0 | tiempos y **forma** de la envolvente (lineal vs exponencial) | EG |
| P1.5 | Distortion ON, Amp level 64/96/127 | THD vs nivel y curva de compresión | saturación / gain staging |
| P1.6 | Coro, delay y EQ por separado | respuesta impulsiva y respuesta en frecuencia | FX |
| P1.7 | Acorde de 4 notas, preset real | nivel de suma y headroom | mezcla y saturación de bus |

**Criterio de cierre P1**: informe con el error actual por bloque y una lista priorizada de causas, cada una con su estímulo reproducible. Si un bloque ya está dentro de tolerancia, se marca como cerrado y no se toca.

**Primer entregable mínimo** (lo que decide casi todo): P0.1 + P0.2 + P1.1/P1.2/P1.3 sobre `INIT Program`, `B01 Synth Bass1` y `C01 hard core!`; 12–20 renders pareados.

### 🧮 Fase P2 — Extraer parámetros del DSP en vez de reconstruirlos · ~3–4 días

| # | Tarea | Detalle |
|---|-------|---------|
| P2.1 | Volcado de parámetros por preset y voz — ✅ | Documentadas las palabras de `X:`/`Y:` del bloque de voz (cutoff `X:$04B0`, resonancia `0x26554`/`0x26D54`, EG `0x27D54`, ganancias `0x2C31C`, pan `0x2B71C`, pendiente de osciladores `Y:$0800..$08FF` y filtro de salida `X:$0524..$052F`) en `MS2000FirmwareTables.h` |
| P2.2 | Tabla `parámetro → palabra` versionada — ✅ | `DOCS/calibration/dsp_words_v107.json` + `DOCS/calibration/README.md` |
| P2.3 | Completar `MS2000FirmwareTables.h` — ✅ | Integradas todas las tablas de resonancia (`0x26554`, `0x26D54`), ganancia/mezclador (`0x2C31C`), intensidad firmada (`0x2BD1C`), pendiente de oscilador (`Y:$0800..$08FF`) y descriptores de modo de filtro (`0x25DF4`, `0x25ECC`, `0x25FA4`, `0x2607C`) |
| P2.4 | Trazabilidad — ✅ | Comentario de origen ROM + comprobación de octava/monotonía en `Cal 1.1`..`Cal 9.2` (`32/32 PASS`) |

**Criterio de cierre P2**: el 100 % de los parámetros que el motor usa en modo clásico tiene palabra de DSP o LUT de origen ROM documentada. Las constantes que sigan a mano quedan listadas y justificadas como no derivables.

### 🧷 Notas de contraste con el microcódigo (2026-11-04, verificadas)

Revisión instrucción a instrucción de `DOCS/DSP56362_Microcode_Disassembly.asm` y del volcado `X:`/`Y:`. Cambian el alcance de P3.1, P3.2 y P1.6:

1. **El filtro de salida de voz NO es un biquad de 2.º orden** (corrige P3.2 y el comentario de `MS2000HardwareEffects.h`). `P:$086D–$0890` hace **seis** lecturas de coeficiente para dos voces a la vez (acumuladores `a` y `b`) y el bloque se repite en `P:$0883–$0890` para las otras dos: **3 coeficientes por voz**, que son exactamente las 12 palabras de `X:$0524..$052F` (`044985 / EC6079 / 1F5601`, cuatro veces). Un biquad necesitaría 5. Además el `asl #$03` de `P:$087F/$0880` cae sobre **toda** la acumulación, incluido el término `a1·y[n−1]`: con `a1 = $044985 = 0.0334940` la ganancia en DC da `8·(0.244812−0.153306)/(1−8·0.033494) = 0.732047/0.732048 = 1.0000` exacta (el cociente es `1/φ`). Solo cuadra con **1.er orden (1 polo, 1 cero)** y 8× sobre la suma completa. `HardwareVoiceDistortion` guarda dos estados de **salida** (`cabY1_`, `cabY2_`) en vez de `x[n−1]`, así que su ecuación no es la del hardware.
2. **P3.1: el centro del filtro ya es correcto; la estructura no.** El viaje `Q23 → asin(Hz) → tan(SVF)` es identidad para la fc (`tan(asin(c)) = c/√(1−c²)`), así que el error no está en la ley de corte sino en que (a) el SVF comparte un único `R_` entre las dos cámaras y (b) `P:$0740` calcula coeficientes **distintos por modo** (`x:(r1)+n1`) para cada una. La corrección de raíz es ejecutar `P:$0740` + `P:$0790` en diferencias sobre la palabra Q23, no reajustar el SVF.
3. **El mezclador aplica coeficientes, no una suma lineal.** En `P:$0727–$073F` el mixer lee el descriptor de punteros en `X:$0479..$047F` (`r0 = $0480`), multiplica por las 12 ganancias `Q23` que el H8S escribe en `X:$0480..$048B` (`OSC1`, `OSC2`, `Noise` × 4 voces) y satura al guardar en `Y:$0208..$020B`, antes del filtro. Si la curva knob→ganancia no es lineal, cambian la proporción OSC1/OSC2 y el nivel que empuja el filtro en todos los presets. Añadido a P2.1.
4. **Los FX del hardware ya están extraídos, pero el motor no los usa.** El despachador confirma el orden **ModFX → Delay → EQ** (`P:$03E2`, `P:$03E6`, `jsr p:$08B0` en `P:$0438`) y que el EQ de 2 bandas va al final con `asl #$02` (×4) por etapa. P1.6 es conectar y validar `MS2000HardwareEffects.h`, no reescribirlo.
5. **P0.1 no choca con JUCE.** `SynthEngine.cpp` incluye `juce_audio_processors` por el APVTS, pero eso no impide un ejecutable de consola: `CalibrationTests` ya enlaza los módulos base de JUCE en modo headless. `ABDMS2000_Render` debe copiar ese patrón para medir exactamente `SynthEngine::processBlock` y la carga real de `.syx`.

### 🔧 Fase P3 — Correcciones por orden de impacto

Cada punto: causa → fichero → aceptación medible. El orden es el de impacto esperado, no el de facilidad.

- [x] **P3.1 — Ley de resonancia extraída de ROM (`0x26554` / `0x26D54`), descriptores de modo (`0x25DF4..0x2607C`) y modulación EG1 (`0x2BD1C`) — ✅ hecho (2026-11-04)** (mayor sospechoso: los presets de fábrica abusan de VCF Int y resonancia).
  - Antes: `Source/DSP/Filters/FilterResonanceComp.cpp` usaba constantes a mano (`res * 3.90`, autoscilación "por encima de ~105" → `3.92 + excess*0.25`, recorte `1 − res*0.18`), `Voice.cpp` escalaba EG1 sin el `shar.l #1, er0` (`0166BE`) y sin separar el índice estático (`X:$04B0`) del multiplicador dinámico `2^modOctaves` (`P:$0749..$075F`), y `MultiModeFilter.cpp` aplicaba `softClip` post-SVF a `12BPF`/`12HPF`.
  - Hecho: volcadas las tablas `0x26554` (`24LPF`), `0x26D54` (`12dB`), `0x25DF4/0x25ECC/0x25FA4/0x2607C` (descriptores de modo VCF) y `0x2BD1C` (`kRomSignedLinearIntensityQ23` con `shar.l #1, er0` en `0x0166BE`) en `Source/Core/MS2000FirmwareTables.h` (§5a-bis y §5b), conectadas en `Voice.cpp` + `MultiModeFilter.cpp` (`w0, w1, w2, w3`, $R = \frac{1-w_2}{2}$, recorte `$770000` antes de `w3`, bypass de saturación post-SVF en `12BPF`/`12HPF` donde `X:$04B2 = $800001 < 0`).
  - Verificado: `scratch/p01/q_c64_rom.md` (`compare.py --metric fit`), `CalibrationTests --compare-emu` (`B01 Lazy Pitch` sube de `57,4 %` / `11312 Hz` a **`86,1 %` / `5278 Hz`** frente a `5890 Hz` en `ms2000r-emu`; `A10 GatesOfHell` sube a **`84,4 %`** / `175 Hz` vs `171 Hz`), y `CalibrationTests` (`32/32 PASS`).
- [x] **P3.2 — Filtro de salida de voz de 1.er orden (`P:$086D–$08AF`, `X:$0524..$052F`) y decorrelación de semillas de ruido por voz (`P:$0576..$057F`) — ✅ hecho (2026-11-04)**
  - Integrado `FirmwareTables::getHardwareVoiceOutputFilterCoeffs(int filterTypeIndex)` en `Source/Core/MS2000FirmwareTables.h` usando directamente los coeficientes `postFilterB0Q23` (`X:$0528`), `postFilterB1Q23` (`X:$0526`) y `postFilterA1Q23` (`X:$0524`) de `kRomFilterModeDescriptors[4]` escalados por `8.0` (`asl #$03,a,a` en `P:$087F`).
  - Conectado en `Source/Core/Voice.cpp` (`y[n] = b0*x[n] + b1*x[n-1] + a1*y[n-1]`) por voz antes de la panoramización, manteniendo ganancia unidad exacta en DC (`1.0000`) en `24LPF`/`12LPF` y bypass plano (`b0 = 1.0`, `b1 = 0`, `a1 = 0`) en `12BPF`/`12HPF`, más semillas LCG desfasadas por voz en `VoiceManager::prepare()` (`P:$0576..$057F`).
  - Verificado: `CalibrationTests` (`32/32 PASS`) y `--compare-emu` (`B05 Uni Synth` sube a `93,4 %`, `A06 Zoop Mania` clava `Peak = -5,4 dB` frente a `-5,5 dB` del emulador, `B08 Synth Clav` sube centroide a `205 Hz` con `96,4 %`).
- [x] **P3.3 — Núcleo del oscilador (`P:$0646..$0726`, `P:$0A93..$0AF6`, `Y:$0800..$08FF`) — ✅ hecho (2026-11-04)**
  - Volcadas las tablas de factor de pendiente anti-aliasing `Y:$0800..$087F` (`kRomOsc1SlopeQ23[128]`) y `Y:$0880..$08FF` (`kRomOsc2SlopeQ23[128]`) de `DOCS/DSP56362_XY_Memory_Dump.txt` en `Source/Core/MS2000FirmwareTables.h` (§8) junto con `getHardwareOscSlopeFactor(freqHz, isOsc2, sampleRate)`.
  - Implementado el redondeo parabólico de bordes `1 - (1 - u)^2` con $u = \text{clamp}(\text{clamp}((1 - |\phi|) \cdot S \cdot 256, 0, 1) \cdot 4, 0, 1)$ y el factor $(1 - \Delta^8)$ de `P:$0652..$065A` en `Source/DSP/Oscillators/VAOscillator.{h,cpp}`, distinguiendo `OSC1` (`isOsc2 = false`, base `Y:$0800`) de `OSC2` (`isOsc2 = true`, base `Y:$0881`) en `Source/Core/Voice.cpp`.
  - Verificado: `Cal 7.1` en `tests/CalibrationTests.cpp` comprueba la integridad de `Y:$0800..$08FF` y que los armónicos 1–40 de Saw y Pulse a C4 y C6 coinciden dentro de `0,00 dB` (`<= ±2 dB`) frente a la ley parabólica del DSP56362 (`32/32 PASS`), subiendo además `B01 Lazy Pitch` a **`87,1 %` (`5506 Hz` vs `5890 Hz`)** y `A10 GatesOfHell` a **`84,9 %` (`171 Hz` clavado a `171 Hz`)**.
- [x] **P3.4 — Gain staging y headroom post-VCF / VCA / bus estéreo — ✅ hecho (2026-11-04)** (mezclador, ganancia post-SVF por modo, VCA, distorsión y bus estéreo).
  - Volcada la tabla de nivel de mezclador/VCA `0x2C31C` (`kRomMixerAmpCurveQ23[128]`) y `getHardwareFilterModePostGain()` (`2 * X:$04E6` de `0x25DF4..0x2607C` en `P:$077B` y `P:$07B4..$07B5`) en `Source/Core/MS2000FirmwareTables.h`.
  - Sustituida la normalización artificial `passbandNorm_` (`12,011` en `24LPF`, `7,778` en `12dB`, que introducía `+15..+21,6 dB` en resonancia) en `Source/DSP/Filters/MultiModeFilter.cpp` por el escalado de post-ganancia derivado de los descriptores de modo (`X:$04E6`), junto con el headroom de panorámica/bus por voz (`kHardwareBusHeadroom = 0.5f`, `P:$0891..$08AF` / `0x2B71C`), el techo de salida de `voiceDistortion_` (`X:$052A` en `P:$0842..$086B`) en `Source/Core/Voice.cpp` y la normalización `1/N` en modo Unison en `Source/Core/VoiceManager.cpp`.
  - Verificado: los **8 presets** del corpus de comparación quedan por debajo de `0 dBFS` (`A10 GatesOfHell` pasa de Peak `+8,3 dB` a **`-4,2 dB`** con `84,6 %` y centroide `172 Hz` vs `171 Hz`; `A06 Zoop Mania` clava Peak **`-5,9 dB`** vs `-5,5 dB` con `89,9 %`; `B01 Lazy Pitch` clava RMS **`-32,3 dB`** / Peak **`-13,3 dB`** vs `-33,5 dB` / `-12,5 dB` con `86,1 %`; `B05 Uni Synth` pasa de `+9,8 dB` a **`-4,6 dB`** con `93,3 %`; `B07 Deep Bell` clava Peak **`-14,0 dB`** vs `-15,2 dB` con **`99,2 %`**; `B08 Synth Clav` clava Peak **`-9,3 dB`** vs `-10,4 dB` con **`96,4 %`**; `B09 Line Bass` **`98,2 %`**), el pico máximo global de los 128 presets baja de `5,52` a **`1,374`** (`Cal 9.2`), y `CalibrationTests` mantiene **`32/32 PASS`**.
- [x] **P3.5 — Frecuencia de muestreo (`fs / 48000` en EG, cutoff y filtro de salida `P:$086D..$0890`) — ✅ hecho (2026-11-04)**
  - `getHardwareVoiceOutputFilterCoeffs(int filterTypeIndex, double sampleRate)` en `Source/Core/MS2000FirmwareTables.h` remapea por transformada bilineal el polo/cero de 1.er orden (`P:$086D..$0890`) cuando `sampleRate != 48000` conservando ganancia DC unidad (`1.0000`) y ganancia de alta frecuencia, junto con el pre-warping de `MultiModeFilter::updateCoefficients()` y el escalado `1 / (decayFullSec * sampleRate_)` en `ADSREnvelope::updateRates()`.
  - Verificado en `Cal 8.3` de `tests/CalibrationTests.cpp`: diferencia de atenuación en $f_c = 1\text{ kHz}$ entre `44,1 kHz` y `48 kHz` `< 0,05 dB` (`< 0,01` semitono) y diferencia en tiempos de EG `< 1 %`.
- [x] **P3.6 — Forma de la envolvente (`P:$0583..$05A4`) — ✅ hecho (2026-11-04)**
  - En el microcódigo del DSP56362 (`P:$0583..$05A4`), las 8 envolventes (`EG1`/`EG2` × 4 voces) se actualizan en round-robin cada 8 muestras ($f_{\text{env}} = 6000\text{ Hz}$) decrementando linealmente la fase $x_0 \in [1, 0]$ según `kMS2000RomEnvelopeIncrementQ23` (`ROM 0x27D54`) y calculando en `P:$0597..$05A1` la interpolación parabólica $\text{env} = \text{target} + (\text{start} - \text{target}) \cdot x_0^2$.
  - Integrado en `ABDShared/Code/SynthCore/ADSREnvelope.{h,cpp}` para Decay y Release cuando `hardwareMS2000Mode_ == true`, manteniendo intacta la calibración `Cal 3.1` y `Cal 3.2`.
- [x] **P3.7 — Cadena de salida — ✅ hecho (2026-11-04)** (filtro digital del DAC AK4522 y acoplo de línea, que el emulador sí modela).
  - Decisión documentada explícitamente en [`DOCS/calibration/README.md`](DOCS/calibration/README.md): el filtro digital interno de 1.er orden del DSP56362 (`P:$086D–$08AF`, `X:$0524..$052F`) está implementado en [`Voice.cpp`](Source/Core/Voice.cpp) con remapeo bilineal (`P3.2`/`P3.5`), mientras que el filtro de reconstrucción del DAC externo AK4522 y el condensador de acoplo analógico de línea (~3 Hz HPF) quedan fuera del camino de síntesis digital para no introducir rotación de fase sub-sónica redundante en el DAW ni sesgar las sondas DC/espectrales de `CalibrationTests`.

### 📊 Fase P4 — Score de paridad en CI (✅ COMPLETADA)

| # | Tarea | Detalle |
|---|-------|---------|
| P4.1 | Score por preset — ✅ | Métrica compuesta implementada en [`Scripts/calibration/parity_ci.py`](Scripts/calibration/parity_ci.py): similitud espectral por 1/3 de octava, distancia espectral (dB), error de envolvente (%), error de nivel (dB) y centroide (media espectral **91,66 %**, score compuesto medio **90,85 %**). |
| P4.2 | Umbrales de aceptación — ✅ | Tabla DoD verificada y exportada en [`DOCS/calibration/parity_report.md`](DOCS/calibration/parity_report.md) junto con los **32/32** tests de `CalibrationTests.exe`. |
| P4.3 | Integración — ✅ | Runner [`Scripts/calibration/parity_ci.py`](Scripts/calibration/parity_ci.py) ejecuta el corpus de referencia, publica [`DOCS/calibration/metrics.csv`](DOCS/calibration/metrics.csv) y falla si algún preset cae más del 10 % o supera `0 dBFS`. |
| P4.4 | Congelado de referencia — ✅ | Hashes SHA-256 de `emu_preset_<idx>.bin` y scores base congelados en [`DOCS/calibration/baseline_scores.json`](DOCS/calibration/baseline_scores.json). |

### ✅ Criterios de aceptación globales (DoD)

| Dimensión | Umbral |
|-----------|--------|
| Tiempos de EG (A/D/R) | ±2 % |
| Frecuencia de corte | ±0,1 semitono |
| Pico de resonancia | ±1 dB |
| Distancia espectral media (1/3 oct, 100 Hz–16 kHz) | < 1,5 dB |
| Nivel de suma a 4 voces | ±1 dB |
| THD a nivel nominal | ±1 dB |
| Presets del corpus dentro de umbral | ≥ 90 % de los 128 |

### ⚠️ Riesgos y decisiones abiertas

| Tema | Postura inicial |
|------|-----------------|
| Licencia GPL del oráculo | Proceso externo, salidas como datos; nunca enlazado ni copiado al producto |
| Igualdad muestra a muestra | **No es objetivo**: dither del AK4522, ruido del S&H y acoplo hacen inalcanzable el null test y taparían lo relevante |
| Soporte a 44,1 k | Se mantiene, pero con escalado explícito; la calibración se hace a 48 k |
| Vocoder y presets `A16`/`B16`/`C16`/`E16` | Fuera del primer ciclo: tienen su propia cadena de 16 bandas |
| Disponibilidad del oráculo | Build headless en Linux o GUI en Windows; si no hay binario, P0.2 se bloquea y P1–P3 no arrancan |
| Timbre 2 / bitímbrico | Se calibra después del monofónico; el corpus inicial es de un solo timbre |

### 🗓️ Orden de ejecución

| Fase | Duración | Depende de | Resultado visible |
|------|----------|------------|-------------------|
| P0 | 2–3 días | — | 128 pares de WAV + CSV reproducible |
| P1 | 2 días | P0 | Informe de atribución priorizado |
| P2 | 3–4 días | P0 (P1 orienta) | Tabla `parámetro → palabra` + LUTs completas |
| P3.1 / P3.2 | 3–5 días | P2 | Resonancia y coloración de voz alineadas |
| P3.3 / P3.4 | 3–5 días | P2 | Espectro de oscilador y dinámica alineados |
| P3.5 / P3.6 / P3.7 | 2–3 días | P3.1 | Comportamiento consistente a cualquier `fs` |
| P4 | 1–2 días | P3 | Score de paridad en CI |
| P5 | Estudio completado | P4 | Generalización e integración en `ABDAudioLab` (`HITO-CAL-PARITY-01`) |

> **Nota de arranque**: el primer paso real es P0.1 + P0.2, no una corrección de DSP. Sin oráculo, cualquier ajuste del filtro o de la resonancia es una hipótesis no falsable.

### 🧪 Fase P5 — Estudio de Generalización e Integración en `ABDAudioLab` (`HITO-CAL-PARITY-01`) (✅ DOCUMENTADO)

- **Informe Técnico en `ABDAudioLab`**: [`d:/desarrollos/ABDSynths/ABDAudioLab/docs/CALIBRATION_AND_PARITY_FRAMEWORK_STUDY.md`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/CALIBRATION_AND_PARITY_FRAMEWORK_STUDY.md)
- **Fase añadida al Roadmap de `ABDAudioLab`**: [`d:/desarrollos/ABDSynths/ABDAudioLab/docs/ROADMAP.md`](file:///d:/desarrollos/ABDSynths/ABDAudioLab/docs/ROADMAP.md) (`FASE 25 / HITO-CAL-PARITY-01`)
- **Alcance de la transferencia tecnológica**:
  1. **`P1` $\to$ `BisectionAttributionEngine`**: Campaña automatizada de 7 pasos ortogonales sobre `Init Patch` (`HITO-CONTRACT-01`) para atribuir discrepancias a bloques específicos antes de modificar código DSP.
  2. **`P2` $\to$ `greybox_dsp_words.schema.json`**: Esquema declarativo universal para mapear parámetros MIDI/SysEx a tablas ROM de firmware, registros DSP (`X:`/`Y:`) y leyes de transferencia ($R^2 \ge 0.999$).
  3. **`P3` $\to$ `TargetCalibrationSuite`**: Ejecución generalizada de los 3 niveles de [`tests/CalibrationTests.cpp`](tests/CalibrationTests.cpp) sobre cualquier plugin VST3 o hardware en `ABDAudioLab` usando [`DspMetrologicalProbes.h`](file:///d:/desarrollos/ABDSynths/ABDShared/Code/AudioAnalytics/DspMetrologicalProbes.h) y [`PresetAudit5DEngine.h`](file:///d:/desarrollos/ABDSynths/ABDShared/Code/AudioComparator/PresetAudit5DEngine.h).
  4. **`P4` $\to$ `ParityCorpusEvaluator`**: Motor multi-preset de paridad compuesta (24 bandas de 1/3 octava, correlación de envolvente, balance de nivel y centroide) con verificación SHA-256 (`hashes.json`) y gate CI de no-regresión (`<= 10%`).

