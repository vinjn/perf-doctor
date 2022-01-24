@echo off
set dt=%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%
set TIMESTAMP=%dt: =0%
set FILENAME=perf_%TIMESTAMP%
IF not "%~1"=="" set FILENAME=%~1
adb exec-out screencap -p > %FILENAME%.png