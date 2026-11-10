"""Strict schema-v2 agreement and a separate raw-observation audit of synthetic cases."""
import json
from collections import defaultdict

CLAIMS={'target','bssid','disconnectCount','linkedEapolCount','disconnectSpanMs','networkMatch',
        'linkedDelayMinMs','linkedDelayMaxMs','untimedEapolContextCount'}


def unique(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise ValueError('Duplicate JSON key')
        result[key]=value
    return result


def parse(text):return json.loads(text,object_pairs_hook=unique)
def canonical(value):return json.dumps(value,sort_keys=True,separators=(',',':'),allow_nan=False)


def supplied(record):
    matches=[parse(m['content'].split('\n',1)[1]) for m in record['messages'][:-1]
             if m['role']=='user' and m['content'].startswith('VERIFIED FACTS\n')]
    if len(matches)!=1:raise ValueError('Expected one fact payload')
    return matches[0]


def validate(text,record):
    data=supplied(record);answer=parse(text)
    if not isinstance(answer,dict) or set(answer)!={'schemaVersion','captureId','page','assessments'}:
        raise ValueError('Invalid grounded envelope')
    if type(answer['schemaVersion']) is not int or answer['schemaVersion']!=2 or type(answer['page']) is not int:
        raise ValueError('Invalid grounded version/page type')
    if answer['captureId']!=data['captureId'] or answer['page']!=data['page']:
        raise ValueError('Recording/page mismatch')
    expected={f['factId']:f['claims'] for f in data['facts']}
    if len(expected)!=len(data['facts']):raise ValueError('Duplicate supplied fact')
    assessments=answer['assessments']
    if not isinstance(assessments,list) or len(assessments)!=len(expected):raise ValueError('Missing/extra facts')
    seen=set()
    for item in assessments:
        if not isinstance(item,dict) or set(item)!={'factId','claims'}:raise ValueError('Unsupported assessment fields')
        identity=item['factId']
        if not isinstance(identity,str) or identity in seen or identity not in expected:raise ValueError('Unknown/duplicate fact')
        seen.add(identity)
        if canonical(item['claims'])!=canonical(expected[identity]):raise ValueError('Contradictory typed claims')
    return answer


def key(frame):
    net=frame['network'];sa=frame['source'];da=frame['destination']
    def unicast(s):return s is not None and not int(s[:2],16)&1
    target=sa if unicast(net) and da==net and sa!=net and unicast(sa) else \
           da if unicast(net) and sa==net and da!=net and unicast(da) else None
    return net,target,(sa,da) if target is None else None


def audit_record(record):
    if record['task']!='REVIEW_GROUNDED_CAPTURE':raise ValueError('Wrong grounded task')
    if record['labelStatus']!='native-engine-checked-contract-target-not-attack-ground-truth':raise ValueError('Unreviewed grounded labels')
    observations=record['auditObservations'];by_id={f['frameId']:f for f in observations}
    if len(by_id)!=len(observations):raise ValueError('Duplicate raw frame IDs')
    valid=[f for f in observations if not f['malformed'] and f['timestampValid']]
    disconnects=defaultdict(list)
    for f in valid:
        if f['typeSubtype'] in (10,12):disconnects[key(f)].append(f)
    episodes=[];window=record['auditFollowOnMs']*1_000_000
    for pair,requests in disconnects.items():
        requests.sort(key=lambda f:(f['timeNanos'],f['frameId']));groups=[]
        for f in requests:
            if not groups or f['timeNanos']-groups[-1][-1]['timeNanos']>record['auditEpisodeGapMs']*1_000_000:groups.append([])
            groups[-1].append(f)
        for group in groups:
            net,target,_=pair;linked=[];candidates=[]
            for f in valid:
                if not f['eapol']:continue
                comparable=(target in [f['source'],f['destination']]) if target is not None else net is not None and f['network']==net
                if not comparable:continue
                preceding=[d for d in requests if d['timeNanos']<f['timeNanos']]
                latest=max(preceding,key=lambda d:(d['timeNanos'],d['frameId'])) if preceding else None
                follows=latest in group and 0<f['timeNanos']-latest['timeNanos']<=window if latest else False
                earlier=0<=group[0]['timeNanos']-f['timeNanos']<=window
                if follows or earlier:candidates.append(f)
                if follows and target is not None and key(f)==pair:
                    linked.append((f,(f['timeNanos']-latest['timeNanos'])//1_000_000))
            same=any(f['network']==net and net is not None for f in candidates)
            different=any(f['network'] is not None and f['network']!=net for f in candidates)
            match='SAME_DEVICE_AND_ACCESS_POINT' if (linked or same) and net is not None and target is not None else \
                'UNKNOWN' if net is None else 'SAME_ACCESS_POINT_TARGET_UNKNOWN' if target is None and same else \
                'DIFFERENT_ACCESS_POINTS' if target is not None and different else \
                'UNKNOWN' if target is None or any(f['network'] is None for f in candidates) else 'NO_LINKED_EAPOL'
            untimed=[f for f in observations if f['eapol'] and not f['malformed'] and not f['timestampValid'] and
                     target is not None and target in [f['source'],f['destination']]]
            claims=dict(target=target,bssid=net,disconnectCount=len(group),linkedEapolCount=len(linked),
                disconnectSpanMs=(group[-1]['timeNanos']-group[0]['timeNanos'])//1_000_000,networkMatch=match,
                linkedDelayMinMs=min((v for f,v in linked),default=None),linkedDelayMaxMs=max((v for f,v in linked),default=None),
                untimedEapolContextCount=len(untimed))
            refs={f['frameId'] for f in group+candidates+[f for f,v in linked]}
            episodes.append((group[0]['timeNanos'],net or '',target or '',claims,refs))
    episodes.sort(key=lambda e:e[:3]);page_ids=set(record['auditPageIds'])
    expected={f'episode-{i+1}':e[3] for i,e in enumerate(episodes) if e[4]&page_ids}
    data=supplied(record);actual={f['factId']:f['claims'] for f in data['facts']}
    if canonical(actual)!=canonical(expected):raise ValueError('Native supplied facts differ from raw-observation audit')
    for fact in data['facts']:
        if set(fact['claims'])!=CLAIMS:raise ValueError('Claim schema mismatch')
        if any(type(i) is not int or i not in by_id for i in fact['frameIds']):raise ValueError('Unsupplied frame reference')
    validate(record['messages'][-1]['content'],record)
    return len(expected)
