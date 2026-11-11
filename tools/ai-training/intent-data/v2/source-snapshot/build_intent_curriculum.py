"""Expanded, reproducible authoring curriculum. Parameter variants are not new users."""
from fractions import Fraction
from pathlib import Path
import hashlib
import json
from analytic_intent import check_shape
from intent_curriculum_audit import audit_record


def q(surface,value=None,scale=None):return (surface,Fraction(value) if value is not None else None,scale)
POOL=[q('a quarter-second','1/4',1000),q('half a second','1/2',1000),q('three quarters of a second','3/4',1000),
      q('0.125 seconds','1/8',1000),q('1.25 seconds','5/4',1000),q('2.5 s','5/2',1000),q('750 ms',750,1),
      q('one and a half minutes','3/2',60000),q('a quarter minute','1/4',60000),q('0.05 minutes','1/20',60000),
      q('1/2 seconds','1/2',1000),q('500 microseconds',500,Fraction(1,1000)),q('1500 microseconds',1500,Fraction(1,1000)),
      q('0 milliseconds',0,1),q('3600001 ms',3600001,1),q('61 minutes',61,60000),q('1 ms',1,1),
      q('one hour',1,3600000),q('30 seconds',30,1000),q('3 seconds',3,1000),q('0.002 hours','1/500',3600000),
      q('0.5 milliseconds','1/2',1),q('1.5 milliseconds','3/2',1),q('-2 seconds',-2,1000)]
VALID=[item for item in POOL if item[1] is not None and (item[1]*item[2]).denominator==1 and 1<=item[1]*item[2]<=3600000]
INVALID=[item for item in POOL if item not in VALID]


def target(follow=None,gap=None,minimum=None,scope=None,unsupported=False):
    if unsupported:return dict(schemaVersion=1,status='UNSUPPORTED',reason='NO_SUPPORTED_ATOM')
    numbers=[]
    for item,default in [(follow,30000),(gap,3000)]:
        if item is None:numbers.append(default);continue
        items=item if isinstance(item,list) else [item]
        vals=[v[1]*v[2] if v[1] is not None and v[2] is not None else None for v in items]
        if None in vals or len(set(vals))!=1 or vals[0].denominator!=1 or not 1<=vals[0]<=3600000:
            return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
        numbers.append(int(vals[0]))
    try:
        n=Fraction(str(minimum)) if minimum is not None else Fraction(1)
        if n.denominator!=1 or not 1<=n<=1000000:raise ValueError()
    except (ValueError,ZeroDivisionError):return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
    scope=scope or 'DEVICE_AP_PAIR'
    if scope not in ('DEVICE_AP_PAIR','RECORDING'):return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
    return dict(schemaVersion=1,status='DRAFT',followOnMs=numbers[0],episodeGapMs=numbers[1],minLinkedEpisodes=int(n),scope=scope)


TRAIN_DRAFT=[
 'For one device and AP, require {n} linked disconnect episodes. Wait up to {f} for EAPOL; split request groups after gaps exceeding {g}.',
 'Use an episode gap of {g} and an EAPOL follow-on window of {f}. Require {n} episodes for a single device/AP pair.',
 'Across this recording, count {n} episodes in total with subsequent same-pair EAPOL within {f}. Use {g} to separate episodes.',
 'Do not combine different device/AP pairs for the threshold of {n} episodes. Follow-on: {f}. Episode gap: {g}.',
 'Build the disconnect/security-message analytic: exchange window {f}; request gap {g}; minimum {n} episodes for one pair.',
 'Start a new episode when requests are more than {g} apart. Find later EAPOL within {f}; report {n} linked episodes for the same pair.',
 'For the same device and AP, look for EAPOL within {f} after a request. Keep the episode gap and count at defaults.',
 'For the same device/AP sequence analytic, change only episode separation to {g}. Keep the other defaults.',
 'Set both the follow-on interval and episode gap to {f}. Require {n} linked episodes for one device and AP.',
 'For the entire capture combined, require {n} linked episodes. Separate episodes using {g}; use {f} to wait for EAPOL.',
 'Use the default episode gap. Require {n} matching episodes for one pair, allowing security messages for up to {f} after each request.',
 'Keep the follow-on interval at its default. Set the request gap to {g} and require {n} linked episodes across the recording.'
]
TRAIN_CLARIFY=[
 'Build a same-pair disconnect/EAPOL analytic. Set the follow-on window to {bare}; otherwise defaults.',
 'Use {g} to split episodes and {bare} for the EAPOL window. The latter has no specified unit.',
 'Require {n} linked episodes for one pair. Wait up to {bare} for EAPOL, with no time unit given.',
 'Allow {f} for EAPOL, but set the episode gap to {bare}.',
 'For the disconnect-sequence analytic, separate request episodes after {bare}; the unit is unspecified.',
 'Require {n} episodes for one pair, with an episode gap of {bare} and default follow-on timing.',
 'Set the EAPOL window to exactly {f} and exactly {g} simultaneously. Keep other settings default.',
 'Create the same-pair analytic with an explicit EAPOL window of {f}. Do not round or replace this value.',
 'Use default timing for the same-pair analytic, requiring exactly {badn} linked episodes.'
]
TOPICS=['Bluetooth advertising floods','cellular tower changes','RSSI differences against beacons','GPS interference',
        'battery drain','DNS changes','completed password theft','proof of physical transmitter identity',
        'a completed reconnect from one EAPOL','a BLE connection map','a standalone disconnect-rate burst',
        'future physical attacks','a fixed saved client address','a fixed saved AP address','USB device changes','camera motion']
