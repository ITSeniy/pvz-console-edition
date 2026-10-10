@echo off
setlocal
cd /d "%~dp0"
rem Visual Studio with C++ tools: PVZ_VS if already set, then vswhere, then the
rem standard install folders (vswhere finds nothing when the installer's
rem instance registry is missing). Labels instead of blocks: the expanded
rem ProgramFiles(x86) contains a parenthesis.
if defined PVZ_VS goto :have_vs
set "PVZ_VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%PVZ_VSWHERE%" goto :scan_vs
for /f "usebackq delims=" %%i in (`"%PVZ_VSWHERE%" -latest -prerelease -products * -property installationPath`) do set "PVZ_VS=%%i"
if defined PVZ_VS goto :have_vs
:scan_vs
for /d %%v in ("%ProgramFiles%\Microsoft Visual Studio\*" "%ProgramFiles(x86)%\Microsoft Visual Studio\*") do (
    for /d %%e in ("%%~v\*") do if exist "%%~e\VC\Auxiliary\Build\vcvars64.bat" set "PVZ_VS=%%~e"
)
if defined PVZ_VS goto :have_vs
echo Visual Studio with C++ tools was not found. Set PVZ_VS to its installation folder. 1>&2
exit /b 1
:have_vs
if not exist "%PVZ_VS%\VC\Auxiliary\Build\vcvars64.bat" (
    echo PVZ_VS does not contain VC\Auxiliary\Build\vcvars64.bat: "%PVZ_VS%" 1>&2
    exit /b 1
)
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
