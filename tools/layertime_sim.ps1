# LayerTime watch app: build for the tactix 7 AMOLED (epix2pro51mm) and run
# it in the Connect IQ simulator for a preview before any watch install.
#
# Uses the Connect IQ SDK the SDK Manager marks current
# (%APPDATA%\Garmin\ConnectIQ\current-sdk.cfg), the repository's own
# developer key (garmin\developer_key, never copied or moved), and the
# SDK's monkeyc.bat, simulator.exe and monkeydo.bat. Stops at the first
# failure and says which step failed.
#
# Run (from any PowerShell window):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\tools\layertime_sim.ps1"
# Add one of:
#   -Preview   the preview build (preview.jungle): fixed home-screen scenarios
#              (normal, alert, disconnected, stale weather, missing data); a tap
#              on the LAYERTIME title moves to the next one
#   -Test      the unit tests (monkey.jungle with --unit-test), run in the
#              simulator; the results print in this window
#   -LinkTest  the measurement build (linktest.jungle)

param(
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime',
    [switch]$LinkTest,
    [switch]$Preview,
    [switch]$Test
)

$ErrorActionPreference = 'Stop'
function Step([string]$t) { Write-Host ''; Write-Host ('== ' + $t) -ForegroundColor Cyan }
function Fail([string]$t) { Write-Host ''; Write-Host ('FAILED: ' + $t) -ForegroundColor White -BackgroundColor DarkRed; exit 1 }

Step 'Locating the Connect IQ SDK'
$cfg = Join-Path $env:APPDATA 'Garmin\ConnectIQ\current-sdk.cfg'
if (-not (Test-Path $cfg)) { Fail "current-sdk.cfg not found at $cfg (open the Connect IQ SDK Manager once)." }
$sdk = (Get-Content $cfg -Raw).Trim().TrimEnd('\')
$bin = Join-Path $sdk 'bin'
foreach ($f in @('monkeyc.bat', 'monkeydo.bat', 'simulator.exe', 'monkeybrains.jar')) {
    if (-not (Test-Path (Join-Path $bin $f))) { Fail "$f not found in $bin" }
}
Write-Host "SDK: $sdk"
if ($null -eq (Get-Command java -ErrorAction SilentlyContinue)) { Fail 'java is not on PATH; the SDK tools need it.' }

$garmin = Join-Path $RepoDir 'garmin'
$key = Join-Path $garmin 'developer_key'
if (([int]$LinkTest.IsPresent + [int]$Preview.IsPresent + [int]$Test.IsPresent) -gt 1) { Fail 'use at most one of -LinkTest, -Preview, -Test.' }
$jungle = 'monkey.jungle'
$prg = 'LayerTime.prg'
if ($LinkTest) { $jungle = 'linktest.jungle'; $prg = 'LayerTime_linktest.prg' }
if ($Preview) { $jungle = 'preview.jungle'; $prg = 'LayerTime_preview.prg' }
if ($Test) { $prg = 'LayerTime_test.prg' }
$out = Join-Path $garmin ('bin\' + $prg)
if (-not (Test-Path $key)) { Fail "developer key not found at $key" }
if (-not (Test-Path (Join-Path $garmin $jungle))) { Fail "$jungle not found in $garmin" }
New-Item -ItemType Directory -Force -Path (Join-Path $garmin 'bin') | Out-Null

Step "Building $jungle for epix2pro51mm (strict type check)"
Push-Location $garmin
try {
    if ($Test) {
        & (Join-Path $bin 'monkeyc.bat') -o $out -f $jungle -y $key -d epix2pro51mm -w -l 3 --unit-test
    } else {
        & (Join-Path $bin 'monkeyc.bat') -o $out -f $jungle -y $key -d epix2pro51mm -w -l 3 -r
    }
    if ($LASTEXITCODE -ne 0) { Fail 'the build did not succeed; see the compiler output above.' }
} finally { Pop-Location }
Write-Host "Built: $out"

Step 'Starting the simulator'
$running = Get-Process -Name 'simulator' -ErrorAction SilentlyContinue
if ($null -eq $running) {
    Start-Process -FilePath (Join-Path $bin 'simulator.exe')
    Start-Sleep -Seconds 4
} else {
    Write-Host 'Simulator already running.'
}

if ($Test) {
    Step 'Running the unit tests in the simulated epix2pro51mm'
    & (Join-Path $bin 'monkeydo.bat') $out epix2pro51mm /t
    if ($LASTEXITCODE -ne 0) { Fail 'monkeydo reported an error running the tests; see the output above.' }
    Write-Host ''
    Write-Host 'DONE: the unit tests ran. Read the PASS and FAIL lines and the summary above.' -ForegroundColor Green
    exit 0
}

Step 'Loading LayerTime into the simulated epix2pro51mm'
& (Join-Path $bin 'monkeydo.bat') $out epix2pro51mm
if ($LASTEXITCODE -ne 0) { Fail 'monkeydo could not load the app into the simulator.' }

Write-Host ''
Write-Host 'DONE: LayerTime is running in the simulator.' -ForegroundColor Green
if ($Preview) {
    Write-Host 'Preview build: a tap on the LAYERTIME title moves to the next scenario (NORMAL, ALERT, DISCONNECTED,'
    Write-Host 'STALE WEATHER, MISSING DATA) and names it in a toast. START on the Recon entry opens the Recon page;'
    Write-Host 'MENU opens the Controls. The preview build is for the simulator only.'
} else {
    Write-Host 'With no LayerWand in range the Recon entry shows NO LAYERWAND / SEARCHING. START on the Recon entry'
    Write-Host 'opens the Recon page; MENU opens the Controls (Link diagnostics is the last item); BACK returns home.'
}
exit 0
