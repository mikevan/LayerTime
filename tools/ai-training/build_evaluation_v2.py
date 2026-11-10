"""Freeze a new synthetic diagnostic set before evaluating the development adapter.

These cases were authored in the same project, not independently collected.
They must never be fed into the current training pilot or represented as a field
benchmark. Expected facts are outside the model's supplied messages.
"""
import hashlib
import json
from pathlib import Path
from wireless_curriculum import frame, AP, OTHER, CLIENT, SECOND


def build():
    schema=json.loads(__import__('wireless_curriculum').build()[0]['outputSchema'])
    system=('Review this passive wireless recording. Lead with the most important finding in plain language, then explain the supporting frames and limits. '
            'Report significant repeated disconnect-request patterns without declaring attacker intent or identity established. '
            'Compare the actual client and BSSID, timestamps and capture coverage. A displayed sender address is not authenticated ownership. '
            'EAPOL alone does not establish a completed reconnect, a complete handshake or password loss. '
            'Return the specified JSON with supporting frame IDs. Do not invent unseen outcomes. An empty issue list is appropriate when this sample establishes no reportable concern.')
    cases=[]
    def add(name,observations,facts,coverage=None):
        cases.append(dict(name=name,observations=observations,expectedFacts=facts,coverage=coverage or {}))
    add('sparse_ids_reverse_exchange',
        [frame(i,100*n) for n,i in enumerate([2,11,35,51,70,94,105])]+[
            dict(frame(120,900,32),source=CLIENT,destination=AP)],
        dict(warningExpected=True,disconnectFrames=7,eapolFrames=1,clientMatch=True,bssidMatch=True,
             order='exchange-after-requests',mustNotClaim=['120 disconnects','completed reconnect','physical sender proven']))
    add('two_targets_only_one_exchange',
        [frame(i,100*i) for i in range(6)]+[frame(20+i,110*i,client=SECOND) for i in range(6)]+[frame(40,900,32,client=SECOND)],
        dict(warningExpected=True,perClientDisconnectCounts={CLIENT:6,SECOND:6},exchangeClient=SECOND,
             mustNotClaim=['first client reconnected','password stolen']))
    add('broadcast_then_client_exchange',
        [frame(i,80*i,client='ff:ff:ff:ff:ff:ff') for i in range(10)]+[frame(21,1000,32)],
        dict(warningExpected=True,disconnectFrames=10,target='broadcast',laterExchangeClient=CLIENT,
             mustNotClaim=['ten devices disconnected','all devices reconnected','broadcast equals one target']))
    add('malformed_burst_valid_normal_exchange',
        [dict(frame(i,50*i),malformed=True) for i in range(9)]+[frame(30,800,32),frame(31,900,32)],
        dict(warningExpected=False,malformedFrames=9,usableDisconnectFrames=0,eapolFrames=2,
             mustNotClaim=['nine valid deauthentication frames','confirmed attack']))
    add('same_client_network_unknown',
        [frame(i,60*i) for i in range(8)]+[dict(frame(18,900,32),network=None)],
        dict(warningExpected=True,disconnectFrames=8,laterExchangeBssid='unresolved',
             mustNotClaim=['verified same network','confirmed reconnect']))
    add('valid_requests_invalid_exchange_clock',
        [frame(i,70*i) for i in range(7)]+[dict(frame(22,1000,32),timestampValid=False)],
        dict(warningExpected=True,disconnectFrames=7,clientMatch=True,bssidMatch=True,order='unresolved',
             mustNotClaim=['exchange followed requests with verified timing']))
    add('ordinary_exchange_two_access_points',
        [frame(3,0,32),frame(7,100,32),frame(18,200,32,OTHER),frame(24,300,32,OTHER)],
        dict(warningExpected=False,disconnectFrames=0,eapolFrames=4,bssids=2,
             mustNotClaim=['evil twin established','deauth attack observed','network guaranteed safe']))
    add('capture_gap_between_events',
        [frame(i,90*i) for i in range(6)]+[frame(40,120000,32)],
        dict(warningExpected=True,disconnectFrames=6,elapsedAfterLastRequestMs=119550,
             mustNotClaim=['immediate reconnect','continuous observation']),
        dict(knownGapMs=[1000,119000],continuousObservation=False))
    rows=[]
    for case in cases:
        data=dict(observations=case['observations'],relatedEpisodes=[],coverage=case['coverage'])
        rows.append(dict(id='diagnostic-v2-'+case['name'],family='diagnostic-v2-'+case['name'],
            sourceId='layertime-authored-evaluation-v2',task='REVIEW_CAPTURE',split='test',
            labelStatus='authored-expectations-not-independent-field-labels',
            evidenceIds=[r['frameId'] for r in case['observations']],expectedFacts=case['expectedFacts'],
            outputSchema=json.dumps(schema),
            messages=[dict(role='system',content=system),dict(role='user',content='RECORDING DATA\n'+json.dumps(data)),
                dict(role='assistant',content='{"schemaVersion":1,"issues":[]}')]))
    return rows


if __name__=='__main__':
    out=Path(__file__).parent/'evaluation-data/v2'
    if out.exists():raise SystemExit('Evaluation set already frozen; use a new version for changes')
    out.mkdir(parents=True)
    raw=''.join(json.dumps(row,sort_keys=True)+'\n' for row in build())
    (out/'test.jsonl').write_text(raw,encoding='utf-8')
    actual_sha=hashlib.sha256((out/'test.jsonl').read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(dict(examples=8,sha256=actual_sha,
        evaluationOnly=True,releaseEligible=False,limitations=[
            'Synthetic project-authored diagnostic cases, not independent-environment evidence.',
            'Expected facts require interpretation review, not keyword-only grading.',
            'Assistant placeholders are not gold completions and must never be used for training.',
            'Frozen before the multitask adapter was evaluated on these cases.']),indent=2),encoding='utf-8')
    print(json.dumps(dict(examples=8,sha256=actual_sha)))
