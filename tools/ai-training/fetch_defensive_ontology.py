"""Versioned primary defensive ontology intake with its full reuse notice."""
import csv
import hashlib
import io
import json
from pathlib import Path
import requests

VERSION='1.6.0'


def acquire(url,path,limit=64*1024*1024):
    if path.exists():return path.read_bytes()
    chunks=[];size=0
    with requests.get(url,stream=True,timeout=(15,40)) as response:
        response.raise_for_status()
        for chunk in response.iter_content(1024*1024):
            size+=len(chunk)
            if size>limit:raise ValueError('Source exceeds bounded intake size')
            chunks.append(chunk)
    raw=b''.join(chunks);path.write_bytes(raw);return raw


if __name__=='__main__':
    root=Path(__file__).parent/f'external-data/mitre-d3fend-{VERSION}';root.mkdir(parents=True,exist_ok=True)
    notices=acquire('https://d3fend.mitre.org/tou/',root/'TERMS-OF-USE.html')
    if b'royalty-free license' not in notices or b'copyright designation' not in notices:
        raise ValueError('Expected permission notice missing; review current terms')
    files=[]
    for name in ['d3fend.csv','d3fend.json']:
        url=f'https://d3fend.mitre.org/ontologies/d3fend/{VERSION}/{name}'
        raw=acquire(url,root/name)
        files.append(dict(file=name,url=url,bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
    rows=list(csv.DictReader(io.StringIO((root/'d3fend.csv').read_text(encoding='utf-8-sig'))))
    manifest=dict(schemaVersion=1,version=VERSION,source='MITRE D3FEND',sourceUrl='https://d3fend.mitre.org/resources/ontology/',
        license='MITRE D3FEND Terms of Use; retained in TERMS-OF-USE.html',
        termsSha256=hashlib.sha256(notices).hexdigest(),files=files,csvRows=len(rows),
        copyright='Copyright 2025 The MITRE Corporation. D3FEND is a trademark of The MITRE Corporation.',
        trainingApproved=False,releaseEligible=False,
        limitations=['Ontology concepts and inferred relationships are not field telemetry or evidence that an incident happened.',
            'Imported ATT&CK, ATLAS and SPARTA material has separate terms; do not indiscriminately ingest imported technique text.',
            'Select relevant passive defensive-analysis concepts before constructing training examples.',
            'Retain the full copyright designation and license with derived copies.'])
    (root/'intake-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(dict(version=VERSION,files=files,csvRows=len(rows),columns=list(rows[0]) if rows else [])))
