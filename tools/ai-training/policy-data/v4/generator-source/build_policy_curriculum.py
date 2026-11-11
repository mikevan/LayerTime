"""Explicit synthetic policy-reporting families with controlled augmentation.

This teaches grounded verbalization of the app's configured warning criteria.
Variants change addresses, identifiers, counts and times; they are not distinct
field environments or independent source concepts. Never label them as attacks.
"""
import argparse
import hashlib
import json
import random
import sys
from pathlib import Path
from evidence_facts import summarize
from evaluate_local_model import validate_assessment
from wireless_curriculum import frame, build as corrective

FAMILIES={
 'train':['same_identity','other_network','other_client','broadcast','occasional','malformed','ordinary_exchange','capture_gap'],
 'validation':['reverse_exchange','invalid_exchange_clock','slow_many_requests','multiple_network_exchanges'],
 'test':['unresolved_exchange_network','two_targets','unknown_reason_single','untrusted_timestamp_burst']}
SYSTEM=('Explain the supplied passive wireless analytic in plain language. Start with the important warning, then the facts. '
        'The supplied disconnectBurstPolicy uses six observed disconnect requests within three seconds. '
        'Report each group that meets that policy, without claiming an attack is proven. '
        'For groups below the threshold, do not invent a disruption warning. '
        'Use the exact observed counts, client and BSSID; qualify timing and coverage limits. '
        'A frame is a request, not proof of delivery or disconnection. EAPOL is an exchange, not proof of a completed reconnect or password compromise. '
        'Addresses identify claimed frame contents, not the physical sender. Cite supporting supplied frame IDs. '
        'Return only the specified JSON assessment. No issues means no policy-triggering concern established by this sample, not guaranteed safety.')


