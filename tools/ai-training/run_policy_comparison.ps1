param([Parameter(Mandatory=$true)][int]$TrainingProcessId)

# Desktop experiment queue only. This script neither builds nor deploys an app.
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location -LiteralPath $repoRoot
$pythonPath = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
$runRoot = Join-Path $PSScriptRoot 'runs'
$trainingRoot = Join-Path $runRoot 'qwen4b-policy-verbalization-v1'
$queueRoot = Join-Path $runRoot 'policy-comparison-v1'
if (Test-Path -LiteralPath $queueRoot) { throw 'Use a new queue version; existing experiment must not be overwritten.' }
New-Item -ItemType Directory -Path $queueRoot | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $queueRoot 'queue-source.ps1')
$started = [DateTime]::UtcNow.ToString('o')
$queueResults = @()
function Write-QueueState([string]$phase) {
    [ordered]@{startedUtc=$started;updatedUtc=[DateTime]::UtcNow.ToString('o');phase=$phase;
        trainingProcessId=$TrainingProcessId;results=@($queueResults);releaseEligible=$false} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $queueRoot 'status.json') -Encoding utf8
}

# Hold an OS process object, rather than deciding completion from a stale ledger.
$trainingCommand = (Get-CimInstance Win32_Process -Filter "ProcessId=$TrainingProcessId").CommandLine
if (!$trainingCommand -or !$trainingCommand.Contains('train_lora.py') -or
    !$trainingCommand.Contains('qwen4b-policy-verbalization-v1')) {
    throw 'The supplied live process is not the expected training job.'
}
$trainingProcess = Get-Process -Id $TrainingProcessId
$trainingProcess.StartTime.ToUniversalTime().ToString('o') |
    Set-Content -LiteralPath (Join-Path $queueRoot 'training-process-start.txt')
$trainingCommand | Set-Content -LiteralPath (Join-Path $queueRoot 'training-command.txt')
Write-QueueState 'waiting-for-exact-training-process'
while (!$trainingProcess.WaitForExit(1000)) { }
Write-QueueState 'checking-training-artifacts'

$adapterReady = $false
if ((Test-Path "$trainingRoot/development-manifest.json") -and
    (Test-Path "$trainingRoot/training-metrics.json") -and
    (Test-Path "$trainingRoot/weight-stability.json")) {
    $weights = Get-Content "$trainingRoot/weight-stability.json" -Raw | ConvertFrom-Json
    $ledger = Get-Content "$trainingRoot/optimizer-stability.json" -Raw | ConvertFrom-Json
    $adapterReady = $weights.allWeightsFinite -and $weights.nonzeroLoraBTensors -gt 0 -and $ledger.recordedIterations -eq 128
}
if ($adapterReady) {
    # Recheck actual saved tensors on CPU. A ledger/manifest alone is insufficient.
    $env:CUDA_VISIBLE_DEVICES = '-1'
    & $pythonPath -c "import sys;sys.path.insert(0,'tools/ai-training');from training_stability import inspect_adapter;result=inspect_adapter('tools/ai-training/runs/qwen4b-policy-verbalization-v1/adapter_model.safetensors');print(result);assert result['allWeightsFinite'] and result['nonzeroLoraBTensors']>0" *> (Join-Path $queueRoot 'saved-weight-recheck.txt')
    $adapterReady = $LASTEXITCODE -eq 0
    Remove-Item Env:CUDA_VISIBLE_DEVICES
}
$env:HF_HUB_OFFLINE='1'
$env:HF_DATASETS_OFFLINE='1'
$env:HF_HUB_DISABLE_TELEMETRY='1'
$conditions = @(
    @{name='qwen4b-policy-v4-adapted';model='qwen3-4b-instruct-2507';adapter=$true;set='policy'},
    @{name='qwen4b-policy-v4-base';model='qwen3-4b-instruct-2507';adapter=$false;set='policy'},
    @{name='smollm3-policy-v4-base';model='smollm3-3b';adapter=$false;set='policy'},
    @{name='qwen4b-correlation-v2-adapted';model='qwen3-4b-instruct-2507';adapter=$true;set='correlation'},
    @{name='qwen4b-correlation-v2-base';model='qwen3-4b-instruct-2507';adapter=$false;set='correlation'},
    @{name='smollm3-correlation-v2-base';model='smollm3-3b';adapter=$false;set='correlation'}
)
foreach ($condition in $conditions) {
    if ($condition.adapter -and !$adapterReady) {
        $queueResults += @{condition=$condition.name;state='skipped-failed-adapter-gate'}
        Write-QueueState 'continuing-independent-base-comparisons'
        continue
    }
    $examples = if ($condition.set -eq 'policy') {'tools/ai-training/policy-data/v4/prepared/test.jsonl'} else {'tools/ai-training/correlation-data/v2/candidates.jsonl'}
    $limit = if ($condition.set -eq 'policy') {64} else {19}
    $destination = Join-Path $runRoot $condition.name
    $arguments = @('-u','tools/ai-training/evaluate_local_model.py','--model',"tools/ai-training/models/$($condition.model)",
        '--examples',$examples,'--output',$destination,'--limit',"$limit",'--max-output-tokens','1024','--max-context-tokens','10240')
    if ($condition.adapter) {$arguments += @('--adapter',$trainingRoot)}
    $arguments | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $queueRoot "$($condition.name)-arguments.json")
    Write-QueueState "evaluating-$($condition.name)"
    & $pythonPath @arguments 2>&1 | Tee-Object -FilePath (Join-Path $queueRoot "$($condition.name).log")
    $evaluationExit = $LASTEXITCODE
    $scoringExit = $null
    if ($evaluationExit -eq 0 -and $condition.set -eq 'policy') {
        & $pythonPath tools/ai-training/score_policy_predictions.py --examples $examples --run $destination 2>&1 |
            Tee-Object -FilePath (Join-Path $queueRoot "$($condition.name)-coverage.log")
        $scoringExit = $LASTEXITCODE
    }
    $queueResults += @{condition=$condition.name;evaluationExitCode=$evaluationExit;scoringExitCode=$scoringExit;
        state=$(if ($evaluationExit -eq 0) {'generated-pending-prose-review'} else {'evaluation-failed'})}
    Write-QueueState 'between-sequential-models'
}
Write-QueueState 'queue-terminal-manual-interpretation-review-still-required'
