# LayerWand MEASUREMENT build and flash, Link 0.2 Phase 0
# (claude/link_0_2_phase0_plan.md). Same guards as layerwand_wand_flash.ps1:
# it flashes only the LayerWand whose USB serial number you name with
# -Serial, and refuses a port another program (a logger) is holding.
#
# Builds tdongle_c5_measure: the LayerWand plus the M1 to M5 measurement
# report and the test-only BULK op. Passive. When the measurements are done,
# flash the LayerWand back with layerwand_wand_flash.ps1.
#
# Run without -Serial to list the LayerWands that are plugged in:
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_measure_flash.ps1"
# Then flash one:
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_measure_flash.ps1" -Serial <serial>

param(
    [string]$Serial = '',
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime'
)
$ProjectDir = Join-Path $RepoDir 'devices\lilygo-layerwand'

$ErrorActionPreference = 'Stop'

function Step([string]$text) { Write-Host ''; Write-Host ('== ' + $text) -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host ''; Write-Host ('FAILED: ' + $text) -ForegroundColor White -BackgroundColor DarkRed; exit 1 }

# Every Espressif USB Serial/JTAG port (VID 303A, PID 1001) with its COM
# port and the USB serial number of the device it belongs to. Reads the
# Windows device tree only; opens nothing.
function Get-LayerWands {
    $found = @()
    foreach ($d in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'USB\VID_303A&PID_1001*' })) {
        if ($d.Name -notmatch '\((COM\d+)\)') { continue }
        $port = $Matches[1]
        $id = $d.PNPDeviceID
        # The COM port is usually interface 0 of a composite device; the
        # serial number is the last part of the parent's instance id.
        if ($id -match '&MI_\d\d') {
            try {
                $parent = (Get-PnpDeviceProperty -InstanceId $id -KeyName 'DEVPKEY_Device_Parent' -ErrorAction Stop).Data
                if ($parent -like 'USB\VID_303A&PID_1001\*') { $id = $parent }
            } catch {
                # Without the parent the interface's own id is listed; it is
                # still unique per device and per USB socket.
            }
        }
        $found += [pscustomobject]@{ Port = $port; Serial = ($id -split '\\')[-1] }
    }
    return $found
}

Step 'LayerWands plugged in (read from the device tree; no port is opened)'
$wands = @(Get-LayerWands)
if ($wands.Count -eq 0) { Fail 'no LayerWand (Espressif USB Serial/JTAG, VID 303A PID 1001) is present.' }
$wands | Format-Table -AutoSize | Out-String | Write-Host
if ($Serial -eq '') {
    Write-Host 'Name the LayerWand to flash with -Serial <serial> from the table above.'
    Write-Host 'The baseline LayerWand is the one whose port the baseline logger opened ("Opened COMn" in its window); do not name it.'
    exit 0
}
$target = @($wands | Where-Object { $_.Serial -eq $Serial })
if ($target.Count -ne 1) { Fail "no LayerWand with serial $Serial is plugged in (or more than one matched)." }
$portName = $target[0].Port
Write-Host "Target: serial $Serial on $portName"

Step 'Checking the toolchain and the repository'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $python)) { Fail "pioarduino core python not found at $python" }
if (-not (Test-Path (Join-Path $ProjectDir 'platformio.ini'))) { Fail "platformio.ini not found in $ProjectDir" }
$core = & $python -c "import platformio; print(platformio.__title__ + ' ' + platformio.__version__)" 2>&1
if ($LASTEXITCODE -ne 0) { Fail "the core in the penv did not answer: $core" }
Write-Host "Core: $core"
if ("$core" -notmatch '^pioarduino ') { Fail "expected the pioarduino core in the penv, found: $core" }

Step "Checking that $portName is free (a port held by a logger is refused here)"
$probe = New-Object System.IO.Ports.SerialPort($portName, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$probe.DtrEnable = $false
$probe.RtsEnable = $false
try { $probe.Open(); $probe.Close() } catch { Fail "$portName is busy ($($_.Exception.Message)). If this is the baseline LayerWand under its logger, you named the wrong device; nothing was flashed." }

Step 'Building tdongle_c5_measure (the LayerWand plus Phase 0 measurement)'
& $python -m platformio run -d $ProjectDir -e tdongle_c5_measure
if ($LASTEXITCODE -ne 0) { Fail 'tdongle_c5_measure did not build. Nothing was flashed.' }

Step "Uploading tdongle_c5_measure to serial $Serial on $portName"
& $python -m platformio run -d $ProjectDir -e tdongle_c5_measure -t upload --upload-port $portName
if ($LASTEXITCODE -ne 0) { Fail 'upload did not complete. Check the cable and the port, then run again.' }

Write-Host ''
Write-Host "DONE: the LayerWand with serial $Serial is running tdongle_c5_measure." -ForegroundColor Green
Write-Host 'Next: start its logger with'
Write-Host ('  powershell -ExecutionPolicy Bypass -File "' + (Join-Path $RepoDir 'devices\lilygo-layerwand\tools\layerwand_wand_log.ps1') + '" -Serial ' + $Serial)
exit 0
