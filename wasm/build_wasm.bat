@echo off

echo =======================================================
echo          ABDMS2000 - Compilacion WebAssembly (WASM)
echo =======================================================

where emcmake >nul 2>&1
if not errorlevel 1 goto :EMSDK_OK

REM Intentar activar emsdk via emsdk_env.bat (D:\desarrollos\emsdk, %EMSDK% o C:\emsdk)
if exist "D:\desarrollos\emsdk\emsdk_env.bat" call "D:\desarrollos\emsdk\emsdk_env.bat"
if defined EMSDK if exist "%EMSDK%\emsdk_env.bat" call "%EMSDK%\emsdk_env.bat"
if exist "C:\emsdk\emsdk_env.bat" call "C:\emsdk\emsdk_env.bat"

where emcmake >nul 2>&1
if not errorlevel 1 goto :EMSDK_OK

REM Fallback: agregar las rutas manualmente al PATH si emcmake sigue sin estar en PATH.
if exist "D:\desarrollos\emsdk\upstream\emscripten\emcmake.bat" (
    set "PATH=D:\desarrollos\emsdk;D:\desarrollos\emsdk\upstream\emscripten;%PATH%"
    goto :EMSDK_OK
)
if exist "D:\desarrollos\emsdk\upstream\emscripten\emcmake.exe" (
    set "PATH=D:\desarrollos\emsdk;D:\desarrollos\emsdk\upstream\emscripten;%PATH%"
    goto :EMSDK_OK
)
if exist "C:\emsdk\upstream\emscripten\emcmake.bat" (
    set "PATH=C:\emsdk;C:\emsdk\upstream\emscripten;%PATH%"
    goto :EMSDK_OK
)
if exist "C:\emsdk\upstream\emscripten\emcmake.exe" (
    set "PATH=C:\emsdk;C:\emsdk\upstream\emscripten;%PATH%"
    goto :EMSDK_OK
)

echo [ERROR] Emscripten SDK (emcmake) no encontrado en PATH, D:\desarrollos\emsdk ni C:\emsdk.
echo Asegurate de haber activado el entorno de Emscripten (emsdk_env.bat).
exit /b 1

:EMSDK_OK

REM Correr desde este directorio (wasm/) sin cambios de cwd: -S . -B build_wasm.
REM (La version anterior hacia cd build_wasm + -S ..\wasm, que resuelve a
REM wasm/wasm: inexistente. Ademas los artefactos copian a ..\WebUI.)
cd /d "%~dp0"

if not exist build_wasm mkdir build_wasm

echo [1/3] Configurando CMake con Emscripten (Ninja)...
call emcmake cmake -G Ninja -S . -B build_wasm -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :ERROR

echo [2/3] Compilando modulo ms2000_dsp.js...
cmake --build build_wasm --target ms2000_dsp
if errorlevel 1 goto :ERROR

echo [3/3] Copiando artefactos a WebUI/src/wasm/...
if not exist ..\WebUI\src\wasm mkdir ..\WebUI\src\wasm
copy /Y build_wasm\ms2000_dsp.* ..\WebUI\src\wasm\ >nul

echo =======================================================
echo  Compilacion WebAssembly finalizada con exito!
echo =======================================================
exit /b 0

:ERROR
cd ..
echo [ERROR] Fallo en la compilacion WASM.
exit /b 1
