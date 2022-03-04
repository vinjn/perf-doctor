set mydate=%date:/=%
set DATE=%mydate: =_%
set dt=%TIME:~0,2%_%TIME:~3,2%_%TIME:~6,2%
set TIMESTAMP=%dt: =0%
set PKG_NAME=com.sina.weibo
IF not "%~1"=="" set PKG_NAME=%~1
call python %~dp0\app_profiler.py -p %PKG_NAME% -r="-e task-clock:u -f 100000 -m 2048 -g --duration 1" --ndk_path %~dp0\ndk
call python %~dp0\report_html.py --ndk_path %~dp0\ndk --report_path %~dp0\%PKG_NAME%_%DATE%_%TIMESTAMP%.html