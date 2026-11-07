# Hunter build and flash for Increment 2B (architecture A: Recon and the
# LayerTime Link on one C5), for the SECOND Hunter only.
#
# Builds tdongle_c5_hunter and uploads it to the one Hunter whose USB serial
# number you name with -Serial. It never opens, resets or flashes any other
# port: the Increment 2A baseline Hunter and its logger are left alone.
# As a second guard, the script opens the named device's port once before
# flashing; while the baseline logger holds the baseline's port that open
# fails, so the baseline cannot be flashed by naming it by mistake.
#
# Uses the pioarduino core already installed in your penv (the core the
# pioarduino IDE runs). Stops at the first failure and says which step
# failed.
#
# Run without -Serial to list the Hunters that are plugged in (port and
# USB serial number; no port is opened):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-hunter\tools\hunter_flash.ps1"
# Then flash the second Hunter:
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-hunter\tools\hunter_flash.ps1" -Serial <serial>

param(
    [string]$Serial = '',
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime'
)
$ProjectDir = Join-Path $RepoDir 'devices\lilygo-hunter'

$ErrorActionPreference = 'Stop'

function Step([string]$text) { Write-Host ''; Write-Host ('== ' + $text) -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host ''; Write-Host ('FAILED: ' + $text) -ForegroundColor White -BackgroundColor DarkRed; exit 1 }

# Every Espressif USB Serial/JTAG port (VID 303A, PID 1001) with its COM
# port and the USB serial number of the device it belongs to. Reads the
# Windows device tree only; opens nothing.
function Get-Hunters {
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

Step 'Hunters plugged in (read from the device tree; no port is opened)'
$wands = @(Get-Hunters)
if ($wands.Count -eq 0) { Fail 'no Hunter (Espressif USB Serial/JTAG, VID 303A PID 1001) is present.' }
$wands | Format-Table -AutoSize | Out-String | Write-Host
if ($Serial -eq '') {
    Write-Host 'Name the second Hunter with -Serial <serial> from the table above.'
    Write-Host 'The baseline Hunter is the one whose port the baseline logger opened ("Opened COMn" in its window); do not name it.'
    exit 0
}
$target = @($wands | Where-Object { $_.Serial -eq $Serial })
if ($target.Count -ne 1) { Fail "no Hunter with serial $Serial is plugged in (or more than one matched)." }
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
try { $probe.Open(); $probe.Close() } catch { Fail "$portName is busy ($($_.Exception.Message)). If this is the baseline Hunter under its logger, you named the wrong device; nothing was flashed." }

Step 'Building tdongle_c5_hunter (Recon and LayerTime Link, architecture A)'
& $python -m platformio run -d $ProjectDir -e tdongle_c5_hunter
if ($LASTEXITCODE -ne 0) { Fail 'tdongle_c5_hunter did not build. Nothing was flashed.' }

Step "Uploading tdongle_c5_hunter to serial $Serial on $portName"
& $python -m platformio run -d $ProjectDir -e tdongle_c5_hunter -t upload --upload-port $portName
if ($LASTEXITCODE -ne 0) { Fail 'upload did not complete. Check the cable and the port, then run again.' }

Write-Host ''
Write-Host "DONE: the Hunter with serial $Serial is running tdongle_c5_hunter." -ForegroundColor Green
Write-Host 'Next: start its logger with'
Write-Host ('  powershell -ExecutionPolicy Bypass -File "' + (Join-Path $RepoDir 'devices\lilygo-hunter\tools\hunter_log.ps1') + '" -Serial ' + $Serial)
exit 0
