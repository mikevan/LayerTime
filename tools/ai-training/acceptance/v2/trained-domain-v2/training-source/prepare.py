"""Prepare an auditable local seed corpus. No downloads, model calls, or telemetry."""
import argparse
import hashlib
import json
from pathlib import Path

SPLITS = ('train', 'validation', 'test')

def canonical(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(',', ':'))

def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def validate_records(records, registry):
    if registry.get('schemaVersion') != 1:
        raise ValueError('Unsupported source registry')
    seen_ids, seen_content, families = set(), {}, {}
    for record in records:
        if record['split'] not in SPLITS or not record['family']:
            raise ValueError('Invalid split or family')
        identifier = record['id']
        if identifier in seen_ids:
            raise ValueError('Duplicate example identity')
        seen_ids.add(identifier)
        family = record['family']
        if family in families and families[family] != record['split']:
            raise ValueError('Capture family leaks across splits')
        families[family] = record['split']
        source = registry['sources'].get(record['sourceId'])
        if not source or source.get('allowedForTraining') is not True or not source.get('license'):
            raise ValueError('Source lacks documented permission')
        profile=source.get('validationProfile')
        if profile:
            if profile=='disconnect-policy-v1':
                from audit_policy_curriculum import audit_row
                audit_row(record)
            elif profile=='grounded-contract-v1':
                from grounded_contract import audit_record
                audit_record(record)
            elif profile=='analytic-intent-v1':
                from analytic_intent import audit_record
                audit_record(record)
            elif profile=='analytic-intent-curriculum-v2':
                from intent_curriculum_audit import audit_record
                audit_record(record)
            elif profile=='analytic-quantity-extraction-v1':
                from analytic_quantity import audit_record
                audit_record(record)
            elif profile=='analytic-quantity-ontology-v1':
                from quantity_ontology import audit_record
                audit_record(record)
            else:raise ValueError('Unsupported source validation profile')
        if record.get('task') not in ('REVIEW_CAPTURE','REVIEW_GROUNDED_CAPTURE','PLAN_ANALYTIC','PLAN_ANALYTIC_QUANTITIES') or not record.get('labelStatus'):
            raise ValueError('Unsupported or unlabelled task')
        messages = record['messages']
        if not messages or messages[0]['role'] != 'system' or messages[-1]['role'] != 'assistant':
            raise ValueError('Invalid supervised conversation')
        if any(m['role'] not in ('system', 'user', 'assistant') or not isinstance(m['content'], str) for m in messages):
            raise ValueError('Invalid conversation role or content')
        if any(m['role'] == 'assistant' for m in messages[:-1]):
            raise ValueError('This initial corpus supports one supervised completion')
        fingerprint = hashlib.sha256(canonical(messages).encode()).hexdigest()
        if fingerprint in seen_content and seen_content[fingerprint] != record['split']:
            raise ValueError('Identical conversation leaks across splits')
        seen_content[fingerprint] = record['split']
        if record['task']=='PLAN_ANALYTIC_QUANTITIES':
            if profile not in ('analytic-quantity-extraction-v1','analytic-quantity-ontology-v1'):raise ValueError('Quantity task requires its source-grounding audit')
            continue
        if record['task']=='PLAN_ANALYTIC':
            if profile not in ('analytic-intent-v1','analytic-intent-curriculum-v2'):raise ValueError('Analytic intent task requires its authored-label audit')
            continue
        if record['task']=='REVIEW_GROUNDED_CAPTURE':
            if profile!='grounded-contract-v1':raise ValueError('Grounded task requires its raw-observation audit')
            continue
        target = json.loads(messages[-1]['content'])
        if set(target) != {'schemaVersion', 'issues'} or type(target['schemaVersion']) is not int or target['schemaVersion'] != 1:
            raise ValueError('Invalid assessment target')
        evidence = set(record['evidenceIds'])
        if any(type(n) is not int or n < 0 for n in evidence):
            raise ValueError('Invalid evidence identity')
        if not isinstance(target['issues'], list) or len(target['issues']) > 32:
            raise ValueError('Invalid issue list')
        for issue in target['issues']:
            if set(issue) != {'headline', 'explanation', 'frameIds'}:
                raise ValueError('Invalid issue fields')
            if not isinstance(issue['headline'], str) or not 1 <= len(issue['headline']) <= 160:
                raise ValueError('Invalid issue headline')
            if not isinstance(issue['explanation'], str) or not 1 <= len(issue['explanation']) <= 2000:
                raise ValueError('Invalid issue explanation')
            refs = issue['frameIds']
            if not isinstance(refs, list) or not 1 <= len(refs) <= 256 or any(type(n) is not int or n not in evidence for n in refs):
                raise ValueError('Fabricated or missing evidence reference')
    return records

