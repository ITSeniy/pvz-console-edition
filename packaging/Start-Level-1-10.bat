@echo off
rem Adventure from level 1-10 (fresh first playthrough: plants of 1-1..1-9).
rem The save is copied once to runtime-data\level-1-10\user and then keeps your
rem progress. Delete that folder to start again from 1-10.
cd /d "%~dp0"
set "SAVE=runtime-data\level-1-10\user"
if not exist "%SAVE%\584109FF" (
    robocopy "saves\level-1-10" "%SAVE%" /E /NFL /NDL /NJH /NJS /NP >nul
    if errorlevel 8 (
        echo Could not copy the 1-10 save to %SAVE%.
        pause
        exit /b 1
    )
)
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Mouse -UserDataRoot "runtime-data/level-1-10/user" --pvz_test_unlock=true %*
set "PVZ_EXIT=%ERRORLEVEL%"
if not "%PVZ_EXIT%"=="0" (
    echo Game exited with code %PVZ_EXIT%. Please send the latest file from logs.
    pause
)
exit /b %PVZ_EXIT%
