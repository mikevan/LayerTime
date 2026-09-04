# Files support/issues/*.md as GitHub issues, in filename order.
#
# Each .md starts with a single "# Title" line; the rest becomes the body.
# Bodies live in files rather than inline so nothing has to survive
# PowerShell quoting.
#
# Needs the GitHub CLI, authenticated as you:
#     winget install GitHub.cli
#     gh auth login
#
# Then, from the repo root:
#     .\support\create_issues.ps1 -WhatIf     show what would be filed
#     .\support\create_issues.ps1             file them

param([switch]$WhatIf)

$ErrorActionPreference = 'Stop'

$root   = Split-Path -Parent $PSScriptRoot
$issues = Join-Path $root 'support\issues'

Set-Location $root

if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw "GitHub CLI not found. Install with: winget install GitHub.cli"
}

gh auth status *> $null
if ($LASTEXITCODE -ne 0) { throw "Not authenticated. Run: gh auth login" }

$files = Get-ChildItem -Path $issues -Filter '*.md' | Sort-Object Name
if ($files.Count -eq 0) { throw "No issue files in $issues" }

Write-Host "$($files.Count) issue(s) to file." -ForegroundColor Cyan
Write-Host ''

foreach ($file in $files) {
    $lines = Get-Content -Path $file.FullName -Encoding UTF8
    $title = ($lines[0] -replace '^#\s*', '').Trim()
    $body  = ''
    if ($lines.Count -gt 1) {
        $body = ($lines[1..($lines.Count - 1)] -join "`n").Trim()
    }

    if (-not $title) { throw "$($file.Name): first line must be '# Title'" }

    if ($WhatIf) {
        Write-Host "would file: $title" -ForegroundColor Yellow
        continue
    }

    Write-Host "filing: $title" -ForegroundColor Cyan
    gh issue create --title $title --body $body
    if ($LASTEXITCODE -ne 0) { throw "Failed on $($file.Name)" }
}

if (-not $WhatIf) {
    Write-Host ''
    Write-Host 'Done. Delete support/issues once they are filed - GitHub is the tracker now.' -ForegroundColor Green
}
