# Análisis DRY Transversal II — ABDBankManager ↔ ABDCZ101 ↔ ABDEep

> **Fecha:** 2026-09-08
> **Objetivo:** Aplicar el mismo análisis del informe anterior (`ANALISIS_DRY_COMPARTIDO.md`) a tres proyectos más antiguos del ecosistema: **ABDBankManager**, **ABDCZ101** y **ABDEep**. Identificar piezas reutilizables en ambos sentidos respecto a `ABDSharedCode`, `ABDSharedAssets` y `ABDMS2000`. Responder a la duda específica sobre si **ABDBankManager debe partirse en dos**.
> **Alcance:** Solo análisis. No se ha modificado código.

---

## 1. Resumen ejecutivo

1. **ABDBankManager NO debe disolverse en ABDSharedCode/ABDSharedAssets.** Ya está de facto dividido en capas (`cpp/` + `packages/*` + `WebUI/` + `apps/juce-plugin`); lo que necesita es **formalizar esa división interna en paquetes npm publicables** y que los synth consuman esos paquetes. Lo único que toca a los repos compartidos ya está en su sitio (contratos normativos JSON en ABDSharedAssets; codecs C++ en ABDSharedCode).
2. **ABDBankManager tiene duplicidades internas activas**: `fingerprint.js` (16 líneas de diff) y `searchPatches.js` (4 líneas) existen a la vez en `packages/core/src` (canónico) y en `WebUI/src/core` (copia a mano). El pipeline de generación solo empaqueta `library.js`, no el resto.
3. **ABDCZ101 y ABDEep no consumen nada compartido** (cero referencias `@abdsynths/*`, cero enlaces CMake a ABDShared*). Ambos tienen implementaciones paralelas de los mismos conceptos que ya existen compartidos o que el plan `SynthCore` del informe I contempla.
4. Confirmación empírica del módulo **`SynthCore`**: existen **3 implementaciones** de VoiceManager (MS2000, CZ101 con `VoiceAssignmentStrategy`, ABDEep con `SynthEngine_VoiceManager`), **2** de `AudioThreadSnapshot` (MS2000/CZ101) y **2–3** de LFO/Envelope.
5. ABDEep aporta piezas de **alto valor todavía no compartidas**: filtros ZDF reales (`JunoVCF_ZDF`, `MoogLadderVCF`, `KorgMS20VCF`, `VAOnePoleFilter`, `VcfVoicing`) que complementan los modelos LUT de `LutDSP`; y `DriftEngine`. Son candidatos claros a un módulo compartido de filtros.
6. ABDCZ101 duplica el codec de nibbles Casio inline en `SysExManager.cpp` (`decodeNibblePair`) cuando `HardwareDrivers/CasioNibbleCodec.h` ya lo implementa (y soporta ambos órdenes de nibble).
7. El generador de registros (`registry_generator.js`) existe en **3 forks** (MS2000 454 líneas de diff con CZ101, 753 con ABDEep): candidato a generador compartido parametrizado por proyecto.
8. Incoherencia detectada en ABDBankManager: conviven `package-lock.json` (npm) y `pnpm-lock.yaml`, y **no está incluido en el workspace pnpm raíz** del monorepo — por eso MS2000 lo consume por copia de WebUI + CMake, y no por paquete.

---

## 2. ABDBankManager — mapa interno y la cuestión de la división

### 2.1 Estructura real (verificada)

```
ABDBankManager/
├── cpp/                      → ABDBankManagerCore (lib estática C++ para plugins JUCE)
│   ├── ABDBankManagerCore.*  → Library/Bank/Patch en ValueTree v1, IPC por callback
│   ├── BankManagerWebViewAdapter.* → puente C++ ⇄ WebUI
│   ├── FactoryContentLoader.*      → carga de .abdbank de fábrica embebidos
│   └── Pro800Midi.*                → driver específico Behringer Pro-800
├── Source/                   → SSOT TypeScript (se transpila a WebUI)
│   ├── Contracts/Models/*.ts → 11 ModelContracts (cz, juno, ms2000, dm12, dx7…)
│   ├── Contracts/SysEx/codec.ts    → SSOT TS de codificaciones SysEx (7↔8, nibble, checksums)
│   ├── Contracts/Adapters|Import|Export|HardwareLink
│   └── State/ParameterRegistry.gen.*
├── packages/                 → esqueleto npm (workspaces internos)
│   ├── core/       @abdsynths/bank-manager-core       (real: models, operations, search, validation)
│   ├── contracts/  @abdsynths/bank-manager-contracts  (real: Models/index.ts)
│   ├── adapters/   @abdsynths/bank-manager-adapters   (stub: solo package.json)
│   └── ui/         @abdsynths/bank-manager-ui         (stub: solo package.json)
├── WebUI/                    → SPA completa (app.js 2.145 líneas, core/ 24 ficheros, store/, ui/)
│   └── src/contracts/gen/    → artefactos generados (library.gen.js, contractRegistry.gen.js, modelContracts.gen.js)
├── apps/juce-plugin/         → host JUCE de referencia
└── scripts/                  → generadores + sync_contracts.mjs (valida contra ABDSharedAssets)
```

### 2.2 La duda planteada: ¿partirlo en dos?

**Respuesta corta: no partirlo hacia los repos compartidos; sí formalizar la división que ya tiene dentro.**

Razonamiento por piezas:

