@echo off
setlocal EnableExtensions

set "SCRIPT_DIR=%~dp0"
set "PLUGIN_NAME=NinjamNext.vst3"
set "ARTEFACTS_DIR=%SCRIPT_DIR%build\NinjamNext_artefacts"
set "PLUGIN_BUNDLE="
set "BUILD_CONFIG="
set "HOST_EXE="

for %%C in (Debug RelWithDebInfo Release MinSizeRel) do (
  if exist "%ARTEFACTS_DIR%\%%C\VST3\%PLUGIN_NAME%\Contents\x86_64-win\NinjamNext.vst3" (
    set "PLUGIN_BUNDLE=%ARTEFACTS_DIR%\%%C\VST3\%PLUGIN_NAME%"
    set "BUILD_CONFIG=%%C"
    goto :found_plugin
  )
)

echo Could not find a built %PLUGIN_NAME% under "%ARTEFACTS_DIR%".
echo Build first with: build_win.bat
exit /b 1

:found_plugin
if defined MINIHOST_EXE if exist "%MINIHOST_EXE%" set "HOST_EXE=%MINIHOST_EXE%"

if not defined HOST_EXE (
  for %%P in (
    "%SCRIPT_DIR%..\minihost\build\minihost_artefacts\Debug\minihost.exe"
    "%SCRIPT_DIR%..\minihost\build\minihost_artefacts\RelWithDebInfo\minihost.exe"
    "%SCRIPT_DIR%..\minihost\build\minihost_artefacts\Release\minihost.exe"
    "%SCRIPT_DIR%minihost.exe"
  ) do (
    if not defined HOST_EXE if exist "%%~fP" set "HOST_EXE=%%~fP"
  )
)

if not defined HOST_EXE (
  echo Could not find minihost.exe.
  echo Set MINIHOST_EXE to your minihost path, for example:
  echo   set "MINIHOST_EXE=D:\Nykwil\Audio\minihost\build\minihost_artefacts\Debug\minihost.exe"
  exit /b 1
)

echo Launching %PLUGIN_NAME% ^(%BUILD_CONFIG%^) with "%HOST_EXE%".
"%HOST_EXE%" %* "%PLUGIN_BUNDLE%"
exit /b %errorlevel%
