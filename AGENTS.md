# AGENTS.md

## BankManager y modales en ABDMS2000

- No asumir que “BankManagerModal” es una sola pieza. En este proyecto parece haber al menos dos: el contenedor modal propio del WebUI principal de ABDMS2000 y el modal interno del iframe bancario.
- El iframe bancario no es un recurso externo ligero. Parece tener su propia estructura en `WebUI/abdbank/` (src/, bridge/, vendor/, contracts/, components/, etc.).
- Cuando `WebUI/abdbank/index.html`, `app.js` o `bridge/bridgeManager.js` parecen “no estar”, verifica primero si es problema de lectura en sesión y no ausencia real de la carpeta. `WebUI/abdbank/` puede ser visible con subcarpetas aunque algunos archivos estén bloqueados.
- `ABDBankManager/` no está en este checkout. No asumir que puedes leer `ABDBankManager/packages/ui/src/index.js` o confirmar exports del paquete UI desde ABDMS2000 cuando el repo hermano no está clonado.
- El puente parece pasar por `window.__synthBridge` / `window.parent.__synthBridge`. El modal del synth parece exponerlo cuando recibe `options.synthBridge`. Parece una relación real entre el modal del synth y el iframe bancario.

## Canales bancarios y sync

- Parece haber sync separados con roles distintos: uno ligado al modal del synth y otro ligado al contenido bancario del iframe. No tratar todos los “sync de bank” como un mismo canal.
- El ResourceProvider parece tener una rama dedicada a `bankwebui://` y una precaución explícita para que `abdbank/index.html` no colisione con el `index.html` del synth. Eso parece una razón real del canal bancario separado, no solo un detalle pendiente de limpiar.

## Filosofía del usuario para BankManager

- El objetivo declarado parece ser una única fuente de verdad para BankManager, preferiblemente en `ABDSharedCode` / `ABDSharedAssets`, no mantener copias divergentes ni sincronizaciones que terminan duplicando y desincronizando.
- Si hay evolución de una copia en otro sitio, el criterio parece ser fusionar o eliminar lo peor, quedándose con la mejor versión o una fusión de versiones, no mantener forks paralelos.
