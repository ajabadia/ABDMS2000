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
| DSP.6 | ✅ Migrar parámetros Voice.cpp a `normalizedToDomain()` | ✅ Done | Migración de `DSPUtils::convertSysExToCutoffHz` a `conversion::normalizedToDomain("filterCutoff", ...)` en Voice.cpp | 1 parámetro critical migrado (filterCutoffNorm) |
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