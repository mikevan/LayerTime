"""Local frozen-model evidence baseline. Does not train on or approve held-out answers."""
import argparse
import json
import time
import hashlib
from pathlib import Path

def with_evidence_facts(messages):
    from evidence_facts import summarize
    result=[]
    for message in messages:
        updated=dict(message)
        if message['role']=='user' and message['content'].startswith('RECORDING DATA\n'):
            data=json.loads(message['content'].split('\n',1)[1])
            data['derivedEvidenceFacts']=summarize(data['observations'])
            updated['content']='RECORDING DATA\n'+json.dumps(data)
        result.append(updated)
    return result


def evaluation_messages(record,evidence_facts=False,policy_guided=False,merge_system_messages=False):
    if not record['messages'] or record['messages'][-1]['role']!='assistant':
        raise ValueError('Evaluation examples require a separated assistant target or placeholder')
    if record.get('datasetVersion','').startswith('blue-team-compact-'):
        from compact_domain_contract import schema
        from grounded_contract import canonical
        if record.get('taskContractVersion')!='1.0.0' or canonical(json.loads(record['outputSchema']))!=canonical(schema(record)):
            raise ValueError('Compact task contract mismatch')
        if canonical(schema(record)) not in record['messages'][0]['content']:raise ValueError('Compact schema absent from instructions')
        context=[dict(m) for m in record['messages'][:-1]]
    elif record.get('datasetVersion','').startswith('blue-team-domain-'):
        from blue_team_contracts import schema
        from grounded_contract import canonical
        if record.get('taskContractVersion')!='1.0.0' or canonical(json.loads(record['outputSchema']))!=canonical(schema(record)):
            raise ValueError('Domain task contract mismatch')
        if canonical(schema(record)) not in record['messages'][0]['content']:raise ValueError('Domain schema absent from model instructions')
        # Domain training messages already contain the complete contract. Preserve
        # that interface instead of adding a second copy only during evaluation.
        context=[dict(m) for m in record['messages'][:-1]]
    else:context=[{'role':'system','content':'Return only a JSON object matching this schema: '+record['outputSchema']}]+record['messages'][:-1]
    if evidence_facts or policy_guided:context=with_evidence_facts(context)
    if policy_guided:
        context.insert(1,dict(role='system',content='The supplied derivedEvidenceFacts contain deterministic analytic results. '
            'For every group whose disconnectBurstPolicy.thresholdMet is true, report the significant repeated disconnect-request pattern with its supporting frame IDs. '
            'These configured thresholds are warning criteria, not proof of an attack. Explain client/network matches, timing and coverage limits from the supplied data. '
            'Do not invent completed disconnects, reconnects, compromised passwords, or physical sender identity. '
            'Do not report a disruption burst where no valid group meets the supplied threshold.'))
    if merge_system_messages:
        leading=[]
        while context and context[0]['role']=='system':leading.append(context.pop(0)['content'])
        if leading:context.insert(0,dict(role='system',content='\n\n'.join(leading)))
    return [dict(m,content=m['content'].replace('<|','< |')) for m in context]


def render_evaluation_prompt(tokenizer,record,evidence_facts=False,policy_guided=False,merge_system_messages=False):
    context=evaluation_messages(record,evidence_facts,policy_guided,merge_system_messages)
    prompt=tokenizer.apply_chat_template(context,tokenize=False,add_generation_prompt=True,enable_thinking=False)
    for message in context:
        if message['content'].strip() and message['content'] not in prompt:
            raise ValueError('Chat template omitted supplied '+message['role']+' content; adapt roles explicitly before evaluating')
    return prompt

def supplied_ids(record):
    if 'suppliedEvidenceIds' in record:return set(record['suppliedEvidenceIds'])
    for message in record.get('messages',[]):
        if message['role']=='user' and message['content'].startswith('RECORDING DATA\n'):
            data=json.loads(message['content'].split('\n',1)[1])
            allowed={row['frameId'] for row in data['observations']}
            for network in data.get('networkContext',[]):allowed.update(network['frameIds'])
            for episode in data.get('relatedEpisodes',[]):
                allowed.update(episode.get('disconnectFrames',[]));allowed.update(episode.get('eapolFrames',[]))
                for comparison in episode.get('networkComparison',[]):allowed.update(comparison['frames'])
            if not allowed.issubset(record['evidenceIds']):raise ValueError('Supplied evidence is absent from capture index')
            return allowed
    raise ValueError('No recording evidence supplied')

def validate_assessment(text,record):
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('Duplicate JSON key')
            result[key]=value
        return result
    result=json.loads(text,object_pairs_hook=unique)
    if not isinstance(result,dict) or set(result)!={'schemaVersion','issues'} or type(result['schemaVersion']) is not int or result['schemaVersion']!=1:
        raise ValueError('Invalid assessment envelope')
    if not isinstance(result['issues'],list) or len(result['issues'])>32:raise ValueError('Invalid issues')
    supplied=supplied_ids(record)
    for issue in result['issues']:
        if not isinstance(issue,dict) or set(issue)!={'headline','explanation','frameIds'}:raise ValueError('Invalid issue fields')
        if not isinstance(issue['headline'],str) or not 1<=len(issue['headline'])<=160:raise ValueError('Invalid headline')
        if not isinstance(issue['explanation'],str) or not 1<=len(issue['explanation'])<=2000:raise ValueError('Invalid explanation')
        if not isinstance(issue['frameIds'],list) or not 1<=len(issue['frameIds'])<=256:raise ValueError('Invalid references')
        if any(type(i) is not int or i not in supplied for i in issue['frameIds']):raise ValueError('Fabricated reference')
    return result


