import json
import unittest
from wireless_curriculum import build
from evaluate_local_model import validate_assessment


class CurriculumTests(unittest.TestCase):
    def test_targets_reference_only_supplied_observations(self):
        for row in build():
            validate_assessment(row['messages'][-1]['content'], row)

    def test_cases_do_not_reuse_existing_evaluation_families(self):
        from pathlib import Path
        existing = [json.loads(s) for s in (Path(__file__).parent/'seed-data/prepared/test.jsonl').read_text().splitlines()]
        self.assertFalse({r['family'] for r in existing} & {r['family'] for r in build()})

    def test_training_candidates_are_not_mislabeled_as_independent_test_data(self):
        rows=build()
        self.assertEqual(12,len(rows))
        self.assertTrue(all(r['split']=='train' for r in rows))
        self.assertTrue(all('not-independent' in r['labelStatus'] for r in rows))


if __name__ == '__main__':unittest.main()
