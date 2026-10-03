# LayerTime watch app: build the PRODUCTION app (monkey.jungle) for the
# tactix 7 AMOLED (Connect IQ device epix2pro51mm), sign it with the
# repository's own developer key (devices\garmin-tactix\developer_key; read in place,
# never copied or moved), and install it on the watch over USB.
#
# What it does, stopping at the first failure and saying which step failed:
#   1. Builds devices\garmin-tactix\bin\LayerTime.prg from monkey.jungle, release mode (-r),
#      with the SDK the Connect IQ SDK Manager marks current.
#   2. Checks the PRG is the production build: it must carry LayerTime's app
#      id and must NOT contain the preview build's or the linktest build's
#      test-only strings.
#   3. Finds the watch: a Garmin USB device (vendor 091E) that shows a
#      GARMIN\Apps folder, as a portable device (MTP) or as a drive.
#   4. Reads the first bytes of every .prg already in GARMIN\Apps to find an
#      earlier LayerTime build (same app id). One found: it is replaced under
#      its own file name. None: the app is installed as LayerTime.prg. More
#      than one: the script stops and lists them, and changes nothing.
#   5. Copies the PRG, then copies it back from the watch and compares SHA-256
#      hashes, so "installed" means the exact bytes are on the watch.
#
# It never touches a serial port, so a running LayerWand logger is not
# affected. It never deletes anything on the watch.
#
# Run (from any PowerShell window, watch connected by its USB cable):
#   powershell -ExecutionPolicy Bypass -File "C:\workspace\TUltra-Project\LayerTime\devices\garmin-tactix\tools\layertime_install.ps1"

param(
    [string]$RepoDir = 'C:\workspace\TUltra-Project\LayerTime'
)

$ErrorActionPreference = 'Stop'
function Step([string]$t) { Write-Host ''; Write-Host ('== ' + $t) -ForegroundColor Cyan }
function Fail([string]$t) { Write-Host ''; Write-Host ('FAILED: ' + $t) -ForegroundColor White -BackgroundColor DarkRed; exit 1 }

# LayerTime's Connect IQ app id (devices\garmin-tactix\manifest.xml), as the PRG stores it.
$appIdHex = '138526A970794AFB8318DF0370265696'
$appId = [byte[]]::new(16)
for ($i = 0; $i -lt 16; $i++) { $appId[$i] = [Convert]::ToByte($appIdHex.Substring(2 * $i, 2), 16) }

function Test-HasAppId([byte[]]$bytes) {
    $limit = [Math]::Min($bytes.Length, 512) - 16
    for ($o = 0; $o -le $limit; $o++) {
        $hit = $true
        for ($k = 0; $k -lt 16; $k++) { if ($bytes[$o + $k] -ne $appId[$k]) { $hit = $false; break } }
        if ($hit) { return $true }
    }
    return $false
}

function Test-Contains([byte[]]$bytes, [string]$text) {
    $ascii = [System.Text.Encoding]::ASCII.GetString($bytes)
    return $ascii.Contains($text)
}

