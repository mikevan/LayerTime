import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from prepare import prepare
from training_recovery import validate_resume


class TrainingRecoveryTests(unittest.TestCase):
    def setup_run(self,root):
        sources={'train_lora.py':b'fixed training source'}
        requested=dict(max_steps=4,checkpoint_steps=2,resume_last_checkpoint=True,pause_at_checkpoint=False)
        rows=[]
        for split in ('train','validation','test'):
            target=dict(schemaVersion=1,status='CLARIFY',reason='AMBIGUOUS_SETTINGS')
            rows.append(dict(id=split,family=split,split=split,task='PLAN_ANALYTIC',sourceId='unit',
                labelStatus='authored-intent-native-compiler-checked-development-only',expectedIntent=target,
                messages=[dict(role='system',content='unit'),dict(role='user',content=split),dict(role='assistant',content=json.dumps(target))]))
        seed=root/'seeds.jsonl';seed.write_text('\n'.join(json.dumps(r) for r in rows),encoding='utf-8')
        registry=root/'sources.json';registry.write_text(json.dumps(dict(schemaVersion=1,sources=dict(unit=dict(allowedForTraining=True,license='project authored',validationProfile='analytic-intent-v1')))),encoding='utf-8')
        manifest=prepare(seed,registry,root/'corpus')
        runtime=dict(sourceSha256={name:hashlib.sha256(raw).hexdigest() for name,raw in sources.items()},
            corpusManifestSha256=hashlib.sha256((root/'corpus/manifest.json').read_bytes()).hexdigest(),
            requestedArguments=dict(requested,resume_last_checkpoint=False,pause_at_checkpoint=True))
        (root/'runtime-manifest.json').write_text(json.dumps(runtime))
        checkpoint=root/'checkpoint-2';checkpoint.mkdir()
        for name in ('adapter_model.safetensors','adapter_config.json','optimizer.pt','scheduler.pt','scaler.pt','rng_state.pth'):(checkpoint/name).write_bytes(b'unit fixture, not real model state')
        (checkpoint/'trainer_state.json').write_text(json.dumps(dict(global_step=2)))
        (checkpoint/'checkpoint-integrity.json').write_text(json.dumps(dict(globalStep=2,allWeightsFinite=True,
            sha256=hashlib.sha256((checkpoint/'adapter_model.safetensors').read_bytes()).hexdigest())))
        (root/'optimizer-stability.json').write_text(json.dumps(dict(steps=[dict(iteration=i,optimizerStepSkipped=False) for i in (1,2,3)])))
        (root/'gradient-checks.json').write_text(json.dumps([dict(iteration=i,finite=True) for i in (1,2,3)]))
        return requested,sources,manifest

    def test_matching_recovery_keeps_only_checkpoint_committed_iterations(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            checkpoint,step,_,steps,checks=validate_resume(root,args,sources,manifest)
            self.assertEqual(2,step);self.assertEqual('checkpoint-2',checkpoint.name)
            self.assertEqual([1,2],[s['iteration'] for s in steps]);self.assertEqual(2,len(checks))

    def test_changed_arguments_and_source_are_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            with self.assertRaisesRegex(ValueError,'arguments'):validate_resume(root,dict(args,max_steps=5),sources,manifest)
            with self.assertRaisesRegex(ValueError,'source'):validate_resume(root,args,{'train_lora.py':b'changed'},manifest)

    def test_changed_corpus_is_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            with (root/'corpus/train.jsonl').open('a') as stream:stream.write(' ')
            with self.assertRaises(ValueError):validate_resume(root,args,sources,manifest)

    def test_incomplete_latest_checkpoint_does_not_silently_fall_back(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            (root/'checkpoint-2/scaler.pt').unlink()
            with self.assertRaisesRegex(ValueError,'incomplete'):validate_resume(root,args,sources,manifest)

    def test_duplicate_ledger_iterations_are_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            (root/'optimizer-stability.json').write_text(json.dumps(dict(steps=[dict(iteration=1),dict(iteration=1)])))
            with self.assertRaisesRegex(ValueError,'duplicated'):validate_resume(root,args,sources,manifest)

    def test_changed_checkpoint_weights_are_refused(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);args,sources,manifest=self.setup_run(root)
            (root/'checkpoint-2/adapter_model.safetensors').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'integrity'):validate_resume(root,args,sources,manifest)


if __name__=='__main__':unittest.main()
