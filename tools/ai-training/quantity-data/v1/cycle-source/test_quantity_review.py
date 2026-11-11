import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from analytic_quantity import normalize
from review_quantity_run import review


class QuantityReviewTests(unittest.TestCase):
    def fixture(self,root):
        source=Path(__file__).parent/'quantity-data/v1/evaluation.jsonl'
        all_rows=[json.loads(x) for x in source.read_text(encoding='utf-8').splitlines()]
        records=[next(r for r in all_rows if r['evaluationRole']==role) for role in ('fresh_test','known_development')]
        examples=root/'examples.jsonl';examples.write_text('\n'.join(json.dumps(r) for r in records),encoding='utf-8')
        rows=[]
        for r in records:
            output=r['messages'][-1]['content'];value,reason=normalize(output,r['messages'][-2]['content'])
            rows.append(dict(id=r['id'],output=output,predictedIntent=value,normalizationReason=reason,
                quantityProposalValid=True,intentAgreementValid=True,schemaAndReferencesValid=True))
        (root/'responses.json').write_text(json.dumps(rows))
        (root/'report.json').write_text(json.dumps(dict(examples=2,examplesSha256=hashlib.sha256(examples.read_bytes()).hexdigest(),intentAgreementPasses=2,adapterSha256='fixture')))
        return examples

    def test_separate_groups_and_final_meaning(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);examples=self.fixture(root);result=review(examples,root)
            self.assertTrue(result['primaryAuthoredTestGatePassed'])
            for g in result['groups'].values():self.assertEqual(1,g['exactRawProposalPasses']);self.assertEqual(1,g['exactIntentPasses'])
            self.assertFalse(result['releaseEligible'])

    def test_rejects_incomplete_or_inconsistent_saved_results(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);examples=self.fixture(root);path=root/'responses.json';rows=json.loads(path.read_text())
            path.write_text('[]')
            with self.assertRaisesRegex(ValueError,'Incomplete'):review(examples,root)
            rows[0]['predictedIntent']['followOnMs']=123;path.write_text(json.dumps(rows))
            with self.assertRaisesRegex(ValueError,'values disagree'):review(examples,root)
