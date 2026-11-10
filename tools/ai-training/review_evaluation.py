"""Revalidate saved generations and keep interpretation review separate from syntax."""
import argparse
import hashlib
import json
from pathlib import Path
from evaluate_local_model import validate_assessment


def review_responses(responses, records, reviews):
    indexed = {r['id']: r for r in records}
    if len(indexed) != len(records):
        raise ValueError('Duplicate example IDs')
    seen = set()
    results = []
    for response in responses:
        identity = response['id']
        if identity in seen or identity not in indexed:
            raise ValueError('Duplicate or unknown response ID')
        seen.add(identity)
        digest = hashlib.sha256(response['output'].encode('utf-8')).hexdigest()
        row = dict(id=identity, outputSha256=digest, schemaAndReferencesValid=False,
                   interpretationStatus='unreviewed', qualified=False)
        try:
            validate_assessment(response['output'], indexed[identity])
            row['schemaAndReferencesValid'] = True
        except (ValueError, TypeError, KeyError) as exc:
            row['validationError'] = str(exc)
        decision = reviews.get(identity)
        if decision:
            if decision['outputSha256'] != digest:
                raise ValueError('Review does not match saved output')
            if decision['status'] not in ('pass', 'fail') or not decision.get('findings'):
                raise ValueError('Review requires a decision and findings')
            row['interpretationStatus'] = decision['status']
            row['findings'] = decision['findings']
            row['qualified'] = row['schemaAndReferencesValid'] and decision['status'] == 'pass'
        results.append(row)
    return dict(examples=len(results), reviewed=sum(r['interpretationStatus'] != 'unreviewed' for r in results),
                interpretationPasses=sum(r['qualified'] for r in results), releaseEligible=False,
                limitations=['Manual review of synthetic examples is not field qualification.'], results=results)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--responses', type=Path, required=True)
    parser.add_argument('--examples', type=Path, required=True)
    parser.add_argument('--reviews', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    report = review_responses(json.loads(args.responses.read_text(encoding='utf-8')),
        [json.loads(line) for line in args.examples.read_text(encoding='utf-8').splitlines() if line.strip()],
        json.loads(args.reviews.read_text(encoding='utf-8')))
    args.output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k != 'results'}))
