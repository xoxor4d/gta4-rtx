@echo off
setlocal enabledelayedexpansion

echo Initializing GTA IV launch and CPU affinity setup...
echo Assigning 2 Cores to GTA4 and the rest to RTX Remix
echo.

:: Launch GTA IV
start GTAIV.exe

:: Compute CPU core masks
echo Detecting system CPU cores...
powershell -Command "$total = (Get-CimInstance Win32_Processor | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum; if ($total -le 0) { $total = 4 }; $gtaCores = 2; $gtaMask = (1 -shl $gtaCores) - 1; $nvMask = ((1 -shl $total) - 1) - $gtaMask; Write-Output \"$gtaMask $nvMask\"" > masks.txt
set /p line=<masks.txt
del masks.txt 2>nul
for /f "tokens=1,2" %%a in ("%line%") do (
    set "gtaMask=%%a"
    set "nvMask=%%b"
)
echo Assigned mask 0x%gtaMask% to GTA IV and 0x%nvMask% to NvRemixBridge.

:: Wait for both processes to start (up to 120 seconds)
echo Waiting for GTA IV and NvRemixBridge to fully launch...
set /a maxWait=60
set /a waitCount=0

:waitloop
timeout /t 2 >nul

tasklist /fi "imagename eq GTAIV.exe" 2>nul | find /i "GTAIV.exe" >nul
set "GTAIVFound=!errorlevel!"
tasklist /fi "imagename eq NvRemixBridge.exe" 2>nul | find /i "NvRemixBridge.exe" >nul
set "NvRemixFound=!errorlevel!"

if !GTAIVFound! neq 0 goto :checktimeout
if !NvRemixFound! neq 0 goto :checktimeout

goto :setaffinity

:checktimeout
set /a waitCount+=1
if !waitCount! geq !maxWait! (
    echo Timeout reached. Processes may not have started properly.
    goto :cleanup
)
goto :waitloop

:setaffinity
echo Setting CPU affinities...

:: Set affinity for GTAIV.exe
for /f "tokens=2 delims=," %%A in ('tasklist /fi "imagename eq GTAIV.exe" /fo csv /nh 2^>nul') do set "GTAIVPID=%%~A"
if defined GTAIVPID (
    powershell -Command "try { Get-Process -Id !GTAIVPID! -ErrorAction Stop | ForEach-Object { $_.ProcessorAffinity = !gtaMask! } } catch { Write-Warning 'Failed to set affinity for GTAIV.exe' }"
) else (
    echo Warning: GTAIV.exe PID not found.
)

:: Set affinity for NvRemixBridge.exe
for /f "tokens=2 delims=," %%A in ('tasklist /fi "imagename eq NvRemixBridge.exe" /fo csv /nh 2^>nul') do set "NvPID=%%~A"
if defined NvPID (
    powershell -Command "try { Get-Process -Id !NvPID! -ErrorAction Stop | ForEach-Object { $_.ProcessorAffinity = !nvMask! } } catch { Write-Warning 'Failed to set affinity for NvRemixBridge.exe' }"
) else (
    echo Warning: NvRemixBridge.exe PID not found.
)

echo CPU affinities applied successfully.
goto :cleanup

:cleanup
endlocal
exit /b 0