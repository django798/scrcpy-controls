@echo off
rem Copyright (C) 2026 moasm
rem SPDX-License-Identifier: Apache-2.0
setlocal
cd /d "%~dp0"
if exist "scrcpy.exe" (
    scrcpy.exe --control-panel %*
) else if exist "dist\scrcpy-controls-windows-x64\scrcpy.exe" (
    cd /d "dist\scrcpy-controls-windows-x64"
    scrcpy.exe --control-panel %*
) else (
    echo Build first: powershell -ExecutionPolicy Bypass -File tools\build-windows.ps1
    pause
    exit /b 1
)
if errorlevel 1 pause
