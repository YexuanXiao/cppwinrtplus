rem @echo off

if not exist ".\.nuget" mkdir ".\.nuget"
if not exist ".\.nuget\nuget.exe" powershell -Command "$ProgressPreference = 'SilentlyContinue' ; Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile .\.nuget\nuget.exe"

call .nuget\nuget.exe restore cppwinrt.slnx
call .nuget\nuget.exe restore natvis\cppwinrtvisualizer.slnx
call .nuget\nuget.exe restore test\nuget\NugetTest.slnx

set target_version=%1
if "%target_version%"=="" set target_version=3.0.0.0

call msbuild /m /p:Configuration=Release,Platform=x86,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:fast_fwd
call msbuild /m /p:Configuration=Release,Platform=x64,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:fast_fwd
call msbuild /m /p:Configuration=Release,Platform=arm64,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:fast_fwd

rem Build cppwinrt.exe for x86, x64 and arm64
call msbuild /m /p:Configuration=Release,Platform=x86,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:cppwinrt
call msbuild /m /p:Configuration=Release,Platform=x64,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:cppwinrt
call msbuild /m /p:Configuration=Release,Platform=arm64,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:cppwinrt

rem Build nuget
.nuget\nuget pack nuget\YexuanXiao.CppWinRTPlus.nuspec -Properties target_version=%target_version%;cppwinrt_exe_x86=%cd%\_build\x86\Release\cppwinrt.exe;cppwinrt_exe_amd64=%cd%\_build\x64\Release\cppwinrt.exe;cppwinrt_exe_arm64=%cd%\_build\arm64\Release\cppwinrt.exe;cppwinrt_fast_fwd_x86=%cd%\_build\x86\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_x64=%cd%\_build\x64\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_arm64=%cd%\_build\arm64\Release\cppwinrt_fast_forwarder.lib
