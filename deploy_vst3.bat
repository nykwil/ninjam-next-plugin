@echo off
setlocal

set "PLUGIN_NAME=NinjamNext.vst3"
set "PLUGIN_BINARY=NinjamNext.vst3"
set "ARTEFACTS_DIR=%~dp0build\NinjamNext_artefacts"
set "SRC_BUNDLE="
set "BUILD_CONFIG="

for %%C in (RelWithDebInfo Release Debug MinSizeRel) do (
  if exist "%ARTEFACTS_DIR%\%%C\VST3\%PLUGIN_NAME%\Contents\x86_64-win\%PLUGIN_BINARY%" (
    set "SRC_BUNDLE=%ARTEFACTS_DIR%\%%C\VST3\%PLUGIN_NAME%"
    set "BUILD_CONFIG=%%C"
    goto :found_bundle
  )
)

echo Could not find built VST3 bundle under "%ARTEFACTS_DIR%".
echo Build first with: build_win.bat
exit /b 1

:found_bundle
set "VST3_DIR=C:\Program Files\Common Files\VST3"
set "DEST_BUNDLE=%VST3_DIR%\%PLUGIN_NAME%"

if not exist "%VST3_DIR%" mkdir "%VST3_DIR%"
if errorlevel 1 (
  echo Failed to create VST3 destination directory: "%VST3_DIR%"
  exit /b 1
)

powershell -NoProfile -Command "$src=[System.IO.Path]::GetFullPath('%SRC_BUNDLE%'); $dst='%DEST_BUNDLE%'; try { if (Test-Path -LiteralPath $dst) { Remove-Item -LiteralPath $dst -Recurse -Force -ErrorAction Stop }; Copy-Item -LiteralPath $src -Destination $dst -Recurse -Force -ErrorAction Stop; exit 0 } catch { Write-Host $_.Exception.Message; exit 1 }"
if errorlevel 1 (
  echo Failed to copy "%PLUGIN_NAME%" to "%DEST_BUNDLE%".
  echo Ensure your DAW is closed and this shell has write access.
  exit /b 1
)

echo Deployed "%PLUGIN_NAME%" (%BUILD_CONFIG%) to "%DEST_BUNDLE%".
exit /b 0
