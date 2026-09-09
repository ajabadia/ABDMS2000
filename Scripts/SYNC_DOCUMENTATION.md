# Scripts de Sincronización — Inventario y Documentación

> **Filosofía:** Todo código compartido vive en repositorios hermanos (ABDSharedCode,
> ABDSharedAssets, ABDBankManager, ABDScope). Los scripts sincronizan artefactos
> al directorio de trabajo antes de que Vite o CMake los consuman. **No editar
> manualmente los directorios destino** — son artefactos de sincronización.

---

## Inventario de Scripts

| Script | Origen | Destino | Ejecutado en | Consumido por |
|---|---|---|---|---|
| `sync_bankmanager.js` | `../ABDBankManager/WebUI/` | `WebUI/abdbank/` | start.bat, build.bat | ResourceProvider (bankwebui://), iframe BankManagerModal |
| `sync_bankmanager_ui.js` | `../ABDBankManager/packages/ui/src/` | `WebUI/src/components/bank/` | start.bat, build.bat | app.js (BankManagerModal embebido) |
| `sync_scope.js` | `../ABDScope/WebUI/` | `WebUI/abdscope/` | start.bat, build.bat | oscilloscopeModal.js, scope.css |
| `sync_assets.js` | `../ABDSharedAssets/models/` + `brands/` | `WebUI/images/` | start.bat, build.bat | WebUI (imágenes de modelos, logos de marca) |

---

## Detalle por Script

### 1. `sync_bankmanager.js`

**Qué hace:** Copia el WebUI completo de ABDBankManager (excluyendo configs y tests) a `WebUI/abdbank/`.

**Por qué existe:** El modal del Bank Manager se carga en un iframe con protocolo `bankwebui://`. En el build nativo, el ResourceProvider sirve desde disco (`WebUI/abdbank/`) porque el iframe no puede resolver bare imports `@abdsynths/*`.

**Exclusiones:** `vite.config.js`, `package.json`, `tests/`

**No editar:** `WebUI/abdbank/` es sincronizado completamente en cada ejecución.

---

### 2. `sync_bankmanager_ui.js`

**Qué hace:** Copia `BankManagerModal.js` y `BankManagerModal.css` desde el paquete `@abdsynths/bank-manager-ui` (`ABDBankManager/packages/ui/src/`) a `WebUI/src/components/bank/`.

**Por qué existe:** El BankManagerModal embebido en el synth WebUI necesita resolverse sin bare imports (el WebView2 nativo no tiene resolución de paquetes). Esta copia es el "Opción B" del análisis DRY: materializar el paquete como artefacto sincronizado.

**Exclusiones:** Solo copia los 2 archivos listados.

**No editar:** `WebUI/src/components/bank/` es artefacto de sincronización. El test `bankManagerModalSync.test.js` valida que no diverja.

---

### 3. `sync_scope.js`

**Qué hace:** Copia el WebUI de ABDScope (osciloscopio analítico) a `WebUI/abdscope/`.

**Por qué existe:** El osciloscopio se integra como componente embebido. En el build nativo, se empaqueta como binary data (`ABDScopeWebAssets`). En dev, Vite sirve desde `WebUI/abdscope/`.

**Exclusiones:** `vite.config.js`, `vitest.config.js`, `package.json`, `tests/`, `demo/`, `node_modules/`

**No editar:** `WebUI/abdscope/` es sincronizado completamente.

---

### 4. `sync_assets.js`

**Qué hace:** Copia imágenes de modelos (`ABDSharedAssets/models/`) y logos de marca (`ABDSharedAssets/brands/`) a `WebUI/images/`.

**Por qué existe:** Vite dev server sirve archivos desde `WebUI/`, así que las imágenes deben estar ahí. En el build nativo, se empaquetan como binary data via `GLOB_RECURSE WebUI/dist/*`.

**Exclusiones:** Directorio `_review`

**Post-copia:** Replica placeholders en `images/models/thumbs/` para compatibilidad con rutas anidadas de ABDBankManager.

**Evaluar eliminación:** Si el ResourceProvider aprende a servir directo desde `ABDSharedAssets/` (como ya hace `bankwebui://`), este script podría eliminarse. Por ahora es necesario para Vite dev y el build.

---

## Flujo de Ejecución

```
start.bat (dev):
  sync_bankmanager.js     →  WebUI/abdbank/
  sync_bankmanager_ui.js  →  WebUI/src/components/bank/
  sync_scope.js           →  WebUI/abdscope/
  sync_assets.js          →  WebUI/images/
  vite dev server

build.bat (producción):
  sync_bankmanager.js     →  WebUI/abdbank/
  sync_bankmanager_ui.js  →  WebUI/src/components/bank/
  sync_scope.js           →  WebUI/abdscope/
  sync_assets.js          →  WebUI/images/
  vite build              →  WebUI/dist/  (empaqueta todo)
  cmake                   →  embebe dist/ como binary data
```

---

## Reglas

1. **No editar manualmente** los directorios destino (`abdbank/`, `abdscope/`, `components/bank/`, `images/`).
2. **Origen único:** Cada archivo tiene un solo repositorio fuente. Si necesitas modificar algo, edita en el repo hermano y ejecuta el sync.
3. **Antes de build.bat:** Los sync scripts se ejecutan como paso 0. Si alguno falla, el build se aborta.
4. **En CI/CD:** Los repos hermanos deben estar disponibles (FetchContent o submodules). Los sync scripts verifican existencia y abortan con error claro si falta algún repo.
