"""A separately frozen follow-up for observed coverage gaps; old tests become regressions."""
import copy
import hashlib
import json
from pathlib import Path
from build_quantity_corpus import make
from prepare import prepare
from analytic_quantity import audit_record

SOURCE='layertime-authored-quantity-extraction-v2'


def fresh():
    return [
      ('For WPA2 on 802.11, prepare the standard disconnect-to-EAPOL analytic without changing any settings.',{}),
      ('I want the ordinary per-device/AP disconnect and later EAPOL analytic. Leave all its settings alone.',{}),
      ('Use the standard timing and episode threshold, combining linked episodes from all device/AP pairs.',dict(scope='RECORDING')),
      ('For the WPA3 disconnect/EAPOL analytic, change only the episode threshold to six.',dict(minimum='six')),
      ('Across device/AP pairs use a threshold of seven; leave both timing settings unchanged.',dict(minimum='seven',scope='RECORDING')),
      ('For one pair, threshold 9. EAPOL may follow within 800 ms; request gaps over 0.03 seconds start a new episode.',dict(follow='800 ms',gap='0.03 seconds',minimum='9')),
      ('The recording-wide threshold should be five. Keep EAPOL within 1/5 minute and split request episodes at 0.006 hours.',dict(follow='1/5 minute',gap='0.006 hours',minimum='five',scope='RECORDING')),
      ('Only the follow-on window changes: allow 1/6 hour after requests for EAPOL on the same pair.',dict(follow='1/6 hour')),
      ('Only episode separation changes: use 2/5 minute between request groups for a single device/AP.',dict(gap='2/5 minute')),
      ('Use 1/30 hour for the EAPOL window; keep the episode gap and threshold at their defaults.',dict(follow='1/30 hour')),
      ('For all pairs combined, require 8 episodes with 0.00002 hours for EAPOL and 12000 microseconds for the request gap.',dict(follow='0.00002 hours',gap='12000 microseconds',minimum='8',scope='RECORDING')),
      ('Keep the same-pair count at its default. Request separation is 1/12 minute; EAPOL gets 2/3 minute.',dict(follow='2/3 minute',gap='1/12 minute')),
      ('Use one shared interval of 0.007 minutes for EAPOL and request separation; threshold 7 for one pair.',dict(follow='0.007 minutes',gap='0.007 minutes',minimum='7')),
      ('Make the standard disconnect/EAPOL analytic for Wi-Fi WPA2 with a recording-wide threshold of 6.',dict(minimum='6',scope='RECORDING')),
      ('Leave the timing alone for one device and access point, but change the threshold to eight.',dict(minimum='eight')),
      ('Use 1/24 hour for the follow-on interval and otherwise keep the same-pair defaults.',dict(follow='1/24 hour')),
      ('For one pair use eleven as the exchange window, with no unit specified.',dict(follow='eleven')),
      ('Across the recording require 3 episodes and use 700.5 microseconds for the request gap.',dict(gap='700.5 microseconds',minimum='3',scope='RECORDING')),
      ('For one pair choose either 2 seconds or 9 seconds as the EAPOL window; I have not decided.',dict(follow=['2 seconds','9 seconds'])),
      ('Set a same-pair episode threshold of 3/2 and leave timing unchanged.',dict(minimum='3/2')),
      ('Build an analytic that proves an EAPOL exchange leaked the network password.',dict(unsupported=True)),
      ('I want only a signal-strength warning with no disconnect or EAPOL correlation.',dict(unsupported=True)),
      ('Filter the analytic to a specific device address that I will type later.',dict(unsupported=True)),
      ('Create a workflow that confirms the exact time a completed reconnect happened from deauth and EAPOL alone.',dict(unsupported=True)),
    ]


