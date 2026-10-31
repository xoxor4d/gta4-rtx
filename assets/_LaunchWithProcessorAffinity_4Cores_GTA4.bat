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

echo Initializing GTA IV launch and CPU affinity setup...
echo Assigning 4 physical cores to GTA4 and the rest to RTX Remix (no shared cores)
echo.

:: Launch GTA IV
start GTAIV.exe

:: Compute CPU core masks (64-bit, capped at 64 logical processors = one Windows processor group)
:: With SMT both threads of a core are adjacent logical processors, so N physical cores span 2N of them
:: (hybrid Intel CPUs list their hyperthreaded performance cores first)
echo Detecting system CPU cores...
powershell -Command "$cpus = Get-CimInstance Win32_Processor; $total = ($cpus | Measure-Object -Property NumberOfLogicalProcessors -Sum).Sum; $physical = ($cpus | Measure-Object -Property NumberOfCores -Sum).Sum; if ($total -le 0) { $total = 4 }; if ($physical -le 0) { $physical = $total }; [int]$threadsPerCore = 1; if ($total -gt $physical) { $threadsPerCore = 2 }; if ($total -gt 64) { $total = 64 }; $gtaBits = 4 * $threadsPerCore; if ($gtaBits -ge $total) { $gtaBits = [Math]::Max(1, [Math]::Floor($total / 2)) }; if ($total -ge 64) { [long]$allMask = -1 } else { [long]$allMask = (1L -shl $total) - 1 }; if ($gtaBits -ge 64) { [long]$gtaMask = -1 } else { [long]$gtaMask = ((1L -shl $gtaBits) - 1) -band $allMask }; [long]$nvMask = $allMask -band (-bnot $gtaMask); if ($nvMask -eq 0) { $nvMask = $allMask }; Write-Output \"$total $gtaMask $nvMask $($gtaMask.ToString('X')) $($nvMask.ToString('X')) $physical\"" > masks.txt
set /p line=<masks.txt
del masks.txt 2>nul
for /f "tokens=1-6" %%a in ("%line%") do (
    set "totalCores=%%a"
    set "gtaMask=%%b"
    set "nvMask=%%c"
    set "gtaHex=%%d"
    set "nvHex=%%e"
    set "physicalCores=%%f"
)
echo Detected %totalCores% logical processors on %physicalCores% physical cores.
echo Assigned mask 0x%gtaHex% to GTA IV and 0x%nvHex% to NvRemixBridge.

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
    powershell -Command "try { Get-Process -Id !GTAIVPID! -ErrorAction Stop | ForEach-Object { $_.ProcessorAffinity = [IntPtr]([long](!gtaMask!)) } } catch { Write-Warning 'Failed to set affinity for GTAIV.exe' }"
) else (
    echo Warning: GTAIV.exe PID not found.
)

:: Set affinity for NvRemixBridge.exe
for /f "tokens=2 delims=," %%A in ('tasklist /fi "imagename eq NvRemixBridge.exe" /fo csv /nh 2^>nul') do set "NvPID=%%~A"
if defined NvPID (
    powershell -Command "try { Get-Process -Id !NvPID! -ErrorAction Stop | ForEach-Object { $_.ProcessorAffinity = [IntPtr]([long](!nvMask!)) } } catch { Write-Warning 'Failed to set affinity for NvRemixBridge.exe' }"
) else (
    echo Warning: NvRemixBridge.exe PID not found.
)

echo CPU affinities applied successfully.
goto :cleanup

:cleanup
endlocal
exit /b 0
