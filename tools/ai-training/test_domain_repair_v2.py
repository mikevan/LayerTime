import json
import unittest
from pathlib import Path
from domain_training_profile import rows,verify
from repair_domain_views_v2 import repair

class DomainRepairTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.source=rows(Path(__file__).parent/'domain-data/admitted-development-v1/train.jsonl')
    def test_existing_pair_question_is_not_lost(self):
        source=next(r for r in self.source if r['task']=='EXPLAIN_EVIDENCE_CONTRAST')
        changed=repair(source,0)
        self.assertEqual(source['messages'][1]['content'].split('\n',1)[0],changed['messages'][1]['content'].split('\n',1)[0])
    def test_same_pair_view_and_target_follow_observed_keys(self):
        source=next(r for r in self.source if r.get('family')=='pair_count')
        changed=repair(source,0);e=json.loads(changed['messages'][1]['content'].split('\n',1)[1])['facts'][0]['value']
        self.assertEqual(e['requests']['target'],e['exchange']['client'])
        self.assertEqual(e['requests']['network'],e['exchange']['network'])
        self.assertGreater(e['exchange']['timeMs'],max(e['requests']['timesMs']))
        self.assertEqual('same-pair-later',changed['repair']['category'])
    def test_different_network_view_keeps_mismatch(self):
        source=next(r for r in self.source if r.get('family')=='other_network')
        changed=repair(source,0);e=json.loads(changed['messages'][1]['content'].split('\n',1)[1])['facts'][0]['value']
        self.assertNotEqual(e['requests']['network'],e['exchange']['network'])
        self.assertEqual('different-network',changed['repair']['category'])
    def test_repaired_corpus_keeps_acceptance_excluded(self):
        verify(Path(__file__).parent/'domain-data/admitted-development-v2')

if __name__=='__main__':unittest.main()