def build():
    base=Path(__file__).parent;old=base/'quantity-data/v1';out=base/'quantity-data/v2'
    out.mkdir(exist_ok=False);template=json.loads((old/'native-template.json').read_text())
    (out/'native-template.json').write_text(json.dumps(template,indent=2))
    rows=[json.loads(s) for s in (old/'seeds.jsonl').read_text(encoding='utf-8').splitlines() if json.loads(s)['split']!='test']
    numbers=['2','3','4','5','6','7','8','9','two','three','four','five','six','seven','eight','nine']
    fractions=['1/3 hour','2/3 hour','1/6 hour','1/12 hour','1/30 hour','1/60 hour','1/5 hour','3/4 hour',
      '7/8 hour','5/6 hour','1/8 minute','3/5 minute','1/12 minute','1/4 hour','2/5 hour','1/10 minute']
    finishes=['Keep the built-in settings.','Use the standard defaults.','Leave all timing and count settings untouched.','Do not change the timing or threshold.']
    for family in range(16):
      for i in range(16):
        n=numbers[i];q=fractions[i];protocol=['Wi-Fi','802.11','WPA2','WPA3'][i%4];end=finishes[i//4]
        if family==0:text=f'Build the standard {protocol} disconnect/EAPOL analytic for each device/AP pair. {end}';spec={}
        elif family==1:text=f'For {protocol}, I want the usual disconnect followed by EAPOL flow per pair. {end}';spec={}
        elif family==2:text=f'Create the usual {protocol} disconnect/EAPOL analytic, counting across this recording. {end}';spec=dict(scope='RECORDING')
        elif family==3:text=f'Use the ordinary {protocol} disconnect-to-EAPOL recipe and combine all pairs. {end}';spec=dict(scope='RECORDING')
        elif family==4:text=f'For one device/AP set threshold {n}. The EAPOL window is 1/4 minute, and the request gap is 900 ms.';spec=dict(minimum=n,follow='1/4 minute',gap='900 ms')
        elif family==5:text=f'Create the same-pair analytic with a threshold of {n}; both timing intervals remain at default.';spec=dict(minimum=n)
        elif family==6:text=f'Combine device/AP pairs for a recording-wide threshold of {n}. Set the request gap to 500 ms and EAPOL window to 0.02 minutes.';spec=dict(minimum=n,follow='0.02 minutes',gap='500 ms',scope='RECORDING')
        elif family==7:text=f'The threshold across this recording is {n}; use the usual timing for disconnect/EAPOL correlation.';spec=dict(minimum=n,scope='RECORDING')
        elif family==8:text=f'Only change the same-pair follow-on window to {q}. Leave the other settings at default.';spec=dict(follow=q)
        elif family==9:text=f'For a single device/AP, change only request episode separation to {q}. Everything else stays at default.';spec=dict(gap=q)
        elif family==10:text=f'Use {q} as the EAPOL window for the recording-wide analytic, with the default gap and count.';spec=dict(follow=q,scope='RECORDING')
        elif family==11:text=f'Across all pairs use the default EAPOL window and count; change the request gap to {q}.';spec=dict(gap=q,scope='RECORDING')
        elif family==12:text=f'For a single pair, the follow-on window is {n} with no time unit. Keep other defaults.';spec=dict(follow=n)
        elif family==13:text=f'Use {n} for the episode gap across the recording, with its time unit undecided.';spec=dict(gap=n,scope='RECORDING')
        elif family==14:text=f'Build a {protocol} workflow that proves a password was recovered from {n} EAPOL exchanges.';spec=dict(unsupported=True)
        else:text=f'For {protocol}, make a flow for only {n} signal-strength changes without any disconnect/EAPOL correlation.';spec=dict(unsupported=True)
        row=make(f'quantity-v2-extra-{family:02}-{i:02}',f'quantity-v2-extra-wording-{family:02}','train',text,spec,template)
        row['sourceId']=SOURCE;rows.append(row)
    # Validation is disjoint wording, not a copy of the added training requests.
    for i,q in enumerate(fractions):
        scope='RECORDING' if i%2 else 'DEVICE_AP_PAIR'
        text=f'Validation wording: EAPOL may arrive within {q}; leave request separation and threshold at their defaults for '+('all pairs combined.' if i%2 else 'one pair.')
        row=make(f'quantity-v2-validation-{i:02}',f'quantity-v2-validation-wording-{i%2}','validation',text,dict(follow=q,scope=scope),template)
        row['sourceId']=SOURCE;rows.append(row)
    evaluation=[]
    for i,(text,spec) in enumerate(fresh()):
        row=make(f'quantity-v2-fresh-{i:02}',f'quantity-v2-fresh-wording-{i:02}','test',text,spec,template)
        row['sourceId']=SOURCE;row['evaluationRole']='fresh_test';rows.append(row);evaluation.append(row)
    for line in (old/'evaluation.jsonl').read_text(encoding='utf-8').splitlines():
        row=json.loads(line);row['evaluationRole']='known_development';evaluation.append(row)
    prompts={}
    for row in rows:
        prompt=row['messages'][-2]['content']
        if prompt in prompts:raise ValueError('Duplicate training/validation/test request')
        prompts[prompt]=row['split'];audit_record(row)
    assert not {r['messages'][-2]['content'] for r in evaluation}&{r['messages'][-2]['content'] for r in rows if r['split']=='train'}
    seed=out/'seeds.jsonl';seed.write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in rows)+'\n',encoding='utf-8')
    (out/'evaluation.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in evaluation)+'\n',encoding='utf-8')
    (out/'fresh-evaluation.jsonl').write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in evaluation if r['evaluationRole']=='fresh_test')+'\n',encoding='utf-8')
    registry=copy.deepcopy(json.loads((old/'sources.json').read_text()))
    for source in registry['sources'].values():source['approvedSeedSha256']=hashlib.sha256(seed.read_bytes()).hexdigest()
    registry['sources'][SOURCE]=dict(license='Project-authored synthetic coverage-gap curriculum; development only',allowedForTraining=True,approvedSeedSha256=hashlib.sha256(seed.read_bytes()).hexdigest(),validationProfile='analytic-quantity-extraction-v1')
    (out/'sources.json').write_text(json.dumps(registry,indent=2))
    manifest=prepare(seed,out/'sources.json',out/'prepared','Defaults, protocol names, threshold extraction and rational intervals; synthetic development only')
    print(json.dumps({s:v['examples'] for s,v in manifest['splits'].items()}))


if __name__=='__main__':build()
