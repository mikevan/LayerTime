"""Replay source-extraction responses; score language choices and final intent separately."""
import argparse
import hashlib
import json
from pathlib import Path
from analytic_quantity import audit_record,normalize,validate,parse,canonical


def review(examples,run):
    records=[json.loads(s) for s in examples.read_text(encoding='utf-8').splitlines() if s.strip()]
    rows=json.loads((run/'responses.json').read_text(encoding='utf-8'))
    report=json.loads((run/'report.json').read_text(encoding='utf-8'))
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    if report['examplesSha256']!=digest(examples):raise ValueError('Evaluation data changed')
    if [r['id'] for r in rows]!=[r['id'] for r in records] or report['examples']!=len(records):raise ValueError('Incomplete or foreign responses')
    groups={};failures=[]
    for record,row in zip(records,rows):
        audit_record(record)
        role=record['evaluationRole']
        if role not in ('fresh_test','known_development'):raise ValueError('Unknown evaluation role')
        predicted=None;reason='INVALID_PROPOSAL';raw=False
        try:
            predicted,reason=normalize(row['output'],record['messages'][-2]['content'])
            raw=canonical(parse(row['output']))==canonical(record['expectedProposal'])
        except (ValueError,TypeError,KeyError):pass
        try:validate(row['output'],record);valid=True
        except (ValueError,TypeError,KeyError):valid=False
        if valid!=row['schemaAndReferencesValid'] or valid!=bool(row.get('intentAgreementValid')):raise ValueError('Saved validity disagrees')
        if (predicted is not None)!=bool(row.get('quantityProposalValid')):raise ValueError('Saved normalization disagrees')
        if predicted is not None and (predicted!=row['predictedIntent'] or reason!=row['normalizationReason']):raise ValueError('Saved values disagree')
        g=groups.setdefault(role,dict(examples=0,sourceBoundProposals=0,exactRawProposalPasses=0,exactIntentPasses=0,
            supportedRequests=0,supportedCorrect=0,timeSettingsCorrect=0,clarificationRequests=0,
            clarificationsCorrect=0,guessedDraftInsteadOfClarifying=0,unsupportedRequests=0,unsupportedCorrect=0,normalizationReasons={}))
        g['examples']+=1;g['sourceBoundProposals']+=predicted is not None;g['exactRawProposalPasses']+=raw;g['exactIntentPasses']+=valid
        g['normalizationReasons'][reason]=g['normalizationReasons'].get(reason,0)+1
        status=record['expectedIntent']['status']
        if status=='DRAFT':
            g['supportedRequests']+=1;g['supportedCorrect']+=valid
            g['timeSettingsCorrect']+=predicted is not None and predicted['status']=='DRAFT' and all(predicted[k]==record['expectedIntent'][k] for k in ('followOnMs','episodeGapMs'))
        elif status=='CLARIFY':
            g['clarificationRequests']+=1;g['clarificationsCorrect']+=valid
            g['guessedDraftInsteadOfClarifying']+=predicted is not None and predicted['status']=='DRAFT'
        else:g['unsupportedRequests']+=1;g['unsupportedCorrect']+=valid
        if not valid:failures.append(dict(id=row['id'],evaluationRole=role,expected=record['expectedIntent'],predicted=predicted,reason=reason,output=row['output']))
    if sum(g['exactIntentPasses'] for g in groups.values())!=report['intentAgreementPasses']:raise ValueError('Report total disagrees')
    fresh=groups['fresh_test']
    return dict(groups=groups,primaryAuthoredTestGatePassed=fresh['exactIntentPasses']==fresh['examples'],
        examplesSha256=digest(examples),responsesSha256=digest(run/'responses.json'),adapterSha256=report['adapterSha256'],
        failures=failures,releaseEligible=False,limitations=['Authored synthetic requests, not independent users or field qualification.',
        'Arithmetic is performed by an exact conversion atom, not learned by the model.',
        'Raw extraction can differ while normalized meaning agrees; both are reported.'])


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--examples',type=Path,required=True);p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args()
    if a.output.exists():raise ValueError('Preserve existing reviews')
    result=review(a.examples,a.run);a.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='failures'},indent=2))
