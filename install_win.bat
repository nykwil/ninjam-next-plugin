@echo off
setlocal

set "PLUGIN_NAME=NinjamNext.vst3"
set "ARTEFACTS_ROOTS=%~dp0build\NinjamNext_artefacts"
set "DEST_ROOT=%~dp0..\VST3"
set "DEST_BUNDLE=%DEST_ROOT%\%PLUGIN_NAME%"

powershell -NoProfile -Command ^
  "$ErrorActionPreference='Stop';" ^
  "$plugin=$env:PLUGIN_NAME;" ^
  "$roots=@($env:ARTEFACTS_ROOTS -split ';') | Where-Object { $_ -and (Test-Path -LiteralPath $_) };" ^
  "$candidates=foreach($root in $roots){ Get-ChildItem -LiteralPath $root -Directory -Recurse -Filter $plugin -ErrorAction SilentlyContinue | Where-Object { Get-ChildItem -LiteralPath (Join-Path $_.FullName 'Contents') -File -Recurse -Filter $plugin -ErrorAction SilentlyContinue | Select-Object -First 1 } | Select-Object FullName, LastWriteTime };" ^
  "$src=$candidates | Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName;" ^
  "if(-not $src){ Write-Host ('Could not find built VST3 bundle for ' + $plugin + '.'); exit 1 };" ^
  "$destRoot=[System.IO.Path]::GetFullPath($env:DEST_ROOT);" ^
  "$dest=[System.IO.Path]::GetFullPath($env:DEST_BUNDLE);" ^
  "New-Item -ItemType Directory -Force -Path $destRoot | Out-Null;" ^
  "if(Test-Path -LiteralPath $dest){ Remove-Item -LiteralPath $dest -Recurse -Force };" ^
  "Copy-Item -LiteralPath $src -Destination $dest -Recurse -Force;" ^
  "Write-Host ('Deployed ' + $plugin + ' from ' + $src + ' to ' + $dest + '.');"

if errorlevel 1 exit /b 1
exit /b 0
