"""Offline source observations and bounded independent decoder checks; no radio operations."""
from collections import Counter,defaultdict
from fractions import Fraction
import hashlib
import json
import os
from pathlib import Path


def decode(raw,linktype):
    if linktype==127:
        if len(raw)<8 or raw[0]!=0:return None,'invalid_radiotap'
        offset=int.from_bytes(raw[2:4],'little')
        if not 8<=offset<=len(raw):return None,'invalid_radiotap_length'
        packet=raw[offset:]
    elif linktype==105:packet=raw
    else:return None,'unsupported_linktype'
    if len(packet)<2:return None,'short_frame_control'
    fc=int.from_bytes(packet[:2],'little');kind=(fc>>2)&3;subtype=(fc>>4)&15
    if fc&3 or kind not in (0,2):return None,'control_or_unsupported_version'
    if len(packet)<24:return None,'short_address_header'
    addr=lambda b:':'.join(f'{v:02x}' for v in b)
    a1,a2,a3=[addr(packet[a:a+6]) for a in (4,10,16)];to_ds=bool(fc&0x100);from_ds=bool(fc&0x200)
    source,destination,bssid=a2,a1,a3;header=24
    if kind==2:
        if to_ds and from_ds:
            if len(packet)<30:return None,'short_wds_header'
            source,destination,bssid=addr(packet[24:30]),a3,None;header=30
        elif to_ds:source,destination,bssid=a2,a3,a1
        elif from_ds:source,destination,bssid=a3,a1,a2
        if subtype&8:
            if len(packet)<header+2:return None,'short_qos_header'
            aggregate=bool(packet[header]&0x80);header+=2
            if fc&0x8000:header+=4
        else:aggregate=False
    else:aggregate=False
    protected=bool(fc&0x4000)
    eapol=kind==2 and not protected and not aggregate and packet[header:header+8]==bytes.fromhex('aaaa03000000888e')
    client=None
    if bssid is not None:
        if source==bssid and destination!='ff:ff:ff:ff:ff:ff':client=destination
        elif destination==bssid:client=source
    return dict(type=kind,subtype=subtype,receiver=a1,transmitter=a2,source=source,destination=destination,bssid=bssid,
        client=client,protected=protected,eapol=eapol,disconnect=kind==0 and subtype in (10,12)),None


def alias(address):
    return None if address is None else 'addr-'+hashlib.sha256(('wpa3-source-alias-v1:'+address).encode()).hexdigest()[:16]