| Pieza de BankManager | ¿A dónde debería vivir? | Motivo |
|---|---|---|
| Contratos normativos de hardware (JSON: `korg_ms2000.json`…) | **ABDSharedAssets/contracts** (ya está) | SSOT de identidad hardware; BankManager ya lo valida con `sync_contracts.mjs`. Dirección correcta, no tocar |
| Codec SysEx C++ (`SysExCodec`, `CasioNibbleCodec`, `NRPNParser`) | **ABDSharedCode/HardwareDrivers** (ya está) | Ya duplicado ahí; el plan del informe I migra a MS2000. BankManager no usa C++ codec (usa el TS) |
| `Source/Contracts/SysEx/codec.ts` (SSOT TS) | **Queda en BankManager** | Es la contraparte TS del mismo algoritmo. No se puede compartir literalmente entre lenguajes; la mitigación es validación cruzada por tests (mismos vectores en C++ y TS) |
| `packages/core` (Library/Bank/Patch, fingerprint, search) | **Queda en BankManager** (como paquete) | Lógica de dominio de bancos; no es infraestructura transversal |
| `packages/contracts` (ModelContracts TS) | **Queda en BankManager** (como paquete) | Lógica de adaptadores por modelo sobre los JSON normativos; acoplada al dominio de BankManager |
| `packages/adapters` (import/export por marca) | **Formalizar** (hoy stub) | Es la capa que crece con cada synth soportado |
| `packages/ui` (BankManagerModal, modelSelector, panelFactory) | **Formalizar** (hoy stub) | Es exactamente lo que MS2000 hoy consume por **copia de WebUI** (`sync_bankmanager.js`) e iframe |
| `cpp/ABDBankManagerCore` | **Queda en BankManager** | Dominio de bancos con ValueTree; consumo C++ vía `add_subdirectory`/FetchContent (MS2000 ya lo hace, `v0.3.0-lib`) |
| `cpp/FactoryContentLoader` | Queda (candidato lejano a WebView2Bridge) | Genérico pero acoplado al formato `.abdbank` |
| `cpp/Pro800Midi` | Mover a `packages/adapters` (conceptualmente) o mantener junto a core | Es driver de modelo concreto, igual que los adapters TS |
| `WebUI/` (SPA standalone) | **Queda como app consumidora** | Debe adelgazar: importar de `packages/*` en vez de duplicar |

**Conclusión:** BankManager ya ES la "parte compartida" para el dominio de bancos. El problema no es dónde vive sino que **sus capas internas no están terminadas** (`adapters` y `ui` son stubs) y que **no está en el workspace raíz**, lo que obliga a MS2000 a consumirlo por copia.

### 2.3 Duplicidades internas detectadas (evidencia)

| Par de ficheros | Diff (`diff -w`) | Riesgo |
|---|---|---|
| `packages/core/src/operations/fingerprint.js` ↔ `WebUI/src/core/fingerprint.js` | **16 líneas** | Fork activo; el paquete es el canónico pero la WebUI edita la copia |
| `packages/core/src/search/searchPatches.js` ↔ `WebUI/src/core/searchPatches.js` | **4 líneas** | Idem |
| `WebUI/src/core/` (24 ficheros: importEngine, exportEngine, hexEditor, deduplication, undoHistory, sysexParser…) vs `packages/core/src` (10) | — | 14 ficheros de lógica de negocio **solo existen en la WebUI**: si otro proyecto quiere el core sin la SPA, no puede |
| `WebUI/src/ui/components/rotaryKnob.js` ↔ MS2000 `rotaryKnob.js` | 293 líneas | Dos knobs con API distinta; candidato a unificar en `@abdsynths/shared` (informe I §4.2) |
| `WebUI/src/store/paramStore.js` (133 l.) ↔ MS2000 `paramStore.js` (221 l.) | 247 líneas | Idem: dos paramStore incompatibles |
| SysEx en 3 lenguajes/tiers | — | C++ compartido (HardwareDrivers), C++ local MS2000 (a migrar), TS (codec.ts). Algoritmos idénticos; mitigable con vectores de test compartidos |
| `BankManagerModal.js` (origen) ↔ copia en MS2000 | 60 líneas | Ya forked la copia del modal |

### 2.4 Acciones propuestas para ABDBankManager

1. **Terminar `packages/adapters` y `packages/ui`**: mover a `packages/ui` los componentes realmente reutilizables (`BankManagerModal`, `modelSelector`, `panelFactory`, `rotaryKnob` si se unifica) y a `packages/adapters` los engines de import/export + `Pro800Midi` conceptual.
2. **Eliminar las copias de fingerprint/searchPatches en `WebUI/src/core`**: o se generan (extender `build_core_web.js` con más entry points) o la WebUI importa del paquete (ya tiene `vite build` en `build:webui`; el modo importmap estático solo necesita los `.gen.js`).
3. **Incorporar ABDBankManager al `pnpm-workspace.yaml` raíz** del monorepo y decidir un único lockfile (hoy conviven `package-lock.json` y `pnpm-lock.yaml`).
4. Cuando eso exista, **MS2000 deja de necesitar `sync_bankmanager.js`**: importa `@abdsynths/bank-manager-ui` (modal) por workspace y enlaza el core C++ como ya hace.
5. **Tests cruzados de codecs**: un fixture compartido (vectores 7↔8 bits, nibbles CZ, checksums Roland/Yamaha) ejecutado por la suite C++ de HardwareDrivers y por la TS de codec.ts.

---

## 3. ABDCZ101 — piezas reutilizables en ambos sentidos

### 3.1 Qué tiene y cómo está

- C++: `Source/Core` (VoiceManager 16 voces + `VoiceAssignmentStrategy`, `AudioThreadSnapshot`, `CZ5000VoiceStrategy`), `Source/MIDI` (`SysExManager` con nibble decoder inline), DSP propio (osciladores fase-distorsión CZ, efectos), resource provider propio (115 líneas).
- WebUI: estructura moderna (`src/ui`, `src/contracts`, `src/engine` con AudioWorklet `cz101Worklet.js`), teclado propio (`keyboard.js`, ~1.210 líneas de diff con el de MS2000), **filmstrips propios** (`filmstrips.js`), osciloscopio canvas propio, y **un bank manager propio de 867 líneas** basado en localStorage (anterior a ABDBankManager).
- Cero consumo de repos compartidos (ni pnpm ni CMake).

### 3.2 Dirección CZ101 → compartido

