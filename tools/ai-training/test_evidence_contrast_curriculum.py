import json
import unittest
from pathlib import Path
from compact_domain_contract import validate


def differences(left,right,path=()):
    if isinstance(left,dict) and isinstance(right,dict):
        if set(left)!=set(right):return [path]
        return [p for key in left for p in differences(left[key],right[key],path+(key,))]
    return [] if left==right else [path]


class EvidenceContrastTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root=Path(__file__).parent/'domain-data/evidence-contrast-v1'
        cls.records=[json.loads(s) for s in (cls.root/'candidates.jsonl').read_text().splitlines()]

    def test_only_declared_evidence_field_changes_within_pair(self):
        byid={r['id']:r for r in self.records}
        for record in self.records:
            partner=byid[record['provenance']['pairedWith']]
            left=json.loads(record['messages'][1]['content'].split('\n',1)[1])['facts']
            right=json.loads(partner['messages'][1]['content'].split('\n',1)[1])['facts']
            self.assertEqual(left[1],right[1])
            self.assertEqual([tuple(record['provenance']['changedPath'])],differences(left[0]['value'],right[0]['value']))
            self.assertNotEqual(record['messages'][-1]['content'],partner['messages'][-1]['content'])
            self.assertEqual(record['split'],partner['split'])

    def test_frozen_membership_and_no_training_admission(self):
        families={}
        for record in self.records:
            self.assertEqual(families.setdefault(record['family'],record['split']),record['split'])
            self.assertFalse(record['trainingApproved'])
            validate(record['messages'][-1]['content'],record)
        self.assertEqual(16,len(families))
        self.assertEqual({'rate-exposure'}, {r['family'] for r in self.records if r['split']=='validation'})
        self.assertEqual({'mic-result'}, {r['family'] for r in self.records if r['split']=='test'})

    def test_input_contains_definitions_but_not_gold_or_review_status(self):
        registry=json.loads((self.root/'source-registry.json').read_text())
        for record in self.records:
            user=record['messages'][1]['content'];context=json.loads(user.split('\n',1)[1])
            self.assertTrue(context['facts'][1]['value']['definitions'])
            for reference in record['provenance']['sourceRefs']:
                self.assertIn(reference,registry)
            for name in ('trainingApproved','labelStatus','headlines','meanings','pairedWith','changedPath'):
                self.assertNotIn(name,user)

    def test_exposure_arithmetic_matches_examples(self):
        records=[r for r in self.records if r['family']=='rate-exposure']
        ratios=[]
        for record in records:
            obs=json.loads(record['messages'][1]['content'].split('\n',1)[1])['facts'][0]['value']
            ratios.append((obs['current']['records']/obs['current']['seconds'])/(obs['reference']['records']/obs['reference']['seconds']))
        self.assertEqual([1,6],ratios)


if __name__=='__main__':unittest.main()
