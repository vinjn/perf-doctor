pyinstaller.exe -p tidevice\tidevice -F tidevice\tidevice\__main__.py -n tidevice.exe --distpath bin

cd %~dp0\vc2019
set proj=perf-doctor.sln
set msbuild="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
set msbuild2="D:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe"
REM msbuild %proj% /p:Configuration=Debug /p:Platform=x64 /v:minimal /m
%msbuild% %proj% /p:Configuration=Release /p:Platform=x64 /v:minimal /m
%msbuild2% %proj% /p:Configuration=Release /p:Platform=x64 /v:minimal /m
REM msbuild %proj% /p:Configuration=Debug_Shared /p:Platform=x64 /v:minimal /m
REM msbuild %proj% /p:Configuration=Release_Shared /p:Platform=x64 /v:minimal /m
cd ..