| Pieza | Valor | Destino |
|---|---|---|
| `VoiceAssignmentStrategy.h` (estrategia de asignación como interfaz) | El diseño por estrategia es **mejor** que el hardcodeo de MS2000; útil para `SynthCore` | Fusionar en `SynthCore::VoiceAllocator` (junto a `LutDSP/VoiceAllocator.h`) |
| `CZ5000VoiceStrategy.h` | Patrón "estrategia por modelo" | Mismo destino, como ejemplo |
| Nibble decode inline de `SysExManager.cpp` | Ya cubierto por `CasioNibbleCodec` (que además soporta HighFirst/LowFirst) | **Eliminar**, no extraer: adoptar el compartido |
| `filmstrips.js` | Versión simple; MS2000 ya tiene una mejor (`sliderFilmstrip.js`) | No extraer; la unificación sale sola al adoptar el knob/slider compartido |
| `cz101Worklet.js` / engine | Específico CZ | Local |

### 3.3 Dirección compartido → CZ101 (adopciones)

| Módulo compartido | Sustituye a | Nota |
|---|---|---|
| `ABDShared::HardwareDrivers::CasioNibbleCodec` | `decodeNibblePair` inline | Verificar orden de nibbles (CZ = low-first, el codec soporta ambos) |
| `ABDShared::WebView2Bridge` | `Source/PluginEditor_ResourceProvider.cpp` (115 l.) | El provider compartido ya sirve binarios + fallback disco |
| `MidiKeyboard` (`@abdsynths/midi-keyb`) | `WebUI/src/ui/keyboard.js` | El paquete ya declara heredar features de CZ101 (wear stains, QWERTY, flechas de octava); migrar el consumo |
| `@abdsynths/bank-manager-*` | `WebUI/src/ui/bankManager.js` (867 l., localStorage) | Es la "Fase 7" que el propio README de BankManager tiene pendiente (migración CZ101/ABDEep) |
| Estilos ABDSharedAssets (tokens + componentes) | `WebUI/src/styles/*` locales | Cascada de 3 niveles (informe I §7) |
| `ABDShared::AudioComparator` | — | Validar paridad CZ101 emulado vs hardware real (el contrato `casio_cz101.json` ya existe) |

### 3.4 Bloqueo heredado

CZ101 está en el workspace pnpm raíz pero **no tiene `@abdsynths/*` en su package.json** ni bundling en el build nativo: aplica exactamente la misma restricción del informe I (§6.1). La adopción JS depende de resolver el pipeline nativo (Opción A) o del vendor-sync (Opción B).

---

## 4. ABDEep — piezas reutilizables en ambos sentidos

### 4.1 Qué tiene y cómo está

- C++ maduro y modular: `Source/DSP` con **filtros modelados de altísima calidad** (`JunoVCF_ZDF` + proceso, `MoogLadderVCF`, `KorgMS20VCF`, `VAOnePoleFilter`, `VcfVoicing`), `DriftEngine` (deriva analógica), `ModulationMatrix`, `Envelope`, `LFO` (128 líneas de diff con el LFO de MS2000: mismo linaje, fork activo).
- `Source/Core` con utilidades genéricas de dominio banco/patch: `BankFileReader`, `PatchByteCodec`, `RoundTripValidator`, `PatchDiffTypes`, `ParametersSpec_*` (especificación declarativa de parámetros).
- WebUI antigua por estructura: **242 ficheros JS planos** en `WebUI/js` (puente dual `bridge-dual*.js`, teclado fragmentado en 7 módulos `keyboard*.js`, scope en 5 módulos). `css/tokens.css` es un fork antiguo del sistema de tokens compartido (301 líneas de diff con `ABDSharedAssets/styles/tokens.css`); `keyboard.css` diverge en 531.
- Cero consumo de repos compartidos (CMake sin ABDShared*, WebUI sin `@abdsynths/*`).
- El propio `MidiKeyboard` declara haber tomado de ABDEep sus mejores features (curvas de velocidad, presión, displacements): **ABDEep es origen, hoy superado por el paquete.**

### 4.2 Dirección ABDEep → compartido (el aporte más valioso del análisis)

| Pieza | Valor | Destino propuesto |
|---|---|---|
| `JunoVCF_ZDF.{h,cpp}` + `VcfVoicing.h` | Filtro ZDF real (topología de preservación de cero delay) con voicing por componente | Nuevo módulo **`ABDShared::Filters`** (o extensión de `LutDSP`): coexistiría con los modelos LUT como "camino exacto" |
| `MoogLadderVCF`, `KorgMS20VCF`, `VAOnePoleFilter` | Modelos clásicos reutilizables por todos los synth | `ABDShared::Filters` |
| `DriftEngine` | Deriva analógica transversal (VCO drift, jitter) | `ABDShared::SynthCore` o `Filters` |
| `PatchByteCodec` + `RoundTripValidator` + `PatchDiffTypes` | Patrón codec-de-patch + validación round-trip con corpus | Evaluar tras la Fase de SynthCore; encaja con la filosofía de fixtures compartidos de BankManager |
| `ParametersSpec_*` (spec declarativa de parámetros) | Idea convergente con los ParameterRegistry generados | A medio plazo: unificar con el generador compartido (§5) |
| Módulos `keyboard_*` JS | Ya absorbidos por `@abdsynths/midi-keyb` | Nada que hacer: el paquete es el heredero |

### 4.3 Dirección compartido → ABDEep (adopciones)

| Módulo compartido | Sustituye a | Nota |
|---|---|---|
| `MidiKeyboard` | 7 ficheros `keyboard*.js` | Mayor simplificación inmediata de la WebUI |
| Tokens/componentes ABDSharedAssets | `css/tokens.css`, `controls.css`, `buttons.css`, `navbar.css`, `keyboard.css` | Fork antiguo del mismo sistema; la cascada de 3 niveles está diseñada para esto |
| `ABDShared::AudioComparator` | `scripts/generate_parity_fixture.js` + `hw_roundtrip_validate.js` (artesanales) | ABDEep ya tiene la filosofía de paridad; el comparador le da dictamen formal pass/warn/fail |
| `ABDShared::AutoUpdater` | — | ABDEep tiene pipeline de release propio (`verify_release.ps1`); evaluar si AutoUpdater encaja |
| `HardwareMidiDetect` | Detección manual actual | Contratos `behringer_deepmind12.json` ya existen |

