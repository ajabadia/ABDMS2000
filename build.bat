@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo          ABDMS2000 - Compilacion Release
echo =======================================================

echo [0/6] Sincronizando ABDMIDIKeyb compartido...
xcopy /Y /Q "..\ABDMIDIKeyb\src\keyboard.js" "WebUI\src\components\keyboard.js" >nul 2>&1
xcopy /Y /Q "..\ABDMIDIKeyb\src\utils.js" "WebUI\src\components\utils.js" >nul 2>&1
xcopy /Y /Q "..\ABDMIDIKeyb\src\keyboard.css" "WebUI\src\components\keyboard.css" >nul 2>&1

echo [1/6] Generando registros y contratos...
node Scripts/registry_generator.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al generar los registros.
    exit /b %ERRORLEVEL%
)

echo [2/6] Compilando modulo WebAssembly WASM...
call wasm\build_wasm.bat
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la compilacion WebAssembly WASM.
    exit /b %ERRORLEVEL%
)

echo [3/6] Actualizando version de build y empaquetado WebUI...
node Scripts/build_webui.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al empaquetar WebUI.
    exit /b %ERRORLEVEL%
)

echo [4/6] Configurando CMake...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la configuracion de CMake.
    exit /b %ERRORLEVEL%
)

echo [5/6] Compilando Standalone y VST3...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la compilacion de Standalone y VST3.
    exit /b %ERRORLEVEL%
)

echo =======================================================
echo  [EXITO] Compilacion completada con exito! (6/6)
echo  Standalone: build\ABDMS2000_artefacts\Release\Standalone\ABDMS2000.exe
echo  VST3:       build\ABDMS2000_artefacts\Release\VST3\ABDMS2000.vst3
echo =======================================================

echo.
echo [LANZANDO] Iniciando ABDMS2000 Standalone...
start "" "build\ABDMS2000_artefacts\Release\Standalone\ABDMS2000.exe"
