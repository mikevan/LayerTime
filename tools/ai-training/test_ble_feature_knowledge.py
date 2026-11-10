import json
from pathlib import Path
import unittest
from build_ble_feature_knowledge import toy


class BleFeatureMeaningTests(unittest.TestCase):
    def test_address_distribution_newness_and_address_type_are_distinct(self):
        stable_private=toy(64,1,True);new_public=toy(64,64,False,previous=False)
        self.assertEqual(0,stable_private['address_entropy']);self.assertEqual(1,stable_private['random_address_fraction']);self.assertEqual(0,stable_private['new_address_fraction'])
        self.assertEqual(1,new_public['address_entropy']);self.assertEqual(0,new_public['random_address_fraction']);self.assertEqual(1,new_public['new_address_fraction'])

    def test_payload_repetition_can_coexist_with_length_variation(self):
        f=toy(64,1,True,variable=True)
        self.assertGreater(f['payload_repeat_fraction'],0.9);self.assertGreater(f['ad_data_length_std'],0)

    def test_contrast_pairs_share_context_and_exclude_review_metadata(self):
        path=Path(__file__).parent/'domain-data/ble-feature-knowledge-v1/counterexamples.jsonl'
        rows=[json.loads(s) for s in path.read_text().splitlines()];groups={}
        for r in rows:
            groups.setdefault(r['family'],[]).append(r)
            self.assertNotIn('trainingApproved',json.dumps(r['messages']))
            answer=json.loads(r['messages'][-1]['content']);self.assertTrue(answer['featureReferences'])
        self.assertEqual(8,len(groups))
        for pair in groups.values():
            self.assertEqual(2,len(pair));self.assertEqual(pair[0]['messages'][1]['content'].split('\n',1)[1],pair[1]['messages'][1]['content'].split('\n',1)[1])
