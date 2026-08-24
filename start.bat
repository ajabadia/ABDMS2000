@echo off
REM ============================================================
REM start.bat - Servidor de desarrollo local para ABDMS2000 WebUI
REM ============================================================

echo ============================================================
echo  Iniciando Servidor Web Local en Puerto 8384
echo  URL de Acceso: http://localhost:8384
echo ============================================================

npx -y sirv-cli WebUI --port 8384 --cors --single --dev
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo El servidor se ha detenido o no se pudo iniciar.
)
