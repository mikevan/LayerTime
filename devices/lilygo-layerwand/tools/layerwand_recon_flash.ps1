# LayerWand build and flash for the Recon no-link baseline (Slice 1, Increment 2A).
#
# Builds the Link firmware (tdongle_c5) as a compile check, then builds and
# uploads the Recon baseline firmware (tdongle_c5_recon) to LayerWand, using
# the pioarduino core already installed in your penv (the same core the
# pioarduino IDE runs). Stops at the first failure and says which step failed.
#
# This script only builds and flashes. Logging the baseline is a separate
# script (layerwand_baseline_log.ps1), so a logging problem never triggers
# another flash.
#
# Run (from any PowerShell window):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_recon_flash.ps1"
#
# Before running: close the pioarduino serial monitor and any logger, because
# the upload needs the port.

param(
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime',
    [switch]$SkipLinkBuild
)
$ProjectDir = Join-Path $RepoDir 'devices\lilygo-layerwand'

$ErrorActionPreference = 'Stop'

function Step([string]$text) { Write-Host ''; Write-Host ('== ' + $text) -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host ''; Write-Host ('FAILED: ' + $text) -ForegroundColor White -BackgroundColor DarkRed; exit 1 }

Step 'Checking the toolchain and the repository'
$python = Join-Path $env:USERPROFILE '.platformio\penv\Scripts\python.exe'
if (-not (Test-Path $python)) { Fail "pioarduino core python not found at $python" }
if (-not (Test-Path (Join-Path $ProjectDir 'platformio.ini'))) { Fail "platformio.ini not found in $ProjectDir" }
$core = & $python -c "import platformio; print(platformio.__title__ + ' ' + platformio.__version__)" 2>&1
if ($LASTEXITCODE -ne 0) { Fail "the core in the penv did not answer: $core" }
Write-Host "Core: $core"
if ("$core" -notmatch '^pioarduino ') { Fail "expected the pioarduino core in the penv, found: $core" }

Step 'Checking that LayerWand is plugged in and its port is free'
$portName = $null
foreach ($d in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.PNPDeviceID -like 'USB\VID_303A&PID_1001*' })) {
    if ($d.Name -match '\((COM\d+)\)') { $portName = $Matches[1]; break }
}
if ($null -eq $portName) { Fail 'LayerWand (Espressif USB Serial/JTAG, VID 303A PID 1001) is not present. Plug it in and run again.' }
Write-Host "LayerWand is on $portName"
$probe = New-Object System.IO.Ports.SerialPort($portName, 115200, [System.IO.Ports.Parity]::None, 8, [System.IO.Ports.StopBits]::One)
$probe.DtrEnable = $false
$probe.RtsEnable = $false
try { $probe.Open(); $probe.Close() } catch { Fail "$portName is busy ($($_.Exception.Message)). Close the serial monitor or the logger and run again." }

if (-not $SkipLinkBuild) {
    Step 'Building tdongle_c5 (Link firmware, compile check only, not uploaded)'
    & $python -m platformio run -d $ProjectDir -e tdongle_c5
    if ($LASTEXITCODE -ne 0) { Fail 'tdongle_c5 did not build. Nothing was flashed.' }
}

Step 'Building tdongle_c5_recon (Recon no-link baseline firmware)'
& $python -m platformio run -d $ProjectDir -e tdongle_c5_recon
if ($LASTEXITCODE -ne 0) { Fail 'tdongle_c5_recon did not build. Nothing was flashed.' }

Step "Uploading tdongle_c5_recon to LayerWand on $portName"
& $python -m platformio run -d $ProjectDir -e tdongle_c5_recon -t upload --upload-port $portName
if ($LASTEXITCODE -ne 0) { Fail 'upload to LayerWand did not complete. Check the cable and the port, then run again.' }

Write-Host ''
Write-Host 'DONE: LayerWand is running the Recon no-link baseline firmware.' -ForegroundColor Green
Write-Host 'Next: start the logger with'
Write-Host '  powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\lilygo-layerwand\tools\layerwand_baseline_log.ps1"'
Write-Host 'then unplug and replug LayerWand when the logger says it is waiting for a power-on boot.'
exit 0
