"""Create an append-only local review archive; never upload or claim publication clearance."""
import argparse
from collections import Counter
from datetime import datetime,timezone
import hashlib
import json
from pathlib import Path
import shutil

DATA_ROOTS=('seed-data','builder-data','corrective-data','correlation-data','defensive-concepts',
    'evaluation-data','grounded-data','intent-data','knowledge-data','policy-data','quantity-data','ble-teaching','domain-data')
RUN_PREFIXES=('qwen4b-intent-v','qwen4b-quantity-v','qwen4b-grounded-v','qwen4b-domain-')
RUN_FILES={'runtime-manifest.json','development-manifest.json','training-metrics.json','trainer-state.json',
    'continuation-audit.json','weight-stability.json','optimizer-stability.json','gradient-checks.json',
    'responses.json','report.json','replayed-review.json','base-intake-manifest.json','adapter_config.json',
    'checkpoint-pause.json','paused-phase-metrics.json','prose-review.json','paired-review.json','replayed-review-detailed.json'}


def digest(data):return hashlib.sha256(data).hexdigest()


def export(root,out):
    root=root.resolve();out=out.resolve()
    if out.exists():raise ValueError('Archive already exists; create a new named snapshot')
    selected=set()
    for name in DATA_ROOTS:
        folder=root/name
        if folder.exists():
            selected.update(p for p in folder.rglob('*') if p.is_file() and p.suffix in ('.json','.jsonl','.md','.py','.kt','.txt','.html'))
    selected.update(p for p in root.iterdir() if p.is_file() and p.suffix in ('.py','.ps1','.md','.txt','.json'))
    runs=root/'runs'
    if runs.exists():
        for folder in runs.iterdir():
            if folder.is_dir() and folder.name.startswith(RUN_PREFIXES):
                selected.update(p for p in folder.iterdir() if p.is_file() and (p.name in RUN_FILES or p.name.endswith('-audit.json')))
                for sub in ('training-source','experiment-source','corpus'):
                    if (folder/sub).exists():selected.update(p for p in (folder/sub).rglob('*') if p.is_file() and p.suffix in ('.py','.json','.jsonl'))
    # Read before creating output, so an invalid source cannot be mistaken for a completed archive.
    files=[];items=[];contents={};membership={}
    for path in selected:
        relative=path.relative_to(root).as_posix()
        if relative.startswith('runs/') and relative.endswith('/corpus/train.jsonl'):
            for line in path.read_text(encoding='utf-8-sig').splitlines():
                if line.strip():
                    canonical=json.dumps(json.loads(line),sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf-8')
                    membership.setdefault(digest(canonical),set()).add(relative.split('/')[1])
    for path in sorted(selected):
        relative=path.relative_to(root).as_posix();raw=path.read_bytes();contents[relative]=raw
        entry=dict(path='files/'+relative,originalRelativePath=relative,bytes=len(raw),sha256=digest(raw))
        if path.suffix=='.jsonl':
            count=0;statuses=Counter();splits=Counter()
            for line,text in enumerate(raw.decode('utf-8-sig').splitlines(),1):
                if not text.strip():continue
                row=json.loads(text);count+=1
                if not isinstance(row,dict):raise ValueError('Training rows must be objects')
                canonical=json.dumps(row,sort_keys=True,separators=(',',':'),ensure_ascii=False).encode('utf-8')
                item=dict(file=entry['path'],line=line,canonicalContentSha256=digest(canonical),
                    includedInRunTrainingCorpora=sorted(membership.get(digest(canonical),set())),
                    archiveStatus='superseded-draft' if 'draft' in relative.lower() else 'inspect-original-label-and-run-membership')
                for key in ('id','split','task','sourceId','family','labelStatus','evaluationRole'):
                    if key in row:item[key]=row[key]
                items.append(item);statuses[str(row.get('labelStatus','not-recorded'))]+=1;splits[str(row.get('split','not-recorded'))]+=1
            entry.update(jsonlItems=count,labelStatuses=dict(statuses),splits=dict(splits))
        files.append(entry)
    out.mkdir(parents=True)
    for relative,raw in contents.items():
        target=out/'files'/relative;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(raw)
    docs=root.parent.parent/'docs';document_names=['ai-training-process-and-transparency.md','ai-unit-clarification-cycle-2026-11-10.md','ai-quantity-extraction-cycle-2026-11-10.md','ai-builder-intent-cycle-2026-11-10.md','ai-grounded-training-cycle-2026-11-10.md','blue-team-ontology-dataset-and-training-plan-2026-11-10.md','blue-team-domain-review-and-source-intake-2026-11-10.md','blue-team-domain-baseline-2026-11-10.md']
    document_names.append('blue-team-evidence-contrast-expansion-2026-11-10.md')
    document_names.append('blue-team-eapol-semantic-expansion-2026-11-10.md')
    for name in document_names:
        source=docs/name
        if source.exists():
            raw=source.read_bytes();target=out/'process'/name;target.parent.mkdir(exist_ok=True);target.write_bytes(raw)
            files.append(dict(path='process/'+name,originalRelativePath='../../docs/'+name,bytes=len(raw),sha256=digest(raw)))
    license_file=root.parent.parent/'LICENSE'
    if license_file.exists():
        raw=license_file.read_bytes();(out/'PROJECT-LICENSE.txt').write_bytes(raw)
        files.append(dict(path='PROJECT-LICENSE.txt',originalRelativePath='../../LICENSE',bytes=len(raw),sha256=digest(raw)))
    index=out/'item-index.jsonl';index.write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in items)+'\n',encoding='utf-8')
    manifest=dict(schemaVersion=1,createdUtc=datetime.now(timezone.utc).isoformat(),purpose='Local archive for eventual public inspection and critique',
        publicationStatus='not-published-external-source-and-private-data-review-required',releaseEligible=False,
        files=files,index=dict(path=index.name,sha256=digest(index.read_bytes()),entries=len(items)),
        limitations=['Index entries include identical items copied into prepared/run snapshots; not unique independent examples.',
        'Run-corpus membership means admitted input, not proof of individual example exposure or a successful applied update.',
        'Drafts, candidates, trained items and evaluation questions retain their original labels; inclusion does not imply training admission.',
        'Repository code licence is preserved separately; it is not a blanket licence for all training data. Authorship/permission-to-train labels are not redistribution licences.',
        'Local archive does not establish redistribution rights, label correctness, independent review, or independent timestamping.'],
        exclusions=['Raw external-data downloads; preserve source notices/intake paths and obtain redistribution clearance separately.',
        'Private field captures, ignored model weights, optimizer binaries, caches, console logs and unrelated runs.'])
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    (out/'README.md').write_text('# Training transparency review archive\n\nThis is a local, unpublished snapshot. Start with process/ai-training-process-and-transparency.md.\n\nmanifest.json lists exact file hashes. item-index.jsonl maps every archived JSONL item to its original file, line, identity and available labels. The files/ tree preserves the actual requests, expected answers, source notices, split assignments, generators, and selected experiment records. Draft paths are retained. Repeated copies are not independent examples. Check actual run corpus membership before calling an item trained.\n\nVerify with the archived export_training_transparency.py --verify <this directory>. Report critiques using the snapshot manifest hash, item ID and file/line, supporting evidence and proposed correction. External-source/private-data review remains required before publication. No model is qualified by inclusion.\n',encoding='utf-8')
    verify(out)
    return dict(files=len(files),indexedItems=len(items),manifestSha256=digest((out/'manifest.json').read_bytes()),output=str(out))


def verify(out):
    out=Path(out);manifest=json.loads((out/'manifest.json').read_text(encoding='utf-8'))
    for entry in manifest['files']+[manifest['index']]:
        target=(out/entry['path']).resolve()
        if not target.is_relative_to(out.resolve()):raise ValueError('Archive path escapes snapshot')
        if digest(target.read_bytes())!=entry['sha256']:raise ValueError('Archive content hash changed: '+entry['path'])
    return dict(verified=True,files=len(manifest['files']),indexedItems=manifest['index']['entries'])


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path);p.add_argument('--verify',type=Path)
    a=p.parse_args()
    if bool(a.out)==bool(a.verify):p.error('Choose --out or --verify')
    print(json.dumps(verify(a.verify) if a.verify else export(Path(__file__).parent,a.out),indent=2))
