"""Diagnostic explanation interface: trusted facts stay outside the generated answer."""
from blue_team_contracts import obj,check_shape
from grounded_contract import parse,canonical

FACT_IDS=['capture_summary','event_sequence','capture_context','review_policy']


def schema(record=None):
    text=lambda length:dict(type='string',minLength=1,maxLength=length)
    return obj(dict(schemaVersion={'const':1},headline=text(240),significance=text(800),limitations=text(600),
        passiveNextStep=text(500),factReferences={'type':'array','items':{'enum':FACT_IDS},'minItems':1,'uniqueItems':True}))


def validate(text,record):
    if type(text) is not str or len(text)>16384:raise ValueError('Oversize compact response')
    result=parse(text);canonical(result);check_shape(result,schema())
    for key in ('headline','significance','limitations','passiveNextStep'):
        if not result[key].strip() or len(result[key])>schema()['properties'][key]['maxLength']:raise ValueError('Invalid explanation length')
    refs=result['factReferences']
    if len(refs)!=len(set(refs)):raise ValueError('Duplicate fact references')
    supplied=parse(record['messages'][1]['content'].split('\n',1)[1])
    if not set(refs)<={f['factId'] for f in supplied['facts']}:raise ValueError('Invented fact reference')
    return dict(structureAndReferencesValid=True,proseSemanticReviewRequired=True)
