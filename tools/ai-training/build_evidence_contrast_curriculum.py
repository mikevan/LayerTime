"""Case-specific contrast lessons. Source definitions and project rules remain distinct.

Each pair changes one evidence field. These are authored decoded-feature fixtures,
not raw captures, packet ground truth, or independent expert judgements.
"""
import hashlib
import json
from pathlib import Path
from collections import Counter
from grounded_contract import canonical
from compact_domain_contract import schema,validate
from build_domain_pilot_v3 import LEGEND


def pair(ident,question,base,path,values,headlines,meanings,limit,next_step,refs,split='train'):
    return dict(id=ident,question=question,base=base,changedPath=path,values=values,headlines=headlines,
        meanings=meanings,limit=limit,nextStep=next_step,sourceRefs=refs,split=split)


LESSONS=[
 pair('client-match','Does this exchange belong to the targeted client and network?',
      dict(request=dict(client='client-a',bssid='radio-a'),exchange=dict(client='client-a',bssid='radio-a')),
      ['exchange','client'],['client-a','client-b'],
      ['The exchange matches the targeted client and network.','The exchange belongs to a different client.'],
      ['Both identity keys match; timing still needs to be checked before declaring a follow-on link.',
       'The network matches, but the client does not, so this is not a same-pair exchange.'],
      'Identity agreement alone does not establish time order, a completed reconnect or hostile intent.',
      'Keep the client and network keys beside each event.', ['principle:pair-means-both-identities']),
 pair('network-match','Do these observations establish a same-network link?',
      dict(request=dict(client='client-a',bssid='radio-a',ssid='Camp'),exchange=dict(client='client-a',bssid='radio-a',ssid='Camp')),
      ['exchange','bssid'],['radio-a','radio-b'],
      ['The client and network-radio identities match.','The familiar name hides a different network radio.'],
      ['The shared client and BSSID satisfy the identity part of the link, with timing still unresolved.',
       'The SSID and client match, but the different BSSID fails this same-radio rule.'],
      'A displayed network name is not enough to merge distinct network-radio observations.',
      'Compare both BSSIDs before combining the sequence.', ['principle:pair-means-both-identities','field:bssid']),
 pair('time-order','Is the exchange a later observation under this follow-on rule?',
      dict(requestTimeMs=1000,exchangeTimeMs=1500,samePair=True),['exchangeTimeMs'],[1500,500],
      ['The same-pair exchange occurred after the request.','The exchange occurred before the request.'],
      ['The measured order is forward with a 500 ms gap; the configured window must also be satisfied.',
       'The exchange is 500 ms earlier, so it cannot be the later event under this rule.'],
      'Event order is not proof that the request caused any later exchange.',
      'Preserve direction and the measured gap in the timeline.', ['principle:order-before-link','principle:correlation-not-cause']),
 pair('missing-time','Can the configured window establish the measured time gap?',
      dict(requestTimeMs=1000,exchangeTimeMs=1500,configuredWindowMs=30000),['exchangeTimeMs'],[1500,None],
      ['The observed gap is 500 ms.','The exchange time is unknown.'],
      ['The two timestamps establish a measured gap independently of the 30-second rule setting.',
       'The 30-second setting supplies no missing timestamp and cannot establish a measured gap.'],
      'A configured rule interval is not a measurement of event duration.',
      'Recover timing before applying the time-bounded rule.', ['principle:configuration-not-measurement']),
 pair('window-boundary','Does the supplied delay meet this explicitly inclusive rule?',
      dict(observedDelayMs=1000,rule=dict(minimumMs=0,maximumMs=1000,upperBoundInclusive=True)),['observedDelayMs'],[1000,1001],
      ['The delay meets the inclusive upper boundary.','The delay exceeds the configured boundary.'],
      ['Exactly 1,000 ms is included by the supplied rule; no extra tolerance has been requested.',
       'A 1,001 ms delay is outside this 1,000 ms rule, even though the difference is small.'],
      'A rule match or near miss is not a verdict about hostile intent.',
      'Display the measured delay and rule boundary separately.', ['principle:preserve-requested-policy']),
 pair('disconnect-outcome','What can we report about the outcome of the disconnect request?',
      dict(requestObserved=True,clientDisconnectObserved=False),['clientDisconnectObserved'],[False,True],
      ['A disconnect request was observed; the outcome is unknown.','Both a request and a client disconnection were observed.'],
      ['The request alone does not show that the addressed client disconnected.',
       'The supplied client-state observation supports reporting a disconnection as well as the request.'],
      'Even an observed disconnection does not by itself establish which event caused it or who controlled the sender.',
      'Keep request evidence and client-state evidence separately cited.', ['principle:target-not-sender','principle:correlation-not-cause']),
 pair('address-role','Which address is the decoded receiver rather than the claimed transmitter?',
      dict(decodedFields={'wlan.ra':'client-a','wlan.ta':'radio-a'}),['decodedFields','wlan.ta'],['radio-a','client-a'],
      ['The receiver is client-a; the claimed transmitter is radio-a.','Both decoded address fields contain client-a.'],
      ['The two fields have different roles, so the target must not be reported as the sender.',
       'Equal address values do not remove the distinction between receiver and transmitter roles.'],
      'Neither decoded address authenticates the physical person or device that transmitted the frame.',
      'Retain the field names with their values.', ['field:receiver','field:transmitter']),
 pair('pmf-capability','Does this advertisement say protection is required?',
      dict(decodedFields={'wlan.rsn.capabilities.mfpc':True,'wlan.rsn.capabilities.mfpr':False}),
      ['decodedFields','wlan.rsn.capabilities.mfpr'],[False,True],
      ['Protection is advertised as capable, not required.','Protection is advertised as both capable and required.'],
      ['The capability flag is set while the requirement flag is clear; those are different decoded claims.',
       'Both flags are set in the supplied advertisement, including the explicit requirement flag.'],
      'An advertisement does not prove session negotiation or acceptance of a particular disconnect request.',
      'Compare advertisement, session context and the particular frame separately.', ['field:pmf-capable','field:pmf-required']),
 pair('relative-origin','Does the negative relative time invalidate this event ordering?',
      dict(timeBasis='milliseconds relative to recording marker; negative values are permitted',requestTimeMs=0,exchangeTimeMs=-100),
      ['exchangeTimeMs'],[-100,100],
      ['The exchange is 100 ms before the marker and request.','The exchange is 100 ms after the marker and request.'],
      ['A negative relative timestamp is permitted by this time basis and establishes an earlier event.',
       'The positive relative timestamp establishes a later event, with any window check still separate.'],
      'Relative time needs its declared origin; its sign alone is not a capture-health verdict.',
      'Preserve the time basis beside the timeline.', ['principle:order-before-link','principle:configuration-not-measurement']),
 pair('entropy-type','Does low address entropy tell us the address-type flags?',
      dict(features=dict(address_entropy=0.0,random_address_fraction=1.0)),['features','random_address_fraction'],[1.0,0.0],
      ['All accepted records carry random-address flags despite low entropy.','No accepted records carry random-address flags in this example.'],
      ['Low entropy describes address concentration; the separate address-type fraction is one.',
       'The address-type fraction is zero here; that conclusion comes from the flag measurement, not entropy.'],
      'The flag does not measure address rotation or establish a physical-device identity.',
      'Report entropy and address type as separate measurements.', ['feature:address_entropy','feature:random_address_fraction']),
 pair('newness-type','Do random-address flags establish that new addresses appeared?',
      dict(features=dict(random_address_fraction=1.0,new_address_fraction=0.0)),['features','new_address_fraction'],[0.0,1.0],
      ['Random-address flags are present, but none of these address records are new to the comparison set.',
       'Random-address flags are present and every record uses an address absent from the comparison set.'],
      ['Address type and address newness measure different properties; a random-format address can have been seen before.',
       'The supplied new-address fraction, rather than the address-type flag, supports the newness observation.'],
      'New address values are not a verified count of newly arrived physical devices.',
      'Retain the previous-address comparison set and observation coverage.', ['feature:new_address_fraction','feature:random_address_fraction']),
 pair('repetition-variation','Can repeated advertising data coexist with variation in its length?',
      dict(features=dict(payload_repeat_fraction=0.9,ad_data_length_std=2.0)),['features','ad_data_length_std'],[2.0,0.0],
      ['Advertising data repeats often, while its length still varies.','Advertising data repeats often and has no measured length variation.'],
      ['High exact-data repetition does not require every accepted data item to have the same length.',
       'Zero length variation does not mean all payload bytes are identical; the repetition statistic is separate.'],
      'Neither measurement alone establishes a benign or hostile purpose.',
      'Keep repetition and length variation separate in the explanation.', ['feature:payload_repeat_fraction','feature:ad_data_length_std']),
 pair('advertisement-gatt','Was a GATT read observed in the supplied evidence?',
      dict(advertisedService=True,decodedGattReadObserved=False),['decodedGattReadObserved'],[False,True],
      ['A service was advertised, but no GATT read was supplied.','The supplied evidence includes a decoded GATT read as well as advertising.'],
      ['An advertisement describes advertised service information; it does not establish a characteristic read.',
       'The separate read observation, rather than the advertisement alone, supports reporting GATT activity.'],
      'A read observation does not by itself establish authorization, hostile intent or a completed application outcome.',
      'Cite the advertisement and connection evidence separately.', ['feature:accepted_kind_contract']),
 pair('activity-drop','Does the measurement support reporting complete radio silence?',
      dict(previousObservedRecords=100,currentObservedRecords=20,recorderHealthKnown=False),['currentObservedRecords'],[20,0],
      ['Observed activity fell, but twenty records remain.','No records were observed in the current interval.'],
      ['The count fell from one hundred to twenty, so calling it zero activity would contradict the observation.',
       'The current observed count is zero, but unresolved recorder health prevents a claim of environmental silence.'],
      'An observation drop does not establish deliberate suppression or predict a physical attack.',
      'Check recorder health and matched observation coverage.', ['principle:selected-absence-not-silence']),
 pair('rate-exposure','Are the observed counts evidence of a higher per-second rate?',
      dict(reference=dict(records=100,seconds=10),current=dict(records=600,seconds=60)),['current','seconds'],[60,10],
      ['The counts differ, but both observed rates are ten records per second.','The current observed rate is six times the reference rate.'],
      ['The current interval is six times longer, which explains its sixfold count without a rate increase.',
       'Six hundred records in ten seconds is sixty per second, compared with ten per second in the reference.'],
      'A measured rate change still needs comparable coverage and context before an abnormality or intent claim.',
      'Show interval duration, count and unit-labelled rate together.', ['feature:log_observed_rate'],split='validation'),
 pair('mic-result','Does the Key MIC flag establish successful verification?',
      dict(keyMicFlag=True,independentMicVerification='not_performed'),['independentMicVerification'],['not_performed','passed'],
      ['The flag is present, but verification was not performed.','A separate supplied verification result reports a pass.'],
      ['A decoded flag is not a cryptographic verification result.',
       'The pass claim comes from the independent verification result, not from the flag alone.'],
      'A verification result alone does not establish a physical sender, authorization or overall network safety.',
      'Preserve the verifier provenance and result separately from decoded flags.', ['field:key-mic-flag'],split='test'),
]


