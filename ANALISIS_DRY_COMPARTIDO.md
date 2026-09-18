# Análisis DRY Transversal — ABDMS2000 ↔ ABDSharedCode ↔ ABDSharedAssets

> **Fecha:** 2026-09-08
> **Objetivo:** Identificar qué partes del sintetizador (ABDMS2000) pueden trasladarse a los repositorios compartidos para hacerlas consumibles por otros proyectos, y qué partes ya desarrolladas en los repos compartidos pueden adoptarse en el synth, evitando duplicidades. Filosofía: **una sola fuente de la verdad (zero-copy) para todo el ecosistema ABDSynths**.
> **Alcance:** Solo análisis. No se ha modificado código.

---

## 1. Resumen ejecutivo

1. **Existen 3 duplicaciones exactas de código** entre el synth y `ABDSharedCode` (2 codecs C++ idénticos salvo namespace, y `utils.js` idéntico byte a byte), más una **divergencia activa** en `keyboard.js`/`keyboard.css` que ya ha producido forks de comportamiento (las ruedas pitch/mod).
2. **La infraestructura para el modelo "paquete npm" ya existe y funciona en desarrollo**: hay un workspace pnpm (`pnpm-workspace.yaml`) y `node_modules/@abdsynths/{shared,midi-keyb}` son enlaces al repo compartido. El synth ya importa `@abdsynths/shared` con éxito **en el dev server Vite**.
3. **El único bloqueo real del modelo npm es el build nativo**: `juce_add_binary_data` embebe los fuentes crudos de `WebUI/src/*` y el WebView2 nativo no resuelve bare imports. La solución alineada con la preferencia declarada (Vite) es **empaquetar la WebUI con Vite en `build_webui.js` y embeber el `dist/`**, no los fuentes.
4. Hay módulos compartidos ya construidos que el synth **enlaza pero no usa** (`AutoUpdater`) o que le encajan directamente (`HardwareMidiDetect`, `AudioComparator`).
5. Hay código del synth con valor transversal claro (`VoiceManager`, `AudioThreadSnapshot`, primitivas DSP, controles JS genéricos) que puede extraerse a nuevos módulos compartidos (`SynthCore`, componentes `@abdsynths/shared`).

---

## 2. Mapa del ecosistema — cómo se consume hoy

### 2.1 Monorepo y workspace

`D:\desarrollos\ABDSynths\` es un **workspace pnpm** real:

```yaml
# pnpm-workspace.yaml
packages:
  - ABDSharedAssets        # @abdsynths/shared
  - ABDSharedCode/MidiKeyboard  # @abdsynths/midi-keyb
  - ABDMS2000
  - ABDCZ101
  - ABDEep
