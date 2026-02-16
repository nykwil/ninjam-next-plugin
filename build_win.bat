@echo off
setlocal

set "VSROOT="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if exist "%VSWHERE%" (
  for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
)

if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\18\Insiders" set "VSROOT=C:\Program Files\Microsoft Visual Studio\18\Insiders"
if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\2022\BuildTools" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\BuildTools"
if not defined VSROOT if exist "C:\Program Files\Microsoft Visual Studio\2022\Community" set "VSROOT=C:\Program Files\Microsoft Visual Studio\2022\Community"

if not defined VSROOT (
  echo Could not find a Visual Studio installation with C++ tools.
  exit /b 1
)

call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
if errorlevel 1 exit /b 1

cmake --build build --target NinjamVST3_VST3
if errorlevel 1 exit /b 1
