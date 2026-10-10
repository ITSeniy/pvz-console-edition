@echo off
cd /d "%~dp0"
if not exist "runtime-data\minigames-test\user\584109FF\profile\User\63E83FFF" (
    echo Import the test save first. See docs\minigames-save.md.
    pause
    exit /b 1
)
set "PSModulePath="
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0run.ps1" -Mouse -UserDataRoot "runtime-data/minigames-test/user" --pvz_test_unlock=true %*
exit /b %ERRORLEVEL%
