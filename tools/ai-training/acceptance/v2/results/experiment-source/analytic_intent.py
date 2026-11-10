"""Typed request interpretation checks. Gold labels are authored, not field truth."""
import json
from grounded_contract import parse, canonical


def check_shape(text):
    value=parse(text)
    if not isinstance(value,dict) or type(value.get('schemaVersion')) is not int or value['schemaVersion']!=1:
        raise ValueError('Invalid analytic intent envelope')
    status=value.get('status')
    if status=='DRAFT':
        if set(value)!={'schemaVersion','status','followOnMs','episodeGapMs','minLinkedEpisodes','scope'}:
            raise ValueError('Unknown or missing intent fields')
        for key,maximum in [('followOnMs',3600000),('episodeGapMs',3600000),('minLinkedEpisodes',1000000)]:
            if type(value[key]) is not int or not 1<=value[key]<=maximum:raise ValueError('Invalid bounded intent setting')
        if value['scope'] not in ('DEVICE_AP_PAIR','RECORDING'):raise ValueError('Invalid count scope')
    elif status in ('CLARIFY','UNSUPPORTED'):
        if set(value)!={'schemaVersion','status','reason'}:raise ValueError('Unexpected nondraft fields')
        reason='AMBIGUOUS_SETTINGS' if status=='CLARIFY' else 'NO_SUPPORTED_ATOM'
        if value['reason']!=reason:raise ValueError('Invalid nondraft reason')
    else:raise ValueError('Invalid intent decision')
    return value


def validate(text,record):
    value=check_shape(text)
    if canonical(value)!=canonical(record['expectedIntent']):raise ValueError('Intent differs from authored request meaning')
    return value


def audit_record(record):
    if record.get('task')!='PLAN_ANALYTIC' or record.get('labelStatus')!='authored-intent-native-compiler-checked-development-only':
        raise ValueError('Unreviewed analytic intent task')
    if record['messages'][-2]['role']!='user':raise ValueError('Missing request')
    return validate(record['messages'][-1]['content'],record)
