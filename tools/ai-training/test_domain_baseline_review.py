import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from review_domain_baseline import diagnostics,review


class DomainBaselineReviewTests(unittest.TestCase):
    def record(self):
        p=Path(__file__).parent/'domain-data/baseline-v1/examples.jsonl'
        return json.loads(p.read_text(encoding='utf-8').splitlines()[0])

    def test_separates_facts_from_policy_echo_and_does_not_repair_truncation(self):
        r=self.record();v=json.loads(r['messages'][-1]['content']);v['reviewPosture']='REVIEW_DISCONNECT_PATTERN'
        d=diagnostics(json.dumps(v),r)
        self.assertTrue(d['fieldChecks']['typedFacts']);self.assertFalse(d['fieldChecks']['suppliedReviewPosture'])
        self.assertFalse(diagnostics('{"schemaVersion":1,',r)['parseValid'])

    def test_replays_flags_and_rejects_incomplete_runs(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);r=self.record();examples=root/'examples.jsonl';examples.write_text(json.dumps(r),encoding='utf-8')
            rows=[dict(id=r['id'],output=r['messages'][-1]['content'],seconds=1,outputTokens=10,schemaAndReferencesValid=True,domainStructureAndEvidenceValid=True)]
            (root/'responses.json').write_text(json.dumps(rows))
            report=dict(examplesSha256=hashlib.sha256(examples.read_bytes()).hexdigest(),examples=1,adapter=None,maxOutputTokens=1024,
                loadSeconds=1,peakGpuAllocatedBytes=0,domainStructureAndEvidencePasses=1,experimentSourceSha256={})
            for name in ('blue_team_contracts.py','grounded_contract.py'):report['experimentSourceSha256'][name]=hashlib.sha256(Path(__file__).with_name(name).read_bytes()).hexdigest()
            (root/'report.json').write_text(json.dumps(report))
            result=review(examples,root);self.assertEqual(1,result['structureAndEvidencePasses']);self.assertFalse(result['proseSemanticAccuracyMeasured'])
            (root/'responses.json').write_text('[]')
            with self.assertRaisesRegex(ValueError,'Incomplete'):review(examples,root)