### 4.4 Advertencia de esfuerzo

ABDEep es el proyecto de mayor tamaño (242 JS + DSP extenso). Su adopción del modelo compartido es la más rentable a largo plazo pero la más cara en el corto plazo. Estrategia recomendada: **cero big-bang**; adoptar primero lo que sustituye código entero (teclado → 1 paquete, estilos → cascada), extraer después los filtros ZDF (son autocontenidos y con tests), y dejar el resto de la WebUI para una migración gradual por módulos.

---

## 5. Matriz transversal de duplicidades (los 5 proyectos + shared)

| Concepto | MS2000 | CZ101 | ABDEep | BankManager | Ya compartido | Plan |
|---|---|---|---|---|---|---|
| VoiceManager/asignación | ✔ propio | ✔ propio (+Strategy) | ✔ propio | — | `LutDSP/VoiceAllocator` (parcial) | **`SynthCore`** (informe I) |
| AudioThreadSnapshot | ✔ | ✔ (fork, 178 l.) | (DiagnosticSnapshots) | — | — | **`SynthCore`** |
| LFO / Envelope | ✔ | ✔ | ✔ (forks, 128/129 l.) | — | — | **`SynthCore`** |
| Filtros ZDF/modelados | MultiModeFilter local | propios | **✔ referencia** | — | `LutDSP` solo LUT | **`ABDShared::Filters`** (nuevo) |
| Codec 7↔8 C++ | copia local (a migrar) | — | — | — | `HardwareDrivers` ✔ | Fase 0 informe I |
| Nibble Casio | — | inline (a migrar) | — | — | `CasioNibbleCodec` ✔ | adoptar en CZ101 |
| Codec SysEx TS | — | — | — | ✔ SSOT (codec.ts) | — | fixtures cruzados |
| ResourceProvider WebView | propio (a migrar) | propio (a migrar) | propio | propio (adapter) | `WebView2Bridge` ✔ | adoptar en CZ101/ABDEep |
| registry_generator.js | ✔ (base) | fork (454 l.) | fork (753 l.) | propio (registry_core.js) | — | generador compartido parametrizado |
| Teclado JS | copia manual forked | propio (origen) | origen (7 módulos) | — | `midi-keyb` ✔ | adoptar en los 3 |
| Tokens/componentes CSS | fork parcial | propios | fork antiguo | propios | `ABDSharedAssets` ✔ | cascada en los 4 |
| Bank UI | iframe+copias | propio (867 l.) | propio (`bank_sampler*`) | ✔ el original | `packages/ui` (a formalizar) | adoptar paquete |
| BankManagerModal.js | copia (60 l. fork) | — | — | ✔ origen | `packages/ui` | copia → import |

---

## 6. Plan de fases recomendado (continuación del informe I)

### Fase 0bis — BankManager por dentro (previa a todo consumo cruzado)
1. Formalizar `packages/adapters` y `packages/ui`; terminar el empaquetado de core (fingerprint/search dentro del bundle generado o import directo).
2. Unificar toolchain pnpm (un lockfile) e incluir ABDBankManager en el workspace raíz.
3. Tests cruzados de codecs (vectores compartidos C++ ↔ TS).

### Fase 1 — ABDCZ101 (el más barato de los dos synth antiguos)
4. C++: adoptar `CasioNibbleCodec` y `WebView2Bridge`; eliminar duplicados inline.
5. JS: `midi-keyb` + estilos compartidos (con vendor-sync B si el pipeline nativo aún no bundlea).
6. Bank: migrar su bankManager local al paquete de BankManager (su "Fase 7").

### Fase 2 — ABDEep (extracciones de valor)
7. Crear **`ABDShared::Filters`** con `JunoVCF_ZDF`, `MoogLadderVCF`, `KorgMS20VCF`, `VAOnePoleFilter`, `VcfVoicing`, `DriftEngine` (extraídos de ABDEep, con sus tests). MS2000 y el resto podrán consumirlos.
8. ABDEep adopta `midi-keyb` (borra 7 módulos), estilos compartidos y `AudioComparator` en su pipeline de paridad.

### Fase 3 — Generador compartido
9. Unificar `registry_generator.js` en un generador compartido (ABDSharedCode/Scripts o paquete npm interno) con configuración por proyecto (schema + naming + salidas h/js). Los tres forks convergen.

### Fase 4 — SynthCore (ya definido en el informe I)
10. La convergencia VoiceManager×3 / Snapshot×2 / LFO×3 / Envelope×3 confirma el módulo; usar `VoiceAssignmentStrategy` de CZ101 como interfaz y el robo por prioridad de MS2000 como política por defecto.

Cada fase es independiente; 0bis y 1 son los quick wins de este segundo análisis.

---

## 7. Riesgos y mitigaciones específicos

| Riesgo | Mitigación |
|---|---|
| Mover componentes a `packages/ui` rompe la SPA standalone de BankManager | La SPA pasa a importar del paquete en el mismo repo; CI de BankManager valida ambos consumidores (SPA y plugin host) |
| Los forks de registry_generator tienen semánticas distintas (454/753 líneas) | No unificar a ciegas: inventariar diferencias reales de salida por proyecto antes del generador común |
| Filtros ABDEep dependen de utilidades internas suyas (`DSPHelpers`, `VAOnePoleFilter`) | Extraer el subgrafo de dependencias completo y con tests; interfaz estanca |
| CZ101/ABDEep no bundlean su WebUI nativa | Misma Opción A/B del informe I; no intentar adopciones JS sin resolver eso (o usar vendor-sync temporal) |
| Doble lockfile en BankManager genera instalaciones distintas en CI | Elegir pnpm (coherente con el monorepo), borrar `package-lock.json`, añadir a CI |
| El bankManager de CZ101 tiene comportamiento propio (slots, LCD wiring) no cubierto por el modal de BankManager | Mapear features antes de migrar; si faltan, subirlas a `packages/ui` (no mantener fork local) |

