"""Fail-closed acceptance grading. Prose truth requires explicit hash-bound review."""
import argparse
import hashlib
import json
import math
from pathlib import Path
from compact_domain_contract import validate


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def lines(path):return [json.loads(s) for s in path.read_text().splitlines() if s.strip()]


def score_case(record,response,rubric,review,gates):
    failures=[]
    try:
        validate(response['output'],record);answer=json.loads(response['output'])
    except (ValueError,TypeError,KeyError):
        answer=None;failures.append('structure_or_references')
    if answer:
        if len(answer['headline'])>gates['maxHeadlineCharacters'] or len(answer['headline'].split())>gates['maxHeadlineWords']:
            failures.append('headline_length')
        words=sum(len(answer[field].split()) for field in ('headline','significance','limitations','passiveNextStep'))
        if words>gates['maxExplanationWords']:failures.append('explanation_length')
    seconds=response.get('seconds')
    if type(seconds) not in (int,float) or not math.isfinite(seconds) or seconds<0 or seconds>gates['maxCompleteResponseSecondsDesktop']:
        failures.append('desktop_complete_response_time')
    output_hash=hashlib.sha256(response['output'].encode()).hexdigest()
    if review is None:failures.append('semantic_review_missing')
    else:
        if review.get('outputSha256')!=output_hash:raise ValueError('Semantic review is for different output')
        criteria=review.get('criteria',[])
        if len(criteria)!=len(rubric['criticalCriteria']):raise ValueError('Incomplete critical criterion review')
        for expected,actual in zip(rubric['criticalCriteria'],criteria):
            if actual.get('criterion')!=expected or type(actual.get('passed')) is not bool or len(actual.get('note','').strip())<10:
                raise ValueError('Invalid explicit semantic judgement')
            if not actual['passed']:failures.append('critical: '+expected)
    return dict(id=record['id'],scenario=rubric['scenario'],passed=not failures,failures=failures,
        seconds=seconds,outputSha256=output_hash,semanticReviewPresent=review is not None)


def score(suite,run,review_path=None):
    protocol=json.loads((suite/'protocol.json').read_text())
    for name,value in protocol['files'].items():
        if digest(suite/name)!=value:raise ValueError('Frozen acceptance suite changed')
    records=lines(suite/'examples.jsonl');rubrics={r['id']:r for r in lines(suite/'rubrics.jsonl')}
    responses=json.loads((run/'responses.json').read_text());report=json.loads((run/'report.json').read_text())
    if report['examplesSha256']!=digest(suite/'examples.jsonl'):raise ValueError('Run used a different suite')
    byid={r['id']:r for r in responses}
    if len(byid)!=len(responses) or set(byid)!={r['id'] for r in records}:raise ValueError('Missing or duplicate response')
    if (report['maxOutputTokens'],report['maxContextTokens'])!=(protocol['maxOutputTokens'],protocol['maxContextTokens']):
        raise ValueError('Run differs from frozen token budgets')
    reviews={}
    if review_path:
        ledger=json.loads(review_path.read_text())
        if ledger['responsesSha256']!=digest(run/'responses.json'):raise ValueError('Review ledger source changed')
        reviews={r['id']:r for r in ledger['reviews']}
        if len(reviews)!=len(ledger['reviews']) or set(reviews)!=set(byid):raise ValueError('Incomplete or duplicate semantic reviews')
    results=[score_case(r,byid[r['id']],rubrics[r['id']],reviews.get(r['id']),protocol['gates']) for r in records]
    return dict(suiteProtocolSha256=digest(suite/'protocol.json'),responsesSha256=digest(run/'responses.json'),
        reportSha256=digest(run/'report.json'),reviewSha256=digest(review_path) if review_path else None,
        examples=len(results),passed=sum(r['passed'] for r in results),developmentCandidateAcceptancePassed=all(r['passed'] for r in results),
        cases=results,androidQualified=False,deploymentEligible=False,
        semanticReviewKind='explicit-agent-or-human-judgements-not-independent-expert-validation',
        limitations=['Authored fixtures with overlapping concepts; not prospective field or user-comprehension qualification.',
            'Semantic judgements are reviewer annotations, not automatic proof of prose truth.',
            'Desktop complete response timing is not first-token, Android or emergency-summary timing.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--suite',type=Path,required=True);parser.add_argument('--run',type=Path,required=True)
    parser.add_argument('--review',type=Path);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    if args.output.exists():raise ValueError('Preserve prior acceptance score')
    result=score(args.suite,args.run,args.review);args.output.write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
