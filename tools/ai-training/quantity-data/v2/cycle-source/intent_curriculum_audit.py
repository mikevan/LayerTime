"""Independent decimal arithmetic and lexical checks for the v2 authored curriculum."""
import re
from decimal import Decimal, InvalidOperation
from analytic_intent import audit_record as audit_intent
from grounded_contract import canonical

WORDS={'a':'1','a quarter':'0.25','quarter':'0.25','a half':'0.5','half':'0.5','half a':'0.5','three quarters':'0.75',
       'one and a half':'1.5','two and a half':'2.5','one':'1','two':'2','three':'3','four':'4',
       'five':'5','six':'6','seven':'7','eight':'8','nine':'9','ten':'10','zero':'0','fifteen':'15','thirty':'30','sixty':'60'}
UNITS={'ms':1,'millisecond':1,'milliseconds':1,'s':1000,'sec':1000,'seconds':1000,'second':1000,
       'minutes':60000,'minute':60000,'min':60000,'hours':3600000,'hour':3600000,'h':3600000,
       'microseconds':Decimal('0.001'),'microsecond':Decimal('0.001'),'us':Decimal('0.001')}


def duration(text):
    text=text.lower().strip()
    # Hyphenated words are quantities, while a leading minus remains a sign.
    text=re.sub(r'(?<=[a-z0-9])-(?=[a-z])',' ',text)
    text=text.replace(' of a ',' ')
    match=re.fullmatch(r'(.+?)\s*(microseconds?|milliseconds?|seconds?|minutes?|hours?|ms|sec|min|us|s|h)',text)
    if not match:return None
    number,unit=match.groups();number=number.strip()
    number=WORDS.get(number,number)
    try:
        if '/' in number:
            numerator,denominator=number.split('/')
            result=Decimal(numerator)*UNITS[unit]/Decimal(denominator)
        else:result=Decimal(number)*UNITS[unit]
        if not result.is_finite() or result!=result.to_integral_value() or not 1<=result<=3600000:return None
        return int(result)
    except (InvalidOperation,ValueError,ZeroDivisionError):return None


def expected(spec):
    if spec.get('unsupported'):
        return dict(schemaVersion=1,status='UNSUPPORTED',reason='NO_SUPPORTED_ATOM')
    values={}
    for field,default in [('follow',30000),('gap',3000)]:
        item=spec.get(field)
        if item is None:values[field]=default;continue
        parts=item if isinstance(item,list) else [item]
        converted=[duration(p) for p in parts]
        if None in converted or len(set(converted))!=1:return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
        values[field]=converted[0]
    minimum=spec.get('minimum')
    if minimum is None:minimum=1
    else:
        try:
            minimum=Decimal(WORDS.get(str(minimum),str(minimum)))
            if not minimum.is_finite() or minimum!=minimum.to_integral_value() or not 1<=minimum<=1000000:raise ValueError()
            minimum=int(minimum)
        except (InvalidOperation,ValueError):return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
    scope=spec.get('scope') or 'DEVICE_AP_PAIR'
    if scope not in ('DEVICE_AP_PAIR','RECORDING'):return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
    return dict(schemaVersion=1,status='DRAFT',followOnMs=values['follow'],episodeGapMs=values['gap'],minLinkedEpisodes=minimum,scope=scope)


def audit_record(record):
    audit_intent(record)
    audit=record['auditCurriculum']
    prompt=record['messages'][-2]['content']
    if audit['template'].format(**audit['fields'])!=prompt:raise ValueError('Prompt differs from frozen template fields')
    for key in ('follow','gap'):
        value=audit['spec'].get(key)
        for surface in value if isinstance(value,list) else [value]:
            if surface is not None and surface not in prompt:raise ValueError('Label quantity absent from request')
    minimum=audit['spec'].get('minimum')
    if minimum is not None and str(minimum) not in prompt:raise ValueError('Label count absent from request')
    if canonical(expected(audit['spec']))!=canonical(record['expectedIntent']):
        raise ValueError('Independent arithmetic/decision audit disagrees with target')
    return record['expectedIntent']
