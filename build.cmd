@echo off
setlocal
set "VSLANG=1033"
set "CYINSIGHTS_CONFIG=%~1"
if "%CYINSIGHTS_CONFIG%"=="" set "CYINSIGHTS_CONFIG=Release"
if not defined CYINSIGHTS_VS (
    for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "CYINSIGHTS_VS=%%i"
)
if not defined CYINSIGHTS_VS (
    echo Install Visual Studio with C++ and CMake, or set CYINSIGHTS_VS.
    exit /b 1
)
call "%CYINSIGHTS_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "VSLANG=1033"
set "CYINSIGHTS_CMAKE=%CYINSIGHTS_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
"%CYINSIGHTS_CMAKE%" -S "%~dp0." -B "%~dp0build\%CYINSIGHTS_CONFIG%" -G Ninja -DCMAKE_MAKE_PROGRAM="%CYINSIGHTS_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe" -DCMAKE_BUILD_TYPE=%CYINSIGHTS_CONFIG% -DCYGI_SOURCE="%~dp0ThirdParty\CyGPUInspector"
if errorlevel 1 exit /b 1
python "%~dp0Tools\check_msvc_dependencies.py" "%~dp0build\%CYINSIGHTS_CONFIG%"
if errorlevel 1 exit /b 1
if "%~2"=="--compile-only" (
    "%CYINSIGHTS_CMAKE%" --build "%~dp0build\%CYINSIGHTS_CONFIG%" --target CyInsightsUi CyGpuAnalysis CyCapture CyImGui CyTimingModel CyRemoteCapture CyCounterModel CyTimingComparison CyNativeStorage
    if errorlevel 1 exit /b 1
    exit /b 0
)
if "%~2"=="--tests-only" (
    "%CYINSIGHTS_CMAKE%" --build "%~dp0build\%CYINSIGHTS_CONFIG%" --target CyComponentTests
    if errorlevel 1 exit /b 1
    "%CYINSIGHTS_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir "%~dp0build\%CYINSIGHTS_CONFIG%" --output-on-failure
    if errorlevel 1 exit /b 1
    exit /b 0
)
"%CYINSIGHTS_CMAKE%" --build "%~dp0build\%CYINSIGHTS_CONFIG%" --target CyInsightsApp CyCaptureTool CyComponentTests
exit /b %errorlevel%
