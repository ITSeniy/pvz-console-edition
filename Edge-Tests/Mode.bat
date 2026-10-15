@echo off
rem Any game mode: Mode.bat <id>  (ids in README.md)
if "%~1"=="" ( echo Usage: Mode.bat ^<id^> & pause & exit /b 1 )
call "%~dp0_launch.bat" mode-%~1 "progress=50,mode=%~1,sun=9990"
