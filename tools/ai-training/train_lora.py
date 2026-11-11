"""Local-only LoRA smoke training. Outputs a development adapter, never a release model."""
import argparse
import json
import shutil
import importlib.metadata
import hashlib
import sys
import math
from pathlib import Path
from prepare import verify

def main():
    source_names=['train_lora.py','completion_loss.py','prepare.py','assemble_development_corpus.py','training_stability.py',
                  'audit_policy_curriculum.py','evaluate_local_model.py','evidence_facts.py','grounded_contract.py','analytic_intent.py','intent_curriculum_audit.py','training_recovery.py','analytic_quantity.py','quantity_ontology.py',
                  'domain_training_profile.py','compact_domain_contract.py','blue_team_contracts.py','repair_domain_views_v2.py','frozen_cpu_layers.py']
    source_snapshot={name:Path(__file__).with_name(name).read_bytes() for name in source_names}
    parser=argparse.ArgumentParser()
    parser.add_argument('--dataset',type=Path,required=True)
    parser.add_argument('--base-model',type=Path,help='An already downloaded, licensed local model directory')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--initial-adapter',type=Path,help='Continue task training from an audited local adapter, with a fresh optimizer')
    parser.add_argument('--checkpoint-steps',type=int,default=0,help='Retain two optimizer/RNG checkpoints for bounded-run recovery; zero disables')
    parser.add_argument('--pause-at-checkpoint',action='store_true',help='Cleanly stop after the first saved checkpoint to verify recovery')
    parser.add_argument('--resume-last-checkpoint',action='store_true',help='Recover only with matching data, source and training arguments')
    parser.add_argument('--max-tokens',type=int,default=4096)
    parser.add_argument('--max-steps',type=int,default=2,help='Optimizer updates; default retains the original smoke test')
    parser.add_argument('--gradient-accumulation',type=int,default=1)
    parser.add_argument('--initial-loss-scale',type=float,default=65536.0,
                        help='Initial CUDA FP16 gradient loss scale; recorded in runtime provenance')
    parser.add_argument('--qlora',action='store_true',help='NF4 base weights with trainable LoRA adapters')
    parser.add_argument('--multitask',action='store_true',help='Verify source-grounded explanation tasks alongside capture reviews')
    parser.add_argument('--domain',action='store_true',help='Use the explicitly admitted compact blue-team domain profile')
    parser.add_argument('--lora-rank',type=int,default=8)
    parser.add_argument('--lora-alpha',type=int,default=16)
    parser.add_argument('--learning-rate',type=float,default=1e-4)
    parser.add_argument('--cpu-frozen-layers',action='store_true',help='Keep frozen embedding and output matrices on CPU to reduce GPU memory')
    parser.add_argument('--merge-system-messages',action='store_true',help='Preserve all leading system instructions for single-system native templates')
    parser.add_argument('--dry-run',action='store_true')
    args=parser.parse_args()
    if args.domain and args.multitask:parser.error('Choose one corpus profile')
    if not 1<=args.lora_rank<=128 or not 1<=args.lora_alpha<=256:parser.error('Invalid LoRA configuration')
    if not math.isfinite(args.learning_rate) or not 0<args.learning_rate<=1e-3:parser.error('Invalid learning rate')
    if not 1<=args.max_steps<=100000 or not 1<=args.gradient_accumulation<=256:
        parser.error('Invalid optimizer update or accumulation count')
    if not math.isfinite(args.initial_loss_scale) or args.initial_loss_scale<1:
        parser.error('Initial loss scale must be finite and at least one')
    if args.checkpoint_steps<0 or args.checkpoint_steps>args.max_steps:
        parser.error('Checkpoint interval must be zero or within the update budget')
    if (args.pause_at_checkpoint or args.resume_last_checkpoint) and not args.checkpoint_steps:
        parser.error('Checkpoint recovery/pause requires a checkpoint interval')
    if args.pause_at_checkpoint and args.resume_last_checkpoint:parser.error('Resume without the pause flag')
    if args.domain:
        from domain_training_profile import verify as verify_domain
        manifest=verify_domain(args.dataset)
    elif args.multitask:
        from assemble_development_corpus import verify as verify_multitask
        manifest=verify_multitask(args.dataset)
    else:manifest=verify(args.dataset)
    if args.dry_run:
        print(json.dumps({'corpusVerified':True,'modelLoaded':False,'tokenLengthsMeasured':False,
            'optimizerUpdatesRequested':args.max_steps,'gradientAccumulation':args.gradient_accumulation,
            'baseQuantization':'NF4 double quantization' if args.qlora else 'FP16',
            'leadingSystemMessagesMerged':args.merge_system_messages,
            'splits':manifest['splits'],'releaseEligible':False},indent=2))
        return
    if args.base_model is None or not args.base_model.is_dir() or args.output is None:
        parser.error('Training requires --base-model pointing to local files and --output')
    if not 512<=args.max_tokens<=8192:
        parser.error('--max-tokens must be between 512 and 8192')
    requested={name:str(value) if isinstance(value,Path) else value for name,value in vars(args).items()}
    resume=None;resume_step=0;steps=[];gradient_checks=[];saved_runtime=None
    if args.resume_last_checkpoint:
        from training_recovery import validate_resume
        resume,resume_step,saved_runtime,steps,gradient_checks=validate_resume(args.output,requested,source_snapshot,manifest)
    elif args.output.exists() and any(args.output.iterdir()):
        parser.error('Use an empty output directory to preserve existing adapters')

    # Imported only for an explicitly requested training run. No account, hub upload, or remote model endpoint.
    import torch
    from datasets import Dataset
    from transformers import AutoTokenizer, AutoModelForCausalLM, Trainer, TrainingArguments, DataCollatorForSeq2Seq, BitsAndBytesConfig, TrainerCallback, set_seed
    from peft import LoraConfig, get_peft_model, prepare_model_for_kbit_training, PeftModel
    if not torch.cuda.is_available():
        raise RuntimeError('This initial training configuration requires a CUDA GPU')
    # Seed before adapter initialization, not only when Trainer is constructed.
    set_seed(42)
    # Turing GPUs cannot use Flash Attention. Avoid silently falling back to the
    # quadratic-memory math kernel for long evidence pages (including checkpoint replay).
    torch.backends.cuda.enable_flash_sdp(False)
    torch.backends.cuda.enable_math_sdp(False)
    torch.backends.cuda.enable_mem_efficient_sdp(True)
    # The pinned Transformers SDPA adapter assumes native GQA implies Flash/math.
    # Explicit KV expansion selects its existing equivalent path, supported by
    # the memory-efficient kernel on this RTX 2070.
    from transformers.integrations import sdpa_attention
    sdpa_attention.use_gqa_in_sdpa=lambda attention_mask,key: False
    tokenizer=AutoTokenizer.from_pretrained(str(args.base_model),local_files_only=True)
    if tokenizer.eos_token_id is None:
        raise RuntimeError('The selected tokenizer must define its end-of-sequence token')
    tokenizer.pad_token=tokenizer.eos_token
    tokenizer.padding_side='right'

    def tokenize_split(split):
        rows=[]
        path=args.dataset/manifest['splits'][split]['file']
        for line in path.read_text(encoding='utf-8').splitlines():
            if not line.strip():continue
            example=json.loads(line)
            messages=example['messages']
            from evaluate_local_model import render_evaluation_prompt
            prefix=render_evaluation_prompt(tokenizer,example,merge_system_messages=args.merge_system_messages)
            prefix_ids=tokenizer(prefix,add_special_tokens=False)['input_ids']
            answer_ids=tokenizer(messages[-1]['content'],add_special_tokens=False)['input_ids']+[tokenizer.eos_token_id]
            ids=prefix_ids+answer_ids
            if len(ids)>args.max_tokens:
                raise ValueError(f"{example['id']} exceeds context budget ({len(ids)} tokens). Reduce evidence page size; do not truncate labels.")
            rows.append({'input_ids':ids,'attention_mask':[1]*len(ids),'labels':[-100]*len(prefix_ids)+answer_ids})
        return Dataset.from_list(rows)

    # The held-out test split is verified for integrity above but never supplied to the trainer.
    train=tokenize_split('train');validation=tokenize_split('validation')
    print(json.dumps({'trainingTokenLengths':[len(r['input_ids']) for r in train],
                      'validationTokenLengths':[len(r['input_ids']) for r in validation]}),flush=True)
    args.output.mkdir(parents=True,exist_ok=True)
    if resume is None:
        shutil.copytree(args.dataset,args.output/'corpus')
        snapshot_dir=args.output/'training-source';snapshot_dir.mkdir()
        for name,raw in source_snapshot.items():(snapshot_dir/name).write_bytes(raw)
    runtime=dict(python=sys.version,packages={name:importlib.metadata.version(name) for name in
        ['torch','transformers','peft','datasets','accelerate','bitsandbytes']},
        sourceSha256={name:hashlib.sha256(raw).hexdigest() for name,raw in source_snapshot.items()},
        corpusManifestSha256=hashlib.sha256((args.output/'corpus/manifest.json').read_bytes()).hexdigest(),
        requestedArguments=requested)
    if saved_runtime is not None:
        if runtime['packages']!=saved_runtime['packages'] or runtime['python']!=saved_runtime['python']:
            raise ValueError('Recovery Python/package runtime changed')
        runtime=saved_runtime
        history=args.output/'recovery-history';history.mkdir(exist_ok=True)
        attempt=history/str(len(list(history.iterdir()))+1);attempt.mkdir()
        for filename in ('optimizer-stability.json','gradient-checks.json'):
            shutil.copy2(args.output/filename,attempt/filename)
        recovery=dict(checkpoint=str(resume),globalStep=resume_step,
            checkpointSha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in resume.iterdir() if p.is_file()})
        shutil.copytree(resume,attempt/'checkpoint')
        (attempt/'recovery.json').write_text(json.dumps(recovery,indent=2),encoding='utf-8')
    base_manifest=args.base_model/'intake-manifest.json'
    if base_manifest.exists():
        if resume is not None and (args.output/'base-intake-manifest.json').read_bytes()!=base_manifest.read_bytes():
            raise ValueError('Recovery base-model intake changed')
        if resume is None:shutil.copy2(base_manifest,args.output/'base-intake-manifest.json')
        runtime['baseIntakeManifestSha256']=hashlib.sha256(base_manifest.read_bytes()).hexdigest()
    if args.initial_adapter:
        initial=args.initial_adapter
        if not base_manifest.exists() or (initial/'base-intake-manifest.json').read_bytes()!=base_manifest.read_bytes():
            raise ValueError('Initial adapter must have the same pinned base-model intake manifest')
        initial_dir=args.output/'initial-adapter'
        if resume is None:
            initial_dir.mkdir()
            for filename in ('adapter_model.safetensors','adapter_config.json'):
                shutil.copy2(initial/filename,initial_dir/filename)
        elif hashlib.sha256((initial_dir/'adapter_model.safetensors').read_bytes()).hexdigest()!=runtime['initialAdapterSha256']:
            raise ValueError('Recovery initial-adapter ancestry changed')
        integrity=__import__('training_stability').inspect_adapter(initial_dir/'adapter_model.safetensors')
        if not integrity['allWeightsFinite']:raise FloatingPointError('Initial adapter has nonfinite weights')
        runtime['initialAdapterSha256']=hashlib.sha256((initial_dir/'adapter_model.safetensors').read_bytes()).hexdigest()
        runtime['optimizerState']='Fresh optimizer; initial adapter weights only, not resumed optimizer state'
    if resume is None:(args.output/'runtime-manifest.json').write_text(json.dumps(runtime,indent=2),encoding='utf-8')
    load_options=dict(local_files_only=True,dtype=torch.float16,attn_implementation='sdpa')
    if args.qlora:
        load_options.update(device_map={'':0},quantization_config=BitsAndBytesConfig(
            load_in_4bit=True,bnb_4bit_quant_type='nf4',bnb_4bit_use_double_quant=True,
            bnb_4bit_compute_dtype=torch.float16))
    model=AutoModelForCausalLM.from_pretrained(str(args.base_model),**load_options)
    if args.cpu_frozen_layers:
        from frozen_cpu_layers import offload_frozen_io
        torch.set_num_threads(4)
        offload_frozen_io(model)
    if args.qlora:
        model=prepare_model_for_kbit_training(model,use_gradient_checkpointing=True,
            gradient_checkpointing_kwargs={'use_reentrant':False})
    model.config.use_cache=False
    if args.initial_adapter:
        model=PeftModel.from_pretrained(model,args.output/'initial-adapter',local_files_only=True,is_trainable=True)
    else:
        model=get_peft_model(model,LoraConfig(r=args.lora_rank,lora_alpha=args.lora_alpha,lora_dropout=0.05,
            target_modules=['q_proj','v_proj'],task_type='CAUSAL_LM'))
    arguments=TrainingArguments(output_dir=str(args.output),max_steps=args.max_steps,per_device_train_batch_size=1,
        per_device_eval_batch_size=1,gradient_accumulation_steps=args.gradient_accumulation,learning_rate=args.learning_rate,fp16=True,
        gradient_checkpointing=True,gradient_checkpointing_kwargs={'use_reentrant':False},
        eval_strategy='no',save_strategy='steps' if args.checkpoint_steps else 'no',
        save_steps=args.checkpoint_steps or 500,save_total_limit=2,logging_steps=1,report_to=[],seed=42,
        dataloader_num_workers=0)
    from completion_loss import completion_loss
    from training_stability import summarize_steps, inspect_adapter, clip_finite_gradients
    class StabilityCallback(TrainerCallback):
        def on_step_end(self,args,state,control,**kwargs):
            steps.append(dict(iteration=state.global_step,
                optimizerStepSkipped=bool(trainer.accelerator.optimizer_step_was_skipped)))
            (Path(args.output_dir)/'optimizer-stability.json').write_text(
                json.dumps(dict(summarize_steps(steps),steps=steps),indent=2),encoding='utf-8')
        def on_save(self,arguments,state,control,**kwargs):
            checkpoint=args.output/f'checkpoint-{state.global_step}'
            weights=checkpoint/'adapter_model.safetensors'
            integrity=inspect_adapter(weights)
            integrity.update(globalStep=state.global_step,sha256=hashlib.sha256(weights.read_bytes()).hexdigest())
            (checkpoint/'checkpoint-integrity.json').write_text(json.dumps(integrity,indent=2),encoding='utf-8')
            if not integrity['allWeightsFinite']:raise FloatingPointError('Checkpoint contains nonfinite adapter weights')
            if args.pause_at_checkpoint:
                (args.output/'checkpoint-pause.json').write_text(json.dumps(dict(globalStep=state.global_step,
                    requestedMaxSteps=args.max_steps,reason='Prespecified clean checkpoint recovery check')),encoding='utf-8')
                control.should_training_stop=True
    class CompletionTrainer(Trainer):
        def compute_loss(self,model,inputs,return_outputs=False,num_items_in_batch=None):
            loss,outputs=completion_loss(model,inputs)
            if not bool(torch.isfinite(loss)):raise FloatingPointError('Nonfinite completion loss; stopping development run')
            return (loss,outputs) if return_outputs else loss
    trainer=CompletionTrainer(model=model,args=arguments,train_dataset=train,eval_dataset=validation,
        data_collator=DataCollatorForSeq2Seq(tokenizer,padding=True,label_pad_token_id=-100),callbacks=[StabilityCallback()])
    # Replace before Trainer prepares its optimizer, which captures this scaler.
    if trainer.accelerator.scaler is None:
        raise RuntimeError('Expected a CUDA FP16 gradient scaler')
    trainer.accelerator.scaler=torch.amp.GradScaler('cuda',init_scale=args.initial_loss_scale)
    # This experiment is single-device. Inspect unscaled gradients before clipping
    # can turn an overflowing FP32 norm into silently zeroed gradients.
    if trainer.accelerator.num_processes != 1:
        raise RuntimeError('Audited clipping currently supports one training process only')
    def checked_clip(parameters,max_norm,norm_type=2):
        scale=trainer.accelerator.scaler.get_scale() if trainer.accelerator.scaler is not None else None
        trainer.accelerator.unscale_gradients()
        try:
            norm=clip_finite_gradients(parameters,max_norm,norm_type)
        except (FloatingPointError,ValueError):
            gradient_checks.append(dict(iteration=trainer.state.global_step+1,lossScale=scale,finite=False))
            (args.output/'gradient-checks.json').write_text(json.dumps(gradient_checks,indent=2),encoding='utf-8')
            raise
        gradient_checks.append(dict(iteration=trainer.state.global_step+1,lossScale=scale,
                                    finite=True,unscaledL2Norm=float(norm),normDtype='float64'))
        (args.output/'gradient-checks.json').write_text(json.dumps(gradient_checks,indent=2),encoding='utf-8')
        return norm
    trainer.accelerator.clip_grad_norm_=checked_clip
    trained=trainer.train(resume_from_checkpoint=str(resume) if resume else None)
    if args.pause_at_checkpoint:
        (args.output/'paused-phase-metrics.json').write_text(json.dumps(trained.metrics,indent=2),encoding='utf-8')
        print(json.dumps(dict(checkpointPaused=True,globalStep=trainer.state.global_step,finalCandidateSaved=False)),flush=True)
        return
    if trainer.state.global_step!=args.max_steps or [s['iteration'] for s in steps]!=list(range(1,args.max_steps+1)):
        raise RuntimeError('Final optimizer ledger does not cover the requested updates exactly once')
    metrics=dict(trained.metrics)
    metrics.update(trainer.evaluate())
    metrics['peakGpuAllocatedBytes']=torch.cuda.max_memory_allocated()
    metrics['globalStep']=trainer.state.global_step
    metrics['resumedFromStep']=resume_step
    metrics['auditedIterations']=len(steps)
    (args.output/'training-metrics.json').write_text(json.dumps(metrics,indent=2),encoding='utf-8')
    trainer.state.save_to_json(str(args.output/'trainer-state.json'))
    trainer.save_model(str(args.output));tokenizer.save_pretrained(str(args.output))
    integrity=inspect_adapter(args.output/'adapter_model.safetensors')
    (args.output/'weight-stability.json').write_text(json.dumps(integrity,indent=2),encoding='utf-8')
    if not integrity['allWeightsFinite']:raise FloatingPointError('Saved adapter contains nonfinite weights')
    (args.output/'development-manifest.json').write_text(json.dumps({
        'schemaVersion':1,'purpose':'development adapter training; not domain qualification',
        'optimizerUpdatesRequested':args.max_steps,'gradientAccumulation':args.gradient_accumulation,
        'initialAdapterSha256':runtime.get('initialAdapterSha256'),'checkpointSteps':args.checkpoint_steps,
        'baseQuantization':'NF4 double quantization' if args.qlora else 'FP16',
        'baseModel':str(args.base_model.resolve()),'corpus':manifest,'releaseEligible':False,
        'blueTeamTrained':False,'note':'Adapter training smoke only. Requires domain review, independent evaluation, merge, quantization and release qualification.'
    },indent=2),encoding='utf-8')

if __name__=='__main__':main()
