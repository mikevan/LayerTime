import json
import hashlib
import unittest
from pathlib import Path
from build_evaluation_v2 import build
from evaluate_local_model import supplied_ids


class EvaluationV2Tests(unittest.TestCase):
    def test_frozen_manifest_hashes_actual_file_bytes(self):
        root=Path(__file__).parent/'evaluation-data/v2'
        if not root.exists():self.skipTest('Frozen diagnostic data absent')
        manifest=json.loads((root/'manifest.json').read_text())
        self.assertEqual(manifest['sha256'],hashlib.sha256((root/'test.jsonl').read_bytes()).hexdigest())

    def test_expectations_never_enter_the_model_messages(self):
        for row in build():
            self.assertIn('expectedFacts',row)
            for message in row['messages'][:-1]:
                self.assertNotIn('warningExpected',message['content'])
                self.assertNotIn('mustNotClaim',message['content'])
            self.assertEqual(set(row['evidenceIds']),supplied_ids(row))

    def test_new_cases_are_excluded_from_pilot_corpus(self):
        corpus=Path(__file__).parent/'knowledge-data/prepared-v1'
        if not corpus.exists():self.skipTest('Local pilot corpus absent')
        training=[json.loads(s) for s in (corpus/'train.jsonl').read_text().splitlines()]
        self.assertFalse({r['id'] for r in training}&{r['id'] for r in build()})
        self.assertFalse({r['family'] for r in training}&{r['family'] for r in build()})


if __name__=='__main__':unittest.main()
