@echo off
setlocal
cd /d "%~dp0"
set "PVZ_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%PVZ_VSWHERE%" -latest -products * -property installationPath`) do set "PVZ_VS=%%i"
if not defined PVZ_VS exit /b 1
call "%PVZ_VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
set "PVZ_CMAKE=%PVZ_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "PVZ_NINJA=%PVZ_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
set "PVZ_CLANG=%PVZ_VS%\VC\Tools\Llvm\x64\bin\clang-cl.exe"
if "%1"=="configure" (
    "%PVZ_CMAKE%" -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo "-DCMAKE_CXX_COMPILER=%PVZ_CLANG%" "-DCMAKE_MAKE_PROGRAM=%PVZ_NINJA%" "-DCMAKE_PREFIX_PATH=%CD%\tools\rexglue-sdk\win-amd64" -DREXSDK_VERSION=0.10.0
) else if "%1"=="test" (
    "%PVZ_VS%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\ctest.exe" --test-dir build --output-on-failure
) else (
    "%PVZ_CMAKE%" --build build --parallel 4 %*
)
exit /b %errorlevel%
