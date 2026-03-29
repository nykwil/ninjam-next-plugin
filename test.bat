@echo off
setlocal EnableExtensions

call "%~dp0run.bat" --test
if errorlevel 1 (
    echo FAIL: Plugin did not load correctly.
    exit /b 1
)
echo PASS: Plugin loaded and processed audio successfully.