---

## 8. Anexo — verificaciones realizadas

- Estructura completa de ABDBankManager: `cpp/` (1.341 líneas, 4 unidades + tests CTest), `Source/Contracts` (11 ModelContracts TS + `SysEx/codec.ts` SSOT), `packages/{core,contracts}` con src real y `{adapters,ui}` stub, `WebUI/src` (app 2.145 l., core 24 ficheros, gen con 3 artefactos), `apps/juce-plugin`, `scripts/sync_contracts.mjs` (valida contra ABDSharedAssets), `.gitmodules` (solo JUCE).
- Diffs internos BankManager: fingerprint 16 l., searchPatches 4 l., rotaryKnob 293 l. vs MS2000, paramStore 247 l. vs MS2000, BankManagerModal 60 l. vs copia MS2000.
- CZ101: `VoiceManager` (diff 256/568 l. vs MS2000), `AudioThreadSnapshot` (178), `SysExManager.cpp` con `decodeNibblePair` inline, `bankManager.js` 867 l. localStorage, `keyboard.js` 1.210 l. de diff, `filmstrips.js` propio; sin enlaces ABDShared en CMake ni `@abdsynths` en WebUI.
- ABDEep: inventario de `Source/DSP` (JunoVCF_ZDF, MoogLadderVCF, KorgMS20VCF, VAOnePoleFilter, VcfVoicing, DriftEngine…), 242 ficheros en `WebUI/js` (7 de teclado, 5 de scope, familia bridge-dual), `css/tokens.css` diff 301 l. vs compartido, `keyboard.css` diff 531 l., `registry_generator.js` diff 753 l. vs MS2000; sin consumo compartido.
- Workspace raíz: `pnpm-workspace.yaml` NO incluye ABDBankManager; BankManager tiene `package-lock.json` + `pnpm-lock.yaml` simultáneos.

> **NOTA (post-implementación):** el estado descrito en este anexo corresponde al momento del análisis. La sección §9 documenta la implementación ya realizada, que cambia varios de esos puntos.

---

## 9. Centralización de contratos ABDBankManager ↔ ABDSharedAssets — implementación

> **Fecha:** 2026-09-08. Estrategia confirmada: **IDs canónicos + alias**, **categorías caso a caso** (§9.2), **infraestructura primero**.

### 9.1 Cambios realizados

**A. Esquema JSON (ABDSharedAssets — SSOT normativo)**
- `hardware_profile.schema.json`: nuevo campo `aliases` (ids alternativos, p. ej. kebab-case heredados de manifests `.abdbank` existentes) y campos `bankManagement` alineados con los ModelContracts TS (`patchDataSize`, `bankCapacity`, `banks`, `programsPerBank`, `sysexProtocol`).
- Los 22 contratos JSON validan contra el esquema (`JSON.parse` OK 22/22).
- **Bug de datos corregido:** `korg_ms2000.json` ya no declara `korg-prophecy` como `compatible` (el formato Prophecy es distinto: 0x41, 312B vs 288B — un patch Prophecy no cabe en un banco MS2000).
- Añadidos `aliases` en todos los modelos con gemelo kebab-case (`korg_ms2000` ↔ `korg-ms2000`, etc.), y `sysexProtocol`/metadatos sembrados desde el TS (fuente contrastada con hardware).

**B. ModelContracts TS (BankManager — lógica runtime)**
- Bugs corregidos: `yamaha-dx7.ts` heredaba `compatibleModels: ['yamaha-dx7ii']` autorreferenciado; `behringer-dm12` divergencia numérica unificada.

**C. Validador real (`scripts/sync_contracts.mjs`)**
- Ya no solo imprime: **compara** TS vs JSON campo a campo con resolución por `aliases`, clasificando en STRICT (error, bloquea) e INFO/COVERAGE (aviso).
- Normaliza autoinclusión en `compatibleModels` (ruido, no divergencia).
- **Resultado: 13 modelos comparados · 0 errores STRICT · 26 INFO** (categorías pendientes de decisión, §9.2).

**D. Eliminación de forks internos (WebUI → packages/core)**
- `Scripts/build_core_web.js` extendido: genera `WebUI/src/contracts/gen/core.gen.js` (fingerprint + library + search empaquetados desde `packages/core/src/index.js`, que ahora reexporta `FINGERPRINT_VERSION`).
- `WebUI/src/core/{fingerprint,searchPatches,libraryOperations}.js` convertidos en **shims de reexportación** (patrón ya usado por `modelContracts.js`): fuente única en `packages/core`, artefacto `gen/` para servir estático (sirv + importmap, sin bundler).
- `packages/core/src/index.js` exporta la API pública completa.
- Copia muerta eliminada del alcance: `WebUI/src/components/contracts/` (sin importadores, sep-01) identificada como sobrante.

**E. packages/ui formalizado**
- `packages/ui/{package.json,src/index.js}` con exports reales (`BankManagerModal`, `modelSelector`, `panelFactory` — fuente única que hoy se duplica en ABDMS2000 por copia+iframe).
- `packages/ui/tests/drift.test.js`: falla si las fuentes reales divergen del contrato de exports (anti-drift hasta que MS2000 consuma el paquete).

**F. Toolchain pnpm unificada**
- `ABDSynths/pnpm-workspace.yaml` raíz incluye ahora `ABDBankManager` (y sus `packages/*`).
- Eliminado `package-lock.json` de BankManager (quedaba `pnpm-lock.yaml` como único lockfile).

### 9.2 Tabla de divergencias de categorías (caso a caso) — ✅ APLICADA

