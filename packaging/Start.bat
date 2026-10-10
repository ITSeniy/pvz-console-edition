@echo off
cd /d "%~dp0"
rem Let Windows PowerShell restore its module paths if launched from PowerShell 7.
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Mouse %*
set "PVZ_EXIT=%ERRORLEVEL%"
if not "%PVZ_EXIT%"=="0" (
    echo Game exited with code %PVZ_EXIT%. Please send the latest file from logs.
    pause
)
exit /b %PVZ_EXIT%
