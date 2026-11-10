"""Correct and curate a new review version; does not authorize or perform training."""
from collections import Counter,defaultdict
import copy
import hashlib
import json
from pathlib import Path
import numpy as np
from blue_team_cases import CASES
from blue_team_source_cards import build as source_cards
from build_blue_team_domain import canonical,row,write_rows
from blue_team_contracts import attach,validate

EXTRA_CONCEPTS=[
('disruption_hypothesis','A possible service-disruption interpretation requiring evidence of affected operation as well as observed requests.'),
('credential_exposure_context','Security mode, visible exchange material and credential context needed to assess exposure; observed EAPOL alone does not establish password loss.'),
('unauthorized_ap_hypothesis','A possible unauthorized radio requiring ownership/authorization evidence beyond a familiar network name.'),
('advertising_resource_pressure','A possible resource-impact interpretation that needs observed load or service effects and an appropriate baseline.'),
('interference_hypothesis','A possible radio-interference interpretation, with deliberate and unintentional causes kept unresolved unless corroborated.'),
('recorder_artifact_alternative','A change caused by the observing system rather than the monitored environment.'),
('ordinary_reconnect_alternative','A possible ordinary reconnect explanation evaluated against device/network history and context.'),
('roaming_alternative','A possible change of network radio during ordinary client operation; names alone do not establish continuity.'),
('privacy_rotation_alternative','An address-change interpretation consistent with protocol privacy behavior, without declaring every similar pattern benign.'),
('congestion_alternative','A possible shared-capacity explanation for performance changes that requires path and exposure context.'),
('management_protection_observation','Observed management-frame protection information; a header flag alone is not proof of ownership, validity or success.'),
('security_mode_observation','Observed network security-mode information with its source and version context; never infer it solely from a name.'),
('exchange_material_visibility','Which exchange fields are actually visible and usable for the proposed defensive claim.'),
('review_posture','A declared draft reporting policy that selects the next review action; not a calibrated probability of hostile intent.')]
WIFI_FAMILIES={c[0] for c in CASES if c[1]=='wifi'}


def identity_facts(frames):
    unique={}
    for f in frames:unique.setdefault(f['observationKey'],f)
    groups=defaultdict(lambda:dict(disconnectRequestIds=[],exchangeIds=[]))
    for f in unique.values():
        if f['kind'] not in ('disconnect_request','eapol'):continue
        key=(f['target'],f['network']);groups[key]['disconnectRequestIds' if f['kind']=='disconnect_request' else 'exchangeIds'].append(f['id'])
    return [dict(target=k[0],network=k[1],**v) for k,v in sorted(groups.items())]


def model_view(capture,context,answer):
    """Keep class names, generator controls and fixed role-address shortcuts out of model messages."""
    ids={capture:'capture-'+hashlib.sha256(('visible-capture:'+capture).encode()).hexdigest()[:20]}
    for ref in context['evidenceReferences']:
        ids[ref]='observation-'+hashlib.sha256(('visible-frame:'+ref).encode()).hexdigest()[:20]
    addresses={}
    for event in context['sequenceObservations']:
        ids[event['id']]='observation-'+hashlib.sha256(('visible-frame:'+event['id']).encode()).hexdigest()[:20]
        for field in ('target','network'):
            old=event[field]
            if old!='ff:ff:ff:ff:ff:ff':
                raw=hashlib.sha256((capture+':address:'+old).encode()).digest()[:5]
                addresses[old]='02:'+':'.join(f'{b:02x}' for b in raw)
    mapping={**ids,**addresses}
    if len(set(mapping.values()))!=len(mapping):raise ValueError('Visible identity collision')
    def transform(value):
        if isinstance(value,str):return mapping.get(value,value)
        if isinstance(value,list):return [transform(x) for x in value]
        if isinstance(value,dict):return {mapping.get(k,k):transform(v) for k,v in value.items()}
        return value
    return transform(context),transform(answer),mapping


def posture(facts,context):
    if facts['uniqueDisconnectRequests']>=6:return 'PROMPT_SEQUENCE_REVIEW' if facts['linkedExchanges'] else 'REVIEW_DISCONNECT_PATTERN'
    if context.get('observedActivityDrop') and not context.get('recorderHealthKnown'):return 'CHECK_CAPTURE_HEALTH'
    if context.get('wifiDrop') and context.get('bleDrop'):return 'CHECK_CROSS_RADIO_ALIGNMENT'
    if context.get('unit','specified') is None:return 'ASK_FOR_TIME_UNIT'
    if context.get('toolsDisagree') or 'modelAFlag' in context:return 'RECONCILE_EVIDENCE'
    return 'REVIEW_CONTEXT_AND_BASELINE'