El TS usa un set genérico interno `[Bass,Lead,Pad,FX,Keys,Perc,Synth,Other]`; el JSON usa taxonomías fieles al hardware. Decisión transversal aplicada: **el JSON (SSOT normativo) manda**, y el TS define un *mapping* UI cuando su organización genérica lo necesite. Estado: **aplicada en el TS el 2026-09-08** (verificado con `sync_contracts.mjs`: 0 STRICT, 0 INFO de categorías):

| Modelo | TS (runtime) | JSON (SSOT) | Análisis | Recomendación |
|---|---|---|---|---|
| `korg-ms2000` | genérico | `Bass,Lead,Pad,Motion,Arp,Split,Bell,Other` | El hardware no clasifica patches; los nombres reflejan la convención Korg MS2000 (Motion/Arp/Split/Bell) | **JSON manda**; mapping en TS si la UI lo requiere |
| `korg-prophecy` | genérico | `Solo,Bass,Lead,Brass,Reed,Strings,Bell,Other` | Nombres fieles a la Prophecy (monofónica: Solo/Reed) | **JSON manda** |
| `roland-juno106` | genérico | `Bass,Lead,Pad,Strings,Brass,FX,Organ,Other` | `Organ` refleja el carácter Juno (polysquares/órganos) | **JSON manda** |
| `behringer-deepmind12` | genérico | `Bass,Lead,Pad,Strings,Brass,FX,Keys,Other` | Espejo del navegador de patches DeepMind | **JSON manda** |
| `behringer-deepmind6` | genérico + **`defaultCategory: 'UNK'`** | igual deepmind12 | `UNK` es un **bug TS** (categoría inválida en UI) | **JSON manda** + corregir `UNK` → `Other` |
| `behringer-pro800` | genérico | `Bass,Lead,Pad,Strings,Brass,FX,Poly,Other` | `Poly` como categoría estilo Sequential | **JSON manda** |
| `yamaha-dx7` | genérico | `Piano,Brass,Strings,Bass,Lead,Guitar,Perc,Other` | Grupos clásicos de la ROM DX7 | **JSON manda** |
| `yamaha-dx7ii` | hereda genérico | igual DX7 | Cohere con DX7 | **JSON manda** |
| `roland-aira-bitrazer` | `[Patch,Other]` (placeholder) | `Filter,Crusher,Delay,Custom` | AIRA son efectos: la taxonomía de síntesis no aplica | **JSON manda**; eliminar placeholder |
| `roland-aira-torcido` | idem | `Distortion,Overdrive,Custom` | idem | **JSON manda** |
| `roland-aira-demora` | idem | `Delay,Buffer,Custom` | idem | **JSON manda** |
| `roland-aira-scooper` | idem | `Looper,Scatter,Custom` | idem | **JSON manda** |
| *defaultCategory AIRA* | `Patch` | `Custom` | `Custom` es más honesto que `Patch` para efectos | **JSON manda** |

### 9.3 Verificación final

| Comprobación | Resultado |
|---|---|
| `scripts/sync_contracts.mjs` | **0 STRICT**, 26 INFO (tabla §9.2) |
| 22 contratos JSON vs schema | **22/22 OK** |
| `Scripts/build_core_web.js` | gen `library.gen.js` + `core.gen.js` + 3 shims ✓ |
| Suites de lo tocado (core, ui/drift, fingerprint, libraryOperations, searchPatches, persistence) | **6 ficheros · 104 tests ✓** |
| Suite completa | 816/860 — los 44 fallos son **pre-existentes** y ajenos a esta iteración: `rolandJunoAdapter` (mapeo de direcciones en fixtures reales), `casioCzAdapter` (checksums de fixtures factory), `bridgeManager` (entorno jsdom: `window.addEventListener`) |

**Pendiente (siguiente iteración):** que ABDMS2000 consuma `@bankmanager/ui` por paquete — ✅ hecho (ver §9.5).

### 9.6 Reparación de las 3 suites pre-existentes — ✅ SUITE COMPLETA VERDE (2026-09-08)

**Resultado: 51/51 ficheros · 860/860 tests ✓** (antes: 44 fallos). Causas raíz y correcciones:

| Suite | Causa raíz | Corrección |
|---|---|---|
| `bridgeManager.test.js` (2) | El gancho `beforeunload` a nivel de módulo llamaba `window.addEventListener` sin comprobar que exista (los tests usan un shim mínimo de `window`) | Guardia refinada: `typeof window.addEventListener === 'function'` (intención original del guard) |
| `casioCzAdapter.test.js` (9) | Tres bugs del contrato TS: (a) checksum CZ cubre desde el byte 6 (canal), no 7 — 0/16 vs 16/16 mensajes; (b) `verifyChecksum` trataba el banco completo (16 mensajes concatenados) como un mensaje; (c) el byte de modelo varía (0x12-0x15) y el contrato canónico cz101 rechazaba los otros tres modelos | `isCasioSysEx` acepta la familia vía tabla `MODEL_BY_BYTE` (con tamaño de datos 128/288B por modelo); `verifyChecksum` valida todos los mensajes del fichero; `buildPatchSysEx` calcula el checksum sobre la región correcta; `parseFile` reporta el modelo real detectado |
| `rolandJunoAdapter.test.js` (33) | (a) Los fixtures generados contenían datos de 8 bits (ilegales en SysEx MIDI, dominio 7-bit) que rompían el framing F0..F7 al concatenar 64 frames; (b) el frame single-patch Juno no lleva modelId y el parseador asumía siempre juno106; (c) split naive F0/F7 perdía frames | Generador corregido a dominio 7-bit (`& 0x7F`) + fixtures Juno/HS60 regenerados; parseador con recuperación por stride fijo (frames de 23B con anclas validadas); adapter resuelve el modelo por nombre de fichero (`Juno106_…` → juno106) |

