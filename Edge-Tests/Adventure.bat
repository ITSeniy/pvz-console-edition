@echo off
rem Any Adventure level: Adventure.bat <1..50>  (1-1 = 1, 5-10 = 50)
if "%~1"=="" ( echo Usage: Adventure.bat ^<1..50^> & pause & exit /b 1 )
call "%~dp0_launch.bat" adventure-%~1 "adventure=%~1"
