"""Report repetition honestly; this is not a measure of semantic diversity."""
import json
import hashlib
from pathlib import Path
from collections import Counter
from audit_domain_pilot_v3 import rows


def audit(path):
    records=rows(path); tasks={}
    for task in sorted({r['task'] for r in records}):
        selected=[r for r in records if r['task']==task]
        counts=Counter(r['messages'][-1]['content'] for r in selected)
        tasks[task]=dict(items=len(selected),exactDistinctAnswers=len(counts),
            families=len({r['family'] for r in selected}),largestExactAnswerReuse=max(counts.values()))
    return dict(inputSha256=hashlib.sha256(path.read_bytes()).hexdigest(),items=len(records),tasks=tasks,
        exactDistinctAnswers=len({r['messages'][-1]['content'] for r in records}),
        limitations=['Exact string uniqueness is not semantic diversity or correctness.',
            'Different measurement windows can share answers; that does not create new expert knowledge.',
            'The real BLE descriptions use only three training sessions from one testbed.',
            'This curriculum is suitable only as a proposed interface/interpretation pilot, not as the substantial domain training program requested.'],
        substantialDomainTrainingReady=False,trainingPerformed=False)


if __name__=='__main__':
    path=Path(__file__).parent/'domain-data/pilot-v3-curriculum/proposed-train.jsonl'
    result=audit(path);(path.parent/'density-audit.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
