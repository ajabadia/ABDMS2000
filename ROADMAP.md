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

- [ ] **UX4.1 — Conectar Métodos DSP C++ Ya Escritos (Vocoder Pan 16 Bandas, HPF Gate y Arp Key Sync)**:
  - Registrar en `parameters-spec.json` (`npm run gen:registry`) y conectar en `SynthEngine.cpp` / `WasmBridge.cpp`:
    - **`vocoderBandPan1`..`vocoderBandPan16`**: Conectar a `vocoder_.setBandPan(b, pan)` (`Source/DSP/Vocoder/MS2KVocoder16Band.h:47`) y añadir el modo/fila de **Panorama Estéreo de 16 Bandas (`L63..CNT..R63`)** con macros de espacialización (`Center`, `Alternating L/R`, `Low-Center / High-Wide`) en `renderVocoderDrawer`, junto con las etiquetas de frecuencia reales (`75 Hz` a `5000 Hz`).
    - **`vocoderHpfGate`** y **`vocoderHpfThreshold`**: Conectar a `vocoder_.setHPFGateEnabled(bool)` y desacoplar el umbral de gate respecto a `vocoderThreshold` en `SynthEngine.cpp:427-428`.
    - **`arpKeySync`**: Conectar a `arpeggiator_.setKeySync(bool)` (`Source/DSP/Arpeggiator.h:59`) y exponer el toggle en `renderArpDrawer`.
- [ ] **UX4.2 — Corrección de Offsets SysEx del Bloque Vocoder (`MS2000HardwareProgram.h`)**:
  - Corregir los desplazamientos de bytes `+14..+45` en `MS2000HardwareProgram::vocoderFields()` (`Source/State/MS2000HardwareProgram.h:501-569`) para alinear exactamente niveles (`+22..+37`) y panoramas (`+38..+53`) con la especificación oficial Korg MS2000 MIDI Implementation y añadir test de round-trip SysEx en `DSPCoreTests.cpp`.
- [ ] **UX4.3 — Paridad de Parámetros por Timbre Restantes del Mapa SysEx MS2000 (Categoría D)**:
  - Incorporar al contrato de parámetros, motor DSP y sus respectivos Drawers (`Voice/Pitch`, `Osc2`, `Amp`, `Envelopes`, `Delay`, `Arp`):
    - **Pitch & Voice por Timbre (Bytes `+2..+7`)**: `Timbre Transpose` (`-24..+24 st`), `Timbre Tune` (`-50..+50 cents`), `Bend Range` (`-12..+12 st`), `Vibrato Intensity` (`-63..+63`), `Trigger Mode` (`Single` / `Multi` retrigger de envolventes en Mono/Unison) y `Key Priority` (`Last` / `Low` / `High`).
    - **Osciladores, Amp y Envolventes (Bytes `+12`, `+19`, `+25`)**: Modo combinado `OSC2 Ring + Sync` (`osc2Mod = 3`), selector `Amp Switch` (`EG2` vs `Gate` órgano) y `EG1 / EG2 Reset` (reinicio a cero vs ataque desde nivel actual).
    - **Delay Tempo Sync y Arpegiador (Bytes `+10`, `+12`, `+245`)**: `Delay Tempo Sync` + `Delay Sync Ratio` (`1/32` a `1/1`), `Arp Swing` (`50%..75%`) y `Arp Target Timbre` (`Both` / `Timbre 1` / `Timbre 2`).