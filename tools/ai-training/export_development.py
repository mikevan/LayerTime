"""Merge a smoke adapter and package a debug-only GGUF. Never marks it release eligible."""
import argparse
import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--resume-export',action='store_true',help='Reuse the merged weights after a conversion failure')
    args=parser.parse_args()
    run=args.run.resolve()
    manifest=json.loads((run/'development-manifest.json').read_text())
    if manifest['releaseEligible'] or manifest['blueTeamTrained']:
        raise ValueError('This exporter accepts unqualified smoke adapters only')
    root=Path(__file__).resolve().parents[2]
    runtime=root/'third_party/llama.cpp'
    lock=json.loads((root/'third_party/llama.cpp.lock.json').read_text())
    revision=subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()
    if revision not in lock.values():raise ValueError('Runtime revision differs from the checked-in lock')
    output=run/'export'
    output.mkdir(exist_ok=args.resume_export)
    base=Path(manifest['baseModel'])
    import torch
    from transformers import AutoModelForCausalLM,AutoTokenizer
    from peft import PeftModel
    merged=output/'merged'
    if not args.resume_export:
        model=AutoModelForCausalLM.from_pretrained(str(base),local_files_only=True,dtype=torch.float16)
        model=PeftModel.from_pretrained(model,str(run),local_files_only=True).merge_and_unload()
        model.save_pretrained(merged,safe_serialization=True)
        AutoTokenizer.from_pretrained(str(base),local_files_only=True).save_pretrained(merged)
        del model
    elif not (merged/'model.safetensors').is_file():raise ValueError('No merged weights to resume')
    weights=output/'weights.gguf'
    subprocess.run([sys.executable,str(runtime/'convert_hf_to_gguf.py'),str(merged),
                    '--outfile',str(weights),'--outtype','q8_0'],check=True)
    with weights.open('rb') as stream:sha=hashlib.file_digest(stream,'sha256').hexdigest()
    package=dict(schemaVersion=1,modelId='layertime-qwen3-06b-development',modelVersion=run.name,
                 weightsFile='weights.gguf',sha256=sha,promptFormat='qwen3-chatml-no-thinking-v1',
                 contextTokens=8192,blueTeamTrained=False,releaseEligible=False,
                 tasks=['REVIEW_CAPTURE','EXPLAIN_EVIDENCE','CREATE_ANALYTIC','DEFINE_ATOM','DEFINE_AGENT'],
                 baseModel=json.loads((base/'source.json').read_text()),runtimeRevision=revision,
                 trainingCorpus=manifest['corpus'])
    (output/'package.json').write_text(json.dumps(package,indent=2),encoding='utf-8')
    destination=root/'devices/layertime-controller-android/app/src/debug/assets/ai/development'
    destination.mkdir(parents=True,exist_ok=True)
    for item in (weights,output/'package.json'):shutil.copy2(item,destination/item.name)
    shutil.copy2(base/'LICENSE',destination/'Qwen-LICENSE')
    print(json.dumps({'debugPackage':str(destination),'bytes':weights.stat().st_size,'sha256':sha,
                      'releaseEligible':False},indent=2))

if __name__=='__main__':main()
