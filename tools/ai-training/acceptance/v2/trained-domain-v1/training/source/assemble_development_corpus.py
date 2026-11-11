"""Assemble distinct capture-review and source-grounded explanation objectives.

This is a small development curriculum, not a qualified domain corpus. Knowledge
cards are split by source document, never by paraphrases of one source passage.
The existing capture families keep their original split assignments.
"""
import argparse
import hashlib
import json
from pathlib import Path
from prepare import canonical, validate_records, digest


def validate_knowledge(row,cards):
    card=cards.get(row.get('cardId'))
    if not card:raise ValueError('Unknown knowledge card')
    if row.get('task')!='EXPLAIN_DEFENSIVE_CONCEPT':raise ValueError('Invalid knowledge task')
    if row.get('sourceId')!='layertime-authored-knowledge-v1':raise ValueError('Invalid knowledge provenance')
    if row.get('family')!=card['sourceId']:raise ValueError('Knowledge family differs from its source')
    if row['split']!=('train' if card['sourceId']=='nist-sp800-94' else 'validation'):
        raise ValueError('Knowledge document split leakage')
    target=json.loads(row['messages'][-1]['content'])
    expected=dict(schemaVersion=1,answer=card['sourceFact'],references=[dict(sourceId=card['sourceId'],pdfPage=card['pdfPage'])])
    if target!=expected:raise ValueError('Answer or references do not match reviewed card')
    if row['messages'][1]['content']!=card['question']:raise ValueError('Question changed from teaching card')


