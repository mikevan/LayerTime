# Builds the single merged binary the browser flasher serves.
#
#   docs/index.html  ->  docs/manifest.json  ->  docs/firmware/layertime.bin
#
# ESP Web Tools flashes one image at offset 0, so the four pieces PlatformIO
# produces have to be merged first. Offsets and flash settings come from
# boards/lilygo-t-watch-ultra.json (qio / 80 MHz / 16 MB) and the partition
# table in LilyGoLib's factory example (app0 at 0x10000).
#
# Run from anywhere with the PlatformIO venv active:
#     .\support\make_flasher_bin.ps1          merge whatever is already built
#     .\support\make_flasher_bin.ps1 -Build   compile first, then merge
#
# NOTE: PowerShell variable names are case-insensitive, so a local named
# $build would collide with the -Build switch. Hence $buildDir.

param([switch]$Build)

$ErrorActionPreference = 'Stop'

$root     = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $root '.pio\build\twatch_ultra'
$outFile  = Join-Path $root 'docs\firmware\layertime.bin'

Set-Location $root

if ($Build) {
    Write-Host 'Building...' -ForegroundColor Cyan
    pio run
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
}

# boot_app0.bin ships with the Arduino core, not with the project.
$bootloader = Join-Path $buildDir 'bootloader.bin'
$partitions = Join-Path $buildDir 'partitions.bin'
$firmware   = Join-Path $buildDir 'firmware.bin'
$bootApp0   = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'

foreach ($f in @($bootloader, $partitions, $firmware, $bootApp0)) {
    if (-not (Test-Path $f)) {
        throw "Missing: $f`nRun a build first, or pass -Build."
    }
}

New-Item -ItemType Directory -Force -Path (Split-Path $outFile) | Out-Null

Write-Host 'Merging...' -ForegroundColor Cyan

# esptool v5 renamed the command and the options; v4 only knows the old ones.
# Try modern first, fall back once, so this works on either.
$images = @(
    '0x0',     $bootloader,
    '0x8000',  $partitions,
    '0xe000',  $bootApp0,
    '0x10000', $firmware
)

pio pkg exec -p tool-esptoolpy -- esptool --chip esp32s3 merge-bin `
    -o $outFile --flash-mode qio --flash-freq 80m --flash-size 16MB @images

if ($LASTEXITCODE -ne 0) {
    Write-Host 'Modern esptool syntax failed; retrying the pre-v5 form...' -ForegroundColor Yellow
    pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 merge_bin `
        -o $outFile --flash_mode qio --flash_freq 80m --flash_size 16MB @images
}

if ($LASTEXITCODE -ne 0) { throw 'merge failed under both esptool syntaxes.' }

$mb = [math]::Round((Get-Item $outFile).Length / 1MB, 2)
Write-Host ''
Write-Host "Wrote docs\firmware\layertime.bin ($mb MB)" -ForegroundColor Green
Write-Host 'Bump "version" in docs\manifest.json, then commit and push.' -ForegroundColor Yellow