def build(root):
    old=root/'domain-data/v1';out=root/'domain-data/v2';out.mkdir(parents=True,exist_ok=False)
    ont=json.loads((old/'ontology.json').read_text());ont['version']='0.2.0'
    for identifier,definition in EXTRA_CONCEPTS:
        ont['concepts'].append(dict(id=identifier,category='defensive_interpretation',definition=definition,definitionKind='project-authored-scoped-definition'))
        ont['relationships'].append(dict(subject=identifier,predicate='bounded_by',object='claim_strength'))
    (out/'ontology.json').write_text(json.dumps(ont,indent=2));records=[];lineage=[];evidence={};sequences={};maps=[]
    with (old/'synthetic-observations.jsonl').open() as f:
        for line in f:
            capture=json.loads(line);evidence[capture['id']]=identity_facts(capture['frames'])
            unique={}
            for f in capture['frames']:unique.setdefault(f['observationKey'],f)
            sequences[capture['id']]=[{k:f[k] for k in ('id','kind','target','network','timeMs')} for f in unique.values() if f['kind'] in ('disconnect_request','eapol')]
    for line in (old/'candidates.jsonl').read_text(encoding='utf-8').splitlines():
        original=json.loads(line);r=copy.deepcopy(original);r['datasetVersion']='blue-team-domain-v2';r['ontologyVersion']='0.2.0'
        if r['task'] in ('REVIEW_DEFENSIVE_EVIDENCE','PROPOSE_PASSIVE_ANALYTIC_RECIPE'):
            question,raw=r['messages'][1]['content'].split('\n',1);context=json.loads(raw);answer=json.loads(r['messages'][-1]['content'])
            capture=context['captureId'];generation=context['providedContext'];context['facts']['identityGroups']=evidence[capture]
            context['sequenceObservations']=sequences[capture]
            signal=posture(context['facts'],generation)
            if r['task']=='REVIEW_DEFENSIVE_EVIDENCE':
                answer['observedFacts']=copy.deepcopy(context['facts']);answer['reviewPosture']=signal
                answer['postureBasis']='draft-product-review-policy-not-incident-ground-truth'
                if r['family']=='clock_invalid':answer['headline']='The sequence lacks usable timestamps.'
            elif r['family']=='clock_invalid':answer['goal']='Review a sequence whose timestamps cannot support a timed link.'
            context['reportingPolicySignal']=dict(posture=signal,basis='draft-product-review-policy-not-incident-ground-truth')
            # Wi-Fi authoring labels such as exchange='other_client' are not observations.
            if r['family'] in WIFI_FAMILIES:
                clean={}
                if 'networkName' in generation:clean['observedNetworkName']=generation['networkName']
                if 'scope' in generation:clean['requestedCountScope']=generation['scope']
                if 'senderAuthenticated' in generation:clean['senderOwnershipVerified']=generation['senderAuthenticated']
                if generation.get('sameSsid'):
                    clean['observedNetworkNames']=[dict(network=n,name='ExampleNetwork') for n in sorted({e['network'] for e in sequences[capture]})]
                context['providedContext']=clean
            context,answer,mapping=model_view(capture,context,answer)
            r['sourceGenerationContext']=generation  # audit metadata; never included in messages
            maps.append(dict(exampleId=r['id'],sourceCaptureId=capture,sourceToVisible=mapping))
            r['messages'][1]['content']=question+'\n'+canonical(context);r['messages'][-1]['content']=canonical(answer)
            lineage.append(dict(id=r['id'],previousCanonicalSha256=hashlib.sha256(canonical(original).encode()).hexdigest(),
                change='Expose identities/times, remove class/generator/fixed-address shortcuts, and supply a draft application review signal. Original observations are preserved.'))
        records.append(r)
    for i,(identifier,definition) in enumerate(EXTRA_CONCEPTS):
        c=next(c for c in ont['concepts'] if c['id']==identifier);split='test' if i%8==0 else 'validation' if i%8==1 else 'train'
        for j,q in enumerate([f'Explain {identifier} in plain language.',f'What does {identifier} mean in LayerTime?',f'Define {identifier} and preserve its evidence limits.']):
            records.append(row(f'concept-{identifier}-{j}',f'concept-{identifier}',split,'EXPLAIN_DEFENSIVE_CONCEPT',
                'layertime-blue-team-ontology-v2',q,dict(definitionKind=c['definitionKind'],ontologyEntry=c),
                dict(conceptId=identifier,definition=definition,definitionKind=c['definitionKind']),[identifier]))
    cards=source_cards(root/'external-data/nist-reference-library-v1');write_rows(out/'source-backed-cards.jsonl',cards)
    for card in cards:
        if card['sourceId']!='nist-sp800-121r2-upd1':continue
        # Keep this new source document in validation; no random page-level leakage.
        answer=dict(answer=card['sourceFact'],sourceId=card['sourceId'],pdfPage=card['pdfPage'],sourceSha256=card['sourceSha256'],scope=card['scope'])
        context=dict(retrievedSourceFact=card['sourceFact'],sourceDate=card['sourceDate'],**{k:answer[k] for k in ('sourceId','pdfPage','sourceSha256','scope')})
        for j,prefix in enumerate(['','Explain briefly: ','What should a researcher understand here? ']):
            r=row(f"source-{card['id']}-{j}",card['sourceId'],'validation','ANSWER_SOURCED_DEFENSIVE_QUESTION',
                'layertime-nist-bluetooth-paraphrases-v2',prefix+card['question'],context,answer,['provenance','claim_strength'])
            r['sourceReference']={k:card[k] for k in ('sourceId','pdfPage','sourceSha256','pageTextSha256')};records.append(r)
    for r in records:
        r.update(datasetVersion='blue-team-domain-v2',ontologyVersion='0.2.0',trainingApproved=False)
        attach(r);validate(r['messages'][-1]['content'],r)
    write_rows(out/'candidates.jsonl',records);write_rows(out/'changes-from-v1.jsonl',lineage);write_rows(out/'model-visible-id-map.jsonl',maps)
    for split in ('train','validation','test'):write_rows(out/(split+'.jsonl'),[r for r in records if r['split']==split])
    # Proposal only: retain all source examples, cap repetitive BLE tasks and wording variations.
    selection=[];ble=defaultdict(list)
    for r in records:
        if r['split']!='train':continue
        task=r['task'];identifier=r['id']
        if task=='REVIEW_DEFENSIVE_EVIDENCE':
            if int(identifier.rsplit('-',1)[1]) in {round(i*63/23) for i in range(24)}:selection.append(r)
        elif task=='PROPOSE_PASSIVE_ANALYTIC_RECIPE':
            continue  # Deferred until recipe semantics are mapped to actual app tools.
        elif task in ('EXPLAIN_DEFENSIVE_CONCEPT','ANSWER_SOURCED_DEFENSIVE_QUESTION'):
            if identifier.endswith('-0'):selection.append(r)
        elif task=='EXPLAIN_BLE_OBSERVATIONS':ble[(r['family'],r['sourceReference']['featureRow']%4)].append(r)
    feature_names=['payload_repeat_fraction','random_address_fraction','log_observed_rate','unique_address_fraction']
    for (session,topic),items in sorted(ble.items()):
        feature=feature_names[topic]
        items.sort(key=lambda r:(json.loads(r['messages'][1]['content'].split('\n',1)[1])['features'][feature],r['id']))
        selection.extend(items[round(i*(len(items)-1)/9)] for i in range(10))
    if len({r['id'] for r in selection})!=len(selection):raise ValueError('Duplicate selection')
    write_rows(out/'proposed-pilot-selection.jsonl',[dict(id=r['id'],split=r['split'],task=r['task'],canonicalRecordSha256=hashlib.sha256(canonical(r).encode()).hexdigest(),trainingApproved=False) for r in selection])
    manifest=dict(schemaVersion=2,status='agent-reviewed-candidates-owner-domain-review-pending',trainingApproved=False,modelTrainingPerformed=False,
        parentManifestSha256=hashlib.sha256((old/'manifest.json').read_bytes()).hexdigest(),candidateExamples=len(records),
        ontologyConcepts=len(ont['concepts']),ontologyRelationships=len(ont['relationships']),sourceBackedCards=len(cards),
        tasks=dict(Counter(r['task'] for r in records)),splits=dict(Counter(r['split'] for r in records)),
        correctedTaskExamples=len(lineage),proposedPilotExamples=len(selection),proposedPilotTasks=dict(Counter(r['task'] for r in selection)),
        wifiEvidence='Separate wifi-reference-v1 campaign: external_review only; not selected or trained.',
        syntheticEvidence=dict(path='../v1/synthetic-observations.jsonl',sha256=hashlib.sha256((old/'synthetic-observations.jsonl').read_bytes()).hexdigest()),
        limitations=['Agent review is not independent expert validation.','New Bluetooth reference is historically scoped and retains a rights-review requirement.',
        'Proposed selection is not admission or a final training-token budget.','Unvalidated recipes remain non-executable proposals.'],files={})
    for p in out.iterdir():
        if p.is_file():manifest['files'][p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':
    m=build(Path(__file__).parent);print(json.dumps({k:v for k,v in m.items() if k!='files'},indent=2))
