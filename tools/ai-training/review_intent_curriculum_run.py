"""Replay complete runs and keep fresh tests separate from known development cases."""
import argparse
import hashlib
import json
from pathlib import Path
from analytic_intent import check_shape,validate,audit_record as audit_intent
from intent_curriculum_audit import audit_record as audit_curriculum


def review(examples,run):
    records=[json.loads(s) for s in examples.read_text(encoding='utf-8').splitlines() if s.strip()]
    responses=run/'responses.json'
    rows=json.loads(responses.read_text(encoding='utf-8'))
    report=json.loads((run/'report.json').read_text(encoding='utf-8'))
    if report['examplesSha256']!=hashlib.sha256(examples.read_bytes()).hexdigest():raise ValueError('Evaluation data changed')
    if [r['id'] for r in rows]!=[r['id'] for r in records] or report['examples']!=len(records):raise ValueError('Incomplete or foreign responses')
    groups={};failures=[]
    for record,row in zip(records,rows):
        role=record['evaluationRole']
        if role=='fresh_test':audit_curriculum(record)
        elif role=='known_development':audit_intent(record)
        else:raise ValueError('Unknown evaluation role')
        try:predicted=check_shape(row['output'])
        except (ValueError,TypeError,KeyError):predicted=None
        try:validate(row['output'],record);valid=True
        except (ValueError,TypeError,KeyError):valid=False
        if valid!=row['schemaAndReferencesValid'] or valid!=bool(row.get('intentAgreementValid')):raise ValueError('Saved validity flag disagrees with replay')
        if bool(predicted)!=bool(row.get('intentShapeValid')):raise ValueError('Saved shape flag disagrees with replay')
        group=groups.setdefault(role,dict(examples=0,shapePasses=0,exactIntentPasses=0,supportedRequests=0,
            supportedCorrect=0,timeSettingsCorrect=0,clarificationRequests=0,clarificationsCorrect=0,
            guessedDraftInsteadOfClarifying=0,unsupportedRequests=0,unsupportedCorrect=0))
        group['examples']+=1;group['shapePasses']+=predicted is not None;group['exactIntentPasses']+=valid
        expected=record['expectedIntent'];status=expected['status']
        if status=='DRAFT':
            group['supportedRequests']+=1;group['supportedCorrect']+=valid
            group['timeSettingsCorrect']+=predicted is not None and predicted['status']=='DRAFT' and all(predicted[k]==expected[k] for k in ('followOnMs','episodeGapMs'))
        elif status=='CLARIFY':
            group['clarificationRequests']+=1;group['clarificationsCorrect']+=valid
            group['guessedDraftInsteadOfClarifying']+=predicted is not None and predicted['status']=='DRAFT'
        else:group['unsupportedRequests']+=1;group['unsupportedCorrect']+=valid
        if not valid:failures.append(dict(id=row['id'],evaluationRole=role,expected=expected,predicted=predicted,error=row.get('error')))
    if sum(g['exactIntentPasses'] for g in groups.values())!=report['intentAgreementPasses']:raise ValueError('Report total disagrees with replay')
    fresh=groups['fresh_test']
    return dict(groups=groups,primaryAuthoredTestGatePassed=fresh['exactIntentPasses']==fresh['examples'],
        examplesSha256=report['examplesSha256'],responsesSha256=hashlib.sha256(responses.read_bytes()).hexdigest(),
        adapterSha256=report.get('adapterSha256'),failures=failures,releaseEligible=False,
        limitations=['Authored controlled requests; no independent user or field qualification.',
                    'Known development cases are not fresh held-out tests.','Native executability does not establish request meaning.'])


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--examples',type=Path,required=True);parser.add_argument('--run',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    if args.output.exists():raise ValueError('Preserve existing reviews')
    result=review(args.examples,args.run)
    args.output.write_text(json.dumps(result,indent=2),encoding='utf-8')
    print(json.dumps({k:v for k,v in result.items() if k!='failures'},indent=2))
