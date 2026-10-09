# LayerTime pre-flight, Windows entry point.
#
# The gate logic lives in tools/ci/preflight.sh and runs in a POSIX environment
# with a HOST C++ compiler. This wrapper finds a supported environment and runs
# it there, or tells you exactly what is missing. It does NOT assume your ESP32
# cross-compiler provides a host g++; a device toolchain cannot run the host
# tests.
#
# Supported environments (in order of preference):
#   1. WSL2 with g++         - runs the full host profile natively.
#   2. Docker Desktop        - runs the selected profile in the pinned image
#                              (required for --full / firmware builds on Windows).
#
# Usage:  .\preflight.ps1 [--host|--full] [--docker] [--targets a,b]
[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments=$true)] [string[]] $Args)

$ErrorActionPreference = 'Stop'
$repo  = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$wantDocker = $Args -contains '--docker'
$wantFull   = $Args -contains '--full'

function Have($cmd) { return [bool](Get-Command $cmd -ErrorAction SilentlyContinue) }

# --full needs a device toolchain; on Windows that means the pinned image.
if ($wantFull -and -not $wantDocker) {
  Write-Host "Note: --full builds firmware. On Windows, run it in the pinned image: add --docker." -ForegroundColor Yellow
}

if ($wantDocker) {
  if (-not (Have docker)) {
    Write-Error "Prerequisite missing: Docker Desktop is not installed or not on PATH. Install Docker Desktop, or run --host under WSL2. See tools/ci/README.md."
    exit 3
  }
  if (-not $env:LAYERTIME_IMAGE) {
    Write-Error "Prerequisite missing: set LAYERTIME_IMAGE to the pinned image (e.g. ghcr.io/<owner>/layertime-ci:<tag>) before using --docker."
    exit 3
  }
  $argline = ($Args | Where-Object { $_ -ne '--docker' }) -join ' '
  docker run --rm -v "${repo}:/work" -w /work $env:LAYERTIME_IMAGE bash tools/ci/preflight.sh $argline.Split(' ')
  exit $LASTEXITCODE
}

# Host profile: prefer WSL2 with a host g++.
if (Have wsl) {
  $hasGpp = (wsl bash -lc "command -v g++ >/dev/null 2>&1 && echo yes" ) 2>$null
  if ($hasGpp -match 'yes') {
    $wslRepo = (wsl wslpath -a "$repo") 2>$null
    wsl bash -lc "cd '$wslRepo' && bash tools/ci/preflight.sh $($Args -join ' ')"
    exit $LASTEXITCODE
  } else {
    Write-Error "WSL is present but has no host C++ compiler. Install one in your WSL distro (e.g. 'sudo apt-get install -y build-essential'), or use --docker. An ESP32 cross-compiler does not count as a host compiler."
    exit 3
  }
}

Write-Error "No supported environment found. Install WSL2 with build-essential (host g++), or Docker Desktop and run with --docker. See tools/ci/README.md for supported environments."
exit 3
