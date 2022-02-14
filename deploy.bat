set mydate=%date:/=%
set TIMESTAMP=%mydate: =_%
set OUTPUT=perf-doctor-%TIMESTAMP%
rmdir /S /Q %OUTPUT%
mkdir %OUTPUT%
set SRC=bin/

robocopy %SRC%/ %OUTPUT% *.bat
robocopy %SRC%/ %OUTPUT% *.py
robocopy %SRC%/ %OUTPUT% perf-doctor.exe
robocopy %SRC%/ %OUTPUT% unreal-cmd.txt
robocopy %SRC%/ %OUTPUT% UE4CommandLine.txt
robocopy %SRC%/ %OUTPUT% perfetto-template.txt
robocopy %SRC%/ %OUTPUT% *.ini
robocopy %SRC%/adb %OUTPUT%/adb *