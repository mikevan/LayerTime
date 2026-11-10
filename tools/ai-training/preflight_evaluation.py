"""Measure actual model-template input lengths without occupying the training GPU."""
import argparse
import hashlib
import json
from pathlib import Path
from evaluate_local_model import render_evaluation_prompt


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--model',type=Path,required=True)
    parser.add_argument('--examples',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--max-output-tokens',type=int,default=1024)
    parser.add_argument('--max-context-tokens',type=int,default=10240)
    parser.add_argument('--merge-system-messages',action='store_true')
    args=parser.parse_args()
    from transformers import AutoTokenizer,AutoConfig
    tokenizer=AutoTokenizer.from_pretrained(args.model,local_files_only=True)
    config=AutoConfig.from_pretrained(args.model,local_files_only=True)
    if args.max_context_tokens>config.max_position_embeddings:raise ValueError('Context exceeds model configuration')
    rows=[]
    for raw in args.examples.read_text(encoding='utf-8').splitlines():
        record=json.loads(raw)
        prompt=render_evaluation_prompt(tokenizer,record,merge_system_messages=args.merge_system_messages)
        n=len(tokenizer(prompt,add_special_tokens=False)['input_ids'])
        rows.append(dict(id=record['id'],inputTokens=n,budgetFits=n+args.max_output_tokens<=args.max_context_tokens))
    report=dict(model=str(args.model),examples=len(rows),examplesSha256=hashlib.sha256(args.examples.read_bytes()).hexdigest(),
        maximumInputTokens=max(r['inputTokens'] for r in rows),reservedOutputTokens=args.max_output_tokens,
        contextTokens=args.max_context_tokens,allFit=all(r['budgetFits'] for r in rows),gpuUsed=False,
        leadingSystemMessagesMerged=args.merge_system_messages,renderedMessageContentRetentionChecked=True,results=rows)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='results'}))
    if not report['allFit']:raise SystemExit(1)
