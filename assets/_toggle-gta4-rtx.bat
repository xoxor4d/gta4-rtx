@echo off
setlocal EnableDelayedExpansion

:: ============================================================
:: Files to be toggled

set "FILE[1]=d3d9.dll"
set "FILE[2]=a_gta4-rtx.asi"
set "FILE[3]=update\1__remix_fixes.img"
set "FILE[4]=update\1__remix_light_tweaks.img"

set "FILECOUNT=4"

:: ============================================================
:: Check current status
:: first two files are used to determine status

set "STATUS=unknown"

if exist "!FILE[1]!" if exist "!FILE[2]!" (
    set "STATUS=enabled"
)

if exist "!FILE[1]!.REMIX_OFF" if exist "!FILE[2]!.REMIX_OFF" (
    set "STATUS=disabled"
)

echo Current mod status: %STATUS%
echo.

:: ============================================================
:: Inconsistent state handling

if "%STATUS%"=="unknown" (
    echo Error: Files are in an inconsistent state.
    echo Disabling Mod.
    set "STATUS=enabled"
)

:: ============================================================
:: Prompt

if "%STATUS%"=="enabled" (
    set "QUESTION=Would you like to disable the mod? (y/n)"
    set "ACTION=disable"
) else (
    set "QUESTION=Would you like to enable the mod? (y/n)"
    set "ACTION=enable"
)

:PROMPT
set "CHOICE="
set /p "CHOICE=%QUESTION% "

if /i "%CHOICE%"=="y" goto PROCESS
if /i "%CHOICE%"=="n" goto EXIT

echo Please enter y or n
goto PROMPT

:: ============================================================
:: Toggle

:PROCESS

if "%ACTION%"=="disable" (
    for /L %%I in (1,1,%FILECOUNT%) do (
        set "FILE=!FILE[%%I]!"

        for %%F in ("!FILE!") do (
            if exist "!FILE!" (
                ren "!FILE!" "%%~nxF.REMIX_OFF"
            )
        )
    )

    echo.
    echo Mod has been disabled.
    goto EXIT
)

if "%ACTION%"=="enable" (
    for /L %%I in (1,1,%FILECOUNT%) do (
        set "FILE=!FILE[%%I]!"

        if exist "!FILE!.REMIX_OFF" (
            for %%F in ("!FILE!.REMIX_OFF") do (
                set "NAME=%%~nxF"
                set "NAME=!NAME:.REMIX_OFF=!"

                ren "!FILE!.REMIX_OFF" "!NAME!"
            )
        )
    )

    echo.
    echo Mod has been enabled.
    goto EXIT
)

:EXIT
echo.
pause