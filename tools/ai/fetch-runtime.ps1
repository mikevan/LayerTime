$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$lock=Get-Content -LiteralPath (Join-Path $projectRoot 'third_party/llama.cpp.lock.json') -Raw | ConvertFrom-Json
$target=Join-Path $projectRoot 'third_party/llama.cpp'
if(!(Test-Path -LiteralPath $target)) {
    git clone --no-checkout $lock.repository $target
    if($LASTEXITCODE -ne 0){throw 'Runtime clone failed'}
    git -C $target checkout --detach $lock.revision
    if($LASTEXITCODE -ne 0){throw 'Pinned runtime checkout failed'}
}
$actual=git -C $target rev-parse HEAD
if($actual -ne $lock.revision){throw 'Existing runtime revision differs from the lock; preserve it and inspect before changing it.'}
Write-Output "Embedded inference runtime verified at $actual"