# ---------------------------------------------------------------------------
Step 'Locating the Connect IQ SDK, the key and the manifest'
$cfg = Join-Path $env:APPDATA 'Garmin\ConnectIQ\current-sdk.cfg'
if (-not (Test-Path $cfg)) { Fail "current-sdk.cfg not found at $cfg (open the Connect IQ SDK Manager once)." }
$sdk = (Get-Content $cfg -Raw).Trim().TrimEnd('\')
$bin = Join-Path $sdk 'bin'
if (-not (Test-Path (Join-Path $bin 'monkeyc.bat'))) { Fail "monkeyc.bat not found in $bin" }
if ($null -eq (Get-Command java -ErrorAction SilentlyContinue)) { Fail 'java is not on PATH; the SDK compiler needs it.' }
$garmin = Join-Path $RepoDir 'devices\garmin-tactix'
$key = Join-Path $garmin 'developer_key'
if (-not (Test-Path $key)) { Fail "developer key not found at $key" }
$manifest = Get-Content (Join-Path $garmin 'manifest.xml') -Raw
if ($manifest -notmatch ('id="' + $appIdHex + '"')) { Fail 'devices\garmin-tactix\manifest.xml does not carry the LayerTime app id this script expects.' }
if ($manifest -notmatch 'product id="epix2pro51mm"') { Fail 'devices\garmin-tactix\manifest.xml does not target epix2pro51mm.' }
Write-Host "SDK: $sdk"
Write-Host "Key: $key"

# ---------------------------------------------------------------------------
Step 'Building the production app (monkey.jungle, epix2pro51mm, release, strict type check)'
New-Item -ItemType Directory -Force -Path (Join-Path $garmin 'bin') | Out-Null
$prg = Join-Path $garmin 'bin\LayerTime.prg'
if (Test-Path $prg) { Remove-Item $prg }
Push-Location $garmin
try {
    & (Join-Path $bin 'monkeyc.bat') -o $prg -f 'monkey.jungle' -y $key -d epix2pro51mm -w -l 3 -r
    if ($LASTEXITCODE -ne 0) { Fail 'the build did not succeed; see the compiler output above. Nothing was installed.' }
} finally { Pop-Location }
if (-not (Test-Path $prg)) { Fail "the compiler reported success but $prg is missing." }

# ---------------------------------------------------------------------------
Step 'Checking that this is the production build'
$bytes = [System.IO.File]::ReadAllBytes($prg)
if (-not (Test-HasAppId $bytes)) { Fail 'the PRG does not carry the LayerTime app id. Nothing was installed.' }
foreach ($marker in @('STALE WEATHER', 'PREVIEW ')) {
    if (Test-Contains $bytes $marker) { Fail "the PRG contains the preview build's '$marker'. Nothing was installed." }
}
if (Test-Contains $bytes 'hb,start') { Fail "the PRG contains the linktest build's 'hb,start'. Nothing was installed." }
if (-not (Test-Contains $bytes 'NO LAYERWAND')) { Fail 'the PRG does not contain the current home screen text. Nothing was installed.' }
$localHash = (Get-FileHash -Algorithm SHA256 $prg).Hash
Write-Host ("Production PRG: {0}, {1} bytes, SHA-256 {2}" -f $prg, $bytes.Length, $localHash)

# ---------------------------------------------------------------------------
Step 'Finding the watch (Garmin USB device with GARMIN\Apps)'
$shell = New-Object -ComObject Shell.Application
function Get-SubFolder($folder, [string]$name) {
    foreach ($i in $folder.Items()) { if ($i.IsFolder -and $i.Name -eq $name) { return $i.GetFolder } }
    return $null
}
$targets = @()
# Portable device (MTP): shown under This PC, path carries the Garmin USB vendor id.
foreach ($dev in $shell.Namespace(17).Items()) {
    if (-not $dev.IsFolder -or $dev.Path -notmatch 'vid_091e') { continue }
    foreach ($storage in $dev.GetFolder.Items()) {
        if (-not $storage.IsFolder) { continue }
        $g = Get-SubFolder $storage.GetFolder 'GARMIN'
        if ($null -eq $g) { continue }
        $apps = Get-SubFolder $g 'Apps'
        if ($null -ne $apps) { $targets += [pscustomobject]@{ Kind = 'MTP'; Name = ($dev.Name + '\' + $storage.Name); Folder = $apps; Root = $null } }
    }
}
# Mass storage: a drive with GARMIN\GarminDevice.xml and GARMIN\Apps.
foreach ($d in (Get-PSDrive -PSProvider FileSystem)) {
    $root = $d.Root
    if ((Test-Path (Join-Path $root 'GARMIN\GarminDevice.xml')) -and (Test-Path (Join-Path $root 'GARMIN\Apps'))) {
        $targets += [pscustomobject]@{ Kind = 'Drive'; Name = $root; Folder = $null; Root = (Join-Path $root 'GARMIN\Apps') }
    }
}
if ($targets.Count -eq 0) { Fail 'no Garmin watch with a GARMIN\Apps folder is visible. Connect the watch with its USB cable, wait until it appears in File Explorer, and run again.' }
if ($targets.Count -gt 1) { Fail ('more than one Garmin device is visible: ' + (($targets | ForEach-Object { $_.Name }) -join ', ') + '. Connect only the tactix and run again.') }
$t = $targets[0]
Write-Host ("Watch: {0} ({1})" -f $t.Name, $t.Kind)

$tmp = Join-Path $env:TEMP ('layertime_install_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
New-Item -ItemType Directory -Force -Path $tmp | Out-Null

# Copies one file from the watch into $dest (local folder) and returns its local path.
function Get-FromWatch([string]$name, [string]$dest) {
    $local = Join-Path $dest $name
    if ($t.Kind -eq 'Drive') { Copy-Item -LiteralPath (Join-Path $t.Root $name) -Destination $local; return $local }
    $item = $t.Folder.ParseName($name)
    if ($null -eq $item) { return $null }
    $shell.Namespace($dest).CopyHere($item, 0x14)
    $deadline = (Get-Date).AddSeconds(60)
    $lastSize = -1
    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
        if (Test-Path -LiteralPath $local) {
            $size = (Get-Item -LiteralPath $local).Length
            if ($size -gt 0 -and $size -eq $lastSize) { return $local }
            $lastSize = $size
        }
    }
    return $null
}

# ---------------------------------------------------------------------------
Step 'Looking for an earlier LayerTime build on the watch'
$names = @()
if ($t.Kind -eq 'Drive') {
    $names = @(Get-ChildItem -LiteralPath $t.Root -Filter '*.prg' | ForEach-Object { $_.Name })
} else {
    foreach ($i in $t.Folder.Items()) { if (-not $i.IsFolder -and $i.Name -match '\.prg$') { $names += $i.Name } }
}
Write-Host ('PRG files in GARMIN\Apps: ' + $(if ($names.Count) { $names -join ', ' } else { 'none' }))
$scan = Join-Path $tmp 'scan'
New-Item -ItemType Directory -Force -Path $scan | Out-Null
$ours = @()
foreach ($n in $names) {
    $copy = Get-FromWatch $n $scan
    if ($null -eq $copy) { Fail "could not read $n from the watch to check its app id. Nothing was installed." }
    if (Test-HasAppId ([System.IO.File]::ReadAllBytes($copy))) { $ours += $n }
}
if ($ours.Count -gt 1) { Fail ('more than one LayerTime build is on the watch: ' + ($ours -join ', ') + '. Delete all but one of them from GARMIN\Apps in File Explorer, then run again. Nothing was installed.') }
$targetName = 'LayerTime.prg'
if ($ours.Count -eq 1) {
    $targetName = $ours[0]
    Write-Host "Earlier LayerTime build found: $targetName. It will be replaced."
} else {
    if ($names -contains $targetName) { Fail "GARMIN\Apps holds a $targetName that is not LayerTime. Nothing was installed." }
    Write-Host "No earlier LayerTime build found. Installing as $targetName."
}

# ---------------------------------------------------------------------------
Step "Copying the production app to the watch as GARMIN\Apps\$targetName"
$stage = Join-Path $tmp 'stage'
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$staged = Join-Path $stage $targetName
Copy-Item -LiteralPath $prg -Destination $staged
if ($t.Kind -eq 'Drive') {
    Copy-Item -LiteralPath $staged -Destination (Join-Path $t.Root $targetName) -Force
} else {
    if ($ours.Count -eq 1) { Write-Host 'If Windows asks, choose "Replace the file in the destination".' -ForegroundColor Yellow }
    $t.Folder.CopyHere($staged, 0x14)
}

# ---------------------------------------------------------------------------
Step 'Verifying: reading the app back from the watch and comparing hashes'
$check = Join-Path $tmp 'check'
New-Item -ItemType Directory -Force -Path $check | Out-Null
$verified = $false
$deadline = (Get-Date).AddSeconds(90)
while (-not $verified -and (Get-Date) -lt $deadline) {
    Start-Sleep -Seconds 2
    Remove-Item -LiteralPath (Join-Path $check $targetName) -ErrorAction SilentlyContinue
    $back = Get-FromWatch $targetName $check
    if ($null -ne $back -and (Get-FileHash -Algorithm SHA256 -LiteralPath $back).Hash -eq $localHash) { $verified = $true }
}
if (-not $verified) { Fail "the file on the watch does not match the build (or the copy was skipped). The watch may still hold the earlier build." }

Write-Host ''
Write-Host ("DONE: GARMIN\Apps\{0} on the watch is the production build, SHA-256 {1}." -f $targetName, $localHash) -ForegroundColor Green
Write-Host 'Unplug the watch cable now. The watch installs the app when it leaves USB mode.'
Write-Host 'Then open LayerTime from the watch''s app list.'
exit 0
