"""Recheck complete grounded-contract runs; never infer packet reasoning from copying."""
import argparse
import hashlib
import json
import statistics
from pathlib import Path
from grounded_contract import audit_record, supplied, validate


def summarize(examples, run):
    records=[json.loads(line) for line in examples.read_text(encoding='utf-8').splitlines() if line.strip()]
    report=json.loads((run/'report.json').read_text(encoding='utf-8'))
    rows=json.loads((run/'responses.json').read_text(encoding='utf-8'))
    if report['examplesSha256']!=hashlib.sha256(examples.read_bytes()).hexdigest():
        raise ValueError('Evaluation dataset hash mismatch')
    if [r['id'] for r in rows]!=[r['id'] for r in records] or report['examples']!=len(records):
        raise ValueError('Incomplete, duplicated, reordered, or foreign evaluation rows')
    families={}; failures=[]; facts=0; bearing=0; bearing_passes=0; passes=0
    for record,row in zip(records,rows):
        audit_record(record)
        count=len(supplied(record)['facts'])
        facts+=count; bearing+=bool(count)
        try:
            validate(row['output'],record)
            valid=True
        except (ValueError,TypeError,KeyError) as exc:
            valid=False
            failures.append(dict(id=row['id'],error=str(exc)))
        if valid != row['schemaAndReferencesValid']:
            raise ValueError('Saved validity flag disagrees with replay validation')
        passes+=valid; bearing_passes+=valid and bool(count)
        family=record['family']
        families.setdefault(family,[]).append(valid)
    if passes!=report['groundedAgreementPasses']:
        raise ValueError('Report total disagrees with replay validation')
    seconds=sorted(r['seconds'] for r in rows)
    return dict(examples=len(records),exactAgreementPasses=passes,factBearingPages=bearing,
        factBearingPasses=bearing_passes,suppliedFactOccurrences=facts,rawAuditPasses=len(records),
        authoredFamilies=len(families),familiesWithEveryPagePassing=sum(all(v) for v in families.values()),
        familyResults={k:dict(pages=len(v),passes=sum(v)) for k,v in families.items()},
        medianGenerationSeconds=statistics.median(seconds),maximumGenerationSeconds=max(seconds),
        failures=failures,examplesSha256=report['examplesSha256'],adapterSha256=report['adapterSha256'],
        releaseEligible=False,limitations=[
            'Facts are supplied; this is contract adherence, not learned packet interpretation.',
            'Relabeled pages are correlated, not independent trials or field environments.',
            'Desktop generation time excludes model loading and does not measure Android latency.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--examples',type=Path,required=True)
    parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('Preserve existing summaries; choose a new output')
    result=summarize(args.examples,args.run)
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k not in ('failures','familyResults')},indent=2))
