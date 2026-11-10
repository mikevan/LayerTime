"""Correct ambiguous fixture domains/types; preserve the frozen v1 baseline."""
import copy
import hashlib
import json
from pathlib import Path
from build_acceptance_v1 import CASES,SYSTEM
from compact_domain_contract import schema,validate
from grounded_contract import canonical


def build(root):
    old=root/'acceptance/v1';out=root/'acceptance/v2';out.mkdir(exist_ok=False)
    records=[];rubrics=[]
    for index,(name,evidence,headline,criteria) in enumerate(CASES):
        evidence=copy.deepcopy(evidence)
        evidence['domain']='Bluetooth Low Energy' if name.startswith('ble-') else 'Builder configuration' if name=='unit-clarification' else 'radio observation counts' if name=='activity-drop' else 'Wi-Fi'
        if 'requests' in evidence:
            evidence['requests']['kind']='802.11 deauthentication request'
            evidence['requests']['targetRole']='addressed recipient, not a verified sender'
        if 'exchange' in evidence:evidence['exchange']['kind']='observed EAPOL packet'
        if name in ('same-pair','other-network','other-client','earlier-exchange','unknown-times','broadcast'):
            evidence['fieldDefinitions']=dict(network='BSSID alias identifying the observed network radio',client='station identity alias',
                timesMs='observed relative timestamps in milliseconds; null means unknown',
                outcomes='No separate successful disconnection, completed reconnect or credential-recovery evidence supplied')
        ident=f'acceptance-{index+1:02d}'
        answer=dict(schemaVersion=1,headline=headline,significance='Report only the relationship supported by the supplied evidence.',
            limitations='Unknown outcomes and missing context remain unresolved.',passiveNextStep='Review the original evidence and obtain the missing context.',factReferences=['capture_summary'])
        record=dict(id=ident,family=name,split='acceptance',evaluationRole='frozen-product-acceptance',trainingApproved=False,
            datasetVersion='blue-team-compact-acceptance-v2',task='PRODUCT_ACCEPTANCE',taskContractVersion='1.0.0',outputSchema=canonical(schema()),
            evidenceKind='authored-synthetic-evaluation-fixture-not-field-capture',
            messages=[dict(role='system',content=SYSTEM),dict(role='user',content='What is the important finding, its limits and the next passive step?\n'+canonical(dict(facts=[dict(factId='capture_summary',value=evidence)]))),
                dict(role='assistant',content=canonical(answer))])
        validate(record['messages'][-1]['content'],record);records.append(record)
        rubrics.append(dict(id=ident,scenario=name,requiredHeadlineMeaning=headline,criticalCriteria=criteria,
            referenceAnswerScope='Headline meaning guide only; generic other target fields are placeholders, not semantic grading targets.'))
    for name,values in [('examples.jsonl',records),('rubrics.jsonl',rubrics)]:
        (out/name).write_text(''.join(canonical(r)+'\n' for r in values),encoding='utf-8')
    protocol=json.loads((old/'protocol.json').read_text());protocol.update(
        status='corrected-fixtures-frozen-before-v2-baseline',predecessorProtocolSha256=hashlib.sha256((old/'protocol.json').read_bytes()).hexdigest(),
        correction='V1 omitted explicit frame types and radio domains in some inputs; preserve its run but do not use it as a readiness score.',
        independence='Corrected inputs after v1 diagnostic exposure; reusable development acceptance, not a fresh independent benchmark.',
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()},
        generatorSha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        parentGeneratorSha256=hashlib.sha256(Path(__file__).with_name('build_acceptance_v1.py').read_bytes()).hexdigest())
    (out/'protocol.json').write_text(json.dumps(protocol,indent=2));return protocol


if __name__=='__main__':print(json.dumps(build(Path(__file__).parent),indent=2))
