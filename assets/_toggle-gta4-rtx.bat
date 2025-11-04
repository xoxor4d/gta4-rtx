@echo off
setlocal EnableDelayedExpansion

REM
set "DLL1=d3d9.dll"
set "DLL2=a_gta4-rtx.asi"

REM check current status of files
set "STATUS=unknown"
if exist "%DLL1%" if exist "%DLL2%" set "STATUS=enabled"
if exist "%DLL1%.off" if exist "%DLL2%.off" set "STATUS=disabled"

echo Current mod status: %STATUS%
echo.

REM
if "%STATUS%"=="unknown" (
    echo Error: DLL files are in an inconsistent state.
    echo Please ensure both files exist with either .dll or .dll.off extensions.
    pause
    exit /b
)

REM
if "%STATUS%"=="enabled" (
    set "QUESTION=Would you like to disable the mod? (y/n)"
    set "ACTION=disable"
) else (
    set "QUESTION=Would you like to enable the mod? (y/n)"
    set "ACTION=enable"
)

:PROMPT
set "CHOICE="
set /p CHOICE="%QUESTION% "
if /i "%CHOICE%"=="y" goto :PROCESS
if /i "%CHOICE%"=="n" goto :EXIT
echo Please enter y or n
goto :PROMPT

:PROCESS
if "%ACTION%"=="disable" (
    ren "%DLL1%" "d3d9.dll.off"
    ren "%DLL2%" "a_gta4-rtx.asi.off"
    echo Mod has been disabled
) else (
    ren "%DLL1%.off" "d3d9.dll"
    ren "%DLL2%.off" "a_gta4-rtx.asi"
    echo Mod has been enabled
)

:EXIT
echo.
pause