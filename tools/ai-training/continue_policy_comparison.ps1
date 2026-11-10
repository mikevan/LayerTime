# Continue only the four pending conditions after the documented template correction.
# No training, model export, app build or phone deployment occurs here.
$ErrorActionPreference='Stop'
$repoRoot=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
Set-Location -LiteralPath $repoRoot
$pythonPath=Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
$runRoot=Join-Path $PSScriptRoot 'runs'
$queueRoot=Join-Path $runRoot 'policy-comparison-v2'
if(Test-Path -LiteralPath $queueRoot){throw 'Existing queue must not be overwritten.'}
$retention=Get-Content (Join-Path $runRoot 'prompt-retention-audit-v1/report.json') -Raw | ConvertFrom-Json
if(!$retention.qwenCompletedConditionsRemainValid -or
    ($retention.results | Where-Object {!$_.correctedSmolAllMessageContentRetained -or !$_.budgetFits})){throw 'Prompt retention audit failed.'}
$examplesHash=(Get-FileHash 'tools/ai-training/policy-data/v4/prepared/test.jsonl' -Algorithm SHA256).Hash.ToLowerInvariant()
foreach($previous in @('qwen4b-policy-v4-adapted','qwen4b-policy-v4-base')){
    $prior=Get-Content (Join-Path $runRoot "$previous/report.json") -Raw | ConvertFrom-Json
    if($prior.examples -ne 64 -or $prior.examplesSha256 -ne $examplesHash){throw 'Completed Qwen condition differs from frozen corpus.'}
}
$trainingRoot=Join-Path $runRoot 'qwen4b-policy-verbalization-v1'
$adapterHash=(Get-FileHash (Join-Path $trainingRoot 'adapter_model.safetensors') -Algorithm SHA256).Hash.ToLowerInvariant()
if($adapterHash -ne '9fff76a1e4db5162da3aac2c243d3b922e7a42b3086418276caf0e4de78e0166'){throw 'Adapter differs from the checked training artifact.'}
New-Item -ItemType Directory -Path $queueRoot | Out-Null
Copy-Item -LiteralPath $PSCommandPath -Destination (Join-Path $queueRoot 'queue-source.ps1')
$started=[DateTime]::UtcNow.ToString('o');$queueResults=@()
function Write-QueueState([string]$phase){
    [ordered]@{startedUtc=$started;updatedUtc=[DateTime]::UtcNow.ToString('o');phase=$phase;results=@($queueResults);
        reusedValidConditions=@('qwen4b-policy-v4-adapted','qwen4b-policy-v4-base');
        excludedCondition='smollm3-policy-v4-base: task instructions dropped by native template';releaseEligible=$false} |
        ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $queueRoot 'status.json') -Encoding utf8
}
$env:HF_HUB_OFFLINE='1';$env:HF_DATASETS_OFFLINE='1';$env:HF_HUB_DISABLE_TELEMETRY='1'
$conditions=@(
    @{name='smollm3-policy-v4-system-v2';model='smollm3-3b';adapter=$false;set='policy';merge=$true},
    @{name='qwen4b-correlation-v2-adapted';model='qwen3-4b-instruct-2507';adapter=$true;set='timeline';merge=$false},
    @{name='qwen4b-correlation-v2-base';model='qwen3-4b-instruct-2507';adapter=$false;set='timeline';merge=$false},
    @{name='smollm3-correlation-v2-system-v2';model='smollm3-3b';adapter=$false;set='timeline';merge=$true}
)
foreach($condition in $conditions){
    $examples=if($condition.set -eq 'policy'){'tools/ai-training/policy-data/v4/prepared/test.jsonl'}else{'tools/ai-training/correlation-data/v2/candidates.jsonl'}
    $limit=if($condition.set -eq 'policy'){64}else{19}
    $destination=Join-Path $runRoot $condition.name
    $arguments=@('-u','tools/ai-training/evaluate_local_model.py','--model',"tools/ai-training/models/$($condition.model)",
        '--examples',$examples,'--output',$destination,'--limit',"$limit",'--max-output-tokens','1024','--max-context-tokens','10240')
    if($condition.adapter){$arguments+=@('--adapter',$trainingRoot)}
    if($condition.merge){$arguments+='--merge-system-messages'}
    $arguments | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $queueRoot "$($condition.name)-arguments.json")
    Write-QueueState "evaluating-$($condition.name)"
    & $pythonPath @arguments 2>&1 | Tee-Object -FilePath (Join-Path $queueRoot "$($condition.name).log")
    $evaluationExit=$LASTEXITCODE;$scoringExit=$null
    if($evaluationExit -eq 0 -and $condition.set -eq 'policy'){
        & $pythonPath tools/ai-training/score_policy_predictions.py --examples $examples --run $destination 2>&1 |
            Tee-Object -FilePath (Join-Path $queueRoot "$($condition.name)-coverage.log")
        $scoringExit=$LASTEXITCODE
    }
    $queueResults+=@{condition=$condition.name;evaluationExitCode=$evaluationExit;scoringExitCode=$scoringExit;
        state=$(if($evaluationExit -eq 0){'generated-pending-prose-review'}else{'evaluation-failed'})}
    Write-QueueState 'between-sequential-models'
}
Write-QueueState 'queue-terminal-manual-interpretation-review-still-required'
