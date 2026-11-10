"""Bounded comparison of accepted BLE records with Scapy's independent PDU decoder."""
import argparse
import hashlib
import json
import os
from pathlib import Path


def compare(raw, packet):
    from scapy.layers.bluetooth4LE import BTLE, BTLE_ADV
    # Both decoders receive the same captured PDU; this does not independently
    # validate the Nordic wrapper offset, CRC status, or sniffer timestamps.
    decoded=BTLE(raw[7+raw[7]:])
    header=decoded[BTLE_ADV]
    advertiser=bytes.fromhex(header.payload.AdvA.replace(':',''))[::-1]
    checks=dict(kind=int(header.PDU_type)==packet['kind'],
                random=bool(header.TxAdd)==packet['random'],address=advertiser==packet['address'],
                payload=packet['data']==(bytes(header.payload)[6:] if packet['kind']!=1 else b''))
    return [key for key,passed in checks.items() if not passed]


def audit(source, limit=128, scan_limit=50000):
    from scapy.utils import RawPcapNgReader
    from ble_features import decode_advertisement
    results=[]
    for path in sorted(source.rglob('*.pcapng')):
        count=0; scanned=0; failures=[]; indices=[]; digest=hashlib.sha256()
        with RawPcapNgReader(str(path)) as reader:
            for index,(raw,meta) in enumerate(reader):
                scanned+=1
                packet,reason=decode_advertisement(raw,meta.linktype)
                if packet is not None:
                    digest.update(len(raw).to_bytes(4,'little'));digest.update(raw)
                    differences=compare(raw,packet)
                    if differences:failures.append(dict(recordIndex=index,fields=differences))
                    indices.append(index);count+=1
                if count>=limit or scanned>=scan_limit:break
        results.append(dict(file=str(path.relative_to(source)),scanned=scanned,compared=count,
                            recordIndices=indices,comparedRecordsSha256=digest.hexdigest(),mismatches=failures))
    if not results or any(not r['compared'] for r in results):raise ValueError('No comparison records in one or more sessions')
    return dict(schemaVersion=1,comparison='Scapy 2.6.1 legacy BLE PDU fields',
        samples=sum(r['compared'] for r in results),mismatches=sum(len(r['mismatches']) for r in results),
        sessions=results,releaseEligible=False,
        limitations=['First accepted records only; not a random sample or complete decoder qualification.',
                     'Only records accepted by the feature decoder are compared; rejection behavior is not independently validated.',
                     'Shared Nordic wrapper stripping; CRC flags, timestamps and exposure are not independently checked.',
                     'Checks PDU type, address type, advertiser address and advertising data; not Hunter parity.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    cache=Path(__file__).parent/'runs'/'scapy-cache';cache.mkdir(parents=True,exist_ok=True)
    os.environ['XDG_CONFIG_HOME']=str(cache.resolve());os.environ['XDG_CACHE_HOME']=str(cache.resolve())
    report=audit(args.source)
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='sessions'}))
    if report['mismatches']:raise SystemExit(1)