def verify(directory):
    manifest=json.loads((directory/'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schemaVersion')!=1 or manifest.get('releaseEligible') is not False:
        raise ValueError('Invalid development manifest')
    if set(manifest['splits'])!={'train','validation','test'}:raise ValueError('Missing dataset split')
    if digest(directory/'knowledge-cards.jsonl')!=manifest['cardsSha256']:raise ValueError('Teaching cards changed')
    if digest(directory/'sources.json')!=manifest['sourceRegistrySha256']:raise ValueError('Source registry changed')
    cards={c['id']:c for c in [json.loads(s) for s in (directory/'knowledge-cards.jsonl').read_text(encoding='utf-8').splitlines()]}
    registry=json.loads((directory/'sources.json').read_text(encoding='utf-8'))
    captures=[];seen=set();families={};fingerprints={}
    for split,info in manifest['splits'].items():
        if info['file']!=split+'.jsonl':raise ValueError('Invalid corpus filename')
        path=directory/info['file']
        if digest(path)!=info['sha256']:raise ValueError('Split integrity check failed')
        rows=[json.loads(s) for s in path.read_text(encoding='utf-8').splitlines() if s.strip()]
        if len(rows)!=info['examples']:raise ValueError('Split count mismatch')
        if sorted({r['family'] for r in rows})!=info['families']:raise ValueError('Family list mismatch')
        counts={task:sum(r['task']==task for r in rows) for task in sorted({r['task'] for r in rows})}
        if counts!=info['tasks']:raise ValueError('Task count mismatch')
        for row in rows:
            if row['id'] in seen or row['split']!=split:raise ValueError('Duplicate identity or incorrect split')
            seen.add(row['id'])
            source=registry['sources'].get(row['sourceId'],{})
            if source.get('allowedForTraining') is not True:raise ValueError('Source is not approved for development training')
            family=row['family']
            if family in families and families[family]!=split:raise ValueError('Family leaks across splits')
            families[family]=split
            fingerprint=hashlib.sha256(canonical(row['messages']).encode()).hexdigest()
            if fingerprint in fingerprints and fingerprints[fingerprint]!=split:raise ValueError('Conversation leaks across splits')
            fingerprints[fingerprint]=split
            if row['task']=='REVIEW_CAPTURE':captures.append(row)
            elif row['task']=='EXPLAIN_DEFENSIVE_CONCEPT':validate_knowledge(row,cards)
            else:raise ValueError('Unsupported training task')
    validate_records(captures,registry)
    return manifest


def assemble(root,out):
    if out.exists():raise ValueError('Use a new corpus directory')
    cards=[json.loads(s) for s in (root/'knowledge-data/cards.jsonl').read_text(encoding='utf-8').splitlines()]
    from knowledge_cards import build
    if cards!=build(root/'external-data/nist-reference-library-v1'):
        raise ValueError('Teaching cards differ from source-checked definitions')
    registry=json.loads((root/'sources.json').read_text(encoding='utf-8'))
    captures=[]
    for split in ('train','validation','test'):
        captures.extend(json.loads(s) for s in (root/f'seed-data/prepared/{split}.jsonl').read_text(encoding='utf-8').splitlines())
    validate_records(captures,registry)
    rows=list(captures)
    schema=dict(type='object',required=['schemaVersion','answer','references'],additionalProperties=False,
        properties=dict(schemaVersion=dict(const=1),answer=dict(type='string'),references=dict(type='array',
        items=dict(type='object',required=['sourceId','pdfPage'],additionalProperties=False,
        properties=dict(sourceId=dict(type='string'),pdfPage=dict(type='integer'))))))
    for card in cards:
        row=dict(id='knowledge-v1-'+card['id'],family=card['sourceId'],cardId=card['id'],
            split='train' if card['sourceId']=='nist-sp800-94' else 'validation',
            task='EXPLAIN_DEFENSIVE_CONCEPT',sourceId='layertime-authored-knowledge-v1',
            outputSchema=json.dumps(schema),labelStatus='agent-source-checked-development-only',
            messages=[dict(role='system',content='Explain the defensive security concept in plain language. Return only the requested JSON. Cite the supporting source document and PDF page. The cited publications are historical conceptual guidance; do not claim coverage of newer protocol versions.'),
                      dict(role='user',content=card['question']),
                      dict(role='assistant',content=json.dumps(dict(schemaVersion=1,answer=card['sourceFact'],
                            references=[dict(sourceId=card['sourceId'],pdfPage=card['pdfPage'])])))])
        validate_knowledge(row,{c['id']:c for c in cards});rows.append(row)
    out.mkdir(parents=True)
    manifest=dict(schemaVersion=1,purpose='small multitask development curriculum, not domain qualification',
        releaseEligible=False,independentExpertReview=False,splits={},
        inputs={str(p.relative_to(root)):digest(p) for p in [root/'knowledge-data/cards.jsonl',root/'knowledge-data/manifest.json',root/'sources.json']},
        limitations=['24 authored concepts plus 17 existing synthetic capture pages; no new field recordings.',
            'Knowledge validation uses a different source document; capture test families retain original assignments.',
            'Knowledge task has no independent test set in this version.',
            'Application-specific design interpretations on the cards are excluded from supervised answers.',
            'The twelve corrective candidates and quarantined external instruction datasets are excluded.'])
    for split in ('train','validation','test'):
        selected=[r for r in rows if r['split']==split]
        path=out/(split+'.jsonl');path.write_text(''.join(canonical(r)+'\n' for r in selected),encoding='utf-8')
        manifest['splits'][split]=dict(file=path.name,sha256=digest(path),examples=len(selected),
            families=sorted({r['family'] for r in selected}),tasks={task:sum(r['task']==task for r in selected) for task in sorted({r['task'] for r in selected})})
    (out/'knowledge-cards.jsonl').write_text((root/'knowledge-data/cards.jsonl').read_text(encoding='utf-8'),encoding='utf-8')
    manifest['cardsSha256']=digest(out/'knowledge-cards.jsonl')
    (out/'sources.json').write_text(json.dumps(registry,indent=2),encoding='utf-8')
    manifest['sourceRegistrySha256']=digest(out/'sources.json')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    verify(out)
    return manifest


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--verify',action='store_true');args=parser.parse_args()
    print(json.dumps(verify(args.output) if args.verify else assemble(Path(__file__).parent,args.output),indent=2))
