set mydate=%date:/=%
set TIMESTAMP=%mydate: =_%
set OUTPUT=perf-doctor-%TIMESTAMP%
rmdir /S /Q %OUTPUT%
mkdir %OUTPUT%
set SRC=bin/

robocopy %SRC%/ %OUTPUT% *.bat
robocopy %SRC%/ %OUTPUT% *.dll
robocopy %SRC%/ %OUTPUT% *.py
robocopy %SRC%/ %OUTPUT% *.exe
robocopy %SRC%/ %OUTPUT% *.ini
robocopy %SRC%/adb %OUTPUT%/adb *