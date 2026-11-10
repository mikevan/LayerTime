"""Post-hoc diagnostic, not a general natural-language truth or quality score.

Only literal integer request counts followed by 'within N seconds' are checked.
An issue must cite exactly one disconnect group, and its claim must count every
validly decoded request supplied for that group. Other claims are left unchecked.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
from evaluate_local_model import validate_assessment

CLAIM = re.compile(r'\b(\d+) disconnect requests\b[^.]*?\bwithin (\d+(?:\.\d+)?) seconds\b', re.I)


def pair(frame):
    if frame.get('malformed') is not False or frame.get('typeSubtype') not in (10, 12): return None
    network = frame.get('network')
    if not network: return None
    if frame.get('source') == network: return network, frame.get('destination')
    if frame.get('destination') == network: return network, frame.get('source')
    return None


def inspect(record, output):
    if record.get('labelStatus') != 'synthetic-policy-derived-not-attack-ground-truth':
        raise ValueError('Only the bounded synthetic policy corpus is supported')
    data = json.loads(record['messages'][1]['content'].split('\n', 1)[1])
    rows = data['observations']; by_id = {r['frameId']: r for r in rows}
    assessment = validate_assessment(output, record)
    results = []
    for index, issue in enumerate(assessment['issues']):
        matches = list(CLAIM.finditer(issue['explanation']))
        entry = dict(issue=index, literalClaims=len(matches), checked=False)
        keys = {pair(by_id[i]) for i in issue['frameIds'] if i in by_id and pair(by_id[i])}
        if len(matches) != 1 or len(keys) != 1:
            entry['reason'] = 'Wording or cited group is not uniquely supported by this diagnostic'
        else:
            match = matches[0]; key = next(iter(keys))
            frames = [r for r in rows if pair(r) == key]
            declared = int(match[1])
            if declared != len(frames):
                entry['reason'] = 'Claim does not count the entire supplied group; subset timing is not inferred'
            elif not all(r.get('timestampValid') is True and type(r.get('timeNanos')) is int for r in frames):
                entry['reason'] = 'Invalid timestamps cannot establish an observed bound'
            else:
                actual = (max(r['timeNanos'] for r in frames)-min(r['timeNanos'] for r in frames))/1e9
                seconds = float(match[2])
                digits = len(match[2].split('.')[1]) if '.' in match[2] else 0
                tolerance = 0.5 * 10 ** -digits
                entry.update(checked=True, bssid=key[0], client=key[1], requestCount=declared,
                    reportedWithinSeconds=seconds, observedSpanSeconds=actual, roundingToleranceSeconds=tolerance,
                    contradictedByObservedSpan=actual > seconds+tolerance+1e-9,
                    supportingFrameIds=[r['frameId'] for r in frames])
        results.append(entry)
    return results


if __name__ == '__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--examples',type=Path,required=True)
    parser.add_argument('--run',type=Path,required=True);args=parser.parse_args()
    records={r['id']:r for r in [json.loads(s) for s in args.examples.read_text(encoding='utf-8').splitlines()]}
    metadata=json.loads((args.run/'report.json').read_text(encoding='utf-8'))
    if metadata['examplesSha256'] != hashlib.sha256(args.examples.read_bytes()).hexdigest():
        raise ValueError('Run and examples differ')
    results=[];seen=set()
    for response in json.loads((args.run/'responses.json').read_text(encoding='utf-8')):
        identity=response['id']
        if identity in seen or identity not in records:raise ValueError('Unknown or repeated prediction')
        seen.add(identity)
        row=dict(id=identity,outputSha256=hashlib.sha256(response['output'].encode()).hexdigest())
        try:row['issues']=inspect(records[identity],response['output'])
        except (ValueError,TypeError,KeyError) as exc:row['validationError']=str(exc)
        results.append(row)
    report=dict(postHocDiagnostic=True,examples=len(results),allExamplesEvaluated=len(seen)==len(records),
        checkedBounds=sum(x['checked'] for r in results for x in r.get('issues',[])),
        contradictedBounds=sum(x.get('contradictedByObservedSpan',False) for r in results for x in r.get('issues',[])),
        releaseEligible=False,results=results,limitations=[
            'Designed after observing a timing error; separate from prespecified outcome measures.',
            'Unmatched wording, subsets, mixed groups and invalid timestamps are not checked.',
            'A larger within-bound can be true; this does not require it to equal the observed span.',
            'Conservative decimal-rounding tolerance is allowed; passing this check is not prose approval.'])
    (args.run/'duration-bound-diagnostic.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k not in ('results','limitations')}))
