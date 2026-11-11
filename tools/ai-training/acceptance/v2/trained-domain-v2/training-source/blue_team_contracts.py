"""Portable JSON task contracts plus evidence checks; prose still needs semantic review."""
import json
from grounded_contract import parse,canonical

S={'type':'string','minLength':1};N={'type':'integer','minimum':0};STRINGS={'type':'array','items':S}
POSTURES=['PROMPT_SEQUENCE_REVIEW','REVIEW_DISCONNECT_PATTERN','CHECK_CAPTURE_HEALTH','CHECK_CROSS_RADIO_ALIGNMENT','ASK_FOR_TIME_UNIT','RECONCILE_EVIDENCE','REVIEW_CONTEXT_AND_BASELINE']


def obj(properties):return dict(type='object',properties=properties,required=list(properties),additionalProperties=False)


def schema(record):
    task=record['task']
    if task=='EXPLAIN_DEFENSIVE_CONCEPT':return obj(dict(conceptId=S,definition=S,definitionKind={'const':'project-authored-scoped-definition'}))
    if task=='ANSWER_SOURCED_DEFENSIVE_QUESTION':return obj(dict(answer=S,sourceId=S,pdfPage={'type':'integer','minimum':1},sourceSha256=S,scope=S))
    if task=='EXPLAIN_BLE_OBSERVATIONS':return obj(dict(schemaVersion={'const':1},explanation=S,
        evidenceReference=obj(dict(sourceId=S,sessionId=S,featureRow=N,captureSha256=S)),incidentVerdict={'const':'NOT_ESTABLISHED'},scope=S))
    if task=='PROPOSE_PASSIVE_ANALYTIC_RECIPE':return obj(dict(schemaVersion={'const':1},kind={'const':'passive-training-recipe-proposal-not-app-workflow'},
        executable={'const':False},goal=S,operators={'type':'array','items':{'enum':['assess_visibility','filter','group_by','order_by_time','temporal_join','aggregate','summarize','compare_baseline','explain']},'minItems':1},
        linkIdentity={'enum':['DEVICE_AP_PAIR',None]},countScope={'enum':['DEVICE_AP_PAIR','RECORDING']},followOnWindowMs={'type':['integer','null']},limitations=S))
    if task=='REVIEW_DEFENSIVE_EVIDENCE':
        facts=obj(dict(capturedRecords=N,duplicateRecords=N,uniqueDisconnectRequests=N,observedExchanges=N,linkedExchanges=N,
            links={'type':'array','items':obj(dict(requestId=S,exchangeId=S,delayMs={'type':'integer','minimum':1}))},followOnWindowMs=N,
            identityGroups={'type':'array','items':obj(dict(target=S,network=S,disconnectRequestIds=STRINGS,exchangeIds=STRINGS))}))
        return obj(dict(schemaVersion={'const':1},headline=S,significance=S,observedFacts=facts,uncertainty=S,passiveNextStep=S,
            evidenceReferences=STRINGS,reviewPosture={'enum':POSTURES},postureBasis={'const':'draft-product-review-policy-not-incident-ground-truth'}))
    raise ValueError('Unregistered domain task')


def attach(record):
    contract=schema(record);record['taskContractVersion']='1.0.0';record['outputSchema']=canonical(contract)
    record['messages'][0]['content']+=' Task: '+record['task']+'. Return only JSON matching this output contract: '+canonical(contract)+'. Preserve supplied typed facts and references exactly. A supplied reportingPolicySignal is an application policy, not incident ground truth.'
    return record


def check_shape(value,contract):
    # This small checker implements exactly the JSON Schema features emitted above.
    if 'const' in contract and canonical(value)!=canonical(contract['const']):raise ValueError('Wrong constant')
    if 'enum' in contract and canonical(value) not in [canonical(x) for x in contract['enum']]:raise ValueError('Unknown enum')
    kinds=contract.get('type');kinds=[kinds] if isinstance(kinds,str) else kinds
    if kinds:
        matches={'object':type(value) is dict,'array':type(value) is list,'integer':type(value) is int,
                 'string':type(value) is str,'null':value is None,'boolean':type(value) is bool}
        if not any(matches.get(k,False) for k in kinds):raise ValueError('Wrong JSON type')
    if contract.get('type')=='object':
        if set(value)!=set(contract['required']):raise ValueError('Missing or extra fields')
        for k,v in value.items():check_shape(v,contract['properties'][k])
    if contract.get('type')=='array':
        if len(value)<contract.get('minItems',0):raise ValueError('Empty array')
        for v in value:check_shape(v,contract['items'])
    if type(value) is str and len(value)<contract.get('minLength',0):raise ValueError('Empty string')
    if type(value) is int and value<contract.get('minimum',value):raise ValueError('Number below bound')


def validate(text,record):
    if type(text) is not str or len(text)>65536:raise ValueError('Response exceeds bounded text contract')
    value=parse(text);canonical(value);check_shape(value,schema(record))
    context=parse(record['messages'][1]['content'].split('\n',1)[1]);task=record['task']
    def same(a,b):
        if canonical(a)!=canonical(b):raise ValueError('Evidence or reference disagreement')
    if task=='REVIEW_DEFENSIVE_EVIDENCE':
        same(value['observedFacts'],context['facts']);same(value['evidenceReferences'],context['evidenceReferences'])
        same(value['reviewPosture'],context['reportingPolicySignal']['posture'])
    elif task=='ANSWER_SOURCED_DEFENSIVE_QUESTION':
        for k in ('sourceId','pdfPage','sourceSha256','scope'):same(value[k],context[k])
    elif task=='EXPLAIN_BLE_OBSERVATIONS':same(value['evidenceReference'],context['reference'])
    elif task=='EXPLAIN_DEFENSIVE_CONCEPT':same(value['conceptId'],context['ontologyEntry']['id']);same(value['definitionKind'],context['definitionKind'])
    elif task=='PROPOSE_PASSIVE_ANALYTIC_RECIPE':
        expected=parse(record['messages'][-1]['content'])
        for k in ('operators','linkIdentity','countScope','followOnWindowMs'):same(value[k],expected[k])
    return dict(structureAndEvidenceValid=True,proseSemanticReviewRequired=True,productionExecutionAuthorized=False)
