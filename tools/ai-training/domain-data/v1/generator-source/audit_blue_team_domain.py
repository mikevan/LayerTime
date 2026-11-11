"""Independent integrity/arithmetic checks. Not an oracle for the English interpretation labels."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def read_rows(path):
    with path.open(encoding='utf-8') as f:
        for line in f:
            if line.strip():yield json.loads(line)


def expected_facts(frames):
    keyed={};dups=0
    for f in frames:
        key=f['observationKey']
        if key in keyed:
            if {k:v for k,v in keyed[key].items() if k!='id'}!={k:v for k,v in f.items() if k!='id'}:raise ValueError('Conflicting duplicate key')
            dups+=1
        else:keyed[key]=f
    requests=[f for f in keyed.values() if f['kind']=='disconnect_request'];exchanges=[f for f in keyed.values() if f['kind']=='eapol']
    groups={}
    for r in requests:
        if r['target']!='ff:ff:ff:ff:ff:ff' and r['timeMs'] is not None:groups.setdefault((r['target'],r['network']),[]).append(r)
    links=[]
    for e in exchanges:
        if e['timeMs'] is None:continue
        preceding=sorted((r for r in groups.get((e['target'],e['network']),[]) if r['timeMs']<e['timeMs']),key=lambda r:r['timeMs'])
        if preceding and e['timeMs']-preceding[-1]['timeMs']<=30000:
            links.append(dict(requestId=preceding[-1]['id'],exchangeId=e['id'],delayMs=e['timeMs']-preceding[-1]['timeMs']))
    return dict(capturedRecords=len(frames),duplicateRecords=dups,uniqueDisconnectRequests=len(requests),
        observedExchanges=len(exchanges),linkedExchanges=len(links),links=links,followOnWindowMs=30000)


def audit(folder,root):
    m=json.loads((folder/'manifest.json').read_text());assert m['trainingApproved'] is False and m['modelTrainingPerformed'] is False
    for name,meta in m['files'].items():
        p=(folder/name).resolve()
        if not p.is_relative_to(folder.resolve()) or digest(p)!=meta['sha256']:raise ValueError('Changed corpus file: '+name)
    ont=json.loads((folder/'ontology.json').read_text());concepts={c['id'] for c in ont['concepts']}
    if len(concepts)!=len(ont['concepts']):raise ValueError('Duplicate concepts')
    for rel in ont['relationships']:
        if rel['subject'] not in concepts or rel['object'] not in concepts:raise ValueError('Unknown ontology relationship endpoint')
    evidence={};identities={};raw_count=0
    for capture in read_rows(folder/'synthetic-observations.jsonl'):
        frames=capture['frames'];raw_count+=len(frames)
        if len({f['id'] for f in frames})!=len(frames):raise ValueError('Duplicate frame identity')
        evidence[capture['id']]=expected_facts(frames);identities[capture['id']]={f['id'] for f in frames}
    feature_dir=root/'runs/ble-features-v1'
    if digest(feature_dir/'features.npz')!=m['sourceFeatureMatrixSha256'] or digest(feature_dir/'manifest.json')!=m['sourceFeatureManifestSha256']:raise ValueError('Source features/provenance changed')
    data=np.load(feature_dir/'features.npz',allow_pickle=False)
    x,y,groups=data['x'],data['y'],data['groups']
    source=json.loads((feature_dir/'manifest.json').read_text());features=source['features'];sessions={s['sessionId']:s for s in source['sessions']}
    ids=set();families={};counts={};ble_rows=set();split_ids={s:[] for s in ('train','validation','test')}
    for r in read_rows(folder/'candidates.jsonl'):
        if r['id'] in ids:raise ValueError('Duplicate candidate')
        ids.add(r['id']);split=r['split'];split_ids[split].append(r['id'])
        if r['family'] in families and families[r['family']]!=split:raise ValueError('Family leaks across splits')
        families[r['family']]=split
        if r['trainingApproved'] is not False or not set(r['ontologyReferences'])<=concepts:raise ValueError('Unapproved admission or unknown concept')
        user=json.loads(r['messages'][1]['content'].split('\n',1)[1]);answer=json.loads(r['messages'][-1]['content'])
        task=r['task'];counts[task]=counts.get(task,0)+1
        if task=='REVIEW_DEFENSIVE_EVIDENCE':
            key=user['captureId'];expected=evidence[key]
            if user['facts']!=expected or answer['observedFacts']!=expected:raise ValueError('Facts differ from raw observations')
            if not set(answer['evidenceReferences'])<=identities[key]:raise ValueError('Invented frame reference')
        elif task=='EXPLAIN_BLE_OBSERVATIONS':
            ref=r['sourceReference'];i=ref['featureRow'];session=ref['sessionId'];ble_rows.add(i)
            if session!=str(groups[i]) or ref['captureSha256']!=sessions[session]['sha256']:raise ValueError('Wrong BLE provenance')
            if r['publisherSessionLabel']!=int(y[i]) or split!=m['bleSessionSplits'][session]:raise ValueError('Wrong session label/split')
            if user['features']!={k:float(x[i,j]) for j,k in enumerate(features)}:raise ValueError('Changed BLE features')
            if answer['incidentVerdict']!='NOT_ESTABLISHED' or answer['evidenceReference']!=ref:raise ValueError('Unsupported BLE verdict/reference')
            if 'publisherSessionLabel' in r['messages'][1]['content']:raise ValueError('Publisher label leaked into model input')
        elif task=='PROPOSE_PASSIVE_ANALYTIC_RECIPE':
            allowed={'assess_visibility','filter','group_by','order_by_time','temporal_join','aggregate','summarize','compare_baseline','explain'}
            if answer['executable'] is not False or not set(answer['operators'])<=allowed:raise ValueError('Unsupported executable/action recipe')
        elif task=='ANSWER_SOURCED_DEFENSIVE_QUESTION':
            if answer['answer']!=user['retrievedSourceFact'] or any(answer[k]!=user[k] for k in ('sourceId','pdfPage','sourceSha256','scope')):raise ValueError('Source answer/reference lacks supplied grounding')
        elif task=='EXPLAIN_DEFENSIVE_CONCEPT':
            if answer['definition']!=user['ontologyEntry']['definition']:raise ValueError('Definition differs from declared ontology entry')
    for split,expected in split_ids.items():
        actual=[r['id'] for r in read_rows(folder/(split+'.jsonl'))]
        if actual!=expected:raise ValueError('Prepared split differs from candidates')
    if len(ids)!=m['candidateExamples'] or len(ble_rows)!=m['realBleWindows'] or raw_count!=m['syntheticObservations']:raise ValueError('Count mismatch')
    return dict(candidateExamples=len(ids),tasks=counts,syntheticCapturesChecked=len(evidence),typedObservationsChecked=raw_count,
        realBleWindowsChecked=len(ble_rows),familySplitsChecked=len(families),arithmeticAndProvenancePassed=True,
        independentEnglishLabelReview=False,trainingApproved=False,modelTrainingPerformed=False)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--folder',type=Path,default=Path(__file__).parent/'domain-data/v1');a=p.parse_args()
    result=audit(a.folder,Path(__file__).parent);(a.folder/'integrity-audit.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
