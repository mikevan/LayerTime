"""Refuse checkpoint recovery across changed data, code, or training settings."""
import hashlib
import json
from pathlib import Path
from prepare import verify


def validate_resume(output,requested,sources,manifest):
    output=Path(output)
    runtime=json.loads((output/'runtime-manifest.json').read_text(encoding='utf-8'))
    old=runtime['requestedArguments']
    transient={'resume_last_checkpoint','pause_at_checkpoint','dry_run'}
    if {k:v for k,v in requested.items() if k not in transient}!={k:v for k,v in old.items() if k not in transient}:
        raise ValueError('Recovery arguments changed; preserve the original training configuration')
    if {name:hashlib.sha256(raw).hexdigest() for name,raw in sources.items()}!=runtime['sourceSha256']:
        raise ValueError('Recovery training source differs from the saved snapshot')
    if hashlib.sha256((output/'corpus/manifest.json').read_bytes()).hexdigest()!=runtime['corpusManifestSha256'] or verify(output/'corpus')!=manifest:
        raise ValueError('Recovery corpus changed')
    checkpoints=sorted((p for p in output.glob('checkpoint-*') if p.name[11:].isdigit()),key=lambda p:int(p.name[11:]))
    if not checkpoints:raise ValueError('No checkpoint is available')
    checkpoint=checkpoints[-1]
    for filename in ('adapter_model.safetensors','adapter_config.json','optimizer.pt','scheduler.pt','scaler.pt','rng_state.pth','trainer_state.json','checkpoint-integrity.json'):
        if not (checkpoint/filename).is_file():raise ValueError('Latest checkpoint is incomplete: '+filename)
    step=json.loads((checkpoint/'trainer_state.json').read_text(encoding='utf-8'))['global_step']
    if step!=int(checkpoint.name[11:]) or not 0<step<requested['max_steps']:
        raise ValueError('Checkpoint does not lie inside the requested update budget')
    integrity=json.loads((checkpoint/'checkpoint-integrity.json').read_text(encoding='utf-8'))
    if integrity.get('globalStep')!=step or integrity.get('allWeightsFinite') is not True or integrity.get('sha256')!=hashlib.sha256((checkpoint/'adapter_model.safetensors').read_bytes()).hexdigest():
        raise ValueError('Checkpoint weight integrity check failed')
    steps=json.loads((output/'optimizer-stability.json').read_text(encoding='utf-8'))['steps']
    checks=json.loads((output/'gradient-checks.json').read_text(encoding='utf-8'))
    steps=[s for s in steps if s['iteration']<=step];checks=[s for s in checks if s['iteration']<=step]
    if [s['iteration'] for s in steps]!=list(range(1,step+1)) or [s['iteration'] for s in checks]!=list(range(1,step+1)):
        raise ValueError('Checkpoint audit ledger is incomplete or duplicated')
    if not all(c['finite'] for c in checks):raise ValueError('Checkpoint lineage includes nonfinite gradients')
    return checkpoint,step,runtime,steps,checks
