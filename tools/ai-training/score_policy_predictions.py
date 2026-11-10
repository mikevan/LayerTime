"""Score supplied evidence coverage, not the truth of arbitrary generated prose.

Only audited policy-curriculum targets are supported. Empty assistant placeholders
in diagnostic datasets are not gold labels and are explicitly refused.
"""
import argparse
import hashlib
import json
from pathlib import Path
from audit_policy_curriculum import audit_row
from evaluate_local_model import validate_assessment


def disconnect_pairs(record,ids):
    data=json.loads(record['messages'][1]['content'].split('\n',1)[1])
    pairs=set()
    for frame in data['observations']:
        if frame['frameId'] not in ids or frame['malformed'] is not False or frame['timestampValid'] is not True:
            continue
        if frame['typeSubtype'] not in (10,12) or not frame.get('network'):continue
        network=frame['network']
        if frame['source']==network:client=frame['destination']
        elif frame['destination']==network:client=frame['source']
        else:continue
        pairs.add((network,client))
    return pairs


def score(records,responses):
    examples={r['id']:r for r in records}
    if len(examples)!=len(records):raise ValueError('Duplicate example identity')
    for row in records:
        if row.get('labelStatus')!='synthetic-policy-derived-not-attack-ground-truth':
            raise ValueError('Unsupported gold labels; diagnostic placeholders are not targets')
        audit_row(row)
    results=[];seen=set()
    for response in responses:
        identity=response['id']
        if identity not in examples or identity in seen:raise ValueError('Unknown or duplicate prediction identity')
        seen.add(identity);record=examples[identity]
        target=validate_assessment(record['messages'][-1]['content'],record)
        expected=disconnect_pairs(record,{ref for issue in target['issues'] for ref in issue['frameIds']})
        row=dict(id=identity,expectedTriggerGroups=len(expected),schemaAndReferencesValid=False,
                 proseInterpretationReviewed=False,outputSha256=hashlib.sha256(response['output'].encode()).hexdigest())
        try:
            prediction=validate_assessment(response['output'],record)
            refs={ref for issue in prediction['issues'] for ref in issue['frameIds']}
            cited=disconnect_pairs(record,refs)
            covered=expected & cited
            row.update(schemaAndReferencesValid=True,issues=len(prediction['issues']),
                coveredTriggerGroups=len(covered),missingTriggerGroups=len(expected-covered),
                untriggeredCitedGroups=len(cited-expected),nonemptyOnNoTrigger=bool(prediction['issues']) and not bool(expected))
        except (ValueError,TypeError,KeyError) as exc:
            row.update(error=str(exc),coveredTriggerGroups=0,missingTriggerGroups=len(expected))
        results.append(row)
    complete=len(seen)==len(examples)
    return dict(examplesAvailable=len(records),examplesEvaluated=len(results),allExamplesEvaluated=complete,
        schemaAndReferencePasses=sum(r['schemaAndReferencesValid'] for r in results),
        expectedTriggerGroups=sum(r['expectedTriggerGroups'] for r in results),
        coveredTriggerGroups=sum(r['coveredTriggerGroups'] for r in results),
        missingTriggerGroups=sum(r['missingTriggerGroups'] for r in results),
        nonemptyOnNoTriggerCases=sum(bool(r.get('nonemptyOnNoTrigger')) for r in results),
        releaseEligible=False,results=results,limitations=[
            'Citing a triggering group does not prove the generated explanation is correct.',
            'Nonempty output on a non-trigger case requires semantic review; it is not automatically a confirmed false alarm.',
            'Results concern synthetic configured policy criteria, not actual attack labels or independent field performance.',
            'Invalid outputs are unusable and receive zero evidence coverage.',
            'Incomplete prediction files do not establish full-set results.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--examples',type=Path,required=True)
    parser.add_argument('--run',type=Path,required=True);args=parser.parse_args()
    metadata=json.loads((args.run/'report.json').read_text(encoding='utf-8'))
    if metadata.get('examplesSha256')!=hashlib.sha256(args.examples.read_bytes()).hexdigest():
        raise ValueError('Prediction run used different example bytes')
    report=score([json.loads(s) for s in args.examples.read_text(encoding='utf-8').splitlines()],
                 json.loads((args.run/'responses.json').read_text(encoding='utf-8')))
    report['model']=metadata['model'];report['adapterSha256']=metadata.get('adapterSha256')
    (args.run/'policy-coverage-report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('results','limitations')}))
