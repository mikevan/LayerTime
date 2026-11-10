import json
from pathlib import Path
import unittest
from compact_domain_contract import validate


class CompactDomainContractTests(unittest.TestCase):
    def record(self):return json.loads((Path(__file__).parent/'domain-data/compact-diagnostic-v1/examples.jsonl').read_text().splitlines()[0])

    def test_teacher_uses_fact_references_without_reproducing_tables(self):
        r=self.record();v=json.loads(r['messages'][-1]['content'])
        self.assertTrue(validate(r['messages'][-1]['content'],r)['proseSemanticReviewRequired'])
        self.assertNotIn('observedFacts',v);self.assertNotIn('sourceKind',r['messages'][1]['content'])
        from evaluate_local_model import evaluation_messages
        self.assertEqual(r['messages'][:-1],evaluation_messages(r))

    def test_reference_duplicates_unknown_fields_and_length_are_rejected(self):
        r=self.record();base=json.loads(r['messages'][-1]['content'])
        for refs in [['invented'],['capture_summary','capture_summary']]:
            v=dict(base,factReferences=refs)
            with self.assertRaises(ValueError):validate(json.dumps(v),r)
        v=dict(base,headline='x'*241)
        with self.assertRaises(ValueError):validate(json.dumps(v),r)
        v=dict(base,attackConfirmed=True)
        with self.assertRaises(ValueError):validate(json.dumps(v),r)