def load_sources(root):
    registry={};files={}
    for folder,name,kind in [('wireshark-field-teaching-v1','cards.jsonl','field'),
                             ('ble-feature-knowledge-v1','definitions.jsonl','feature'),
                             ('evidence-meaning-teaching-v1','principles.jsonl','principle')]:
        path=root/'domain-data'/folder/name; files[str(path.relative_to(root))]=hashlib.sha256(path.read_bytes()).hexdigest()
        for line in path.read_text().splitlines():
            row=json.loads(line);registry[kind+':'+row['id']]=row
    return registry,files


def build(root):
    import copy
    registry,files=load_sources(root);out=root/'domain-data/evidence-contrast-v1';out.mkdir(exist_ok=False)
    records=[];review=['# Case-specific evidence contrast lessons','','Authored synthetic decoded-feature fixtures; not raw captures or independently reviewed labels.','']
    for lesson in LESSONS:
        review+=['## '+lesson['id'],'',lesson['question'],'']
        for index,value in enumerate(lesson['values']):
            evidence=copy.deepcopy(lesson['base']);target=evidence
            for key in lesson['changedPath'][:-1]:target=target[key]
            target[lesson['changedPath'][-1]]=value
            definitions=[]
            for ref in lesson['sourceRefs']:
                source=registry[ref]
                definitions.append(dict(reference=ref,**{k:source[k] for k in ('definition','interpretationLimit','sourceFact','projectInterpretationLimit','sourceFactScope') if k in source}))
            facts=[dict(factId='capture_summary',value=evidence),dict(factId='capture_context',value=dict(definitions=definitions))]
            answer=dict(schemaVersion=1,headline=lesson['headlines'][index],significance=lesson['meanings'][index],
                limitations=lesson['limit'],passiveNextStep=lesson['nextStep'],factReferences=['capture_summary','capture_context'])
            record=dict(id=lesson['id']+'-'+str(index+1),family=lesson['id'],split=lesson['split'],
                sourceId='layertime-authored-evidence-contrasts-v1',datasetVersion='blue-team-compact-evidence-contrast-v1',
                task='EXPLAIN_EVIDENCE_CONTRAST',taskContractVersion='1.0.0',outputSchema=canonical(schema()),
                labelStatus='agent-authored-source-linked-not-independently-reviewed',trainingApproved=False,releaseEligible=False,
                provenance=dict(kind='synthetic-decoded-feature-fixture-not-raw-pcap',sourceRefs=lesson['sourceRefs'],
                    changedPath=lesson['changedPath'],pairedWith=lesson['id']+'-'+str(2-index)),
                messages=[dict(role='system',content=LEGEND),dict(role='user',content=lesson['question']+'\n'+canonical(dict(facts=facts))),
                          dict(role='assistant',content=canonical(answer))])
            validate(record['messages'][-1]['content'],record);records.append(record)
            review += ['### Variant '+str(index+1),'',canonical(evidence),'',answer['headline'],'',answer['significance'],'',answer['limitations'],'']
    (out/'candidates.jsonl').write_text(''.join(canonical(r)+'\n' for r in records),encoding='utf-8')
    (out/'REVIEW.md').write_text('\n'.join(review),encoding='utf-8')
    # Preserve complete supporting cards separately from model input and all notices.
    (out/'source-registry.json').write_text(json.dumps(registry,indent=2),encoding='utf-8')
    manifest=dict(status='case-specific-teaching-candidates-not-admitted',examples=len(records),pairedContexts=len(LESSONS),
        exactDistinctAnswers=len({r['messages'][-1]['content'] for r in records}),splits=dict(Counter(r['split'] for r in records)),
        sourceFiles=files,trainingPerformed=False,trainingApproved=False,releaseEligible=False,
        limitations=['Each pair changes one field; pairs and principles are not independent field environments.',
            'Validation/test families are not train families, but share explicitly supplied concepts and sources.',
            'Supplementary definitions are input context; success could reflect contextual reasoning rather than memorized knowledge.',
            'Source labels support definitions; concrete scenarios and interpretation targets are project-authored.',
            'No new model weights were trained or qualified by this build.'],
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':print(json.dumps(build(Path(__file__).parent),indent=2))
