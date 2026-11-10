"""Audit separate BLE connection measurements without inventing advertisement labels."""
import argparse
import csv
import hashlib
import json
import math
from pathlib import Path

BITS=['LLID1','LLID0','NESN','SN','MD','CP','RFU1','RFU0']


def audit_stream(path,expected_count):
    with path.open(newline='',encoding='utf-8') as stream:
        reader=csv.DictReader(stream)
        if set(reader.fieldnames or [])!={'t_a','Length',*BITS}:raise ValueError('Unexpected BLE connection CSV schema')
        rows=list(reader)
    if len(rows)!=expected_count:raise ValueError('Record count differs from publisher metadata')
    times=[];invalid_headers=0
    for row in rows:
        time=float(row['t_a'])
        if not math.isfinite(time):raise ValueError('Nonfinite observation timestamp')
        times.append(time)
        try:
            valid=all(int(row[name]) in (0,1) for name in BITS) and 0<=int(row['Length'])<=255
        except ValueError:valid=False
        invalid_headers+=not valid
    descending=sum(b<a for a,b in zip(times,times[1:]))
    return dict(records=len(rows),publisherCount=expected_count,invalidHeaderRows=invalid_headers,
                descendingTimestampTransitions=descending,equalTimestampTransitions=sum(a==b for a,b in zip(times,times[1:])),
                firstTimestampNative=times[0] if times else None,lastTimestampNative=times[-1] if times else None,
                timestampUnit='t_a; README names timestamps but does not explicitly establish units',
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def audit(source):
    manifest=json.loads((source/'intake-manifest.json').read_text())
    if manifest['license']!='CC-BY-4.0':raise ValueError('Unexpected source license')
    entries={f['file']:f for f in manifest['files']}
    for relative,entry in entries.items():
        if hashlib.sha256((source/relative).read_bytes()).hexdigest()!=entry['sha256']:raise ValueError('Source file changed')
    sessions=[]
    for metadata_file in sorted(source.glob('dataset_*/set_*/metadata.json')):
        metadata=json.loads(metadata_file.read_text())
        addresses,counts=metadata['AA'],metadata['num_obs']
        if len(addresses)!=len(counts) or len(set(addresses))!=len(addresses):raise ValueError('Invalid connection metadata')
        streams=[]
        for address,count in zip(addresses,counts):
            if len(address)!=8 or any(c not in '0123456789abcdef' for c in address):raise ValueError('Invalid access-address identifier')
            path=metadata_file.parent/(address+'.csv')
            streams.append(dict(file=str(path.relative_to(source)),accessAddress=address,
                                **audit_stream(path,count)))
        sessions.append(dict(session=str(metadata_file.parent.relative_to(source)),sniffedChannel=metadata['sniffed_ch'],streams=streams))
    return dict(schemaVersion=1,sourceCommit=manifest['commit'],sessions=sessions,
        sessionCount=len(sessions),streamCount=sum(len(s['streams']) for s in sessions),
        records=sum(r['records'] for s in sessions for r in s['streams']),releaseEligible=False,
        trainingApproved=False,limitations=[
            'Access addresses are connection identifiers, not MAC addresses or identities of people.',
            'Single data-channel sampling does not establish radio silence, total packet loss or complete traffic coverage.',
            'No advertisement-spam labels or attack-free certification are provided by this audit.',
            'Different sniffer runs cannot be aligned as simultaneous captures without additional synchronization evidence.',
            'Timestamp units require explicit verification before reporting rates or elapsed seconds.',
            'Header range checks do not validate CRC, application payload meaning or protocol compliance.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--source',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    report=audit(args.source);args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='sessions'}))
