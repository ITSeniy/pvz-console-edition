@echo off
rem Shared launcher: _launch.bat <name> "<pvz_test_start keys>" [extra game args]
rem Each scenario gets a fresh copy of the minigames test save, so a test
rem never changes the save of another one (or your own profile).
setlocal
cd /d "%~dp0.."
set "NAME=%~1"
set "START=%~2"
set "SOURCE=runtime-data\minigames-test\user"
if not exist "%SOURCE%\584109FF" (
    echo The test save is missing: %SOURCE%
    echo See docs\minigames-save.md.
    pause
    exit /b 1
)
set "TARGET=runtime-data\edge-tests\%NAME%\user"
if exist "%TARGET%" rmdir /s /q "%TARGET%"
robocopy "%SOURCE%" "%TARGET%" /E /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 (
    echo Could not copy the test save to %TARGET%.
    pause
    exit /b 1
)
echo Scenario: %NAME%  (%START%)
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "run.ps1" -Mouse -UserDataRoot "runtime-data/edge-tests/%NAME%/user" --pvz_test_unlock=true "--pvz_test_start=%START%" %3 %4 %5 %6
set "PVZ_EXIT=%ERRORLEVEL%"
if not "%PVZ_EXIT%"=="0" (
    echo Game exited with code %PVZ_EXIT%. Please send the latest file from logs.
    pause
)
exit /b %PVZ_EXIT%
