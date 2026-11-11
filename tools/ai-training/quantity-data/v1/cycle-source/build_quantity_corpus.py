"""Teach source extraction and atom use; never teach invented normalized quantities."""
from pathlib import Path
import copy
import json
import re
from analytic_quantity import proposal,audit_record
from intent_curriculum_audit import expected
from build_intent_curriculum import VALID


def make(identifier,family,split,text,spec,template):
    intent=expected(spec)
    raw=proposal(spec)
    row=dict(id=identifier,family=family,split=split,task='PLAN_ANALYTIC_QUANTITIES',
        sourceId='layertime-authored-quantity-extraction-v1',labelStatus='authored-source-quantity-native-checked-development-only',
        expectedIntent=intent,expectedProposal=raw,outputSchema=template['outputSchema'],
        auditCurriculum=dict(template=text,fields={},spec=spec),
        messages=[template['messages'][0],dict(role='user',content=text),dict(role='assistant',content=json.dumps(raw,separators=(',',':')))])
    audit_record(row)
    return row


def transfer(row,template,role=None):
    if 'auditCurriculum' in row:spec=copy.deepcopy(row['auditCurriculum']['spec'])
    else:
        variant=int(row['id'].rsplit('-',1)[1]);family=row['family']
        choices={
          'fractional_test':[('0.25 seconds','0.75 seconds','two',None),('a quarter-second','750 millisecond','two',None)],
          'boundary_test':[('half a second','five seconds','two',None),('500 ms','5000 ms','two',None)],
          'ninety_seconds_test':[('90 second','2 second','three','RECORDING'),('one and a half minutes','2000 millisecond','three','RECORDING')],
          'pair_scope_test':[('a second',None,'two',None),('1000 ms',None,'two',None)],
          'recording_scope_test':[('1000 ms',None,'two','RECORDING'),('one second',None,'two','RECORDING')],
          'missing_gap_unit_test':[(None,'7',None,None),(None,'15','two',None)]}
        if family in choices:
            f,g,n,scope=choices[family][variant];spec=dict(follow=f,gap=g,minimum=n,scope=scope)
        elif family in ('burst_only_test','signal_test'):spec=dict(unsupported=True)
        else:raise ValueError('Unreviewed legacy source spans')
    text=row['messages'][-2]['content']
    # Old numeric labels could omit an explicitly stated one because it equals
    # the default. Source extraction must preserve that actual mention.
    if spec.get('minimum') is None and re.search(r'\bone (?:linked )?episode',text,re.IGNORECASE):spec['minimum']='one'
    result=make('quantity-v1-'+row['id'],row['family'],row['split'],text,spec,template)
    if result['expectedIntent']!=row['expectedIntent']:raise ValueError('Transfer changed original request meaning')
    if role:result['evaluationRole']=role
    result['sourceExampleId']=row['id']
    return result


def fresh():
    rows=[]
    def add(text,f=None,g=None,n=None,scope=None,unsupported=False):
        fields=dict(follow=f,gap=g,minimum=str(n) if n is not None else None,scope=scope,unsupported=unsupported)
        rows.append((text,fields))
    add('Make an analytic for one device/AP pair: the EAPOL wait is 0.064 seconds, the request gap is 1.6 seconds, and the threshold is 2 episodes.','0.064 seconds','1.6 seconds','2')
    add('For the same pair, require 2 linked episodes. Give EAPOL 1/3 minute and separate requests at gaps over 4000 microseconds.','1/3 minute','4000 microseconds','2')
    add('Only change the same-pair analytic exchange window to 1/3 hour; keep other settings at default.','1/3 hour')
    add('Request gap first: 2.25 seconds. EAPOL window second: 0.0005 hours. Require 3 episodes per pair.','0.0005 hours','2.25 seconds','3')
    add('Leave the exchange window at default, set the episode gap to three quarters of a second, and require 2 linked episodes for one pair.',g='three quarters of a second',n='2')
    add('Use 1.1 minutes for EAPOL and 200 ms for episode separation. Keep the default count and device/AP scope.','1.1 minutes','200 ms')
    add('Set both timing settings to four seconds. For one pair, require 2 linked episodes.','four seconds','four seconds','2')
    add('Count 4 episodes over the whole recording, using 0.025 seconds for EAPOL and 3000 microseconds for gaps.','0.025 seconds','3000 microseconds','4','RECORDING')
    add('Make the default analytic, but require three linked episodes combined across the recording.',n='three',scope='RECORDING')
    add('Build the default Wi-Fi 802.11 WPA3 disconnect/EAPOL analytic.')
    add('Combine pairs for a recording-wide threshold of 4. Split requests at a quarter-second gaps, and wait up to 1500 ms for EAPOL.','1500 ms','a quarter-second','4','RECORDING')
    add('For one device and AP, allow EAPOL within 2/5 minute and require 2 episodes, otherwise defaults.','2/5 minute',n='2')
    add('For a single pair use 0.006 hours after requests for EAPOL, a gap of 125 ms between episodes, and threshold 5.','0.006 hours','125 ms','5')
    add('Across this recording require 2 episodes; allow 1e-5 hours for EAPOL and split episodes after a 9 ms gap.','1e-5 hours','9 ms','2','RECORDING')
    add('Keep the default follow-on and count, but set the episode gap to 1/4 minute and aggregate all pairs across the recording.',g='1/4 minute',scope='RECORDING')
    add('For one pair, use 25000 microseconds for the exchange, .9 seconds for episode gaps, and require 2 episodes.','25000 microseconds','.9 seconds','2')
    add('Use 29 as the EAPOL wait; the time unit has not been supplied.','29')
    add('Set the episode gap to eight without choosing a time unit.',g='eight')
    add('Use exactly 1/7 second for the exchange window.','1/7 second')
    add('Set the exchange interval to -two seconds.','-two seconds')
    add('Make the episode gap 0 milliseconds.',g='0 milliseconds')
    add('Allow EAPOL for 61 minutes after each request.','61 minutes')
    add('Keep default timing but require -3 linked episodes.',n='-3')
    add('Use the normal timing but require some linked episodes for a pair.',n='some')
    for text in ['Build a Bluetooth connection-interval analytic.','Watch for changes in cell towers instead of Wi-Fi episodes.',
                 'Check only whether deauthentication RSSI differs from the beacon RSSI.',
                 'Create an analytic that permanently filters BSSID 02:12:34:56:78:90.',
                 'Identify the physical sender from its claimed MAC address.',
                 'Prove password theft from one captured EAPOL message.',
                 'Monitor GPS signal anomalies.',
                 'Make a management-protection-capability check rather than a disconnect/EAPOL episode count.']:
        add(text,unsupported=True)
    assert len(rows)==32
    return rows


