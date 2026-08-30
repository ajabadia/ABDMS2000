@echo off

echo =======================================================
echo          ABDMS2000 - Compilacion WebAssembly (WASM)
echo =======================================================

where emcmake >nul 2>&1
if not errorlevel 1 goto :EMSDK_OK

if exist C:\emsdk\emsdk_env.bat call C:\emsdk\emsdk_env.bat

where emcmake >nul 2>&1
if not errorlevel 1 goto :EMSDK_OK

echo [ERROR] Emscripten SDK (emcmake) no encontrado en PATH ni en C:\emsdk.
echo Asegurate de haber activado el entorno de Emscripten (emsdk_env.bat).
exit /b 1

:EMSDK_OK

if not exist build_wasm mkdir build_wasm

echo [1/3] Configurando CMake con Emscripten (Ninja)...
cd build_wasm
call emcmake cmake -G Ninja -S ..\wasm -B . -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :ERROR

echo [2/3] Compilando modulo ms2000_dsp.js...
call ninja
if errorlevel 1 goto :ERROR

cd ..

echo [3/3] Copiando artefactos a WebUI/src/wasm/...
if not exist WebUI\src\wasm mkdir WebUI\src\wasm
copy /Y build_wasm\ms2000_dsp.* WebUI\src\wasm\ >nul

echo =======================================================
echo  Compilacion WebAssembly finalizada con exito!
echo =======================================================
exit /b 0

:ERROR
cd ..
echo [ERROR] Fallo en la compilacion WASM.
exit /b 1