```

En `ABDMS2000/node_modules/@abdsynths/` hay enlaces simbólicos verificados:
- `shared  -> ../../ABDSharedAssets`
- `midi-keyb -> ../../ABDSharedCode/MidiKeyboard`

Y `ABDMS2000/package.json` ya declara ambas dependencias como `workspace:*`.

### 2.2 Matriz de consumo por proyecto

| Mecanismo | Dónde se usa | Estado |
|---|---|---|
| **CMake `add_subdirectory` local + FetchContent GitHub** | ABDMS2000 ← `ABDSharedCode` (AutoUpdater), ← `ABDBankManager/cpp`, ← `ABDScope` | Funciona. AutoUpdater enlazado pero **sin uso real** en `Source/` |
| **pnpm workspace (bare imports `@abdsynths/*`)** | ABDMS2000 WebUI: `shared-cascade.css` importa `@abdsynths/shared/styles/tokens.css` | Funciona **solo en Vite dev** (puerto 8384). No en build nativo |
| **Copias sincronizadas por script** | `sync_bankmanager.js` (ABDBankManager→`WebUI/abdbank`), `sync_scope.js` (ABDScope→`WebUI/abdscope`), `sync_assets.js` (models/brands→`WebUI/images`) | Automatizadas en `start.bat`/`build.bat`. Anti-fork solo si nadie edita el destino |
| **Copia manual versionada** | `WebUI/src/components/keyboard.{js,css}`, `utils.js` ← MidiKeyboard | **Manual y ya divergida** (ver §3). Los .bat llevan un aviso explícito de que hay que hacerlo a mano |
| **Junctions NTFS** | `bankwebui://` sirve imágenes desde junction `ABDSharedAssets/abdbank` | Funciona en runtime nativo |
| **Sin consumo compartido** | ABDEep, ABDCZ101, ABDScope WebUI, ABDBankManager WebUI: **cero referencias** a `@abdsynths/*` en sus fuentes | Los proyectos hermanos aún no consumen el paquete JS compartido |

> **Conclusión del mapa:** el modelo "paquete npm descargable / single source" ya está medio construido. Lo que falta es (a) que el build nativo lo soporte vía bundling, (b) migrar las copias manuales al mismo mecanismo, y (c) que el resto de proyectos adopten los imports.

---

## 3. Descubrimientos — duplicidades exactas (con evidencia)

Verificado con `diff` byte a byte y `diff -w` (ignorando espacios/CR):

| # | Fichero del synth | Gemelo compartido | Resultado del diff |
|---|---|---|---|
| 1 | `Source/MIDI/SysExCodec.{h,cpp}` | `ABDSharedCode/HardwareDrivers/SysExCodec.{h,cpp}` | **Idénticos** salvo namespace (`ABDMS2000` vs `abd::hw`) |
| 2 | `Source/MIDI/NRPNParser.{h,cpp}` | `ABDSharedCode/HardwareDrivers/NRPNParser.{h,cpp}` | **Idénticos** salvo namespace |
| 3 | `WebUI/src/components/utils.js` | `ABDSharedCode/MidiKeyboard/src/utils.js` | **Idénticos** byte a byte |
| 4 | `WebUI/src/components/keyboard.js` | `MidiKeyboard/src/keyboard.js` | ~60 líneas de diferencia: la copia del synth **re-implementa las ruedas inline**; la compartida delega en `createWheel` de `@abdsynths/shared` (fuente única) |
| 5 | `WebUI/src/components/keyboard.css` | `MidiKeyboard/src/keyboard.css` | Divergencia grande (~1.400 líneas): la compartida importa `wheels.css`/`kbd-buttons.css` de ABDSharedAssets; la del synth duplica esas reglas localmente |
| 6 | Tokens de `WebUI/src/styles/themes.css` | `ABDSharedAssets/styles/tokens.css` + `themes/ms2000.css` | Mismos valores duplicados; la cascada compartida solo actúa en dev Vite |

**Impacto del fork ya existente (caso 4):** la versión compartida del teclado usa la rueda filmstrip compartida (con `destroy()` correcto); la copia local mantiene la implementación antigua embebida. Cualquier mejora futura de ruedas en `ABDSharedAssets/components/wheel.js` **no llegará** al MS2000 compilado nativo.

**Otros solapes no duplicados pero convergentes:**
- `LutDSP/VoiceAllocator.h` (compartido) vs `Source/Core/VoiceManager.{h,cpp}` (synth): dos soluciones al mismo problema (asignación/robo de voces) en repos distintos.
- `Source/Plugin/PluginEditor_ResourceProvider.*` vs `ABDSharedCode/WebView2Bridge/WebView2ResourceProvider.*`: el synth aún usa su provider propio (el compartido existe precisamente para sustituirlo).
- Clases CSS (.chassis, .navbar, .lcd-container, .kbd-*) presentes tanto en estilos compartidos como locales del synth.

---

## 4. Dirección A — qué trasladar DEL synth A los repos compartidos

### 4.1 Candidatos de alto valor (C++)

| Código del synth | Destino propuesto | Justificación |
|---|---|---|
| `Source/Core/VoiceManager.{h,cpp}` (robo de voces por prioridad, mono/unison última-nota, re-trigger sin clics) | Nuevo módulo **`ABDShared::SynthCore`** | Reutilizable por ABDEep, ABDCZ101, ABDOmega… Conviene **converger** con el ya existente `LutDSP/VoiceAllocator.h` en un único asignador |
| `Source/Core/AudioThreadSnapshot.h` (POD lock-free de telemetría a 60 FPS) | `SynthCore` | Patrón genérico; ABDScope ya consume snapshots de este estilo |
| `Source/DSP/Common/PolyBLEP.*`, `Source/DSP/Envelopes/ADSREnvelope.*`, `Source/DSP/Modulation/PortamentoGlide.*`, `Source/DSP/Modulation/LFO.*` | `SynthCore` (o extensión de `LutDSP`) | Primitivas DSP genéricas; hoy todo synth las reescribe |
| `Source/DSP/Vocoder/*` | Diferido | Genérico, pero solo extraer cuando un segundo proyecto lo necesite (DRY no significa anticipar todo) |

**Queda LOCAL por ser específico MS2000/microKORG:** `MIDIMap`, `MS2000ProgramData`, `SysExManager`, `MS2000SysExExporter`, `LCDMenuFormatter`, `MS2000PatchBuilder`, `MS2000FactoryBank`, DWGS/VoxWave/OSC2, `MultiModeFilter`, `ModSequencer` (formato 3×16 de Korg), `VirtualPatchMatrix` (escala de slots Korg).

### 4.2 Candidatos JS (WebUI → `@abdsynths/shared/components/`)

| Código del synth | Destino | Justificación |
|---|---|---|
| `contracts/paramStore.js` | `@abdsynths/shared/components/paramStore.js` | Espejo genérico de APVTS; cualquier WebUI de synth lo necesita |
| `components/rotaryKnob.js`, `sliderFilmstrip.js` | idem | Controles genéricos con binding a paramStore |
| `components/customSelectors.js` (SegmentedSelector, LcdDropdown) | idem | Genéricos |
| `components/slideDrawer.js` | idem | Ya etiquetado "CZ-101 / ABDEep style" en su cabecera |
| `bridge/bridgeCore.js` (patrón JUCE IPC + fallback WASM/WebAudio) | `@abdsynths/shared/components/bridge.js` | Toda WebUI del ecosistema necesita este puente; parametrizable por proyecto |
| `panels/panelScope.js` (VU + canvas genérico) | opcional | El scope serio ya viene de ABDScope |

`package.json` de ABDSharedAssets ya exporta `./components/*`, así que el destino natural existe.

---

## 5. Dirección B — qué ADOPTAR del compartido hacia el synth

| # | Módulo compartido | Uso en ABDMS2000 | Valor |
|---|---|---|---|
| 1 | `ABDShared::HardwareDrivers` (`SysExCodec`, `NRPNParser`) | **Sustituir** las copias locales (§3 casos 1–2) | Elimina 2 duplicaciones exactas; única fuente para el codec Korg |
| 2 | `ABDShared::HardwareMidiDetect` | Detección automática del MS2000 físico vía contratos (`korg_ms2000.json` ya existe en `ABDSharedAssets/contracts`) + picker WebView2 listo | Sustituye selección manual de puertos; habilita dumps SysEx reales hacia/desde hardware |
| 3 | `ABDShared::AudioComparator` + `Certification/` | Suite `Source/Tests/DSPCoreTests.cpp`: comparación A/B contra grabaciones de hardware real con dictamen pass/warn/fail | Paridad hardware-vs-emulación verificable en CI |
| 4 | `@abdsynths/midi-keyb` | Sustituir la copia manual por el paquete (ver arquitectura §7) | Elimina el fork de teclado/ruedas; trae 5 suites de tests propias |
| 5 | `ABDShared::AutoUpdater` | Hoy: enlazado en CMake y **cero referencias en Source/**. Decidir: integrar (config + gancho en editor, según INTEGRATION_GUIDE) o quitar el enlace | Evita dependencia muerta |
| 6 | Estilos compartidos (`lcd.css`, `navbar.css`, `wheels.css`, `kbd-buttons.css`, tokens) | Cascada de 3 niveles también en nativo (ver §7) | Elimina duplicación de tokens y componentes CSS |

---

## 6. Restricciones técnicas verificadas

1. **El WebView2 nativo no resuelve bare imports.** El build nativo embebe los fuentes crudos (`juce_add_binary_data` sobre `WebUI/src/*`) y el resource provider los sirve tal cual; el navegador (WebView2) ejecuta módulos ES sin resolución de `node_modules`. Por eso `keyboard.js` se mantiene "a mano" con imports relativos.
2. **`build_webui.js` NO empaqueta**: solo genera `BuildVersion.h`/`buildVersion.js`. No hay paso de bundling hoy (Vite se usa solo como dev server en `start.bat`).
3. **ABDMS2000_Tests compila los .cpp locales** de `Source/MIDI/` directamente; tras migrar codecs deberá enlazar los del módulo compartido.
4. `HardwareDrivers` completo arrastra `juce_audio_devices`; para consumir solo codecs conviene un target más fino (ver §8, extensión 1).
5. Los proyectos hermanos (ABDEep, CZ101, Scope, BankManager WebUI) todavía no importan `@abdsynths/*`: la adopción del modelo npm será progresiva.

---

## 7. Opciones de arquitectura para el consumo único ("paquete npm, sin copias")

### Opción A — Workspace pnpm + bundling Vite para nativo ✅ (recomendada, alineada con la preferencia declarada)

- **Dev:** como hoy — Vite resuelve `@abdsynths/*` vía enlaces pnpm del workspace.
- **Nativo:** `build_webui.js` ejecuta `vite build` → `WebUI/dist/` (JS/CSS con imports ya resueltos) → `juce_add_binary_data` embebe **dist** en lugar de `src`; el resource provider sirve dist.
- **C++:** se mantiene `add_subdirectory`/FetchContent (ya funciona, con tags de versión para releases).
- **Resultado:** cero scripts de copia para JS; una sola fuente; los proyectos hermanos adoptan el mismo patrón cuando migren.
- **Coste/riesgo:** medio. Hay que adaptar el resource provider al layout de dist (hashes, rutas relativas `base: './'`) y verificar el flujo completo en WebView2. Los vendor minificados actuales (`dexie`, `jszip`, `file-saver`) pasarían a imports normales.

### Opción B — Script de vendor-sync mejorado (transición segura)

- Generalizar el patrón `sync_scope.js` a un `sync_shared_js.js`: copia los módulos compartidos necesarios a `WebUI/vendor-shared/` **reescribiendo** los bare imports a rutas relativas, y añade un test vitest de anti-drift (falla si la copia difiere del origen).
- **Pros:** bajo riesgo, compatible con el build nativo actual desde el día 1; elimina la copia manual.
- **Contras:** sigue habiendo copia (aunque automatizada y verificada); es un parche, no el objetivo.

### Opción C — Junctions NTFS también para JS

- Coherente con la filosofía zero-copy documentada, pero **no resuelve** el bare import en nativo (el WebView2 seguiría sin resolver `@abdsynths/*` dentro de la jerarquia embebida) y no portable a CI/otros equipos sin el monorepo.

**Recomendación:** **A como destino**, con **B como paso intermedio** si se quiere desbloquear ya la migración del teclado sin esperar a reconvertir el pipeline nativo. La opción A elimina también la necesidad de `sync_assets.js` a medio plazo (las imágenes pueden servirse por el provider desde ABDSharedAssets, como ya hace `bankwebui://`).

---

## 8. Extensiones necesarias en los repos compartidos ("ampliar")

1. **Target CMake fino `ABDShared::MidiCodecs`** (INTERFACE: solo `SysExCodec` + `NRPNParser`, sin `juce_audio_devices`) para que el synth no arrastre todo `HardwareDrivers`.
2. **Nuevo módulo `ABDShared::SynthCore`** (estático o INTERFACE): `VoiceManager` (políticas de robo/prioridad), `AudioThreadSnapshot`, `PolyBLEP`, `ADSREnvelope`, `PortamentoGlide`, `LFO`. Converger con `LutDSP/VoiceAllocator.h` (deprecar uno de los dos).
3. **Componentes JS en `@abdsynths/shared/components/`**: `paramStore.js`, `rotaryKnob.js`, `sliderFilmstrip.js`, `customSelectors.js`, `slideDrawer.js`, `bridge.js` (§4.2), con sus hojas de estilo correspondientes.
4. **Script/acción de publicación**: hoy `@abdsynths/shared` es `private` y solo funciona por workspace/file:. Para "descarga de internet" real: publicar en registry privado (GitHub Packages) o mantener FetchContent/workpace pero con **tags de versión** y changelog por release (ya documentado en ABDSharedCode/README).
5. **Contrato de versionado semántico** de los paquetes compartidos y de pinned tags en consumidores (`GIT_TAG v1.x`, no `master`) para evitar rupturas transversales.

---

## 9. Plan de fases recomendado

### Fase 0 — Quick wins (bajo riesgo, alto retorno inmediato)
1. Migrar `SysExCodec` + `NRPNParser` al módulo compartido (con la extensión §8.1). Ajustar namespaces en ~6 ficheros consumidores + tests.
2. Decidir AutoUpdater: integrar o quitar el enlace muerto.
3. Adoptar la versión compartida de `keyboard.js` (la que usa `createWheel`) en cuanto el mecanismo de consumo esté elegido (A o B), eliminando el fork de ruedas.

### Fase 1 — Adoptar del compartido
4. `HardwareMidiDetect` con el contrato `korg_ms2000` (detección + picker).
5. `AudioComparator` en la suite de tests (paridad con hardware).
6. Consolidación CSS: adoptar componentes compartidos, dejar `themes.css` como capa de overrides del host.

### Fase 2 — Extraer del synth hacia compartido
7. Crear `SynthCore` y trasladar VoiceManager/snapshot/primitivas (§4.1), con tests en el repo compartido.
8. Trasladar los componentes JS genéricos (§4.2) y refactorizar la WebUI para importarlos del paquete.

### Fase 3 — Pipeline único
9. `vite build` en `build_webui.js` + embeber `dist` (Opción A completa). Retirar `sync_assets.js`/copias si el provider sirve directamente de ABDSharedAssets.

Cada fase es independiente y revertible; el orden respeta dependencias (los componentes JS compartidos requieren que el consumo npm funcione en nativo, de ahí la Fase 3 al final o la opción B como puente).

---

## 10. Riesgos y mitigaciones

| Riesgo | Mitigación |
|---|---|
| Cambio de namespace rompe consumidores del codec | Migración con `using` temporal o alias de namespace durante una release |
| El bundling Vite altera rutas que el resource provider asume | Probar en Standalone antes de VST3; `base: './'` y mapeo explícito en el provider |
| Regresión visual al adoptar CSS compartido | La cascada de 3 niveles ya diseñada: tokens compartidos primero, overrides del host después; comparar con la demo de ABDSharedAssets |
| Divergencia futura de tests entre repos | Los tests de teclado ya viven en MidiKeyboard; SynthCore nace con su propia suite |
| Builds de CI sin copia local | FetchContent con tags ya cubierto en ambos repos |

---

## 11. Anexo — verificaciones realizadas

- `diff` byte a byte entre las 6 parejas de ficheros de la §3 (resultados en la tabla).
- `grep` de uso real de `AutoUpdater` en `Source/` → 0 referencias.
- `grep` de `@abdsynths` en WebUI/src → imports activos en `shared-cascade.css` y documentación de `app.js` sobre la copia manual del teclado.
- Verificación de `pnpm-workspace.yaml`, enlaces en `node_modules/@abdsynths/` y `package.json` del monorepo.
- Revisión de `Scripts/{sync_assets,sync_bankmanager,sync_scope,build_webui}.js`, `start.bat`, `build.bat`, `vite.config.js`, `CMakeLists.txt` (raíz y de ambos repos compartidos).
- Estructura completa de `ABDSharedCode` (7 módulos + MidiKeyboard con 5 suites vitest) y `ABDSharedAssets` (styles/components/contracts/models/brands/icons/docs/demo).
