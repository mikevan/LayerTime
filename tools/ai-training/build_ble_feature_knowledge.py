"""Source-linked BLE feature meaning and counterexamples, not a new model or attack dataset."""
import hashlib
import json
from pathlib import Path
import numpy as np
from ble_features import FEATURES,summarize

DEFINITIONS=[
('log_observed_rate','Natural log of one plus accepted records per window-second.','This is not a raw packets-per-second value; use a unit-labelled tool measurement.'),
('unique_address_fraction','Distinct accepted address values divided by accepted record count.','A small positive fraction is not zero addresses or a verified count of physical devices.'),
('address_entropy','Shannon entropy of accepted address frequencies divided by log2 of record count, with a one-record fallback of zero.','Address concentration is not the random/public address-type flag or proof of address-generation behavior.'),
('new_address_fraction','Fraction of accepted records whose address is absent from the supplied previous-address set.','Zero newly observed addresses does not mean public addresses or absence of random-address flags.'),
('payload_repeat_fraction','One minus distinct accepted advertising-data hashes divided by accepted record count.','Hash equality is exact data equality, not approximate similarity, intent, constant length, or identity.'),
('random_address_fraction','Fraction of accepted records carrying the decoded random-address-type flag.','This does not measure how often an address changes or verify how an address was generated.'),
('manufacturer_ad_fraction','Fraction of accepted records with a parsed manufacturer-specific AD structure.','This does not authenticate a manufacturer, device, or sender.'),
('adv_ind_fraction','Fraction of accepted records whose legacy advertising PDU kind is 0 (ADV_IND).','The advertised capability is not proof that a connection was established.'),
('adv_nonconn_fraction','Fraction of accepted records whose legacy advertising PDU kind is 2 (ADV_NONCONN_IND).','Nonconnectable advertising is not a record of whether scanning occurred or a full connection history.'),
('scan_response_fraction','Fraction of accepted records whose legacy advertising PDU kind is 4 (SCAN_RSP).','A record fraction is not a count of unique scanners or all devices present.'),
('adv_scan_fraction','Fraction of accepted records whose legacy advertising PDU kind is 6 (ADV_SCAN_IND).','Scannable advertising is not an observed scan request or proof that a scanner was present.'),
('iat_cv','Population standard deviation of adjacent accepted-record time gaps divided by their positive mean; otherwise zero.','Variability alone is not hostility; gaps depend on capture visibility and timestamp validity.'),
('log_mean_iat','Natural log of one plus mean adjacent accepted-record interval in seconds, with an insufficient-interval fallback of zero.','This is not raw milliseconds; zero may reflect insufficient intervals, not instantaneous activity.'),
('log_max_iat','Natural log of one plus maximum adjacent accepted-record interval in seconds, with an insufficient-interval fallback of zero.','It is not a proof of environmental radio silence or a complete capture-duration measurement.'),
('mean_ad_data_length','Mean length in bytes of extracted accepted advertising data.','An average does not establish constant packet or payload size.'),
('ad_data_length_std','Population standard deviation of extracted accepted advertising-data lengths in bytes.','A positive value means lengths vary; do not call them constant.'),
('accepted_kind_contract','This decoder admits legacy advertising PDU kinds 0, 1, 2, 4 and 6.','Scan-request and connection-request traffic is outside these accepted features; do not infer their absence from this vector.')]


def toy(count,addresses,random,kind=0,variable=False,previous=True):
    values=[bytes([2,0,0,0,0,i]) for i in range(addresses)];records=[]
    for i in range(count):
        data=b'x'*(24 if variable and i%2 else 4)
        records.append((i*9.0/max(count-1,1),dict(address=values[i%addresses],kind=kind,random=random,manufacturer=False,data=data)))
    vector,_=summarize(records,10,set(values) if previous else set())
    return {name:float(vector[j]) for j,name in enumerate(FEATURES)}


