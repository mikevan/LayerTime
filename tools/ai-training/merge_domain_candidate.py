"""Merge an audited domain adapter into its clean base on CPU for phone conversion."""
import argparse
import hashlib
import json
from pathlib import Path

def digest(path):
    h=hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda:stream.read(8*1024*1024),b''):h.update(chunk)
    return h.hexdigest()

def merge(base,adapter,output):
    import torch
    from transformers import AutoModelForCausalLM,AutoTokenizer
    from peft import PeftModel
    metrics=json.loads((adapter/'training-metrics.json').read_text())
    stability=json.loads((adapter/'optimizer-stability.json').read_text())
    weights=json.loads((adapter/'weight-stability.json').read_text())
    runtime=json.loads((adapter/'runtime-manifest.json').read_text())
    assert runtime['requestedArguments']['domain']
    expected=runtime['requestedArguments']['max_steps']
    assert metrics['globalStep']==stability['appliedOptimizerUpdates']==expected and stability['skippedOptimizerUpdates']==0
    if runtime['requestedArguments']['initial_adapter']:
        assert digest(adapter/'initial-adapter/adapter_model.safetensors')==runtime['initialAdapterSha256']
    assert weights['allWeightsFinite']
    assert digest(base/'intake-manifest.json')==digest(adapter/'base-intake-manifest.json')
    if output.exists():raise ValueError('Preserve existing merged candidate')
    torch.set_num_threads(4);torch.set_grad_enabled(False)
    model=AutoModelForCausalLM.from_pretrained(str(base),local_files_only=True,dtype=torch.float16,device_map={'':'cpu'},low_cpu_mem_usage=True)
    wrapped=PeftModel.from_pretrained(model,str(adapter),local_files_only=True,is_trainable=False,device_map={'':'cpu'})
    merged=wrapped.merge_and_unload(safe_merge=True)
    merged.save_pretrained(output,safe_serialization=True,max_shard_size='2GB')
    AutoTokenizer.from_pretrained(str(base),local_files_only=True).save_pretrained(output)
    receipt=dict(baseIntakeSha256=digest(base/'intake-manifest.json'),adapterSha256=digest(adapter/'adapter_model.safetensors'),
        initialAdapterSha256=runtime.get('initialAdapterSha256'),appliedUpdatesInThisStage=expected,
        trainingMetricsSha256=digest(adapter/'training-metrics.json'),mergedDtype='float16',safeMerge=True,
        mergedFileSha256={p.name:digest(p) for p in output.iterdir() if p.is_file()},releaseEligible=False)
    (output/'merge-manifest.json').write_text(json.dumps(receipt,indent=2));return receipt

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--base',type=Path,required=True);parser.add_argument('--adapter',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    a=parser.parse_args();print(json.dumps(merge(a.base,a.adapter,a.output),indent=2))
