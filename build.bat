@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo          ABDMS2000 - Compilacion Release
echo =======================================================

echo [0/6] Sincronizando modulos compartidos...
REM NOTA: keyboard.js y utils.js se importan desde @abdsynths/midi-keyb (ABDSharedCode)
REM via Vite workspace. El CSS se importa via JS en app.js. No mantener forks locales.
REM Ver Scripts/SYNC_DOCUMENTATION.md para el inventario completo de sincronizaciones.
node Scripts/sync_bankmanager.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al sincronizar ABDBankManager.
    goto error_exit
)
node Scripts/sync_bankmanager_ui.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al sincronizar BankManagerModal desde packages/ui.
    goto error_exit
)
node Scripts/sync_scope.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al sincronizar ABDScope.
    goto error_exit
)
node Scripts/sync_assets.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al sincronizar ABDSharedAssets.
    goto error_exit
)

echo [1/6] Generando registros y contratos...
node Scripts/registry_generator.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al generar los registros.
    goto error_exit
)

REM Identidad del host (Source/Plugin/HostModelId.gen.h) desde el contrato
REM canonico ya sincronizado en WebUI/abdbank (fuente unica: korgAbdSm002Contract).
node Scripts/generate_host_model_id.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al generar HostModelId.gen.h.
    goto error_exit
)

REM Tramas Korg del contrato (Source/MIDI/KorgChannel.gen.h) desde el mismo contrato
REM sincronizado: el C++ y el TS tienen que direccionar el equipo con el mismo byte
REM (Test 25 de DSPCoreTests.cpp consume estas tramas). Ver Scripts/generate_korg_channel.js.
node Scripts/generate_korg_channel.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al generar KorgChannel.gen.h.
    goto error_exit
)

echo [2/6] Compilando modulo WebAssembly WASM...
pushd wasm
call build_wasm.bat
popd
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la compilacion WebAssembly WASM.
    goto error_exit
)

echo [3/6] Actualizando version de build y empaquetado WebUI...
node Scripts/build_webui.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al empaquetar WebUI.
    goto error_exit
)

echo [4/6] Configurando CMake...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la configuracion de CMake.
    goto error_exit
)

echo [5/6] Compilando Standalone y VST3...
cmake --build build --config Release --target ABDMS2000_All
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la compilacion de Standalone y VST3.
    goto error_exit
)

echo =======================================================
echo  [EXITO] Compilacion completada con exito! (6/6)
echo  Standalone: build\ABDMS2000_artefacts\Release\Standalone\ABDMS2000.exe
echo  VST3:       build\ABDMS2000_artefacts\Release\VST3\ABDMS2000.vst3
echo =======================================================

echo.
echo Presione una tecla para cerrar esta ventana...
pause >nul

echo [LANZANDO] Iniciando ABDMS2000 Standalone...
start "" "build\ABDMS2000_artefacts\Release\Standalone\ABDMS2000.exe"

exit /b 0

:error_exit
echo.
echo [ERROR] Presione una tecla para cerrar esta ventana...
pause >nul
exit /b %ERRORLEVEL%