Notas: los fixtures CZ/MS2000/Prophecy NO se regeneraron (sus suites ya pasaban); el parser de stride fijo es defense-in-depth para ficheros reales con datos que contengan F0/F7. Verificado además: validador de contratos sigue en 0 STRICT / 0 INFO.

### 9.5 ABDMS2000 consume `@abdsynths/bank-manager-ui` — ✅ (2026-09-08)

El modal embebible ya NO se mantiene a mano en ABDMS2000:

- **Fuente única:** `ABDBankManager/packages/ui/src/BankManagerModal.{js,css}` (paquete `@abdsynths/bank-manager-ui`).
- **Sincronización automática:** nuevo `Scripts/sync_bankmanager_ui.js`, invocado en `start.bat` y `build.bat` junto al resto de syncs.
- **Anti-drift:** nuevo test `WebUI/tests/unit/bankManagerModalSync.test.js` (comparación byte a byte contra el paquete; falla indicando el comando de re-sincronización). Verificado: mutación → fallo → re-sync → OK.
- **`app.js` sin cambios funcionales:** el import relativo `./components/bank/BankManagerModal.js` ahora apunta al artefacto sincronizado del paquete (el fichero era idéntico byte a byte, cero cambio de comportamiento).
- **El iframe se mantiene** como transporte del SPA del Bank Manager (aislamiento y same-origin en WebView2 nativo). Sustituirlo por bundle directo es alcance de la Fase 3 (Vite bundle + embeber `dist`).

> **Nota sobre `defaultCategory: 'UNK'`:** corregido en `behringer-dm12d.ts` y `behringer-dm6.ts` ('Other'). El fallback `'UNK'` del modelo de datos (`packages/core/src/models/Patch.js` y `PersistenceEngine.ts`) es otra capa (default de librería para patches sin categorizar) y se deja intacto deliberadamente.

### 9.4 Contratos JSON de variantes — ✅ COBERTURA COMPLETA (2026-09-08)

Creados los 9 contratos JSON que faltaban en `ABDSharedAssets/contracts/` (SSOT de las variantes TS marcadas COVERAGE):

| Contrato nuevo | Familia | Datos distintivos (SSOT del TS) |
|---|---|---|
| `casio_cz1000.json` | Casio CZ | byte legacy 0x13 |
| `casio_cz5000.json` | Casio CZ | 32×2 bancos, byte 0x14 |
| `casio_cz1.json` | Casio CZ | 64×4 bancos, patch 288B, byte 0x15 |
| `roland_juno60.json` | Roland Juno | byte legacy 0x3D |
| `roland_juno6.json` | Roland Juno | byte legacy 0x3C |
| `roland_hs60.json` | Roland Juno | byte legacy 0x3E |
| `korg_microkorg.json` | Korg MS2000 | mismo formato SysEx, byte 0x58 |
| `abd_sm002.json` | Korg MS2000 | **softsynth** (`deviceType: MOCK_DSP`, `isSoftsynth: true`, sin puerto MIDI) |
| `behringer_deepmind12d.json` | DeepMind | mismo formato que DM12 (00 20 32 / 0x20) |

Decisiones de diseño:
- **Contratos lean**: solo `midiIdentification` + `bankManagement` (schema 2.0). Las `functions`/recetas de AudioLab quedan en los contratos base de familia; el consumidor resuelve por `compatibleModels`/`aliases`.
- **`compatibleModels` simétrico**: los clones TS heredaban la lista del padre (asimétrico, sin autoinclusión del hermano). Corregidos en TS (`casio-cz.ts`, `roland-juno.ts`, `korg-ms2000.ts`) para declarar la familia completa en cada variante, espejo de los JSON. Efecto semántico: un banco CZ-101 ahora es visible navegando bajo CZ-1000 aunque no lleve `hardwareIds` (correcto: un blob CZ es válido en cualquier CZ).
- **Test actualizado**: `multiHardware.test.js` documentaba la asimetría antigua como comportamiento esperado; actualizado a la semántica de familia simétrica (53/53 ✓).
- Los `portNameMatches` de variantes sin hardware real (Juno-60/6 por SysEx, HS-60) son best-effort; el byte legacy del TS manda en `sysexProtocol.modelIdByteHex`.

Verificación: **22 modelos comparados · 0 STRICT · 0 INFO/COVERAGE**; 32 JSON parsean OK; suite completa sin regresiones (816 ✓ / 44 pre-existentes).

## 10. Fase 3 + SynthCore — implementación (2026-09-09)

### 10.1 Fase 3 — WebUI empaquetado con Vite y `dist` embebido en nativo — ✅

- **`WebUI/vite.build.config.js`** (nuevo): `vite build` de producción para el WebUI nativo. Resuelve los bare imports `@abdsynths/shared` (el cascade de tokens compartidos ya llega al binario), emite `WebUI/dist/` con nombres estables (`assets/index.js`, `assets/index.css`) y copia los estáticos (`assets/images`, excluyendo `bankmodels` como el glob histórico). Plugin propio de copia con `fs.cpSync` (las `assets/images` son junctions NTFS; `fs.copyFile` falla con EPERM).
- **`Scripts/build_webui.js`**: ahora ejecuta el build Vite tras el versionado (paso [3/6] de `build.bat`), de modo que `dist` lleva la info de build fresca.
- **`CMakeLists.txt`**: `juce_add_binary_data(WebUIAssets ...)` embebe `WebUI/dist/**` en vez de `src/**` (fail-fast si falta `dist/index.html`; excluye `assets/images` — se sirven via `bankwebui://`). Los vendor JS (dexie/jszip/file-saver) solo los usa el SPA de abdbank (servido desde disco), así que salen del binario.
- **ResourceProvider sin cambios**: el Pass-2 por sufijo resuelve `WebUI/dist/index.html` → `/index.html`, `WebUI/dist/assets/index.js` → `/assets/index.js`, etc.
- Verificado: bundle OK (index.js 208 kB, index.css 76 kB, tokens `--color-bg-base` inlinados), build nativo completo (Standalone + VST3) y suite JS de MS2000 sin regresiones.

