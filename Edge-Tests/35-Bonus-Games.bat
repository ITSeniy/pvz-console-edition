@echo off
rem 35-Bonus-Games
rem Mini-Games with the Bonus Games entry unlocked (normally all 20 trophies).
rem Bonus Games: list, icons, scrolling, Back returns to Mini-Games.
rem Start a bonus game; after a win the game should return to Bonus Games.
call "%~dp0_launch.bat" 35-Bonus-Games "challenge=1" --pvz_test_bonus_unlocked=true %*