TRAIN_UNSUPPORTED=['Build a reusable analytic for {topic}.','Create a flow that detects {topic}, rather than disconnect/EAPOL episodes.','I want an analytic specifically for {topic}.']
VAL_DRAFT=[
 'Please count {n} same-pair linked episodes, using {g} between episodes and {f} as the maximum later-exchange interval.',
 'Combine all pairs for a recording-wide count of {n}. EAPOL must occur within {f}; requests start a new episode after a gap over {g}.',
 'For any one client/AP pair, use {f} for the security-message window, {g} for the episode gap, and threshold {n}.'
]
VAL_CLARIFY=['I need the disconnect/EAPOL flow with an exchange window of {bare}, unit undecided.','Use an explicit episode gap of {f} in the same-pair analytic; do not approximate it.']
VAL_UNSUPPORTED=['Can the Builder make a dedicated analytic for {topic}?']


def rows_for_templates(template,split,kind,index,count):
    rows=[]
    for i in range(count):
        f=VALID[(i+index)%len(VALID)];g=VALID[(i*3+index+5)%len(VALID)];n=str(1+(i+index)%7)
        bare=str(7+i*11+index);badn=['0','-2','1.5','1000001','several','unknown','0.25','-5','-7','2.5','1000032','0.75','many','a few','-11','1000100'][i%16]
        scope='RECORDING' if (split=='train' and kind=='draft' and index in (2,9,11)) or (split=='validation' and kind=='draft' and index==1) else 'DEVICE_AP_PAIR'
        minimum=n;follow=f;gap=g;unsupported=False
        if kind=='draft' and split=='train':
            if index==6:gap=None;minimum=None
            if index==7:follow=None;minimum=None
            if index==8:gap=f
            if index==10:gap=None
            if index==11:follow=None
        if kind=='clarify':
            if split=='validation':
                if index==0:follow=q(bare);gap=None;minimum=None
                else:follow=None;gap=INVALID[i%len(INVALID)];f=gap;minimum=None
            elif index<3:follow=q(bare);gap=g if index==1 else None;minimum=n if index==2 else None
            elif index<6:gap=q(bare);follow=f if index==3 else None;minimum=n if index==5 else None
            elif index==6:
                offset=1
                while f[1]*f[2]==g[1]*g[2]:
                    g=VALID[(i+index+offset)%len(VALID)];offset+=1
                follow=[f,g];gap=None;minimum=None
            elif index==7:
                choices=[q(f'{i+1}.5 milliseconds',Fraction(i+1)+Fraction(1,2),1),q(f'{3600001+i} ms',3600001+i,1),
                         q(f'{61+i} minutes',61+i,60000),q(f'-{i+1} seconds',-(i+1),1000)]
                follow=q('0 milliseconds',0,1) if i==0 else choices[i%4]
                f=follow;gap=None;minimum=None
            else:follow=None;gap=None;minimum=badn
        if kind=='unsupported':follow=None;gap=None;minimum=None;scope=None;unsupported=True
        fields=dict(f=f[0],g=g[0],n=n,bare=bare,badn=badn,topic=TOPICS[(i+index)%len(TOPICS)])
        rows.append((f'{split}-{kind}-{index:02}-{i:02}',f'{split}-{kind}-wording-{index:02}',split,template,fields,follow,gap,minimum,scope,unsupported,kind))
    return rows