def extract(root,out):
    from scapy.utils import RawPcapNgReader
    from scapy.layers.dot11 import RadioTap,Dot11
    from scapy.layers.eap import EAPOL
    source=root/'external-data/wpa3-hsngccu-deauth-v1';manifest=json.loads((source/'intake-manifest.json').read_text())
    out.mkdir(parents=True,exist_ok=False);records=[];file_summaries=[];audit_count=0;mismatches=[]
    for file in manifest['files']:
        if not file['path'].endswith('.pcap'):continue
        path=source/file['path'];raw=path.read_bytes()
        if hashlib.sha256(raw).hexdigest()!=file['sha256']:raise ValueError('Source capture changed')
        file_id='capture-'+file['sha256'][:16];counts=Counter();reasons=Counter();times=[]
        with RawPcapNgReader(str(path)) as reader:
            for index,(raw,meta) in enumerate(reader):
                counts['capturedRecords']+=1;p,reason=decode(raw,meta.linktype)
                if p is None:reasons[reason]+=1;continue
                counts['decodedManagementOrData']+=1;counts[f"type_{p['type']}_subtype_{p['subtype']}"]+=1
                # Compare every relevant sequence frame plus the first 128 decodable frames per file.
                if counts['decodedManagementOrData']<=128 or p['disconnect'] or p['eapol']:
                    independent=RadioTap(raw).getlayer(Dot11) if meta.linktype==127 else Dot11(raw)
                    checks=dict(type=int(independent.type)==p['type'],subtype=int(independent.subtype)==p['subtype'],
                        receiver=independent.addr1==p['receiver'],transmitter=independent.addr2==p['transmitter'],
                        eapol=bool(independent.haslayer(EAPOL))==p['eapol'])
                    audit_count+=1
                    if not all(checks.values()):mismatches.append(dict(fileId=file_id,recordIndex=index,fields=[k for k,v in checks.items() if not v]))
                stamp=Fraction((meta.tshigh<<32)+meta.tslow,meta.tsresol)*1000000000
                nanos=int(stamp) if stamp.denominator==1 else None
                if nanos is not None:times.append(nanos)
                if p['disconnect'] or p['eapol']:
                    counts['disconnectRequests' if p['disconnect'] else 'eapolObservations']+=1
                    records.append(dict(id=f'{file_id}:frame-{index}',fileId=file_id,recordIndex=index,timestampNanos=nanos,
                        kind='deauthentication' if p['disconnect'] and p['subtype']==12 else 'disassociation' if p['disconnect'] else 'eapol',
                        source=alias(p['source']),destination=alias(p['destination']),bssid=alias(p['bssid']),client=alias(p['client']),
                        protected=p['protected'],sourceCaptureSha256=file['sha256']))
        file_summaries.append(dict(fileId=file_id,sourceCaptureSha256=file['sha256'],originalSourcePath=file['path'],counts=dict(counts),
            excludedOrUnsupported=dict(reasons),firstTimestampNanos=min(times) if times else None,lastTimestampNanos=max(times) if times else None))
    # Entire source campaign is external-review only. No split at adjacent chunk boundaries.
    by_pair=defaultdict(list)
    for r in records:
        if r['client'] and r['bssid'] and r['timestampNanos'] is not None:by_pair[(r['client'],r['bssid'])].append(r)
    links=[]
    for pair,items in by_pair.items():
        previous=None
        for r in sorted(items,key=lambda x:(x['timestampNanos'],0 if x['kind']=='eapol' else 1)):
            if r['kind']!='eapol':previous=r
            elif previous:
                delay=r['timestampNanos']-previous['timestampNanos']
                if 0<delay<=30000000000:links.append(dict(requestId=previous['id'],exchangeId=r['id'],client=pair[0],bssid=pair[1],delayNanos=delay))
    def write(name,values):
        (out/name).write_text('\n'.join(json.dumps(v,sort_keys=True) for v in values)+'\n',encoding='utf-8')
    write('observations.jsonl',records);write('capture-summaries.jsonl',file_summaries);write('linked-observations.jsonl',links)
    audit=dict(independentDecoder='Scapy',checkedRecords=audit_count,mismatches=mismatches,
        comparedFields=['type','subtype','receiver','transmitter','EAPOL presence'],
        limitations=['Not FCS validation, timestamp/exposure verification, protected-payload decryption, Hunter parity, or independent incident labelling.'])
    (out/'decoder-audit.json').write_text(json.dumps(audit,indent=2))
    (out/'source-intake.json').write_text(json.dumps(manifest,indent=2));(out/'SOURCE-LICENSE.txt').write_bytes((source/'LICENSE').read_bytes())
    report=dict(schemaVersion=1,captureChunks=len(file_summaries),capturedRecords=sum(f['counts']['capturedRecords'] for f in file_summaries),
        disconnectObservations=sum(r['kind']!='eapol' for r in records),eapolObservations=sum(r['kind']=='eapol' for r in records),
        samePairFollowOnLinks=len(links),grouping='one-publisher-source-campaign',split='external_review',trainingApproved=False,
        sourceRevision=manifest['revision'],sourceRepository=manifest['repository'],repositoryDeclaredLicense=manifest['repositoryDeclaredLicense'],
        modelTrainingPerformed=False,limitations=['No independent benign control or per-packet incident labels.',
        'Links are nearest preceding matching client/BSSID within 30 seconds; observations are not credential or intent verdicts.',
        'Identical bytes are not automatically discarded as duplicates; retransmissions and capture duplicates are not resolved.',
        'Addresses in derived files are stable source-local aliases, not a promise of irreversible anonymization.'],files={})
    for p in out.iterdir():
        if p.is_file():report['files'][p.name]=hashlib.sha256(p.read_bytes()).hexdigest()
    (out/'manifest.json').write_text(json.dumps(report,indent=2))
    print(json.dumps({k:v for k,v in report.items() if k not in ('files','limitations')},indent=2))
    if mismatches:raise ValueError('Decoder disagreement; retain as unapproved source audit')
    return report


if __name__=='__main__':
    root=Path(__file__).parent;os.environ['XDG_CONFIG_HOME']=str(root/'.scapy-config');os.environ['XDG_CACHE_HOME']=str(root/'.scapy-cache')
    extract(root,root/'domain-data/wifi-reference-v1')
