<#
Preview by default. No files are staged, committed or pushed without -Execute.
With neither -Paths nor -Acceptance, this pushes existing local commits only.
With -Acceptance, it selects acceptance work and the documentation collection.
With -Paths, it stages only those explicit repository-relative paths first.
No pull, merge, reset, amend, force push or automatic broad staging.
#>
[CmdletBinding()]
param(
    [string[]]$Paths = @(),
    [string]$Message = 'Checkpoint AI acceptance work',
    [string]$MessageFile = '',
    [string]$Remote = 'origin',
    [switch]$Acceptance,
    [switch]$Execute,
    [string]$RepoPath = ''
)

$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($RepoPath)) {
    $scriptDirectory = Split-Path -Parent $MyInvocation.MyCommand.Path
    $RepoPath = Join-Path $scriptDirectory '../..'
}
if ($Acceptance) {
    if ($Paths.Count) { throw 'Use either -Acceptance or -Paths, not both.' }
    $Paths = @(
        'tools/ai-training/acceptance',
        'tools/ai-training/build_acceptance_v1.py',
        'tools/ai-training/build_acceptance_v2.py',
        'tools/ai-training/score_acceptance.py',
        'tools/ai-training/test_acceptance.py',
        'tools/ai-training/checkpoint-and-push.ps1',
        'tools/ai-training/collect_development_docs.py',
        'tools/ai-training/README.md',
        'docs/.gitattributes',
        'docs/ai-development',
        'docs/ai-development-checkpoint-and-acceptance-2026-11-10.md'
    )
}

function Invoke-GitChecked {
    param([string[]]$GitArguments)
    & git -C $script:Repository @GitArguments
    if ($LASTEXITCODE -ne 0) {
        throw "Git failed ($LASTEXITCODE). Stopped; no recovery commands were run."
    }
}

$script:Repository = (Resolve-Path -LiteralPath $RepoPath).Path
$root = & git -C $script:Repository rev-parse --show-toplevel
if ($LASTEXITCODE -ne 0) { throw 'This is not a Git repository.' }
$script:Repository = (Resolve-Path -LiteralPath $root).Path
if ($Acceptance) {
    $manifestPath = Join-Path $script:Repository 'docs/ai-development/document-manifest.json'
    if (-not (Test-Path -LiteralPath $manifestPath)) { throw 'Build the documentation collection before using -Acceptance.' }
    $documentation = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    foreach ($entry in $documentation.documents) {
        $source = [string]$entry.sourcePath
        $isDoc = $source.StartsWith('docs/') -or $source.StartsWith('tools/ai-training/domain-data/') -or
            $source.StartsWith('tools/ai-training/acceptance/') -or
            $source -in @('tools/ai-training/README.md', 'tools/ai-toolkit-evaluation/README.md')
        if (-not $isDoc -or [IO.Path]::GetExtension($source) -notin @('.md', '.docx')) {
            throw "Unexpected documentation source in manifest: $source"
        }
        $Paths += $source
    }
    $Paths = @($Paths | Select-Object -Unique)
}
$resolvedMessageFile = ''
if ($MessageFile) {
    $candidateMessageFile = if ([IO.Path]::IsPathRooted($MessageFile)) { $MessageFile } else { Join-Path $script:Repository $MessageFile }
    $resolvedMessageFile = (Resolve-Path -LiteralPath $candidateMessageFile).Path
    if (-not (Test-Path -LiteralPath $resolvedMessageFile -PathType Leaf)) { throw 'Commit note must be a file.' }
}
$branch = & git -C $script:Repository symbolic-ref --quiet --short HEAD
if ($LASTEXITCODE -ne 0) { throw 'Detached HEAD: select a branch yourself first.' }
$branch = $branch.Trim()
if ($Remote.StartsWith('-')) { throw 'Use a configured remote name.' }
$remotes = @(& git -C $script:Repository remote)
if ($LASTEXITCODE -ne 0 -or $Remote -notin $remotes) { throw "Remote '$Remote' is not configured." }

foreach ($path in $Paths) {
    if ([string]::IsNullOrWhiteSpace($path) -or [IO.Path]::IsPathRooted($path) -or
        $path.StartsWith('-') -or $path -in @('.', '*') -or
        (($path -split '[/\\]') -contains '..') -or $path.StartsWith(':')) {
        throw "Specify individual repository-relative files or folders, not '$path'."
    }
    if (-not (Test-Path -LiteralPath (Join-Path $script:Repository $path))) {
        throw "Path does not exist: $path. Stage deletions separately yourself."
    }
}

Write-Host "Repository: $script:Repository"
Write-Host "Branch: $branch -> $Remote/$branch"
Invoke-GitChecked -GitArguments @('status', '--short')
Write-Host ''
Write-Host 'Latest local commit:'
Invoke-GitChecked -GitArguments @('log', '-1', '--oneline')
if ($Paths.Count) {
    Write-Host "Explicit paths to check in: $($Paths -join ', ')"
    if ($resolvedMessageFile) { Write-Host "Commit note: $resolvedMessageFile" }
    else { Write-Host "Commit message: $Message" }
} else {
    Write-Host 'No paths selected: only existing local commits will be pushed.'
}

if (-not $Execute) {
    Write-Host 'PREVIEW ONLY. Run again with -Execute to perform these actions.'
    return
}

if ($Paths.Count) {
    $alreadyStaged = @(& git -C $script:Repository diff --cached --name-only)
    if ($LASTEXITCODE -ne 0) { throw 'Could not inspect the staging area.' }
    if ($alreadyStaged.Count) { throw 'Files are already staged. Review them yourself before running this script.' }
    # Literal pathspecs prevent wildcard or pathspec expansion.
    $literalPaths = @($Paths | ForEach-Object { ':(literal)' + $_ })
    Invoke-GitChecked -GitArguments (@('add', '--') + $literalPaths)
    # Preserve the exact bytes declared by the selected provenance attributes.
    Invoke-GitChecked -GitArguments (@('add', '--renormalize', '--') + $literalPaths)
    & git -C $script:Repository diff --cached --quiet
    $diffCode = $LASTEXITCODE
    if ($diffCode -eq 1) {
        Invoke-GitChecked -GitArguments @('diff', '--cached', '--stat')
        if ($resolvedMessageFile) {
            Invoke-GitChecked -GitArguments @('commit', '--cleanup=verbatim', '-F', $resolvedMessageFile)
        } else { Invoke-GitChecked -GitArguments @('commit', '-m', $Message) }
    } elseif ($diffCode -eq 0) {
        Write-Host 'Selected paths have no changes to commit.'
    } else { throw 'Could not check staged changes.' }
}

# Normal push only. A remote rejection stops here; no force or automatic merge.
$currentBranch = & git -C $script:Repository symbolic-ref --quiet --short HEAD
if ($LASTEXITCODE -ne 0 -or $currentBranch.Trim() -ne $branch) {
    throw 'Branch changed during this operation. Review it yourself before pushing.'
}
Invoke-GitChecked -GitArguments @('push', '--set-upstream', $Remote, "HEAD:refs/heads/$branch")
Write-Host 'Push completed.'
