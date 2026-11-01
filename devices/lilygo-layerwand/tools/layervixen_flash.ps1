# LayerVixen build and flash (HUNT v1, claude/hunt_v1_layervixen_plan.md).
#
# Builds tdongle_c5_vixen, the fox-hunt beacon, and uploads it to the one
# T-Dongle-C5 whose USB serial number you name with -Serial. It never opens,
# resets or flashes any other port. As a second guard, the script opens the
# named device's port once before flashing; a port a logger holds is refused.
#
# The board is the same as a LayerWand's, so pick the right one: the
# LayerVixen unit is the one you are turning into the fox.
#
# Run without -Serial to list the boards that are plugged in (port and USB
# serial number; no port is opened):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layervixen_flash.ps1"
# Then flash the Vixen:
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layervixen_flash.ps1" -Serial <serial>

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
    Write-Host 'Name the board that becomes the LayerVixen with -Serial <serial> from the table above.'
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
try { $probe.Open(); $probe.Close() } catch { Fail "$portName is busy ($($_.Exception.Message)). Close any logger on that port and run again; nothing was flashed." }

Step 'Building tdongle_c5_vixen (the LayerVixen beacon)'
& $python -m platformio run -d $ProjectDir -e tdongle_c5_vixen
if ($LASTEXITCODE -ne 0) { Fail 'tdongle_c5_vixen did not build. Nothing was flashed.' }

Step "Uploading tdongle_c5_vixen to serial $Serial on $portName"
& $python -m platformio run -d $ProjectDir -e tdongle_c5_vixen -t upload --upload-port $portName
if ($LASTEXITCODE -ne 0) { Fail 'upload did not complete. Check the cable and the port, then run again.' }

Write-Host ''
Write-Host "DONE: the board with serial $Serial is now a LayerVixen (tdongle_c5_vixen)." -ForegroundColor Green
Write-Host 'Check: a short BOOT press lights its screen with LayerVixen and BEACON ON for 10 seconds.'
exit 0