def prepare(seed_file, registry_file, output_dir, purpose='fixture-derived smoke training only'):
    records = [json.loads(line) for line in Path(seed_file).read_text(encoding='utf-8').splitlines() if line.strip()]
    registry = json.loads(Path(registry_file).read_text(encoding='utf-8'))
    for source_id in {r['sourceId'] for r in records}:
        approved=registry['sources'].get(source_id,{}).get('approvedSeedSha256')
        if approved and approved!=digest(seed_file):raise ValueError('Seed bytes differ from approved development source')
    validate_records(records, registry)
    if set(r['split'] for r in records) != set(SPLITS):
        raise ValueError('All three independent family splits are required')
    out = Path(output_dir)
    out.mkdir(parents=True, exist_ok=True)
    manifest = {'schemaVersion': 1, 'purpose': purpose, 'releaseEligible': False,
                'seedSha256': digest(seed_file), 'sourceRegistrySha256': digest(registry_file), 'splits': {}}
    for split in SPLITS:
        selected = [r for r in records if r['split'] == split]
        path = out / f'{split}.jsonl'
        data = '\n'.join(canonical(r) for r in selected) + '\n'
        path.write_text(data, encoding='utf-8')
        manifest['splits'][split] = {'file': path.name, 'sha256': digest(path), 'examples': len(selected),
                                    'families': sorted(set(r['family'] for r in selected))}
    (out / 'sources.json').write_text(json.dumps(registry, indent=2), encoding='utf-8')
    # Hash the exact copied registry, independently of whitespace in the original.
    manifest['sourceRegistrySha256'] = digest(out / 'sources.json')
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
    return manifest

def verify(directory):
    directory = Path(directory)
    manifest = json.loads((directory / 'manifest.json').read_text(encoding='utf-8'))
    if manifest.get('schemaVersion') != 1 or set(manifest['splits']) != set(SPLITS):
        raise ValueError('Unsupported corpus manifest')
    if digest(directory / 'sources.json') != manifest['sourceRegistrySha256']:
        raise ValueError('Source registry integrity check failed')
    records = []
    for split in SPLITS:
        info = manifest['splits'][split]
        if info['file'] != f'{split}.jsonl':
            raise ValueError('Invalid corpus filename')
        path = directory / info['file']
        if digest(path) != info['sha256']:
            raise ValueError('Corpus integrity check failed')
        selected = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line.strip()]
        if len(selected) != info['examples'] or any(r['split'] != split for r in selected):
            raise ValueError('Corpus count or split mismatch')
        if sorted(set(r['family'] for r in selected)) != info['families']:
            raise ValueError('Corpus family list mismatch')
        records.extend(selected)
    validate_records(records, json.loads((directory / 'sources.json').read_text(encoding='utf-8')))
    return manifest

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--seeds', type=Path)
    parser.add_argument('--sources', type=Path, default=Path(__file__).with_name('sources.json'))
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--verify', action='store_true')
    parser.add_argument('--purpose',default='fixture-derived smoke training only')
    args = parser.parse_args()
    if not args.verify and args.seeds is None:
        parser.error('--seeds is required when preparing a corpus')
    result = verify(args.out) if args.verify else prepare(args.seeds, args.sources, args.out,args.purpose)
    print(json.dumps(result, indent=2))
