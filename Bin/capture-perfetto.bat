@echo off
REM https://android.googlesource.com/platform/frameworks/native/+/refs/tags/android-q-preview-5/cmds/atrace/atrace.cpp
REM https://perfetto.dev/docs/reference/trace-config-proto
set dt=%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%
set TIMESTAMP=%dt: =0%
set FILENAME=perf_%TIMESTAMP%
IF not "%~1"=="" set FILENAME=%~1
set CFG_NAME=/sdcard/p.cfg
set TRACE_NAME=/data/misc/perfetto-traces/trace.perf
@REM adb shell setprop debug.graphics.gpu.profiler.perfetto true
@REM adb shell setprop vendor.debug.egl.profiler 1
@REM adb shell setprop debug.egl.profiler 1
@REM adb shell setprop debug.egl.profiler.perfetto 1
adb push p.cfg %CFG_NAME%
adb shell cat %CFG_NAME% | adb shell perfetto --txt -c - -o %TRACE_NAME%
adb pull %TRACE_NAME% %FILENAME%.perfetto
@REM adb exec-out screencap -p > %FILENAME%.png
adb shell rm %TRACE_NAME%
@REM adb shell setprop debug.graphics.gpu.profiler.perfetto false
@REM adb shell setprop vendor.debug.egl.profiler 0
@REM adb shell setprop debug.egl.profiler 0
@REM adb shell setprop debug.egl.profiler.perfetto 0
echo[
@REM echo [96mscreenshot file: %~dp0%FILENAME%.png[0m
echo [96mperfetto file  : %~dp0%FILENAME%.perfetto[0m
echo [96mperfetto viewer: https://ui.perfetto.dev/#!/[0m