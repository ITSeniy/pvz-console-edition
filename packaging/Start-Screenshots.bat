@echo off
rem Screenshot mode: the game renders at 2x (2560x1440 in the default window)
rem and F12 saves the game picture, without the Windows cursor, as a PNG
rem in the "screenshots" folder next to this file.
cd /d "%~dp0"
echo Screenshot mode: press F12 to save a PNG to the screenshots folder.
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Mouse --resolution_scale=2 %*
set "PVZ_EXIT=%ERRORLEVEL%"
if not "%PVZ_EXIT%"=="0" (
    echo Game exited with code %PVZ_EXIT%. Please send the latest file from logs.
    pause
)
exit /b %PVZ_EXIT%
