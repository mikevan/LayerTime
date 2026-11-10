"""Replay compact structure/reference checks; substantive review remains separate."""
import argparse
from collections import defaultdict
import hashlib
import json
import statistics
from pathlib import Path
from compact_domain_contract import validate
from audit_domain_pilot_v3 import rows


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def review(examples,run):
    expected=rows(examples);byid={r['id']:r for r in expected}
    responses=json.loads((run/'responses.json').read_text());report=json.loads((run/'report.json').read_text())
    assert report['examplesSha256']==digest(examples)
    assert len(responses)==len(byid)==len(expected) and {r['id'] for r in responses}==set(byid)
    for name,value in report['experimentSourceSha256'].items():assert digest(run/'experiment-source'/name)==value
    assert digest(run/'experiment-source/compact_domain_contract.py')==digest(Path(__file__).with_name('compact_domain_contract.py'))
    task_groups=defaultdict(list);role_groups=defaultdict(list)
    for response in responses:
        try:validate(response['output'],byid[response['id']]);passed=True
        except (ValueError,KeyError,TypeError):passed=False
        assert passed==response['schemaAndReferencesValid']==bool(response.get('compactStructureAndReferencesValid'))
        task_groups[byid[response['id']]['task']].append(response)
        role_groups[byid[response['id']]['evaluationRole']].append(response)
    def summarize(group):
        return dict(examples=len(group),structureAndReferencesPasses=sum(r['schemaAndReferencesValid'] for r in group),
            medianCompleteAnswerSeconds=statistics.median(r['seconds'] for r in group),
            medianOutputTokens=statistics.median(r['outputTokens'] for r in group),
            tokenLimitHits=sum(r['outputTokens']>=report['maxOutputTokens'] for r in group))
    return dict(**summarize(responses),byTask={k:summarize(v) for k,v in task_groups.items()},
        byRole={k:summarize(v) for k,v in role_groups.items()},responsesSha256=digest(run/'responses.json'),
        examplesSha256=digest(examples),loadSeconds=report['loadSeconds'],peakGpuAllocatedBytes=report['peakGpuAllocatedBytes'],
        proseAccuracyMeasured=False,independentExpertReview=False,trainingPerformed=False,releaseEligible=False,
        limitations=['Previously exposed authored diagnostics; not a fresh field qualification benchmark.',
                    'Desktop complete-generation time is not Android or first-token time.',
                    'Structure/reference validation does not verify the meaning or support for free prose.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--examples',type=Path,required=True);parser.add_argument('--run',type=Path,required=True)
    args=parser.parse_args();out=args.run/'replayed-review.json'
    if out.exists():raise ValueError('Preserve the existing review')
    result=review(args.examples,args.run);out.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
