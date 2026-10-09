@echo off
setlocal
cd /d "%~dp0"

echo ========================================================
echo   TinkerThinker Auto-Flasher Launcher
echo ========================================================
echo.

where uv >nul 2>nul
if %ERRORLEVEL% EQU 0 (
    echo [INFO] uv gefunden. Starte auto_flasher mit uv...
    echo.
    uv run --with-requirements requirements.txt python auto_flasher.py %*
    goto end
)

echo [INFO] uv nicht gefunden, pruefe Standard-Python...
where python >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [FEHLER] Weder uv noch Python wurden auf dem System gefunden!
    echo Bitte installiere uv (https://docs.astral.sh/uv/) oder Python.
    echo.
    pause
    exit /b 1
)

if not exist ".venv\Scripts\activate.bat" (
    echo [INFO] Erstelle virtuelles Environment (.venv)...
    python -m venv .venv
    if %ERRORLEVEL% NEQ 0 (
        echo [FEHLER] Virtuelles Environment konnte nicht erstellt werden!
        pause
        exit /b 1
    )
)

call .venv\Scripts\activate.bat

echo [INFO] Pruefe Abhaengigkeiten...
python -m pip install -q -r requirements.txt
if %ERRORLEVEL% NEQ 0 (
    echo [WARNUNG] Pip-Install schlug fehl, versuche Skript dennoch auszufuehren...
)

echo.
python auto_flasher.py %*

:end
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Flasher wurde mit Status %ERRORLEVEL% beendet.
)
echo.
pause
