"""Replay a complete domain diagnostic; never equate schema/evidence checks with prose truth."""
from collections import defaultdict
import argparse
import hashlib
import json
from pathlib import Path
import statistics
from blue_team_contracts import validate,check_shape,schema
from grounded_contract import parse,canonical


def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()


def diagnostics(text,record):
    try:value=parse(text);canonical(value)
    except (ValueError,TypeError):return dict(parseValid=False,shapeValid=False,fieldChecks={})
    try:check_shape(value,schema(record));shape=True
    except (ValueError,TypeError,KeyError):shape=False
    context=parse(record['messages'][1]['content'].split('\n',1)[1]);checks={}
    if not isinstance(value,dict):return dict(parseValid=True,shapeValid=False,fieldChecks={})
    def check(name,key,expected):checks[name]=key in value and canonical(value[key])==canonical(expected)
    if record['task']=='REVIEW_DEFENSIVE_EVIDENCE':
        check('typedFacts','observedFacts',context['facts']);check('referenceListEcho','evidenceReferences',context['evidenceReferences'])
        allowed=set(context['evidenceReferences'])|{f['id'] for f in context['sequenceObservations']}
        refs=value.get('evidenceReferences')
        checks['referencesGroundedInSuppliedEvidence']=isinstance(refs,list) and bool(refs) and all(isinstance(r,str) and r in allowed for r in refs)
        check('suppliedReviewPosture','reviewPosture',context['reportingPolicySignal']['posture'])
    elif record['task']=='ANSWER_SOURCED_DEFENSIVE_QUESTION':
        for key in ('sourceId','pdfPage','sourceSha256','scope'):check(key,key,context[key])
    elif record['task']=='EXPLAIN_BLE_OBSERVATIONS':check('sourceReference','evidenceReference',context['reference']);check('incidentVerdict','incidentVerdict','NOT_ESTABLISHED')
    elif record['task']=='EXPLAIN_DEFENSIVE_CONCEPT':check('conceptIdentity','conceptId',context['ontologyEntry']['id'])
    return dict(parseValid=True,shapeValid=shape,fieldChecks=checks)


def review(examples,run):
    records=[json.loads(x) for x in examples.read_text(encoding='utf-8').splitlines()]
    rows=json.loads((run/'responses.json').read_text());report=json.loads((run/'report.json').read_text())
    if report['examplesSha256']!=digest(examples):raise ValueError('Changed examples')
    if [r['id'] for r in rows]!=[r['id'] for r in records] or report['examples']!=len(records):raise ValueError('Incomplete or foreign responses')
    if report['adapter'] is not None:raise ValueError('Diagnostic expected an unmodified base model')
    for name in ('blue_team_contracts.py','grounded_contract.py'):
        if digest(Path(__file__).with_name(name))!=report['experimentSourceSha256'][name]:raise ValueError('Replay checker changed')
    by_task=defaultdict(lambda:dict(examples=0,structureAndEvidencePasses=0,seconds=[],outputTokens=[],tokenLimitHits=0))
    by_role=defaultdict(lambda:dict(examples=0,structureAndEvidencePasses=0));failures=[];details=[];field_totals=defaultdict(lambda:dict(assessed=0,passed=0))
    for record,row in zip(records,rows):
        try:validate(row['output'],record);valid=True
        except (ValueError,TypeError,KeyError):valid=False
        if valid!=row['schemaAndReferencesValid'] or valid!=bool(row.get('domainStructureAndEvidenceValid')):raise ValueError('Saved validity disagrees with replay')
        detail=dict(id=row['id'],**diagnostics(row['output'],record));details.append(detail)
        for name,passed in detail['fieldChecks'].items():field_totals[name]['assessed']+=1;field_totals[name]['passed']+=passed
        task=by_task[record['task']];role=by_role[record['evaluationRole']]
        for group in (task,role):group['examples']+=1;group['structureAndEvidencePasses']+=valid
        task['seconds'].append(row['seconds']);task['outputTokens'].append(row['outputTokens']);task['tokenLimitHits']+=row['outputTokens']>=report['maxOutputTokens']
        if not valid:failures.append(dict(id=row['id'],task=record['task'],role=record['evaluationRole'],error=row.get('error'),outputSha256=hashlib.sha256(row['output'].encode()).hexdigest()))
    for task in by_task.values():
        task['medianCompleteAnswerSeconds']=statistics.median(task.pop('seconds'))
        task['medianOutputTokens']=statistics.median(task.pop('outputTokens'))
    result=dict(examples=len(rows),structureAndEvidencePasses=sum(g['structureAndEvidencePasses'] for g in by_task.values()),byTask=dict(by_task),byRole=dict(by_role),
        medianCompleteAnswerSeconds=statistics.median(r['seconds'] for r in rows),maxCompleteAnswerSeconds=max(r['seconds'] for r in rows),
        answersOver90Seconds=sum(r['seconds']>90 for r in rows),tokenLimitHits=sum(r['outputTokens']>=report['maxOutputTokens'] for r in rows),
        loadSeconds=report['loadSeconds'],peakGpuAllocatedBytes=report['peakGpuAllocatedBytes'],examplesSha256=report['examplesSha256'],responsesSha256=digest(run/'responses.json'),
        failures=failures,proseSemanticAccuracyMeasured=False,modelTrainingPerformed=False,releaseEligible=False,
        fieldDiagnostics=details,fieldTotals=dict(field_totals),
        limitations=['16 authored diagnostic examples, not full domain, independent expert/user, or prospective field qualification.',
        'Complete-answer desktop timings are not first-token or Android timings.',
        'Training-family, validation-family and reserved-family results are separate.','Strict typed-fact agreement does not establish correct free prose.'])
    if result['structureAndEvidencePasses']!=report['domainStructureAndEvidencePasses']:raise ValueError('Summary total differs')
    return result


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--examples',type=Path,required=True);p.add_argument('--run',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    if a.output.exists():raise ValueError('Preserve existing review')
    result=review(a.examples,a.run);a.output.write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='failures'},indent=2))