def fresh_test():
    rows=[]
    def add(text,f=None,g=None,n=None,scope=None,unsupported=False,category='conversion'):
        i=len(rows)
        fields=dict(f=f[0] if f else '',g=g[0] if g else '',n=str(n) if n is not None else '')
        rows.append((f'test-{i:02}',f'fresh-request-{i:02}','test',text,fields,f,g,str(n) if n is not None else None,scope,unsupported,category))
    # 24 new supported requests. Values/wordings were fixed before any prediction.
    add('For one device/AP pair, I need {n} linked episodes. Give the security exchange {f}, and use {g} to separate request episodes.',q('0.375 seconds','3/8',1000),q('1250 milliseconds',1250,1),3)
    add('Wait no more than {f} for EAPOL after a disconnect request. Otherwise use the defaults.',q('three quarters of a second','3/4',1000))
    add('Split episodes after gaps over {g}; only then count {n} linked episodes for one pair. The exchange window is {f}.',q('0.2 minutes','1/5',60000),q('0.004 hours','1/250',3600000),4)
    add('For a recording-wide threshold of {n}, set the follow-on window to {f} and the episode gap to {g}.',q('2.25 seconds','9/4',1000),q('0.025 minutes','1/40',60000),2,'RECORDING')
    add('Set both timing intervals to {f} and require {n} linked episodes for a single pair.',q('1.75 seconds','7/4',1000),q('1.75 seconds','7/4',1000),2)
    add('Only change the request-episode gap to {g}; keep the exchange window and episode count at defaults.',g=q('a quarter minute','1/4',60000))
    add('Only change the EAPOL wait to {f}. Keep the default gap and require {n} episodes for the same pair.',q('0.007 hours','7/1000',3600000),n=3)
    add('Across all pairs combined, require {n} episodes. Keep default exchange timing but split requests after {g}.',g=q('375 ms',375,1),n=4,scope='RECORDING')
    add('Use {f} for the exchange, not for the request gap. Keep the default gap and count {n} linked episodes for one pair.',q('1/4 seconds','1/4',1000),n=2)
    add('Use {g} as the gap between request episodes, not the exchange window. Leave the exchange window at default.',g=q('2.5 minutes','5/2',60000))
    add('Keep devices separate for the threshold of {n}; allow exchanges for {f} and split episodes after {g}.',q('0.8 seconds','4/5',1000),q('0.6 seconds','3/5',1000),5)
    add('Combine clients only for the total count of {n}; each linked episode still needs the same client and AP. Use {f} for follow-on and {g} for gaps.',q('4.5 seconds','9/2',1000),q('1.125 seconds','9/8',1000),3,'RECORDING')
    add('Use a maximum later-security-message delay of {f} for one pair, leaving all other settings at defaults.',q('two and a half minutes','5/2',60000))
    add('For the same client and AP, require {n} episodes. The gap is {g}; the exchange wait is {f}.',q('0.001 hours','1/1000',3600000),q('1/2 seconds','1/2',1000),2)
    add('Use default time windows and count {n} linked episodes overall across this capture.',n=6,scope='RECORDING')
    add('Use default time windows, but do not add different device/AP pairs together for the threshold of {n}.',n=6)
    add('Allow {f} for each later EAPOL exchange; set the request gap to {g}; require {n} episodes for one pair.',q('3000 microseconds',3000,Fraction(1,1000)),q('2 ms',2,1),2)
    add('For {n} linked episodes across the recording, use the largest supported exchange window, explicitly {f}, with gap {g}.',q('60 minutes',60,60000),q('0.5 seconds','1/2',1000),2,'RECORDING')
    add('Do not use the default exchange time: use {f}. Leave the gap at default and require one linked episode per pair.',q('a quarter-second','1/4',1000))
    add('Use default exchange timing. Replace the episode gap with {g} and require {n} episodes for the same pair.',g=q('half a second','1/2',1000),n=3)
    add('Use {f} for both intervals. Count {n} linked episodes across all pairs together.',q('0.75 seconds','3/4',1000),q('0.75 seconds','3/4',1000),3,'RECORDING')
    add('Threshold {n} per pair; request gap {g}; EAPOL window {f}.',q('0.015 minutes','3/200',60000),q('325 milliseconds',325,1),4)
    add('Make the default disconnect/EAPOL sequence analytic for any one device/AP pair.')
    add('Keep the normal time defaults but require {n} episodes across the whole recording.',n=2,scope='RECORDING')
    # 16 clear reasons to ask instead of guessing.
    for text,f,g,n in [
      ('Use {f} as the exchange window; I have not chosen its time unit.',q('23'),None,None),
      ('The request-episode gap should be {g}. Keep the other defaults.',None,q('41'),None),
      ('Wait {f} for EAPOL, with default episode separation.',q('9'),None,None),
      ('Use {g} between request episodes, and an exchange window of {f}.',q('2 seconds',2,1000),q('19'),None),
      ('Set follow-on to {f} and the episode gap to {g}; the first value has no unit.',q('27'),q('250 ms',250,1),None),
      ('Require {n} episodes for one pair; use {g} as the gap without assuming its unit.',None,q('8'),3),
      ('Both interval values are {f}; no units are specified.',q('13'),q('13'),None),
      ('Use {f} for the security exchange and do not guess a unit from the default.',q('0.7'),None,None),
      ('Use an explicit exchange window of {f}; preserve precision.',q('2500 microseconds',2500,Fraction(1,1000)),None,None),
      ('Set the episode gap to exactly {g}, without rounding.',None,q('1.25 milliseconds','5/4',1),None),
      ('Make the exchange window exactly {f}.',q('0 ms',0,1),None,None),
      ('Set the episode gap to {g} in the same-pair analytic.',None,q('1.1 hours','11/10',3600000),None),
      ('Keep default timing, but require exactly {n} linked episodes.',None,None,'2.5'),
      ('Use the usual time settings and require {n} linked episodes for one pair.',None,None,'several'),
      ('Use {f} for the EAPOL window and require {n} linked episodes.',q('500 ms',500,1),None,'1000001'),
      ('Use an exchange interval of {f} for one device/AP pair.',q('-0.5 seconds','-1/2',1000),None,None),
    ]:add(text,f,g,n,category='clarification')
    for text in ['Make an analytic for BLE service UUID changes.','Watch for cellular signal loss, with no Wi-Fi episode analysis.',
                 'Use only the standalone deauthentication rate, not subsequent EAPOL.',
                 'Compare the radio signal strength of requests and beacons, rather than linked episodes.',
                 'Create a permanent client-address filter for 02:11:22:33:44:55.',
                 'Confirm a password was compromised using only the EAPOL frames.',
                 'Count GPS interference events instead of Wi-Fi messages.',
                 'Prove a completed reconnect from just one security-exchange frame.']:
        add(text,unsupported=True,category='unsupported')
    assert len(rows)==48
    return rows


