rem @echo off

nuget >nul 2>&1
if %errorlevel% neq 0 (
    echo NuGet is not available. Please install NuGet CLI from https://www.nuget.org/downloads and add it to PATH.
    exit /b 1
)

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
nuget pack nuget\YexuanXiao.CppWinRTPlus.nuspec -Properties target_version=%target_version%;cppwinrt_exe_x86=%cd%\_build\x86\Release\cppwinrt.exe;cppwinrt_exe_amd64=%cd%\_build\x64\Release\cppwinrt.exe;cppwinrt_exe_arm64=%cd%\_build\arm64\Release\cppwinrt.exe;cppwinrt_fast_fwd_x86=%cd%\_build\x86\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_x64=%cd%\_build\x64\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_arm64=%cd%\_build\arm64\Release\cppwinrt_fast_forwarder.lib
