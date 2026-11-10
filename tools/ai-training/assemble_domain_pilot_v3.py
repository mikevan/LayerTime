"""Freeze a proposed compact curriculum, without admitting it to model training."""
import copy
import hashlib
import json
from pathlib import Path
from collections import Counter
from audit_domain_pilot_v3 import audit,rows
from build_domain_pilot_v3 import LEGEND
from compact_domain_contract import schema,validate
from grounded_contract import canonical


def teaching_record(source,kind,definition=False):
    record=copy.deepcopy(source)
    if definition:
        question='Explain this '+('feature definition.' if kind=='feature' else 'evidence interpretation principle.')
        context={k:source[k] for k in ('id','definition','interpretationLimit') if k in source}
        answer=source['definition']; limit=source.get('interpretationLimit','A principle alone does not establish that an incident occurred.')
    else:
        question,body=source['messages'][1]['content'].split('\n',1);context=json.loads(body)
        answer=json.loads(source['messages'][-1]['content'])['answer']
        limit='Apply this answer only to the supplied observations and definition; physical identity, cause and intent need their own evidence.'
    record.update(id='supplement-'+kind+'-'+source['id'],family='supplement-'+kind+'-'+source.get('family',source['id']),
        task='EXPLAIN_FEATURE_OR_EVIDENCE_MEANING',split='train',datasetVersion='blue-team-compact-pilot-v3',
        taskContractVersion='1.0.0',outputSchema=canonical(schema()),trainingApproved=False,releaseEligible=False,
        parentCanonicalSha256=hashlib.sha256(canonical(source).encode()).hexdigest(),
        labelStatus='agent-authored-development-teaching-not-independently-reviewed')
    result=dict(schemaVersion=1,headline='Feature meaning' if kind=='feature' else 'What this evidence supports',
        significance=answer,limitations=limit,passiveNextStep='Keep the original measurements and their definitions available for review.',
        factReferences=['capture_context'])
    record['messages']=[dict(role='system',content=LEGEND.replace('Use one short sentence per text field','Keep each text field brief')),
        dict(role='user',content=question+'\n'+canonical(dict(facts=[dict(factId='capture_context',value=context)]))),
        dict(role='assistant',content=canonical(result))]
    validate(record['messages'][-1]['content'],record)
    return record


def assemble(root):
    fidelity=audit(root); out=root/'domain-data/pilot-v3-curriculum';out.mkdir(exist_ok=False)
    selected=rows(root/'domain-data/pilot-v3/proposed-pilot.jsonl');supplements=[];sources={}
    for folder,kind,definitions in [('ble-feature-knowledge-v1','feature','definitions.jsonl'),
                                   ('evidence-meaning-teaching-v1','evidence','principles.jsonl')]:
        for filename in (definitions,'counterexamples.jsonl'):
            path=root/'domain-data'/folder/filename
            sources[str(path.relative_to(root))]=hashlib.sha256(path.read_bytes()).hexdigest()
            supplements.extend(teaching_record(r,kind,filename==definitions) for r in rows(path))
    combined=selected+supplements
    assert len(combined)==len({r['id'] for r in combined})==982
    assert all(r['split']=='train' and not r['trainingApproved'] for r in combined)
    for name,data in [('supplements.jsonl',supplements),('proposed-train.jsonl',combined)]:
        (out/name).write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in data),encoding='utf-8')
    manifest=dict(status='proposed-development-curriculum-not-admitted',trainingPerformed=False,trainingApproved=False,
        releaseEligible=False,examples=len(combined),supplementExamples=len(supplements),
        sourceFidelityAudit=fidelity,sourceFileSha256=sources,taskCounts=dict(Counter(r['task'] for r in combined)),
        limitations=['57 supplement items comprise 25 definitions/principles and 32 paired questions over 16 toy contexts.',
                    'Counts are teaching items, not independent incidents or environments.',
                    'Source fidelity and schema validation do not establish prose accuracy or independent expert review.',
                    'Reserved families, sessions, real Wi-Fi campaign and new NIST Bluetooth cards are excluded from this training proposal.'],
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2))
    # One example per family/task keeps the human review manageable; the complete JSONL is retained.
    seen=set();review=['# Proposed compact curriculum review','',
        'Development candidates only. No model weights have been updated from this curriculum.',
        'Examples are selected by first occurrence per task/family, not by quality. See the complete JSONL for every item.','']
    for record in combined:
        key=(record['task'],record['family'])
        if key in seen:continue
        seen.add(key);answer=json.loads(record['messages'][-1]['content'])
        review.extend(['## '+record['id'],'','Task: '+record['task']+'; family: '+record['family'],
            '',record['messages'][1]['content'].split('\n',1)[0],'',
            *[label+': '+answer[field] for label,field in [('Headline','headline'),('Meaning','significance'),('Limit','limitations'),('Next step','passiveNextStep')]],''])
    (out/'REVIEW.md').write_text('\n'.join(review),encoding='utf-8')
    return manifest


if __name__=='__main__':print(json.dumps(assemble(Path(__file__).parent),indent=2))
