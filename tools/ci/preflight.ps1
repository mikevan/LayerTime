# LayerTime pre-flight, Windows entry point.
#
# The gate logic lives in tools/ci/preflight.sh and runs in a POSIX
# environment with a host C++ compiler, python3, and git. On Windows that is
# WSL2. This wrapper runs it there, or says exactly what is missing. An ESP32
# cross-compiler is not a host compiler and cannot run the host tests.
#
# Usage:  .\preflight.ps1 [--host|--full] [--targets a,b]
# Exit codes are preflight.sh's: 0 passed, 1 failed, 2 usage or setup error,
# 3 experimental (NOT VALID FOR ACCEPTANCE).
[CmdletBinding()]
param([Parameter(ValueFromRemainingArguments=$true)] [string[]] $Rest)

$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path

if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
  Write-Host 'Prerequisite missing: WSL2 is not installed. See tools/ci/README.md.'
  exit 2
}
$have = wsl bash -lc 'for t in g++ python3 git; do command -v $t >/dev/null 2>&1 || echo $t; done'
if ($LASTEXITCODE -ne 0) { Write-Host 'WSL did not start.'; exit 2 }
if ($have) {
  Write-Host ("Prerequisite missing in WSL: " + ($have -join ', ') + ". Install them in your WSL distro (for example: sudo apt-get install -y build-essential python3 git).")
  exit 2
}
$wslRepo = wsl wslpath -a "$repo"
if ($LASTEXITCODE -ne 0) { Write-Host "WSL cannot see $repo."; exit 2 }
wsl bash -lc "cd '$wslRepo' && bash tools/ci/preflight.sh $($Rest -join ' ')"
exit $LASTEXITCODE
