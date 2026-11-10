import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from test_grounded_contract import fixture
from summarize_grounded_run import summarize


class GroundedSummaryTests(unittest.TestCase):
    def write_run(self,folder):
        record=fixture();record['family']='unit-family'
        examples=folder/'examples.jsonl'
        examples.write_text(json.dumps(record)+'\n',encoding='utf-8')
        (folder/'responses.json').write_text(json.dumps([dict(id=record['id'],output=record['messages'][-1]['content'],
            seconds=1.5,schemaAndReferencesValid=True)]),encoding='utf-8')
        (folder/'report.json').write_text(json.dumps(dict(examplesSha256=hashlib.sha256(examples.read_bytes()).hexdigest(),
            examples=1,groundedAgreementPasses=1,adapterSha256=None)),encoding='utf-8')
        return examples

    def test_replays_output_and_counts_families(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);examples=self.write_run(folder)
            result=summarize(examples,folder)
            self.assertEqual(1,result['factBearingPasses'])
            self.assertEqual(1,result['familiesWithEveryPagePassing'])
            self.assertFalse(result['releaseEligible'])

    def test_rejects_missing_output_and_false_validity_flag(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);examples=self.write_run(folder)
            path=folder/'responses.json';rows=json.loads(path.read_text())
            path.write_text('[]')
            with self.assertRaisesRegex(ValueError,'Incomplete'):summarize(examples,folder)
            rows[0]['output']='{}';path.write_text(json.dumps(rows))
            with self.assertRaisesRegex(ValueError,'validity flag'):summarize(examples,folder)


if __name__=='__main__':unittest.main()