def sample(family,variant,split):
    seed=int.from_bytes(hashlib.sha256(f'{family}:{variant}'.encode()).digest()[:8],'big')
    rng=random.Random(seed)
    def mac():return '02:'+':'.join(f'{rng.randrange(256):02x}' for _ in range(5))
    ap,other,client,second=mac(),mac(),mac(),mac()
    n=rng.randint(6,22)
    if family=='two_targets':n=min(n,16) # Bound combined evidence; never truncate a target's frames.
    interval=rng.randint(25,100);start=1_700_000_000_000+rng.randrange(100000)
    ids=rng.sample(range(0,4096),2*n+5);channel=rng.choice([1,6,11])
    def f(ref,t,kind=12,bssid=ap,target=client,**changes):
        return dict(frame(ref,t,kind,bssid,target),channel=channel,**changes)
    rows=[f(ids[i],start+i*interval) for i in range(n)]
    coverage={}
    if family=='same_identity':rows.append(f(ids[n],start+n*interval+200,32))
    elif family=='other_network':rows.append(f(ids[n],start+n*interval+200,32,bssid=other))
    elif family=='other_client':rows.append(f(ids[n],start+n*interval+200,32,target=second))
    elif family=='broadcast':
        rows=[dict(row,destination='ff:ff:ff:ff:ff:ff') for row in rows]
        rows.append(f(ids[n],start+n*interval+200,32))
    elif family=='occasional':rows=[f(ids[i],start+i*5000) for i in range(rng.randint(1,4))]
    elif family=='malformed':rows=[dict(row,malformed=True) for row in rows]+[f(ids[n],start+n*interval+200,32)]
    elif family=='ordinary_exchange':rows=[f(ids[i],start+i*100,32) for i in range(rng.randint(1,4))]
    elif family=='slow_many_requests':rows=[f(ids[i],start+i*5000) for i in range(n)]
    elif family=='multiple_network_exchanges':
        rows=[f(ids[i],start+i*100,32,bssid=ap if i%2 else other) for i in range(4)]
    elif family=='unknown_reason_single':rows=[f(ids[0],start,reasonCode=65535)]
    elif family=='untrusted_timestamp_burst':rows=[dict(row,timestampValid=False) for row in rows]
    elif family=='capture_gap':
        rows.append(f(ids[n],start+120000,32));coverage=dict(continuousObservation=False,knownGapMs=[start+3000,start+119000])
    elif family=='reverse_exchange':rows.append(f(ids[n],start+n*interval+200,32,source=client,destination=ap))
    elif family=='invalid_exchange_clock':rows.append(f(ids[n],start+n*interval+200,32,timestampValid=False))
    elif family=='unresolved_exchange_network':rows.append(f(ids[n],start+n*interval+200,32,network=None))
    elif family=='two_targets':
        rows.extend(f(ids[n+i],start+i*interval,target=second) for i in range(n))
        rows.append(f(ids[2*n],start+n*interval+200,32,target=second))
    else:raise ValueError('Unknown curriculum family')
    facts=summarize(rows);issues=[]
    by_id={row['frameId']:row for row in rows}
    for group in facts['groups']:
        burst=group['disconnectBurstPolicy']
        if not burst or not burst['thresholdMet']:continue
        count=burst['peakObservedFrames'];seconds=(max(by_id[i]['timeNanos'] for i in burst['frameIds'])-min(by_id[i]['timeNanos'] for i in burst['frameIds']))/1e9
        if group['targetKind']=='broadcast':
            headline='Increase your threat posture: repeated requests asked devices to leave the network'
            explanation=f"{count} disconnect requests were broadcast on network {group['bssid']} within {seconds:.3f} seconds. The capture does not establish how many devices received or acted on them."
        else:
            headline='Increase your threat posture: repeated disconnect requests involved this device'
            explanation=f"Device {group['client']} and network {group['bssid']} appear in {count} disconnect requests within {seconds:.3f} seconds. These requests do not by themselves confirm that the device disconnected."
        refs=list(burst['frameIds'])
        if group['targetKind']=='broadcast':
            explanation+=' A later security exchange for one device is present, but the broadcast requests do not identify one exclusive target or establish how many devices reconnected.'
            refs.append(ids[n])
        elif group['observedEapolFrameCount']:
            refs.extend(group['eapolFrameIds'])
            if not group['allRelevantTimestampsValid']:
                explanation+=' A security exchange for the same device and network is also present, but an invalid timestamp prevents establishing its order.'
            elif group['eapolAfterLastDisconnectWithinWindow']:
                explanation+=' A later security exchange involves that same device and network. It does not prove a completed reconnect, a complete captured handshake, or password loss.'
            else:explanation+=' A security exchange for that same device and network occurs outside the configured 30-second follow-on window; do not describe it as an immediate reconnect.'
        elif family=='other_network':
            explanation+=f' The later security exchange involves the same device on a different network, {other}; it is not a verified reconnect to the targeted network.';refs.append(ids[n])
        elif family=='other_client':
            explanation+=f' The later security exchange involves another device, {second}; it is not evidence that this target reconnected.';refs.append(ids[n])
        elif family=='unresolved_exchange_network':
            explanation+=' A later security-exchange frame has unresolved network identity, so a same-network reconnect cannot be established.';refs.append(ids[n])
        else:explanation+=' No security exchange for this individual target is established by the supplied frames; missing traffic does not prove that no reconnect happened.'
        if coverage:explanation+=' The recording contains a reported gap; continuous observation is not established.'
        explanation+=' The physical sender and intent remain unresolved.'
        issues.append(dict(headline=headline,explanation=explanation,frameIds=list(dict.fromkeys(refs))))
    data=dict(observations=rows,relatedEpisodes=[],coverage=coverage,derivedEvidenceFacts=facts)
    row=dict(id=f'policy-v4-{family}-{variant:03}',family='policy-v4-'+family,split=split,
        sourceId='layertime-policy-synthesis-v4',task='REVIEW_CAPTURE',
        labelStatus='synthetic-policy-derived-not-attack-ground-truth',evidenceIds=list(by_id),
        outputSchema=corrective()[0]['outputSchema'],
        messages=[dict(role='system',content=SYSTEM),dict(role='user',content='RECORDING DATA\n'+json.dumps(data)),
                  dict(role='assistant',content=json.dumps(dict(schemaVersion=1,issues=issues)))])
    validate_assessment(row['messages'][-1]['content'],row)
    return row


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--variants',type=int,default=16);args=parser.parse_args()
    if not 1<=args.variants<=128:raise ValueError('Invalid augmentation count')
    if args.output.exists():raise ValueError('Use a new output directory')
    args.output.mkdir(parents=True)
    sources=args.output/'generator-source';sources.mkdir()
    source_hashes={}
    for name in ['build_policy_curriculum.py','evidence_facts.py','wireless_curriculum.py','evaluate_local_model.py']:
        raw=Path(__file__).with_name(name).read_bytes();(sources/name).write_bytes(raw)
        source_hashes[name]=hashlib.sha256(raw).hexdigest()
    rows=[sample(family,i,split) for split,families in FAMILIES.items() for family in families for i in range(args.variants)]
    path=args.output/'candidates.jsonl';path.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows),encoding='utf-8')
    manifest=dict(schemaVersion=1,originalScenarioFamilies=sum(map(len,FAMILIES.values())),variantsPerFamily=args.variants,
        generatedExamples=len(rows),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),releaseEligible=False,
        trainingApproved=False,purpose='policy-verbalization augmentation, not broad domain training',
        generatorSourceSha256=source_hashes,python=sys.version,
        limitations=['Synthetic variations are not new independent environments.',
            'Targets describe configured warning criteria, not whether an attack occurred.',
            'Generated labels require independent arithmetic and semantic checks before preparation.',
            'Document-backed knowledge examples remain a separate training objective.'])
    (args.output/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(manifest))