def build(root):
    if (root/'seeds.jsonl').exists():raise ValueError('Preserve frozen curriculum')
    template=json.loads((root/'native-template.json').read_text(encoding='utf-8'))
    definitions=[]
    for split,groups,count in [('train',[('draft',TRAIN_DRAFT),('clarify',TRAIN_CLARIFY),('unsupported',TRAIN_UNSUPPORTED)],16),
                               ('validation',[('draft',VAL_DRAFT),('clarify',VAL_CLARIFY),('unsupported',VAL_UNSUPPORTED)],8)]:
        for kind,templates in groups:
            for index,text in enumerate(templates):definitions+=rows_for_templates(text,split,kind,index,count)
    definitions+=fresh_test()
    rows=[];prompts=set()
    for identifier,family,split,text,fields,f,g,n,scope,unsupported,category in definitions:
        prompt=text.format(**fields)
        if prompt in prompts:raise ValueError('Duplicate request: '+prompt)
        prompts.add(prompt)
        def surface(item):return [v[0] for v in item] if isinstance(item,list) else item[0] if item else None
        expected=target(f,g,n,scope,unsupported);check_shape(json.dumps(expected))
        row=dict(id='intent-v2-'+identifier,family=family,split=split,task='PLAN_ANALYTIC',category=category,
            sourceId='layertime-authored-intents-v2',labelStatus='authored-intent-native-compiler-checked-development-only',
            expectedIntent=expected,outputSchema=template['outputSchema'],
            auditCurriculum=dict(template=text,fields=fields,spec=dict(follow=surface(f),gap=surface(g),minimum=n,scope=scope,unsupported=unsupported)),
            messages=[template['messages'][0],dict(role='user',content=prompt),dict(role='assistant',content=json.dumps(expected,separators=(',',':')))])
        audit_record(row);rows.append(row)
    (root/'seeds.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in rows)+'\n',encoding='utf-8')
    print(json.dumps(dict(rows=len(rows),splits={s:sum(r['split']==s for r in rows) for s in ('train','validation','test')},
        decisions={s:{d:sum(r['split']==s and r['expectedIntent']['status']==d for r in rows) for d in ('DRAFT','CLARIFY','UNSUPPORTED')} for s in ('train','validation','test')})))


if __name__=='__main__':build(Path(__file__).parent/'intent-data/v2')