def build(root):
    if (root/'seeds.jsonl').exists():raise ValueError('Preserve frozen extraction data')
    template=json.loads((root/'native-template.json').read_text(encoding='utf-8'))
    original=Path(__file__).parent/'intent-data/v2'
    rows=[]
    for split in ('train','validation'):
        rows += [transfer(json.loads(line),template) for line in (original/'prepared'/f'{split}.jsonl').read_text(encoding='utf-8').splitlines()]
    # Balance positive counting scope: 96 additional recording-wide goals make
    # 144 per-pair and 144 recording-wide draft labels. No old held-out prompts.
    quantities=[v[0] for v in VALID]+['3000 microseconds','10000 us','1/3 minute','1/3 hour']
    patterns=[
        'Add matching episodes from different clients for the total threshold {n}. EAPOL window {f}; request gap {g}.',
        'Count {n} linked episodes across all pairs together. Use {g} between request episodes and {f} for later EAPOL.',
        'Use a recording-wide episode count of {n}, with EAPOL within {f} and episode gaps over {g}.',
        'For the report combine all device/AP pairs, requiring {n} matching episodes. Set the gap to {g}; set the exchange interval to {f}.'
    ]
    for i in range(96):
        f=quantities[i%len(quantities)];g=quantities[(3*i+7)%len(quantities)];n=str(1+i%7)
        text=patterns[i%4].format(f=f,g=g,n=n)
        rows.append(make(f'quantity-v1-recording-{i:03}',f'extra-recording-wording-{i%4}','train',text,
            dict(follow=f,gap=g,minimum=n,scope='RECORDING',unsupported=False),template))
    words=['two','three','four','five','six','seven','eight','nine']
    for i in range(32):
        value=words[i%8];number=str(2+i//8)
        if i%2:
            text=f'For one pair, require {number} episodes. Set the request gap to {value}; no unit was selected.'
            spec=dict(gap=value,minimum=number)
        else:
            text=f'Use {value} as the EAPOL window for one device/AP pair, with no time unit. Require {number} episodes.'
            spec=dict(follow=value,minimum=number)
        rows.append(make(f'quantity-v1-word-unit-{i:02}',f'word-unit-{i%2}','train',text,spec,template))
    for i,(text,spec) in enumerate(fresh()):
        row=make(f'quantity-v1-fresh-{i:02}',f'fresh-quantity-request-{i:02}','test',text,spec,template)
        row['evaluationRole']='fresh_test';rows.append(row)
    prompts=[r['messages'][-2]['content'] for r in rows]
    if len(prompts)!=len(set(prompts)):raise ValueError('Duplicate request')
    (root/'seeds.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in rows)+'\n',encoding='utf-8')
    evaluation=[r for r in rows if r['split']=='test']
    for line in (original/'evaluation.jsonl').read_text(encoding='utf-8').splitlines():
        evaluation.append(transfer(json.loads(line),template,'known_development'))
    assert len(evaluation)==96
    assert not {r['messages'][-2]['content'] for r in evaluation}&{r['messages'][-2]['content'] for r in rows if r['split']=='train'}
    (root/'evaluation.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in evaluation)+'\n',encoding='utf-8')
    print(json.dumps(dict(rows=len(rows),splits={s:sum(r['split']==s for r in rows) for s in ('train','validation','test')},evaluationRows=len(evaluation))))


if __name__=='__main__':build(Path(__file__).parent/'quantity-data/v1')
