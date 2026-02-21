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

call msbuild /m /p:Configuration=Release,Platform=x86,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:cppwinrt

nuget pack nuget\Microsoft.Windows.CppWinRT.nuspec -Properties target_version=%target_version%;cppwinrt_exe=%cd%\_build\x86\Release\cppwinrt.exe;cppwinrt_fast_fwd_x86=%cd%\_build\x86\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_x64=%cd%\_build\x64\Release\cppwinrt_fast_forwarder.lib;cppwinrt_fast_fwd_arm64=%cd%\_build\arm64\Release\cppwinrt_fast_forwarder.lib 
