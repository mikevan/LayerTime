"""Audit the corrected corpus and external packet linkage without asserting prose truth."""
from collections import defaultdict
import hashlib
import json
import re
from pathlib import Path
import numpy as np
from audit_blue_team_domain import expected_facts,read_rows
from blue_team_contracts import validate
from build_blue_team_domain import canonical
from blue_team_cases import CASES


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def audit(root):
    folder=root/'domain-data/v2';m=json.loads((folder/'manifest.json').read_text())
    for name,digest in m['files'].items():
        if sha(folder/name)!=digest:raise ValueError('Changed v2 file: '+name)
    old=root/'domain-data/v1'
    if sha(old/'manifest.json')!=m['parentManifestSha256'] or sha(old/'synthetic-observations.jsonl')!=m['syntheticEvidence']['sha256']:raise ValueError('Changed parent evidence')
    parent=json.loads((old/'manifest.json').read_text());feature_dir=root/'runs/ble-features-v1'
    if sha(feature_dir/'features.npz')!=parent['sourceFeatureMatrixSha256'] or sha(feature_dir/'manifest.json')!=parent['sourceFeatureManifestSha256']:raise ValueError('Changed original BLE features')
    data=np.load(feature_dir/'features.npz',allow_pickle=False);x,y,ble_groups=data['x'],data['y'],data['groups'];feature_names=json.loads((feature_dir/'manifest.json').read_text())['features']
    cards={r['id']:r for r in read_rows(folder/'source-backed-cards.jsonl')};library=root/'external-data/nist-reference-library-v1';pages={}
    for card in cards.values():
        source=card['sourceId']
        if source not in pages:
            if sha(library/(source+'.pdf'))!=card['sourceSha256']:raise ValueError('Changed source document')
            pages[source]=json.loads((library/(source+'.pages.json')).read_text())
        text=pages[source][card['pdfPage']-1]['text']
        normal=lambda t:re.sub(r'\s+',' ',t).lower()
        if sha_bytes(text.encode())!=card['pageTextSha256'] or normal(card['supportAnchor']) not in normal(text):raise ValueError('Source anchor disagreement')
    rawfacts={};raw_sequences={}
    for capture in read_rows(old/'synthetic-observations.jsonl'):
        frames=capture['frames'];facts=expected_facts(frames);by_pair=defaultdict(lambda:dict(disconnectRequestIds=[],exchangeIds=[]));seen=set()
        sequence=[]
        for f in frames:
            if f['observationKey'] in seen:continue
            seen.add(f['observationKey'])
            if f['kind'] in ('disconnect_request','eapol'):sequence.append({k:f[k] for k in ('id','kind','target','network','timeMs')})
            if f['kind']=='disconnect_request':by_pair[(f['target'],f['network'])]['disconnectRequestIds'].append(f['id'])
            elif f['kind']=='eapol':by_pair[(f['target'],f['network'])]['exchangeIds'].append(f['id'])
        facts['identityGroups']=[dict(target=key[0],network=key[1],**v) for key,v in sorted(by_pair.items())];rawfacts[capture['id']]=facts;raw_sequences[capture['id']]=sequence
    mappings={r['exampleId']:r for r in read_rows(folder/'model-visible-id-map.jsonl')}
    records={};groups={};teacher_checks=0;ble_checked=0
    for r in read_rows(folder/'candidates.jsonl'):
        if r['id'] in records:raise ValueError('Duplicate ID')
        records[r['id']]=r
        if r['family'] in groups and groups[r['family']]!=r['split']:raise ValueError('Family leakage')
        groups[r['family']]=r['split'];validate(r['messages'][-1]['content'],r);teacher_checks+=1
        if r['task'] in ('REVIEW_DEFENSIVE_EVIDENCE','PROPOSE_PASSIVE_ANALYTIC_RECIPE'):
            context=json.loads(r['messages'][1]['content'].split('\n',1)[1])
            mapping=mappings[r['id']];inverse={v:k for k,v in mapping['sourceToVisible'].items()}
            def restore(value):
                if isinstance(value,str):return inverse.get(value,value)
                if isinstance(value,list):return [restore(x) for x in value]
                if isinstance(value,dict):return {inverse.get(k,k):restore(v) for k,v in value.items()}
                return value
            original=restore(context);capture=mapping['sourceCaptureId']
            if original['facts']!=rawfacts[capture] or original['sequenceObservations']!=raw_sequences[capture]:raise ValueError('Identity/time disagreement with original observations')
            visible=canonical(r['messages'])
            if capture in visible or r['family'] in visible:raise ValueError('Synthetic scenario identity leaks into messages')
            if any(source_value in visible for source_value in mapping['sourceToVisible']):raise ValueError('Original synthetic identity leaks into model input')
            if r['family'] in {c[0] for c in CASES if c[1]=='wifi'} and set(context['providedContext']) & {'exchange','requestCount','duplicates','broadcast','clockValid','sameSsid'}:raise ValueError('Generator control leaks into model input')
        if r['trainingApproved'] is not False:raise ValueError('Unexpected admission')
        if r['task']=='EXPLAIN_BLE_OBSERVATIONS':
            context=json.loads(r['messages'][1]['content'].split('\n',1)[1]);index=r['sourceReference']['featureRow']
            if context['features']!={name:float(x[index,j]) for j,name in enumerate(feature_names)} or r['family']!=str(ble_groups[index]) or r['publisherSessionLabel']!=int(y[index]):raise ValueError('BLE source value disagreement')
            ble_checked+=1
        elif r['task']=='ANSWER_SOURCED_DEFENSIVE_QUESTION':
            card_id=r['id'][len('source-'):].rsplit('-',1)[0];card=cards[card_id];answer=json.loads(r['messages'][-1]['content'])
            if answer['answer']!=card['sourceFact']:raise ValueError('Sourced teacher differs from reviewed card')
    selection=list(read_rows(folder/'proposed-pilot-selection.jsonl'));selected=set()
    for item in selection:
        r=records[item['id']]
        if item['id'] in selected or r['split']!='train' or sha_bytes(canonical(r).encode())!=item['canonicalRecordSha256']:raise ValueError('Invalid pilot selection')
        selected.add(item['id'])
    wifi=root/'domain-data/wifi-reference-v1';wm=json.loads((wifi/'manifest.json').read_text())
    for name,digest in wm['files'].items():
        if sha(wifi/name)!=digest:raise ValueError('Changed packet reference')
    observations=list(read_rows(wifi/'observations.jsonl'));source_ids={r['id']:r for r in observations};requests=defaultdict(list);expected_links=[]
    for r in observations:
        if r['kind']!='eapol' and r['client'] and r['bssid'] and r['timestampNanos'] is not None:requests[(r['client'],r['bssid'])].append(r)
    # Independent query per exchange, rather than the generator's forward state machine.
    for e in observations:
        if e['kind']!='eapol' or e['timestampNanos'] is None:continue
        matches=[r for r in requests.get((e['client'],e['bssid']),[]) if 0<e['timestampNanos']-r['timestampNanos']<=30000000000]
        if matches:
            r=max(matches,key=lambda x:x['timestampNanos']);expected_links.append((r['id'],e['id'],e['timestampNanos']-r['timestampNanos']))
    saved=list(read_rows(wifi/'linked-observations.jsonl'))
    actual=sorted((r['requestId'],r['exchangeId'],r['delayNanos']) for r in saved)
    if actual!=sorted(expected_links):raise ValueError('Independent source-link disagreement')
    if any(r['requestId'] not in source_ids or r['exchangeId'] not in source_ids for r in saved):raise ValueError('Invented packet reference')
    decoder=json.loads((wifi/'decoder-audit.json').read_text())
    if decoder['mismatches']:raise ValueError('Decoder mismatches')
    result=dict(candidateExamples=teacher_checks,syntheticIdentitySummariesChecked=len(rawfacts),proposedPilotExamples=len(selection),
        independentWifiFrameLinksChecked=len(saved),independentDecoderChecks=decoder['checkedRecords'],wifiAdmission='external_review_only',
        originalBleFeatureRowsChecked=ble_checked,sourceCardsAnchorChecked=len(cards),
        structureAndEvidencePassed=True,independentEnglishReview=False,modelTrainingPerformed=False,trainingApproved=False)
    (folder/'integrity-audit.json').write_text(json.dumps(result,indent=2));return result


def sha_bytes(raw):return hashlib.sha256(raw).hexdigest()


if __name__=='__main__':print(json.dumps(audit(Path(__file__).parent),indent=2))
