"""Audit declared Builder ontology annotations; not an English semantic oracle."""
from analytic_quantity import audit_record as audit_quantities

RULES={'link_pair_identity','count_scope_independence','recording_aggregation','pair_aggregation',
    'minimal_time_span','minimal_count_span','quantity_role_independence','explicit_vs_default','passive_evidence_limits'}


def audit_record(record):
    result=audit_quantities(record)
    annotation=record['ontologyAnnotation']
    if annotation['version']!='0.1.0' or not annotation['rules'] or not set(annotation['rules'])<=RULES:raise ValueError('Unknown ontology rule')
    if annotation['eventLinkIdentity']!='DEVICE_AP_PAIR':raise ValueError('Event linking identity changed')
    expected=record['expectedIntent']
    if expected['status']=='DRAFT' and annotation['countScope']!=expected['scope']:raise ValueError('Count-scope annotation disagrees')
    phrase=annotation.get('countScopePhrase')
    if phrase is not None and phrase not in record['messages'][-2]['content']:raise ValueError('Scope phrase is absent')
    for key,value in annotation['quantitySpans'].items():
        if key not in ('followOnText','episodeGapText','minimumText') or record['expectedProposal'].get(key)!=value:raise ValueError('Quantity annotation disagrees')
    return result


def audit_contrasts(records):
    groups={}
    for record in records:
        group=record.get('ontologyAnnotation',{}).get('contrastGroup')
        if group:groups.setdefault(group,[]).append(record)
    for group,pair in groups.items():
        if len(pair)!=2:raise ValueError('A scope contrast needs exactly two examples')
        a,b=pair
        if {a['expectedIntent']['scope'],b['expectedIntent']['scope']}!={'DEVICE_AP_PAIR','RECORDING'}:raise ValueError('Contrast scopes are not distinct')
        for key in ('followOnMs','episodeGapMs','minLinkedEpisodes'):
            if a['expectedIntent'][key]!=b['expectedIntent'][key]:raise ValueError('Scope contrast changed a quantity')
        stems=[]
        for row in pair:
            phrase=row['ontologyAnnotation']['countScopePhrase']
            stems.append(row['messages'][-2]['content'].replace(phrase,'COUNT_SCOPE'))
        if stems[0]!=stems[1]:raise ValueError('Scope contrast changed event-link wording')
    return len(groups)
