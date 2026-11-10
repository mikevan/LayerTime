"""Freeze the matched interface diagnostic and measure every proposed training item."""
import hashlib
import json
from pathlib import Path
from audit_domain_pilot_v3 import rows
from build_domain_pilot_v3 import transform_record
from evaluate_local_model import render_evaluation_prompt


def freeze(root,base):
    from transformers import AutoTokenizer
    tokenizer=AutoTokenizer.from_pretrained(str(base),local_files_only=True)
    out=root/'domain-data/pilot-v3-evaluation';out.mkdir(exist_ok=False)
    old=rows(root/'domain-data/baseline-v1/examples.jsonl')
    parents={r['id']:r for r in rows(root/'domain-data/v2/candidates.jsonl')}
    examples=[]
    for record in old:
        updated=transform_record(parents[record['id']])
        updated['evaluationRole']=record['evaluationRole']
        updated['evaluationExposure']='Previously exposed diagnostic question, revised interface; not a fresh benchmark'
        examples.append(updated)
    path=out/'examples.jsonl';path.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in examples),encoding='utf-8')
    lengths=[]
    for record in rows(root/'domain-data/pilot-v3-curriculum/proposed-train.jsonl'):
        prompt=render_evaluation_prompt(tokenizer,record)
        incoming=len(tokenizer(prompt,add_special_tokens=False)['input_ids'])
        outgoing=len(tokenizer(record['messages'][-1]['content'],add_special_tokens=False)['input_ids'])+1
        assert incoming+outgoing<=4096, 'Oversize example: '+record['id']
        lengths.append(dict(id=record['id'],inputTokens=incoming,targetTokensIncludingEos=outgoing,totalTokens=incoming+outgoing))
    (out/'training-token-lengths.json').write_text(json.dumps(lengths,indent=2))
    max_input=max(len(tokenizer(render_evaluation_prompt(tokenizer,r),add_special_tokens=False)['input_ids']) for r in examples)
    assert max_input+384<=4096
    protocol=dict(status='frozen-before-new-baseline-or-training',examples=len(examples),examplesSha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        baseModel='Qwen3-4B-Instruct-2507',adapter=None,maxContextTokens=4096,maxOutputTokens=384,maxInputTokens=max_input,
        proposedTrainingExamples=len(lengths),proposedTrainingTotalTokens=sum(r['totalTokens'] for r in lengths),
        proposedTrainingTargetTokensIncludingEos=sum(r['targetTokensIncludingEos'] for r in lengths),
        maximumTrainingExampleTokens=max(r['totalTokens'] for r in lengths),sampling='One greedy response; no repair or retry',
        interpretationReview=['Correct target/network distinction and time order','No configured-window-as-duration claim',
            'No cause, normality, sender identity or successful reconnect inferred without evidence',
            'Feature meanings agree with the pinned extractor contract','Important observations appear before details'],
        limitations=['Same 16 exposed diagnostic questions; interface changed. Not an independent or field qualification test.',
            'Schema/reference pass rates are reported separately from agent prose review.',
            'Training-family and reserved-family/session results must be separated.',
            'No Android timing or on-device qualification measured.'],trainingPerformed=False,releaseEligible=False)
    (out/'protocol.json').write_text(json.dumps(protocol,indent=2));return protocol


if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser();parser.add_argument('--base-model',type=Path,required=True)
    print(json.dumps(freeze(Path(__file__).parent,parser.parse_args().base_model),indent=2))
