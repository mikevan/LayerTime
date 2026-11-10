"""Collect inspectable documentation without moving source files or touching Git."""
import argparse
import hashlib
import json
import re
from pathlib import Path
import os


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def selections(root):
    chosen={}
    for source in (root/'docs').iterdir():
        if source.is_file() and (source.name.startswith(('ai-','blue-team-','LayerTime AI architecture'))
                                or source.name in ('builder-behavior-reporting.md','layerscope-visual-design.md')):
            chosen[source.resolve()]=Path('records')/source.name
    for source,name in [(root/'tools/ai-training/README.md','training-guide.md'),
                        (root/'tools/ai-toolkit-evaluation/README.md','toolkit-evaluation-guide.md')]:
        if source.exists():chosen[source.resolve()]=Path('guides')/name
    for source in (root/'tools/ai-training/domain-data').rglob('*.md'):
        if any('draft' in part.lower() for part in source.parts):continue
        chosen[source.resolve()]=Path('datasets')/source.relative_to(root/'tools/ai-training/domain-data')
    for source in (root/'tools/ai-training/acceptance').rglob('*.md'):
        chosen[source.resolve()]=Path('acceptance')/source.relative_to(root/'tools/ai-training/acceptance')
    return chosen


def collect(root,refresh=False):
    root=root.resolve();out=root/'docs/ai-development';out.mkdir(parents=True,exist_ok=True)
    seal=out/'document-manifest.json'
    if seal.exists():
        if not refresh:raise ValueError('Collection exists; use --verify or an explicit --refresh')
        previous=json.loads(seal.read_text())
        for item in previous['documents']:
            if digest(out/item['collectedPath'])!=item['collectedSha256']:
                raise ValueError('A collected document was edited; preserve/reconcile it before refresh: '+item['collectedPath'])
    chosen=selections(root);entries=[]
    def href(source,destination):return os.path.relpath(destination,source.parent).replace('\\','/')
    for source,relative in sorted(chosen.items(),key=lambda p:p[1].as_posix()):
        destination=out/relative;destination.parent.mkdir(parents=True,exist_ok=True)
        encoding='binary'
        if source.suffix.lower()=='.md':
            raw=source.read_bytes()
            if raw.startswith((b'\xff\xfe',b'\xfe\xff')):
                encoding='utf-16';text=raw.decode(encoding)
            else:
                try:encoding='utf-8-sig';text=raw.decode(encoding)
                except UnicodeDecodeError:encoding='cp1252';text=raw.decode(encoding)
            def rewrite(match):
                label,target=match.group(1),match.group(2);plain=target.strip('<>')
                if plain.startswith(('#','https:','http:','mailto:','plugin:','codex:','app:')):return match.group(0)
                path,sep,fragment=plain.partition('#')
                if not path:return match.group(0)
                original=(source.parent/path).resolve()
                actual=out/chosen[original] if original in chosen else original
                if not actual.is_relative_to(root):return match.group(0)
                rewritten=href(destination,actual)+(sep+fragment if sep else '')
                return '['+label+'](<'+rewritten+'>)'
            text=re.sub(r'\[([^\]]+)\]\((<[^>]+>|[^\s)]+)\)',rewrite,text)
            banner=('> Documentation reference copy. Original repository path: `'+source.relative_to(root).as_posix()+'`. '
                'Historical statements retain their original context; use the collection index and check-in note for current status.\n\n')
            destination.write_text(banner+text,encoding='utf-8')
        else:destination.write_bytes(source.read_bytes())
        entries.append(dict(sourcePath=source.relative_to(root).as_posix(),collectedPath=relative.as_posix(),
            sourceSha256=digest(source),collectedSha256=digest(destination),sourceEncoding=encoding,referenceCopy=True))
    manifest=dict(schemaVersion=1,purpose='Central documentation for AI, Builder, training process, data review and acceptance',
        policy='Originals remain authoritative at their existing paths; reference copies have relative links adjusted for this collection.',
        documents=entries,excluded=['Downloaded source libraries, model weights, captures, caches, copied bulk transparency archives and construction drafts.',
            'Unrelated glasses shopping research and deployment-site firmware/index assets.'],
        refresh='python tools/ai-training/collect_development_docs.py --refresh; edited collected copies must first be reconciled.')
    seal.write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    index=['# AI development documentation','','Start with [CHECKIN-NOTES.md](CHECKIN-NOTES.md) for the current scope, evidence, failures and next steps.',
        'Dataset expansion is paused. The unchanged model passed 1/12 overall development acceptance cases. No domain adapter or Android deployment is qualified.',
        'The owner controls all subsequent commits and pushes; the check-in script only acts when they run it with `-Execute`.','',
        '## Current reading path','',
        '1. [Check-in note](CHECKIN-NOTES.md).',
        '2. [Checkpoint and acceptance process](records/ai-development-checkpoint-and-acceptance-2026-11-10.md).',
        '3. [Acceptance results](acceptance/v2/RESULTS.md).',
        '4. [Ontology and training plan](records/blue-team-ontology-dataset-and-training-plan-2026-11-10.md).',
        '5. [Training transparency process](records/ai-training-process-and-transparency.md).','',
        '## How this collection works','',
        'This directory contains explanatory documents and readable teaching reviews. Earlier research, proposals and narrow training reports are historical; their use of “current” does not override the check-in note. '
        'Original documents remain in place so established code/document references keep working. These are reference copies; document-manifest.json records original paths and both hashes. '
        'Edit the original, then explicitly refresh the collection. Word documents are copied byte-for-byte; Markdown links are adjusted. '
        'Machine-readable fixtures, protocols and model evidence remain in their canonical locations below.','',
        '## Evaluation evidence','',
        '- [Frozen v2 protocol](../../tools/ai-training/acceptance/v2/protocol.json).',
        '- [Case rubrics](../../tools/ai-training/acceptance/v2/rubrics.jsonl).',
        '- [Exact baseline outputs](../../tools/ai-training/acceptance/v2/results/responses.json).',
        '- [Per-criterion semantic review](../../tools/ai-training/acceptance/v2/results/semantic-review.json).',
        '- [Acceptance score](../../tools/ai-training/acceptance/v2/results/acceptance-score.json).','',
        '## All collected documents','', '| Document | Original repository path |','|---|---|']
    for item in entries:index.append('| ['+Path(item['collectedPath']).name+'](<'+item['collectedPath']+'>) | `'+item['sourcePath']+'` |')
    (out/'README.md').write_text('\n'.join(index)+'\n',encoding='utf-8')
    return dict(directory=str(out),documents=len(entries),manifestSha256=digest(seal))


def verify(root):
    out=root/'docs/ai-development';manifest=json.loads((out/'document-manifest.json').read_text())
    for entry in manifest['documents']:
        if digest(root/entry['sourcePath'])!=entry['sourceSha256']:raise ValueError('Source changed: '+entry['sourcePath'])
        if digest(out/entry['collectedPath'])!=entry['collectedSha256']:raise ValueError('Collected copy changed: '+entry['collectedPath'])
    return dict(verified=True,documents=len(manifest['documents']))


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--verify',action='store_true');parser.add_argument('--refresh',action='store_true')
    args=parser.parse_args();root=Path(__file__).resolve().parents[2]
    print(json.dumps(verify(root) if args.verify else collect(root,args.refresh),indent=2))
