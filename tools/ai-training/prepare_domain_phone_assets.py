"""Package only the real trained candidate in the explicitly experimental debug asset slot."""
import argparse
import json
import shutil
import re
from pathlib import Path
from merge_domain_candidate import digest

def prepare(root,weights=None,run=None,build=None,model_version=None):
    app=root.parent.parent/'devices/layertime-controller-android'
    assets=app/'app/src/debug/assets/ai/development';backup=root/'runs/phone-seed-model-backup'
    if not backup.exists():
        backup.mkdir()
        for name in ('package.json','weights.gguf','Qwen-LICENSE'):shutil.copy2(assets/name,backup/name)
        shutil.copy2(app/'app/build/outputs/apk/full/debug/app-full-debug.apk',backup/'app-full-debug.apk')
    shutil.copy2(root/'acceptance/v2/examples.jsonl',assets/'acceptance.jsonl')
    shutil.copy2(root/'acceptance/v2/rubrics.jsonl',assets/'acceptance-rubrics.jsonl')
    gradle=app/'app/build.gradle.kts';raw=gradle.read_bytes()
    current=int(re.search(rb'versionCode = (\d+)',raw).group(1));build=build or current
    assert build>=current,'Do not downgrade the installed build sequence'
    raw=re.sub(rb'versionCode = \d+',('versionCode = '+str(build)).encode(),raw,count=1)
    raw=re.sub(rb'versionName = "[^"]+"',('versionName = "0.1.'+str(build-1)+'"').encode(),raw,count=1)
    gradle.write_bytes(raw)
    if weights is None:return dict(oldModelPreserved=True,acceptanceAssetsCopied=True,build=build,candidatePackaged=False)
    run=run or root/'runs/qwen4b-domain-trained-v1'
    stability=json.loads((run/'optimizer-stability.json').read_text())
    runtime=json.loads((run/'runtime-manifest.json').read_text())
    updates=runtime['requestedArguments']['max_steps']
    assert stability['appliedOptimizerUpdates']==updates and stability['skippedOptimizerUpdates']==0
    parent=runtime['requestedArguments'].get('initial_adapter')
    parent_updates=0
    if parent:
        parent=Path(parent);parent=parent if parent.is_absolute() else root/parent
        assert digest(parent/'adapter_model.safetensors')==runtime['initialAdapterSha256']
        parent_updates=json.loads((parent/'optimizer-stability.json').read_text())['appliedOptimizerUpdates']
    cumulative=parent_updates+updates
    corpus=json.loads((run/'corpus/manifest.json').read_text())
    with weights.open('rb') as stream:assert stream.read(4)==b'GGUF'
    assert weights.stat().st_size>1_000_000_000
    checksum=digest(weights);shutil.copy2(weights,assets/'weights.gguf')
    assert digest(assets/'weights.gguf')==checksum
    shutil.copy2(root/'models/qwen3-4b-instruct-2507/LICENSE',assets/'Qwen-LICENSE')
    package=dict(schemaVersion=1,modelId='layertime-qwen3-4b-domain-development',modelVersion=model_version or 'domain-v1-1090-q4km',
        weightsFile='weights.gguf',sha256=checksum,promptFormat='qwen3-instruct-2507-compact-v1',contextTokens=4096,
        blueTeamTrained=True,releaseEligible=False,developmentOnly=True,tasks=['EXPLAIN_EVIDENCE'],
        trainingPerformed=True,trainingAppliedUpdates=updates,trainingCumulativeAppliedUpdates=cumulative,
        trainingExamples=corpus['splits']['train']['examples'],
        trainingAdapterSha256=digest(run/'adapter_model.safetensors'),
        trainingManifestSha256=digest(run/'corpus/manifest.json'),
        acceptanceExamplesSha256=digest(root/'acceptance/v2/examples.jsonl'),
        acceptanceRubricsSha256=digest(root/'acceptance/v2/rubrics.jsonl'),
        qualification='Actual domain-trained candidate; unqualified for live product assessments; explicit debug evaluation only',
        baseModel=dict(modelId='Qwen/Qwen3-4B-Instruct-2507',license='Apache-2.0',
            intakeSha256=digest(root/'models/qwen3-4b-instruct-2507/intake-manifest.json')),
        quantization='Q4_K_M',experimentalApkCpuMinimum='ARMv8.2 dot-product and FP16; release runtime remains portable',
        runtimeRevision='3b4d6ab3f79651935fba1dd93732f29022a7f5f5')
    (assets/'package.json').write_text(json.dumps(package,indent=2))
    (run/'phone-package-manifest.json').write_text(json.dumps(package,indent=2))
    return package

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--weights',type=Path);p.add_argument('--run',type=Path);p.add_argument('--build',type=int);p.add_argument('--model-version');a=p.parse_args()
    root=Path(__file__).parent
    print(json.dumps(prepare(root,a.weights,a.run,a.build,a.model_version),indent=2))
