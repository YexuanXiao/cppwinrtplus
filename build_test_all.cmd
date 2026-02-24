@echo off

set target_platform=%1
set target_configuration=%2
set target_version=%3
set clean_intermediate_files=%4

if "%target_platform%"=="" set target_platform=x64
if "%target_configuration%"=="" set target_configuration=Release
if "%target_version%"=="" set target_version=1.2.3.4

if not exist ".\.nuget" mkdir ".\.nuget"
if not exist ".\.nuget\nuget.exe" powershell -Command "$ProgressPreference = 'SilentlyContinue' ; Invoke-WebRequest https://dist.nuget.org/win-x86-commandline/latest/nuget.exe -OutFile .\.nuget\nuget.exe"

call .nuget\nuget.exe restore cppwinrt.slnx
call .nuget\nuget.exe restore natvis\cppwinrtvisualizer.slnx
call .nuget\nuget.exe restore test\nuget\NugetTest.slnx

call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:fast_fwd

call msbuild /p:Configuration=%target_configuration%,Platform=%target_platform%,Deployment=Component;CppWinRTBuildVersion=%target_version% natvis\cppwinrtvisualizer.slnx
call msbuild /p:Configuration=%target_configuration%,Platform=%target_platform%,Deployment=Standalone;CppWinRTBuildVersion=%target_version% natvis\cppwinrtvisualizer.slnx

if "%target_platform%"=="arm64" goto :eof

call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:cppwinrt

goto :skip_cmake_module_test

echo "Requires CMake 4.3"
set cppwinrt_exe=%CD%\\_build\\%target_platform%\\%target_configuration%\\cppwinrt.exe
set cmake_build_dir=%CD%\\_build\\%target_platform%\\%target_configuration%\\test_cxx_module
if exist "%cmake_build_dir%" rmdir /s /q "%cmake_build_dir%"
call cmake -S test\\test_cxx_module -B "%cmake_build_dir%" -G "Ninja Multi-Config" -DCPPWINRT_EXE="%cppwinrt_exe%"
if errorlevel 1 exit /b %errorlevel%
call cmake --build "%cmake_build_dir%" --config %target_configuration%
if errorlevel 1 exit /b %errorlevel%
call ctest --test-dir "%cmake_build_dir%" -C %target_configuration% --output-on-failure
if errorlevel 1 exit /b %errorlevel%

:skip_cmake_module_test

call msbuild /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% test\nuget\NugetTest.slnx

call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_cpp20
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_cpp20_no_sourcelocation
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_fast
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_slow
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_module_lock_custom
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_module_lock_none
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\test_module_lock_none
call msbuild /m /p:Configuration=%target_configuration%,Platform=%target_platform%,CppWinRTBuildVersion=%target_version% cppwinrt.slnx /t:test\old_tests\test_old

call run_tests.cmd %target_platform% %target_configuration%
