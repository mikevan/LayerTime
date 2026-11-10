"""Select relevant detection concepts with CSV/ontology identity agreement.

The source supplies definitions and KB articles. These are reference records, not
claims that LayerTime implements the techniques or that observed traffic is an attack.
"""
import csv
import hashlib
import json
import shutil
from pathlib import Path

SELECTED={'D3-PMAD','D3-ISVA','D3-NTCD','D3-CSPP','D3-CAA','D3-NTSA','D3-PCA','D3-DNSTA','D3-RPA',
          'D3-ANAA','D3-APCA','D3-CA','D3-RTA','D3-IPCTA','D3-RTSD','D3-PHDURA','D3-SDA','D3-ANET',
          'D3-AZET','D3-RAPA','D3-UDTA','D3-IAA','D3-HD','D3-IPRA','D3-DNRA'}


def extract(source):
    manifest=json.loads((source/'intake-manifest.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        if hashlib.sha256((source/item['file']).read_bytes()).hexdigest()!=item['sha256']:
            raise ValueError('Defensive source document changed')
    ontology=json.loads((source/'d3fend.json').read_text(encoding='utf-8'))
    nodes={n['d3f:d3fend-id']:n for n in ontology['@graph'] if isinstance(n.get('d3f:d3fend-id'),str)}
    rows=[]
    with (source/'d3fend.csv').open(encoding='utf-8-sig',newline='') as stream:
        for row in csv.DictReader(stream):
            if row['ID'] not in SELECTED:continue
            if row['D3FEND Tactic']!='Detect':raise ValueError('Unexpected technique tactic')
            name=row['D3FEND Technique Level 1'] or row['D3FEND Technique Level 0'] or row['D3FEND Technique']
            node=nodes[row['ID']]
            if name!=node.get('rdfs:label') or row['Definition']!=node.get('d3f:definition'):
                raise ValueError('CSV and ontology disagree on identity or definition')
            if not row['Definition'].strip():raise ValueError('No definition supplied')
            rows.append(dict(id=row['ID'],name=name,definition=row['Definition'],
                article=node.get('d3f:kb-article',''),ontologyNode=node['@id'],sourceVersion=manifest['version'],
                sourceId='mitre-d3fend-1.6.0',referenceStatus='primary-source-cross-format-checked',
                appCapabilityVerified=False,trainingApproved=False,releaseEligible=False))
    if {r['id'] for r in rows}!=SELECTED:raise ValueError('Selected concepts missing or duplicated')
    if len(rows)!=len(SELECTED):raise ValueError('Duplicate selected concept')
    return rows


if __name__=='__main__':
    root=Path(__file__).parent;source=root/'external-data/mitre-d3fend-1.6.0'
    out=root/'defensive-concepts/v1'
    if out.exists():raise SystemExit('Use a new version for changes to the reference pack')
    rows=extract(source);out.mkdir(parents=True)
    path=out/'concepts.jsonl';path.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows),encoding='utf-8')
    shutil.copy2(source/'TERMS-OF-USE.html',out/'TERMS-OF-USE.html')
    shutil.copy2(source/'intake-manifest.json',out/'SOURCE-NOTICES.json')
    manifest=dict(concepts=len(rows),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        termsSha256=hashlib.sha256((out/'TERMS-OF-USE.html').read_bytes()).hexdigest(),
        originalConceptCount=len(SELECTED),trainingApproved=False,releaseEligible=False,
        limitations=['Primary-source definitions, not event labels or demonstrated app capabilities.',
            'Some techniques require telemetry not present in a wireless frame capture.',
            'Imported offensive frameworks and active certificate collection were not selected.',
            'Technical source prose needs separately reviewed plain-language teaching answers.',
            'Preserve the accompanying source copyright and full license notice with copies.'])
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(manifest))
