@echo off
REM ============================================================
REM start.bat - Servidor de desarrollo local para ABDMS2000 WebUI
REM (Vite dev server reemplaza a sirv-cli, mismo puerto 8384)
REM ============================================================

echo ============================================================
echo  Sincronizando modulos compartidos (Bank Manager, Scope y Assets)...
echo ============================================================
REM NOTA: keyboard.js y utils.js se importan desde @abdsynths/midi-keyb (ABDSharedCode)
REM via Vite workspace. El CSS se importa via JS en app.js. No mantener forks locales.
REM BankManagerModal SI se auto-sincroniza desde ABDBankManager\packages\ui
REM (sync_bankmanager_ui.js): no editar WebUI/src/components/bank/ a mano.
node Scripts/sync_bankmanager.js
node Scripts/sync_bankmanager_ui.js
node Scripts/sync_scope.js
node Scripts/sync_assets.js
echo  OK - Modulos sincronizados.

echo ============================================================
echo  Iniciando Vite Dev Server en Puerto 8384
echo  URL de Acceso: http://localhost:8384
echo ============================================================

npx -y vite --config vite.config.js
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo El servidor se ha detenido o no se pudo iniciar.
)
