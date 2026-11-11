"""Independent arithmetic audit of generated policy-verbalization labels.

Uses raw observations and a brute-force sliding-window check, not evidence_facts
or the curriculum generator. This checks arithmetic and evidence coverage, not
human readability, field truth or broad semantic correctness.
"""
import argparse
import collections
import hashlib
import json
import re
from pathlib import Path
from evaluate_local_model import validate_assessment


def audit_row(row):
    data=json.loads(row['messages'][1]['content'].split('\n',1)[1])
    by_id={f['frameId']:f for f in data['observations']}
    if len(by_id)!=len(data['observations']):raise ValueError('Duplicate frame IDs')
    groups=collections.defaultdict(list)
    for f in data['observations']:
        if f['malformed'] is not False or f['typeSubtype'] not in (10,12):continue
        network=f['network']
        if not network:continue
        if f['source']==network:client=f['destination']
        elif f['destination']==network:client=f['source']
        else:continue
        groups[(network,client)].append(f)
    expected={}
    for key,frames in groups.items():
        if not all(f['timestampValid'] is True for f in frames):continue
        peak=[]
        for first in frames:
            window=[f for f in frames if 0<=f['timeNanos']-first['timeNanos']<=3_000_000_000]
            if len(window)>len(peak):peak=window
        if len(peak)>=6:expected[key]=peak
    target=validate_assessment(row['messages'][-1]['content'],row)
    derived=data.get('derivedEvidenceFacts',{}).get('groups',[])
    signaled={(g['bssid'],g['client']):g['disconnectBurstPolicy'] for g in derived if g.get('disconnectBurstPolicy') and g['disconnectBurstPolicy']['thresholdMet']}
    if set(signaled)!=set(expected):raise ValueError('Derived findings differ from raw observations')
    for key,peak in expected.items():
        signal=signaled[key]
        if signal['windowMs']!=3000 or signal['minimumFrames']!=6 or signal['peakObservedFrames']!=len(peak):
            raise ValueError('Derived policy or peak differs from raw observations')
    represented=set()
    for issue in target['issues']:
        refs=[by_id[i] for i in issue['frameIds']]
        disconnects=[f for f in refs if not f['malformed'] and f['typeSubtype'] in (10,12)]
        keys={(f['network'],f['destination'] if f['source']==f['network'] else f['source']) for f in disconnects}
        if len(keys)!=1:raise ValueError('Issue mixes disconnect targets')
        key=next(iter(keys))
        if key in represented or key not in expected:raise ValueError('Duplicate or untriggered warning')
        represented.add(key)
        if len(disconnects)!=len(expected[key]):raise ValueError('Incorrect peak evidence count')
        match=re.search(r'(\d+) disconnect requests (?:were broadcast on network .*? within|within) ([0-9.]+) seconds',issue['explanation'])
        if not match or int(match[1])!=len(disconnects):raise ValueError('Incorrect narrative count')
        actual=(max(f['timeNanos'] for f in disconnects)-min(f['timeNanos'] for f in disconnects))/1e9
        if abs(float(match[2])-actual)>0.00051:raise ValueError('Incorrect narrative duration')
    if represented!=set(expected):raise ValueError('A triggered group is missing')
    return dict(issues=len(target['issues']),triggeredGroups=len(expected))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    rows=[json.loads(s) for s in args.input.read_text(encoding='utf-8').splitlines()]
    results=[dict(id=r['id'],**audit_row(r)) for r in rows]
    report=dict(examples=len(rows),sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
                checked='raw-observation burst arithmetic, narrative counts/durations and target/reference coverage',
                independentExpertReview=False,releaseEligible=False,results=results)
    args.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='results'}))