### 10.2 SynthCore — módulo DSP compartido en ABDSharedCode — ✅

**Extracción a `ABDSharedCode/SynthCore/` (namespace `abd::synth`, 100% JUCE-free):**

| Módulo | Origen | Notas |
|---|---|---|
| `PolyBLEP.{h,cpp}` | ABDMS2000 `Source/DSP/Common` | idéntico modulo namespace |
| `DSPUtils.h` | ABDMS2000 `Source/DSP/Common` | header-only; también resuelve el include de LFO |
| `EnvelopeCurves.h` | ABDMS2000 `Source/DSP/Envelopes` | header-only |
| `ADSREnvelope.{h,cpp}` | ABDMS2000 `Source/DSP/Envelopes` | idéntico modulo namespace |
| `PortamentoGlide.{h,cpp}` | ABDMS2000 `Source/DSP/Modulation` | idéntico modulo namespace |
| `LFO.{h,cpp}` | ABDMS2000 `Source/DSP/Modulation` | include normalizado (`DSPUtils.h` local) |
| `AudioThreadSnapshot.h` | ABDMS2000 `Source/Core` | header-only |
| `VoiceAllocator.h` | **ABDSharedCode/LutDSP** | **convergido** a `abd::synth` (ver abajo) |

- **Consumo MS2000 por shims de compatibilidad**: los 7 headers históricos (`Source/DSP/Common/PolyBLEP.h`, `DSPUtils.h`, `Source/DSP/Envelopes/*`, `Source/DSP/Modulation/{LFO,PortamentoGlide}.h`, `Source/Core/AudioThreadSnapshot.h`) son ahora re-exports (`using` de tipos, namespaces anidados para `DSPUtils::`/`EnvelopeCurves::` — los originales eran namespaces anidados de `ABDMS2000` y el código llama cualificado). Includes por include-dir (`#include "SynthCore/X.h"`), no rutas relativas frágiles entre repos.
- **Los 4 `.cpp` duplicados eliminados** del repo MS2000 (`git rm`); CMake de plugin, tests y **wasm** consumen `ABDShared::SynthCore` / los fuentes compartidos. Diff namespace-normalizado previo: idénticos.
- **VoiceAllocator convergido**: trasladado de `LutDSP` a `SynthCore` (`abd::synth::VoiceAllocator<MaxVoices>`); `LutDSP/VoiceAllocator.h` queda como shim `abd::lutdsp` (template alias) — sin consumidores rotos (tenía cero consumidores; cubierto por test del alias).
- **VoiceManager de MS2000 NO se extrae**: acoplado al DSP chain de `Voice.h` (robbing strategy, latch, portamento por voz). Documentado como convergencia futura con `abd::synth::VoiceAllocator` si se generaliza la política de robo.
- **CMake**: nuevo target `ABDShared_SynthCore` (STATIC) + alias `ABDShared::SynthCore` + target de tests `ABDShared_SynthCore_Tests` (opt-out con `ABDSHAREDCODE_BUILD_SYNTHCORE_TESTS=OFF`).
- **Tests nuevos**: `SynthCoreTests.cpp` standalone (67 checks: DSPUtils, PolyBLEP, curvas+ADSR, portamento, LFO, snapshot, allocator + shim LutDSP). Las expectativas se calibraron contra el comportamiento real del código de producción (byte-idéntico al que MS2000 ya distribuye): rampa cuadrática PolyBLEP antes del step, multiplicadores de decaimiento por muestra, release mínimo 5 ms, τ empírico del glide ~7.5 s para param 0.8, y round-robin que continua el ciclo en vez de reusar al instante la voz liberada.

**Verificación:**
- `ABDShared_SynthCore_Tests`: **67/67 ✓** (Debug, vía build-test de MS2000)
- `ABDMS2000_Tests` (Release, consumiendo el módulo compartido): **103/103 ✓**
- Build nativo completo: **Standalone + VST3 ✓** con el embed `dist` (Fase 3) y SynthCore enlazado
- Sin refs huérfanas a los `.cpp` eliminados (plugin, tests y wasm repuntan a SynthCore)

### 10.3 Política de robo MS2000 generalizada en `abd::synth::VoiceAllocator` — ✅ (2026-09-09)

La escalera de robo del `VoiceManager` de ABDMS2000 (referencia de la casa) se generalizó al agregador compartido **sin acoplarlo a ningún tipo Voice**:

- **`StealHint`** (voice-agnóstico): `slotIndex, midiNote, isVoiceActive, isKeyHeld, isLatched, isReleasing, envelopeLevel, triggerStamp` — el host alimenta una tabla por slot antes de cada consulta.
- **`StealPolicy`**: `LegacyTimestamp` (comportamiento previo, byte-compatible: libre-desde-RR → FIFO por timestamp) | `Ms2000Ladder` (escalera completa: **1.** protección de nota repetida → **2.** voz libre → **3.** robo de latch HOLD/pedal → **4.** robo en fase de release por nivel de envolvente → **5.** FIFO de sostenidas → **6.** fallback RR).
- **`findVoiceToSteal(hints, count, note)`** no muta estado; el host aplica el resultado. **`commitAllocation`/`markSlotReleased`** sincronizan el estado sombra y el cursor RR en el flujo real (pick → apply → commit → rebuild hints).
- **ABDEep** consume el módulo: su `findFreeVoice()` artesanal (libre → release → RR con `static` — bug latente multi-instancia) ahora delega en `voiceAlloc` + `StealHint`; el bug del RR `static` desaparece con el cursor por instancia.
- **ABDMS2000** permanece como implementación de referencia (no se toca su `VoiceManager`).

**Verificación:** `ABDShared_SynthCore_Tests`: **78/78 ✓** (11 checks nuevos de la escalera: retrigger, latch, release-por-nivel, FIFO, fallback con commitAllocation, no-mutación y protección interna sin hints).
