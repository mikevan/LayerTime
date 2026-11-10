"""Independently compare compact candidate evidence to frozen v2 source records.

This checks transformation fidelity, not the truth of generated prose or labels.
"""
import hashlib
import json
from pathlib import Path
from collections import Counter
from compact_domain_contract import validate
from grounded_contract import canonical


def rows(path):
    return [json.loads(s) for s in path.read_text(encoding='utf-8').splitlines() if s.strip()]


def audit_record(record, parent):
    assert record['parentCanonicalSha256']==hashlib.sha256(canonical(parent).encode()).hexdigest(), 'Parent changed'
    assert not record['trainingApproved'], 'Candidate was silently admitted'
    assert (record['id'],record['split'],record['family'])==(parent['id'],parent['split'],parent['family'])
    validate(record['messages'][-1]['content'],record)
    context=json.loads(record['messages'][1]['content'].split('\n',1)[1])
    facts={f['factId']:f['value'] for f in context['facts']}
    original=json.loads(parent['messages'][1]['content'].split('\n',1)[1])
    if parent['task']=='REVIEW_DEFENSIVE_EVIDENCE':
        assert facts['review_policy']==original['reportingPolicySignal'], 'Policy changed'
        if 'event_sequence' in facts:
            expected=original['sequenceObservations']; actual=facts['event_sequence']
            assert len(expected)==len(actual)
            actors=record['sourceActorMap']; events=record['sourceEventMap']
            for old,new in zip(expected,actual):
                assert new==dict(eventRef=events[old['id']],kind=old['kind'],targetRef=actors[old['target']],
                    networkRef=actors[old['network']],observedTimeMs=old['timeMs'],observedSenderRef=None), 'Event changed'
            summary=facts['capture_summary']; source=original['facts']
            for key in ('capturedRecords','duplicateRecords','uniqueDisconnectRequests','observedExchanges','linkedExchanges'):
                assert summary[key]==source[key], 'Count changed: '+key
            assert summary['identityGroups']==[dict(targetRef=actors[g['target']],networkRef=actors[g['network']],
                disconnectRequests=len(g['disconnectRequestIds']),exchangeObservations=len(g['exchangeIds'])) for g in source['identityGroups']]
            assert summary['observedLinkedDelaysMs']==[link['delayMs'] for link in source['links']]
            times=[o['timeMs'] for o in expected]
            span=max(times)-min(times) if times and all(t is not None for t in times) else None
            assert summary['observedSequenceSpanMs']==span, 'Observed span changed'
            assert facts['capture_context']['configuration']==dict(configuredFollowOnWindowMs=source['followOnWindowMs'])
            assert facts['capture_context']['operatorContext']==original['providedContext']
        else:
            assert facts['capture_summary']==dict(observations=original['providedContext']), 'Non-WiFi context changed'
            assert 'configuration' not in facts['capture_context']
    elif parent['task']=='EXPLAIN_BLE_OBSERVATIONS':
        assert facts['capture_summary']==original, 'BLE features changed'
    elif parent['task']=='EXPLAIN_DEFENSIVE_CONCEPT':
        assert facts['capture_context']==dict(ontologyEntry=original['ontologyEntry'])
    elif parent['task']=='ANSWER_SOURCED_DEFENSIVE_QUESTION':
        assert facts['capture_context']==original
    else:raise AssertionError('Unexpected source task')
    # Training/source review status is outside the model-visible question and evidence.
    user=record['messages'][1]['content']
    for forbidden in ('trainingApproved','labelStatus','sourceActorMap','parentCanonicalSha256'):
        assert forbidden not in user, 'Review metadata leaked: '+forbidden


def audit(root):
    folder=root/'domain-data/pilot-v3'; manifest=json.loads((folder/'manifest.json').read_text())
    for name,expected in manifest['files'].items():
        assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==expected
    parents={r['id']:r for r in rows(root/'domain-data/v2/candidates.jsonl')}
    candidates=rows(folder/'candidates.jsonl'); byid={r['id']:r for r in candidates}
    assert len(byid)==len(candidates)==manifest['candidateExamples']
    for record in candidates:audit_record(record,parents[record['id']])
    selected=rows(folder/'proposed-pilot.jsonl')
    for record in selected:
        assert record==byid[record['id']] and record['split']=='train'
    expected={r['id'] for r in rows(root/'domain-data/v2/proposed-pilot-selection.jsonl')}
    assert {r['id'] for r in selected}==expected and len(selected)==manifest['pilotExamples']
    families={}
    for record in candidates:
        previous=families.setdefault((record['task'],record['family']),record['split'])
        assert previous==record['split'], 'Family/session split leakage'
    return dict(candidateEvidenceTransformationsChecked=len(candidates),pilotMembershipChecked=len(selected),
        taskCounts=dict(Counter(r['task'] for r in selected)),structureAndSourceFidelityPassed=True,
        proseAccuracyMeasured=False,independentExpertReview=False,trainingPerformed=False,
        manifestSha256=hashlib.sha256((folder/'manifest.json').read_bytes()).hexdigest())


if __name__=='__main__':
    root=Path(__file__).parent; result=audit(root)
    (root/'domain-data/pilot-v3/source-fidelity-audit.json').write_text(json.dumps(result,indent=2))
    print(json.dumps(result,indent=2))