def build(root):
    out=root/'domain-data/ble-feature-knowledge-v1';out.mkdir(parents=True,exist_ok=False)
    code=root/'ble_features.py';source_sha=hashlib.sha256(code.read_bytes()).hexdigest()
    cards=[dict(id=name,definition=meaning,interpretationLimit=limit,sourceId='layertime-ble-feature-contract-v1',
        sourceCodeSha256=source_sha,sourceFunction='decode_advertisement' if name=='accepted_kind_contract' else 'summarize',
        definitionKind='project-extractor-contract-not-universal-protocol-statistic',trainingApproved=False,independentExpertReview=False)
        for name,meaning,limit in DEFINITIONS]
    cases=[
      ('entropy-vs-type',toy(64,1,True),['address_entropy','random_address_fraction'],'Does low address entropy establish that addresses are public or non-random?',
       'No. Address entropy describes the frequency distribution of address values. The address-type flag is a different measurement; this example has low entropy and random-address flags.'),
      ('newness-vs-type',toy(80,2,True),['new_address_fraction','random_address_fraction'],'Does zero new-address fraction mean no random-address records?',
       'No. Newness compares addresses with the supplied previous-address set. Already observed addresses can still carry random-address flags.'),
      ('positive-not-zero',toy(256,4,False),['unique_address_fraction'],'Does a small positive unique-address fraction mean there were no unique addresses?',
       'No. A positive fraction represents observed distinct address values relative to record count. It is not zero and does not establish a physical-device count.'),
      ('new-public-addresses',toy(64,64,False,previous=False),['new_address_fraction','random_address_fraction'],'Do many newly observed addresses prove use of the random-address format?',
       'No. Address newness and address type are different. Public-format addresses can be new to the comparison set.'),
      ('nonconnectable-not-scan-history',toy(80,1,False,kind=2),['adv_nonconn_fraction','accepted_kind_contract'],'Does nonconnectable advertising establish that no scanning occurred?',
       'No. The PDU type describes advertising capability. These accepted features do not supply the full scan-request or connection history.'),
      ('scannable-not-scan-request',toy(96,1,True,kind=6),['adv_scan_fraction','scan_response_fraction'],'Does scannable advertising prove that a scanner sent a request?',
       'No. ADV_SCAN_IND is an advertising PDU, not an observed scan request. Scanning activity needs its own evidence.'),
      ('logged-values-not-units',toy(120,3,False),['log_observed_rate','log_mean_iat','log_max_iat'],'Can the logged rate and interval values be reported directly as packets per second or milliseconds?',
       'No. These are transformed features. Use separately supplied, unit-labelled tool measurements rather than treating logged values as raw rates or delays.'),
      ('repeat-vs-size-variation',toy(64,1,True,variable=True),['payload_repeat_fraction','mean_ad_data_length','ad_data_length_std'],'Does high repetition establish a constant advertising-data size?',
       'No. Repeated data can include multiple distinct payloads. A positive length standard deviation establishes that the observed lengths vary.')]
    records=[]
    output_schema=dict(type='object',required=['answer','featureReferences'],additionalProperties=False,
        properties=dict(answer=dict(type='string',minLength=1),featureReferences=dict(type='array',minItems=1,uniqueItems=True,items=dict(type='string',enum=[c['id'] for c in cards]))))
    for name,features,refs,question,answer in cases:
        records.append(dict(id=name+'-challenge',family=name,sourceId='layertime-ble-feature-contract-v1',sourceCodeSha256=source_sha,
            sourceKind='synthetic-accepted-record-counterexample-not-raw-pcap',trainingApproved=False,
            task='EXPLAIN_BLE_FEATURE_MEANING',outputSchema=json.dumps(output_schema,sort_keys=True),messages=[dict(role='system',content='Explain the meaning of the supplied BLE feature contract in plain language. Distinguish measurements from interpretations. Do not infer incidents or physical identities. Return only JSON matching: '+json.dumps(output_schema,sort_keys=True)),
            dict(role='user',content=question+'\n'+json.dumps(dict(featureContractVersion='ble-observed-legacy-v1',features=features,definitions=[{k:c[k] for k in ('id','definition','interpretationLimit')} for c in cards if c['id'] in refs]))),
            dict(role='assistant',content=json.dumps(dict(answer=answer,featureReferences=refs)))]))
    confirmations=[
      ('Can low address entropy coexist with random-address-type flags?','Yes. Address concentration and address type are separate measurements; the supplied example demonstrates both together.'),
      ('Can already observed addresses still carry random-address-type flags?','Yes. Zero new-address fraction refers to the previous-address comparison, not the public/random type distinction.'),
      ('Does a positive unique-address fraction establish that distinct address values were observed?','Yes. The fraction is positive, so distinct address values were observed. It does not establish a count of physical devices.'),
      ('Can public-format addresses be new to the comparison set?','Yes. Newness measures the comparison with previous observations, independently of the decoded address type.'),
      ('Does ADV_NONCONN_IND identify a nonconnectable advertising PDU?','Yes. That is the PDU property measured here; it is not a complete history of scans or connections.'),
      ('Does ADV_SCAN_IND identify scannable advertising?','Yes. It identifies the advertising type, while evidence of an actual scanner request is a separate observation.'),
      ('Should logged features be interpreted through their declared transformations and units?','Yes. Use the feature contract and separately supplied unit-labelled measurements rather than reading logged values as raw rates or delays.'),
      ('Does positive advertising-data length standard deviation show that lengths varied?','Yes. The supplied positive standard deviation establishes length variation, even though advertising data also repeats.')]
    for original,(question,answer) in zip(list(records),confirmations):
        updated=json.loads(json.dumps(original));updated['id']=original['family']+'-confirm'
        updated['messages'][1]['content']=question+'\n'+original['messages'][1]['content'].split('\n',1)[1]
        target=json.loads(updated['messages'][-1]['content']);target['answer']=answer;updated['messages'][-1]['content']=json.dumps(target);records.append(updated)
    for name,rows in [('definitions.jsonl',cards),('counterexamples.jsonl',records)]:
        (out/name).write_text('\n'.join(json.dumps(r,sort_keys=True) for r in rows)+'\n')
    (out/'extractor-source.py').write_bytes(code.read_bytes())
    table=['# BLE feature meanings and limits','', 'These describe the versioned project extractor, not incident labels. No model training occurred.','']
    for c in cards:table += ['## '+c['id'],'',c['definition'],'',c['interpretationLimit'],'']
    (out/'FEATURE_MEANINGS.md').write_text('\n'.join(table))
    # Specific counterexample properties are checked independently of the prose targets.
    f=cases[0][1];assert f['address_entropy']==0 and f['random_address_fraction']==1
    f=cases[1][1];assert f['new_address_fraction']==0 and f['random_address_fraction']==1
    f=cases[2][1];assert 0<f['unique_address_fraction']<0.02
    f=cases[3][1];assert f['new_address_fraction']==1 and f['random_address_fraction']==0
    f=cases[4][1];assert f['adv_nonconn_fraction']==1 and f['scan_response_fraction']==0
    f=cases[5][1];assert f['adv_scan_fraction']==1 and f['scan_response_fraction']==0
    f=cases[7][1];assert f['payload_repeat_fraction']>0.9 and f['ad_data_length_std']>0
    manifest=dict(sourceCodeSha256=source_sha,definitionCards=len(cards),contrastScenarios=len(cases),pairedTeachingQuestions=len(records),
        trainingApproved=False,modelTrainingPerformed=False,independentExpertReview=False,
        note='Supplement prompted by diagnostic interpretation failures. Not added to the frozen v2 corpus or evaluated as fresh data.',files={})
    for p in out.iterdir():manifest['files'][p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':
    m=build(Path(__file__).parent);print(json.dumps({k:v for k,v in m.items() if k!='files'},indent=2))
