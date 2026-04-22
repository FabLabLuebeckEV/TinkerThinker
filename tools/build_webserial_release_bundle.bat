@echo off
setlocal
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_webserial_release_bundle.ps1"
if errorlevel 1 (
  echo.
  echo Bundle-Erstellung fehlgeschlagen.
  pause
  exit /b 1
)
echo.
echo Bundle-Erstellung abgeschlossen.
pause
