@echo off

:: Check for admin rights - relaunch with UAC prompt
net session >nul 2>&1
if %errorLevel% neq 0 (
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

:: Set working directory
cd /d "%~dp0"

setlocal enabledelayedexpansion

echo Initializing GTA IV launch with 
echo > TRACY_SYMBOL_OFFLINE_RESOLVE=1
echo > TRACY_SAMPLING_HZ=2000
echo.

set TRACY_SYMBOL_OFFLINE_RESOLVE=1
set TRACY_SAMPLING_HZ=2000

:: Launch GTA IV
start GTAIV.exe