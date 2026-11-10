"""Read-only training inspection; writes separate progress/audit artifacts, never weights."""
import hashlib
import json
import os
import argparse
from pathlib import Path


def inspect(run_name='qwen4b-quantity-v1-adapter',data_version='v1',budget=512):
    if os.environ.get('CUDA_VISIBLE_DEVICES')!='-1':raise ValueError('Run inspection under explicit CPU isolation')
    import torch
    assert not torch.cuda.is_available()
    root=Path(__file__).parent;run=root/'runs'/run_name;data=root/'quantity-data'/data_version
    ledger=json.loads((run/'optimizer-stability.json').read_text())
    checks=json.loads((run/'gradient-checks.json').read_text())
    steps=[s['iteration'] for s in ledger['steps']]
    assert steps==list(range(1,len(steps)+1)) and not any(s['optimizerStepSkipped'] for s in ledger['steps'])
    assert all(g['finite'] for g in checks)
    # Writers update the independent ledgers in order; their live lengths may differ by one.
    assert abs(len(checks)-len(steps))<=1
    audits=[]
    for checkpoint in sorted(run.glob('checkpoint-*'),key=lambda p:int(p.name.split('-')[1])):
        target=run/(checkpoint.name+'-audit.json')
        marker=checkpoint/'checkpoint-integrity.json'
        if target.exists() or not marker.exists():continue
        step=int(checkpoint.name.split('-')[1]);integrity=json.loads(marker.read_text())
        weight=checkpoint/'adapter_model.safetensors'
        assert hashlib.sha256(weight.read_bytes()).hexdigest()==integrity['sha256']
        state=torch.load(checkpoint/'optimizer.pt',map_location='cpu',weights_only=True)
        values=sorted({float(s['step']) for s in state['state'].values()})
        assert values==[float(step)]
        assert all(torch.isfinite(t).all() for s in state['state'].values() for t in s.values() if isinstance(t,torch.Tensor))
        scheduler=torch.load(checkpoint/'scheduler.pt',map_location='cpu',weights_only=True)
        assert scheduler['last_epoch']==step
        audit=dict(checkpointStep=step,optimizerStepValues=values,optimizerStates=len(state['state']),schedulerStep=scheduler['last_epoch'],integrity=integrity)
        target.write_text(json.dumps(audit,indent=2));audits.append(step)
    progress=dict(phase='training',completedTrainingUpdates=len(steps),trainingBudgetUpdates=budget,
        finiteGradientChecks=len(checks),skippedOptimizerUpdates=ledger['skippedOptimizerUpdates'],newlyAuditedCheckpoints=audits,androidDeviceTested=False)
    (data/'progress.json').write_text(json.dumps(progress,indent=2));print(json.dumps(progress))


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--run',default='qwen4b-quantity-v1-adapter');p.add_argument('--data',default='v1');p.add_argument('--budget',type=int,default=512)
    a=p.parse_args();inspect(a.run,a.data,a.budget)
