@echo off
setlocal
cd /d "%~dp0"

rem Command-line build: CMake + Ninja. No .sln, no .vcxproj, no IDE.
rem cl.exe needs the MSVC environment, so pull in vcvars64 first.

if /i "%~1"=="clean" (
    if exist build rmdir /s /q build
    echo removed build\
    exit /b 0
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo build: vswhere.exe not found - is Visual Studio installed?
    exit /b 1
)

set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"

if not defined VSDIR (
    echo build: no Visual Studio install with the C++ toolset
    exit /b 1
)

call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
if errorlevel 1 (
    echo build: vcvars64.bat failed
    exit /b 1
)

set "CONFIG=Release"
if /i "%~1"=="debug" set "CONFIG=Debug"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%CONFIG%
if errorlevel 1 exit /b 1

cmake --build build
if errorlevel 1 exit /b 1

echo.
echo built build\bin\seamcarve.exe
pause