def validate_explanation(text,record):
    def unique(pairs):
        result={}
        for key,value in pairs:
            if key in result:raise ValueError('Duplicate JSON key')
            result[key]=value
        return result
    result=json.loads(text,object_pairs_hook=unique)
    if not isinstance(result,dict) or set(result)!={'schemaVersion','answer','references'}:
        raise ValueError('Invalid explanation fields')
    if type(result['schemaVersion']) is not int or result['schemaVersion']!=1:raise ValueError('Invalid schema version')
    if not isinstance(result['answer'],str) or not result['answer'].strip() or len(result['answer'])>2000:
        raise ValueError('Invalid explanation answer')
    expected=json.loads(record['messages'][-1]['content'])['references']
    refs=result['references']
    if not isinstance(refs,list) or not refs or len(refs)>16:raise ValueError('Invalid explanation references')
    for ref in refs:
        if not isinstance(ref,dict) or set(ref)!={'sourceId','pdfPage'} or type(ref['pdfPage']) is not int or ref not in expected:
            raise ValueError('Unsupported explanation reference')
    return result

def main():
    parser=argparse.ArgumentParser();parser.add_argument('--model',type=Path,required=True)
    parser.add_argument('--examples',type=Path,required=True);parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--limit',type=int,default=5)
    parser.add_argument('--evidence-facts',action='store_true',help='Supply deterministic page-level evidence summaries')
    parser.add_argument('--adapter',type=Path,help='Evaluate a local development LoRA adapter')
    parser.add_argument('--policy-guided',action='store_true',help='Explain explicit deterministic burst findings rather than rediscovering the threshold')
    parser.add_argument('--max-output-tokens',type=int,default=512)
    parser.add_argument('--max-context-tokens',type=int,default=8192)
    parser.add_argument('--merge-system-messages',action='store_true',help='Preserve leading system instructions in templates supporting one system message')
    args=parser.parse_args()
    if not 32<=args.max_output_tokens<=4096 or not 512<=args.max_context_tokens<=32768:
        parser.error('Invalid local evaluation token budget')
    args.output.mkdir(parents=True,exist_ok=False)
    snapshots=args.output/'experiment-source';snapshots.mkdir()
    source_hashes={}
    for name in ['evaluate_local_model.py','evidence_facts.py','grounded_contract.py','analytic_intent.py','analytic_quantity.py','intent_curriculum_audit.py','blue_team_contracts.py','compact_domain_contract.py']:
        raw=Path(__file__).with_name(name).read_bytes();(snapshots/name).write_bytes(raw)
        source_hashes[name]=hashlib.sha256(raw).hexdigest()
    import torch
    from transformers import AutoTokenizer,AutoModelForCausalLM,BitsAndBytesConfig
    from transformers.integrations import sdpa_attention
    torch.backends.cuda.enable_flash_sdp(False);torch.backends.cuda.enable_math_sdp(False)
    torch.backends.cuda.enable_mem_efficient_sdp(True)
    sdpa_attention.use_gqa_in_sdpa=lambda attention_mask,key:False
    tokenizer=AutoTokenizer.from_pretrained(args.model,local_files_only=True)
    quantization=BitsAndBytesConfig(load_in_4bit=True,bnb_4bit_quant_type='nf4',
        bnb_4bit_use_double_quant=True,bnb_4bit_compute_dtype=torch.float16)
    start=time.monotonic()
    model=AutoModelForCausalLM.from_pretrained(args.model,local_files_only=True,quantization_config=quantization,
        device_map={'':0},dtype=torch.float16,attn_implementation='sdpa').eval()
    adapter_sha=None
    if args.adapter:
        from peft import PeftModel
        adapter_file=args.adapter/'adapter_model.safetensors'
        adapter_sha=hashlib.sha256(adapter_file.read_bytes()).hexdigest()
        model=PeftModel.from_pretrained(model,args.adapter,local_files_only=True,is_trainable=False).eval()
    if args.max_context_tokens>model.config.max_position_embeddings:
        raise ValueError('Requested evaluation context exceeds model configuration')
    load=time.monotonic()-start;rows=[];precomputed_fact_examples=0
    records=[json.loads(line) for line in args.examples.read_text(encoding='utf-8').splitlines() if line.strip()][:args.limit]
    for record in records:
        if any(m['role']=='user' and m['content'].startswith('RECORDING DATA\n') and
               isinstance(json.loads(m['content'].split('\n',1)[1]).get('derivedEvidenceFacts'),dict)
               for m in record['messages'][:-1]):precomputed_fact_examples+=1
        prompt=render_evaluation_prompt(tokenizer,record,args.evidence_facts,args.policy_guided,args.merge_system_messages)
        inputs=tokenizer(prompt,return_tensors='pt',add_special_tokens=False).to('cuda')
        if inputs.input_ids.shape[1]+args.max_output_tokens>args.max_context_tokens:raise ValueError('Evidence exceeds evaluation context; never truncate')
        before=time.monotonic()
        with torch.inference_mode():generated=model.generate(**inputs,max_new_tokens=args.max_output_tokens,do_sample=False,
            pad_token_id=tokenizer.eos_token_id)
        output=tokenizer.decode(generated[0,inputs.input_ids.shape[1]:],skip_special_tokens=True)
        result={'id':record['id'],'inputTokens':int(inputs.input_ids.shape[1]),
            'outputTokens':int(generated.shape[1]-inputs.input_ids.shape[1]),'seconds':time.monotonic()-before,'output':output}
        try:
            if record.get('datasetVersion','').startswith('blue-team-compact-'):
                from compact_domain_contract import validate
                parsed=validate(output,record)
                result['compactStructureAndReferencesValid']=True
                result['proseSemanticReviewRequired']=True
            elif record.get('datasetVersion','').startswith('blue-team-domain-'):
                from blue_team_contracts import validate
                parsed=validate(output,record)
                result['domainStructureAndEvidenceValid']=True
                result['proseSemanticReviewRequired']=True
            elif record.get('task')=='PLAN_ANALYTIC_QUANTITIES':
                from analytic_quantity import normalize,validate
                result['predictedIntent'],result['normalizationReason']=normalize(output,record['messages'][-2]['content'])
                result['quantityProposalValid']=True
                parsed=validate(output,record)
                result['intentAgreementValid']=True
            elif record.get('task')=='PLAN_ANALYTIC':
                from analytic_intent import check_shape,validate
                result['predictedIntent']=check_shape(output)
                result['intentShapeValid']=True
                parsed=validate(output,record)
                result['intentAgreementValid']=True
            elif record.get('task')=='REVIEW_GROUNDED_CAPTURE':
                from grounded_contract import validate
                parsed=validate(output,record)
            else:parsed=validate_explanation(output,record) if record.get('task')=='EXPLAIN_DEFENSIVE_CONCEPT' else validate_assessment(output,record)
            result.update(schemaAndReferencesValid=True)
            if not record.get('datasetVersion','').startswith(('blue-team-domain-','blue-team-compact-')):result['issueCount']=len(parsed.get('issues',[]))
            if record.get('task')=='REVIEW_GROUNDED_CAPTURE':result['validatedFactCount']=len(parsed['assessments'])
        except (ValueError,TypeError,KeyError) as exc:result.update(schemaAndReferencesValid=False,error=str(exc))
        rows.append(result)
        (args.output/'responses.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
        print(json.dumps({k:v for k,v in result.items() if k!='output'}),flush=True)
    report={'model':str(args.model),'adapter':str(args.adapter) if args.adapter else None,'adapterSha256':adapter_sha,
        'examplesSha256':hashlib.sha256(args.examples.read_bytes()).hexdigest(),
        'evidenceFactsEnabled':args.evidence_facts or args.policy_guided or precomputed_fact_examples>0,
        'evidenceFactsAddedByEvaluator':args.evidence_facts or args.policy_guided,
        'examplesWithPrecomputedEvidenceFacts':precomputed_fact_examples,'policyGuided':args.policy_guided,
        'maxOutputTokens':args.max_output_tokens,'maxContextTokens':args.max_context_tokens,
        'leadingSystemMessagesMerged':args.merge_system_messages,'renderedMessageContentRetentionChecked':True,
        'experimentSourceSha256':source_hashes,
        'quantization':'NF4 double quantization, FP16 compute','loadSeconds':load,
        'examples':len(rows),'schemaAndReferencePasses':sum(r['schemaAndReferencesValid'] for r in rows),
        'groundedAgreementPasses':sum(r['schemaAndReferencesValid'] for r in rows if 'validatedFactCount' in r),
        'intentAgreementPasses':sum(r.get('intentAgreementValid',False) for r in rows),
        'domainStructureAndEvidencePasses':sum(r.get('domainStructureAndEvidenceValid',False) for r in rows),
        'compactStructureAndReferencesPasses':sum(r.get('compactStructureAndReferencesValid',False) for r in rows),
        'domainProseAccuracyMeasured':False,
        'peakGpuAllocatedBytes':torch.cuda.max_memory_allocated(),'releaseEligible':False,
        'limitations':['Development examples only; inspect each record for synthetic, source-backed or captured-feature provenance; not independent field evaluation.',
            'Valid JSON and existing frame references do not establish correct interpretation.',
            'No grammar constraint used; this measures the frozen model interface baseline.',
            'Evaluation does not update weights; optional adapters were trained separately.']}
    (args.output/'report.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report),flush=True)

if __name__=='__main__':main()
