"""Source-bound quantity proposals plus exact rational arithmetic; no model arithmetic."""
import re
from fractions import Fraction
from grounded_contract import parse,canonical
from intent_curriculum_audit import WORDS,expected as audited_expected

UNITS={'microsecond':Fraction(1,1000),'microseconds':Fraction(1,1000),'us':Fraction(1,1000),
       'millisecond':1,'milliseconds':1,'ms':1,'second':1000,'seconds':1000,'s':1000,'sec':1000,
       'minute':60000,'minutes':60000,'min':60000,'hour':3600000,'hours':3600000,'h':3600000}


def duration(text):
    text=text.lower().strip().replace('µ','u')
    text=re.sub(r'(?<=[a-z0-9])-(?=[a-z])',' ',text).replace(' of a ',' ')
    match=re.fullmatch(r'(.+?)\s*(microseconds?|milliseconds?|seconds?|minutes?|hours?|ms|sec|min|us|s|h)',text)
    if not match:return None
    amount,unit=match.groups();amount=WORDS.get(amount.strip(),amount.strip())
    try:
        value=Fraction(amount)*UNITS[unit]
        return int(value) if value.denominator==1 and 1<=value<=3600000 else None
    except (ValueError,ZeroDivisionError):return None


def literal_pattern(text):return re.compile(r'(?<![\w./-])'+re.escape(text)+r'(?![\w/-]|\.[0-9])',re.IGNORECASE)


def clarify(reason):return dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS'),reason


def normalize(text,prompt):
    root=parse(text)
    if not isinstance(root,dict) or type(root.get('schemaVersion')) is not int or root['schemaVersion']!=1:raise ValueError('Invalid quantity envelope')
    if root.get('status') in ('CLARIFY','UNSUPPORTED'):
        if set(root)!={'schemaVersion','status','reason'}:raise ValueError('Invalid nondraft fields')
        if root['status']=='CLARIFY':
            if root['reason']!='AMBIGUOUS_SETTINGS':raise ValueError('Invalid clarification reason')
            return clarify('MODEL_CLARIFICATION')
        if root['reason']!='NO_SUPPORTED_ATOM':raise ValueError('Invalid unsupported reason')
        return root,'MODEL_UNSUPPORTED'
    if root.get('status')!='PROPOSE' or set(root)!={'schemaVersion','status','followOnText','episodeGapText','minimumText','scope'}:raise ValueError('Invalid quantity proposal fields')
    if root['scope'] not in ('DEVICE_AP_PAIR','RECORDING'):raise ValueError('Invalid count scope')
    literals={}
    for key in ('followOnText','episodeGapText','minimumText'):
        value=root[key]
        if value is not None:
            if not isinstance(value,str) or not value.strip() or len(value)>80:raise ValueError('Quantity must be quoted original text or null')
            value=value.strip()
            if not literal_pattern(value).search(prompt):raise ValueError('Proposed quantity is absent from the request')
        literals[key]=value
    if literals['minimumText'] is not None:
        times=[m.span() for key in ('followOnText','episodeGapText') if literals[key] is not None for m in literal_pattern(literals[key]).finditer(prompt)]
        if not any(not any(m.start()>=a and m.end()<=b for a,b in times) for m in literal_pattern(literals['minimumText']).finditer(prompt)):
            raise ValueError('Episode count was borrowed from a time quantity')
    remaining=prompt
    for literal in sorted(set(v for v in literals.values() if v is not None),key=len,reverse=True):remaining=literal_pattern(literal).sub(' ',remaining)
    remaining=re.sub(r'\b802\.11[a-z0-9]*\b',' ',remaining,flags=re.IGNORECASE)
    if re.search(r'(?<!\w)[0-9]',remaining) or re.search(r'\b(two|three|four|five|six|seven|eight|nine|ten|fifteen|thirty|sixty|quarter|half)\b',remaining,re.IGNORECASE) or re.search(r'\bone[ -]+(microseconds?|milliseconds?|seconds?|minutes?|hours?)\b',remaining,re.IGNORECASE):
        return clarify('UNASSIGNED_QUANTITY')
    f=30000 if literals['followOnText'] is None else duration(literals['followOnText'])
    g=3000 if literals['episodeGapText'] is None else duration(literals['episodeGapText'])
    if f is None or g is None:return clarify('UNIT_OR_PRECISION')
    n=1
    if literals['minimumText'] is not None:
        try:
            value=Fraction(WORDS.get(literals['minimumText'].lower(),literals['minimumText']))
            if value.denominator!=1 or not 1<=value<=1000000:raise ValueError()
            n=int(value)
        except (ValueError,ZeroDivisionError):return clarify('EPISODE_COUNT')
    return dict(schemaVersion=1,status='DRAFT',followOnMs=f,episodeGapMs=g,minLinkedEpisodes=n,scope=root['scope']),'EXACT_CONVERSION'


def proposal(spec):
    if spec.get('unsupported'):return dict(schemaVersion=1,status='UNSUPPORTED',reason='NO_SUPPORTED_ATOM')
    if isinstance(spec.get('follow'),list) or isinstance(spec.get('gap'),list):return clarify('MODEL_CLARIFICATION')[0]
    return dict(schemaVersion=1,status='PROPOSE',followOnText=spec.get('follow'),episodeGapText=spec.get('gap'),
        minimumText=str(spec['minimum']) if spec.get('minimum') is not None else None,scope=spec.get('scope') or 'DEVICE_AP_PAIR')


def validate(text,record):
    value,reason=normalize(text,record['messages'][-2]['content'])
    if canonical(value)!=canonical(record['expectedIntent']):raise ValueError('Normalized proposal differs from request meaning')
    return value


def audit_record(record):
    if record.get('task')!='PLAN_ANALYTIC_QUANTITIES' or record.get('labelStatus')!='authored-source-quantity-native-checked-development-only':raise ValueError('Unreviewed quantity task')
    audit=record['auditCurriculum'];prompt=record['messages'][-2]['content']
    if audit['template'].format(**audit['fields'])!=prompt:raise ValueError('Question changed from frozen specification')
    if canonical(audited_expected(audit['spec']))!=canonical(record['expectedIntent']):raise ValueError('Independent numeric audit disagrees')
    if canonical(parse(record['messages'][-1]['content']))!=canonical(proposal(audit['spec'])):raise ValueError('Teacher did not preserve original quantity slots')
    return validate(record['messages'][-1]['content'],record)
