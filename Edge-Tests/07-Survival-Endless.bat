@echo off
rem 07-Survival-Endless
rem Survival: Day (Endless) with 9990 sun on each new board.
rem After every two flags the seed chooser opens over the lawn:
rem LET'S ROCK!, Menu, quick select/cancel of seeds.
rem Quit to the menu mid-run and come back: "Continue?" dialog.
call "%~dp0_launch.bat" 07-Survival-Endless "progress=50,mode=12,sun=9990" %*
