"""Offline EAPOL header review of the previously pinned external campaign.

No credential operations. Narrow flag categories are not complete handshakes.
"""
import hashlib
import json
import os
from pathlib import Path
from collections import Counter
from wifi_domain_reference import decode

TYPES={0:'EAP packet',1:'Start',2:'Logoff',3:'Key',4:'ASF alert',5:'MKA',
       6:'Generic announcement',7:'Specific announcement',8:'Announcement request'}


def eapol_payload(raw,linktype):
    decoded,_=decode(raw,linktype)
    if not decoded or not decoded['eapol']:return None
    packet=raw[int.from_bytes(raw[2:4],'little'):] if linktype==127 else raw
    fc=int.from_bytes(packet[:2],'little'); header=30 if fc&0x300==0x300 else 24
    if (fc>>4)&8:
        header+=2
        if fc&0x8000:header+=4
    return packet[header+8:]


def summarize_header(payload):
    if len(payload)<4:return dict(headerComplete=False,capturedBytes=len(payload))
    declared=int.from_bytes(payload[2:4],'big');kind=payload[1]
    result=dict(headerComplete=True,version=payload[0],typeCode=kind,typeName=TYPES.get(kind,'Unknown'),
        declaredBodyBytes=declared,capturedDeclaredBodyBytes=min(declared,len(payload)-4),
        capturedBytesAfterHeader=len(payload)-4,trailingBytesBeyondDeclaredBody=max(0,len(payload)-4-declared),
        declaredBodyCaptured=len(payload)>=4+declared)
    # Respect declared bounds: trailing capture/FCS bytes cannot supply a missing key header.
    body=payload[4:4+declared]
    if kind==3 and body:
        result['keyDescriptorType']=body[0]
        if body[0] in (2,254) and len(body)>=3:
            flags=int.from_bytes(body[1:3],'big')
            result['keyFlags']={name:bool(flags&mask) for name,mask in
                [('pairwise',8),('install',64),('ack',128),('mic',256),('secure',512),('error',1024),('request',2048)]}
            if flags&2048:category='key_request'
            elif not flags&8:category='group_key_ack_set' if flags&128 else 'group_key_ack_clear'
            elif flags&128:category='pairwise_install_ack' if flags&64 else 'pairwise_ack_without_install'
            else:category='pairwise_other_or_unresolved'
            result['flagCategory']=category
    return result


def review(root):
    os.environ['XDG_CONFIG_HOME']=str(root/'.scapy-config');os.environ['XDG_CACHE_HOME']=str(root/'.scapy-cache')
    from scapy.utils import RawPcapNgReader
    from scapy.layers.dot11 import RadioTap,Dot11
    from scapy.layers.eap import EAPOL
    import importlib.metadata
    source=root/'external-data/wpa3-hsngccu-deauth-v1';intake=json.loads((source/'intake-manifest.json').read_text())
    out=root/'domain-data/wifi-eapol-type-review-v2';out.mkdir(exist_ok=False)
    original={r['id'] for r in map(json.loads,(root/'domain-data/wifi-reference-v1/observations.jsonl').read_text().splitlines()) if r['kind']=='eapol'}
    rows=[];mismatches=[];total=0
    for item in intake['files']:
        if not item['path'].endswith('.pcap'):continue
        path=source/item['path'];assert hashlib.sha256(path.read_bytes()).hexdigest()==item['sha256']
        with RawPcapNgReader(str(path)) as reader:
            for index,(raw,meta) in enumerate(reader):
                total+=1;payload=eapol_payload(raw,meta.linktype)
                if payload is None:continue
                facts=summarize_header(payload);ident=f"capture-{item['sha256'][:16]}:frame-{index}"
                packet=RadioTap(raw) if meta.linktype==127 else Dot11(raw);independent=packet.getlayer(EAPOL)
                checks=dict(present=independent is not None)
                if facts['headerComplete'] and independent is not None:
                    checks.update(version=int(independent.version)==facts['version'],typeCode=int(independent.type)==facts['typeCode'],
                        declaredBodyBytes=int(independent.len)==facts['declaredBodyBytes'])
                    if 'keyDescriptorType' in facts:checks['keyDescriptorType']=bytes(independent.payload)[0]==facts['keyDescriptorType']
                if not all(checks.values()):mismatches.append(dict(id=ident,failed=[k for k,v in checks.items() if not v]))
                rows.append(dict(id=ident,sourceCaptureSha256=item['sha256'],recordIndex=index,facts=facts,
                    split='external_review',trainingApproved=False))
    assert {r['id'] for r in rows}==original and len(rows)==len(original), 'EAPOL population changed'
    (out/'observations.jsonl').write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows))
    result=dict(status='external-source-header-review-not-training',capturedRecords=total,eapolObservations=len(rows),
        typeCounts=dict(Counter(r['facts'].get('typeName','Incomplete header') for r in rows)),
        descriptorCounts=dict(Counter(str(r['facts'].get('keyDescriptorType','not_applicable_or_missing')) for r in rows)),
        flagCategories=dict(Counter(r['facts'].get('flagCategory','not_classified') for r in rows)),
        incompleteDeclaredBodies=sum(not r['facts'].get('declaredBodyCaptured',False) for r in rows),
        decoderComparison=dict(tool='Scapy',version=importlib.metadata.version('scapy'),checked=len(rows),mismatches=mismatches,
            comparedFields=['EAPOL presence','version','packet type','declared body length','key descriptor type when present'],
            flagCategoriesIndependentlyCompared=False),
        sourceRevision=intake['revision'],trainingApproved=False,modelTrainingPerformed=False,releaseEligible=False,
        limitations=['Header type agreement is not cryptographic, FCS, timestamp or incident verification.',
            'Flag categories are narrow observations, not complete handshake/session identities or attack counts.',
            'Pairwise messages without ACK remain unresolved; variable MIC and key-data interpretations are not guessed.',
            'The 62 chunks belong to one external campaign without an independent benign control.',
            'This is ordinary offline evidence processing, not neural model training.'],
        files={'observations.jsonl':hashlib.sha256((out/'observations.jsonl').read_bytes()).hexdigest()})
    (out/'manifest.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
    if mismatches:raise ValueError('Decoder disagreement; preserve unapproved review')
    return result


if __name__=='__main__':review(Path(__file__).parent)
