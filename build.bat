@echo off
setlocal enabledelayedexpansion

echo =======================================================
echo          ABDMS2000 - Compilacion Release
echo =======================================================

echo [1/4] Generando registros y contratos...
node Scripts/registry_generator.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al generar los registros.
    exit /b %ERRORLEVEL%
)

echo [2/4] Actualizando version de build y empaquetado WebUI...
node Scripts/build_webui.js
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo al empaquetar WebUI.
    exit /b %ERRORLEVEL%
)

echo [3/4] Configurando CMake...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la configuracion de CMake.
    exit /b %ERRORLEVEL%
)

echo [4/4] Compilando Standalone y VST3...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Fallo en la compilacion.
    exit /b %ERRORLEVEL%
)

echo =======================================================
echo  [EXITO] Compilacion completada con exito!
echo  Standalone: build\ABDMS2000_artefacts\Release\Standalone\ABDMS2000.exe
echo  VST3:       build\ABDMS2000_artefacts\Release\VST3\ABDMS2000.vst3
echo =======================================================
