import copy
import hashlib
import json
import unittest
from pathlib import Path
from score_acceptance import score_case,lines


class AcceptanceTests(unittest.TestCase):
    def setUp(self):
        suite=Path(__file__).parent/'acceptance/v1'
        self.record=lines(suite/'examples.jsonl')[0];self.rubric=lines(suite/'rubrics.jsonl')[0]
        self.gates=json.loads((suite/'protocol.json').read_text())['gates']
        self.response=dict(id=self.record['id'],output=self.record['messages'][-1]['content'],seconds=1)
        self.review=dict(outputSha256=hashlib.sha256(self.response['output'].encode()).hexdigest(),criteria=[
            dict(criterion=s,passed=True,note='Synthetic grader test only, not a real model judgement.') for s in self.rubric['criticalCriteria']])

    def test_valid_json_alone_does_not_pass(self):
        result=score_case(self.record,self.response,self.rubric,None,self.gates)
        self.assertFalse(result['passed']);self.assertIn('semantic_review_missing',result['failures'])

    def test_critical_failure_blocks_structurally_valid_response(self):
        review=copy.deepcopy(self.review);review['criteria'][0]['passed']=False
        self.assertFalse(score_case(self.record,self.response,self.rubric,review,self.gates)['passed'])

    def test_stale_review_rejected(self):
        self.review['outputSha256']='0'*64
        with self.assertRaisesRegex(ValueError,'different output'):score_case(self.record,self.response,self.rubric,self.review,self.gates)

    def test_latency_gate_is_not_averaged_away(self):
        self.response['seconds']=30.01
        self.assertIn('desktop_complete_response_time',score_case(self.record,self.response,self.rubric,self.review,self.gates)['failures'])

    def test_all_inputs_reserved_from_training(self):
        suite=Path(__file__).parent/'acceptance/v1'
        records=lines(suite/'examples.jsonl')
        self.assertEqual(12,len(records));self.assertEqual(12,len({r['id'] for r in records}))
        for r in records:
            self.assertEqual('acceptance',r['split']);self.assertFalse(r['trainingApproved'])
            self.assertNotIn('criticalCriteria',r['messages'][1]['content'])

    def test_corrected_fixtures_declare_domains_and_frame_roles(self):
        suite=Path(__file__).parent/'acceptance/v2'
        for record in lines(suite/'examples.jsonl'):
            evidence=json.loads(record['messages'][1]['content'].split('\n',1)[1])['facts'][0]['value']
            self.assertTrue(evidence['domain'])
            if 'requests' in evidence:
                self.assertEqual('802.11 deauthentication request',evidence['requests']['kind'])
                self.assertIn('recipient',evidence['requests']['targetRole'])
                self.assertEqual('observed EAPOL packet',evidence['exchange']['kind'])


if __name__=='__main__':unittest.main()
