"""Explicit developmental admission of existing compact teaching data; no acceptance training."""
import copy
import hashlib
import json
from pathlib import Path
from collections import Counter
from compact_domain_contract import validate
from grounded_contract import canonical

TRAIN_SOURCES={'layertime-blue-team-scenarios-v1','ble-ntmmcw6jj8-v2','layertime-blue-team-ontology-v1',
 'layertime-blue-team-ontology-v2','layertime-ble-feature-contract-v1','layertime-evidence-interpretation-principles-v1',
 'layertime-nist-paraphrases-v1','wireshark-passive-field-semantics-v1','layertime-authored-evidence-contrasts-v1',
 'layertime-source-linked-eapol-lessons-v1'}

def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def rows(p):return [json.loads(s) for s in p.read_text(encoding='utf-8').splitlines() if s.strip()]
def prompt_hash(r):return hashlib.sha256(canonical(r['messages'][:-1]).encode()).hexdigest()


def verify(folder):
    manifest=json.loads((folder/'manifest.json').read_text())
    if manifest.get('profile')!='blue-team-domain-development-v1' or manifest.get('releaseEligible') is not False:
        raise ValueError('Wrong development training profile')
    blocked=set(json.loads((folder/'acceptance-exclusions.json').read_text())['promptSha256'])
    if digest(folder/'acceptance-exclusions.json')!=manifest['acceptanceExclusionsSha256']:raise ValueError('Acceptance exclusions changed')
    seen=set();groups={};prompts={}
    for split,info in manifest['splits'].items():
        path=folder/info['file'];values=rows(path)
        if digest(path)!=info['sha256'] or len(values)!=info['examples']:raise ValueError('Corpus integrity failed')
        for r in values:
            if r['id'] in seen or r['split']!=split:raise ValueError('Duplicate identity or split')
            seen.add(r['id']);key=(r['task'],r['family']);fingerprint=prompt_hash(r)
            if fingerprint in blocked or r['id'].startswith('acceptance-') or r.get('evaluationRole')=='frozen-product-acceptance':
                raise ValueError('Acceptance example entered corpus')
            if key in groups and groups[key]!=split:raise ValueError('Family crosses splits')
            if fingerprint in prompts and prompts[fingerprint]!=split:raise ValueError('Prompt crosses splits')
            groups[key]=split;prompts[fingerprint]=split
            validate(r['messages'][-1]['content'],r)
            if split=='train':
                if r['sourceId'] not in TRAIN_SOURCES or r.get('trainingApproved') is not True:raise ValueError('Unadmitted source')
                if r['sourceId']=='layertime-nist-paraphrases-v1' and r.get('sourceReference',{}).get('sourceId')!='nist-sp800-94':
                    raise ValueError('NIST training scope exceeded')
    return manifest


def prepare(root):
    out=root/'domain-data/admitted-development-v1';out.mkdir(exist_ok=False)
    paths=['domain-data/pilot-v3-curriculum/proposed-train.jsonl','domain-data/evidence-contrast-v1/candidates.jsonl',
        'domain-data/eapol-semantic-teaching-v1/candidates.jsonl','domain-data/wireshark-field-teaching-v1/candidates.jsonl']
    train=[];validation=[];test=[]
    for name in paths:
        for source in rows(root/name):
            if source.get('split') not in ('train','unassigned','validation','test'):raise ValueError('Unexpected candidate split')
            r=copy.deepcopy(source);split='train' if r['split']=='unassigned' else r['split']
            r['id']='domain-admitted-'+r['id'];r['split']=split
            r['admission']=dict(purpose='bounded-development-training-only',independentExpertReview=False,
                originalFile=name,originalCanonicalSha256=hashlib.sha256(canonical(source).encode()).hexdigest())
            r['trainingApproved']=split=='train';r['releaseEligible']=False
            {'train':train,'validation':validation,'test':test}[split].append(r)
    # Whole held-out original families/sessions; one representative per group for bounded loss evaluation.
    held_groups=set()
    for source in rows(root/'domain-data/pilot-v3/candidates.jsonl'):
        if source['split'] not in ('validation','test'):continue
        group=(source['task'],source['family'])
        if group in held_groups:continue
        held_groups.add(group);r=copy.deepcopy(source);r['id']='heldout-'+r['id']
        {'validation':validation,'test':test}[r['split']].append(r)
    exclusions=[]
    for version in ('v1','v2'):exclusions.extend(prompt_hash(r) for r in rows(root/f'acceptance/{version}/examples.jsonl'))
    (out/'acceptance-exclusions.json').write_text(json.dumps(dict(promptSha256=sorted(set(exclusions)),
        policy='No acceptance inputs, reference answers, model outputs or reviews admitted to training.')))
    manifest=dict(schemaVersion=1,profile='blue-team-domain-development-v1',releaseEligible=False,independentExpertReview=False,
        admission='Agent source/contract/family review for a bounded developmental run; not product qualification',
        sourceFiles={name:digest(root/name) for name in paths},splits={},acceptanceExclusionsSha256=digest(out/'acceptance-exclusions.json'),
        sourcePermissions={'projectAuthored':'Project development use; original provenance retained',
            'ble-ntmmcw6jj8-v2':'CC BY 4.0; three training sessions in one testbed, descriptive labels only',
            'nist':'Selected historical SP 800-94 paraphrases only; other documents excluded from optimizer input',
            'wireshark':'Pinned GPL-2.0-or-later decoder facts, attributed excerpts and original licence retained in source artifacts'},
        limitations=['Small source-limited first domain adapter, not completion of substantial domain training.',
            'Repetitive examples retained for one fixed pass; counts are not independent incidents.',
            'Held-out examples share concepts and source context; validation is not field qualification.'])
    for split,values in [('train',train),('validation',validation),('test',test)]:
        path=out/(split+'.jsonl');path.write_text(''.join(canonical(r)+'\n' for r in values),encoding='utf-8')
        manifest['splits'][split]=dict(file=path.name,sha256=digest(path),examples=len(values),
            families=sorted({r['family'] for r in values}),tasks=dict(Counter(r['task'] for r in values)))
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));verify(out)
    return {s:v['examples'] for s,v in manifest['splits'].items()}

if __name__=='__main__':print(json.dumps(prepare(Path(__file__).parent),indent=2))
