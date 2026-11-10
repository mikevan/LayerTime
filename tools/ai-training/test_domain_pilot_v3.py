import copy
import json
import unittest
from pathlib import Path
from audit_domain_pilot_v3 import audit_record
from build_domain_pilot_v3 import transform_record


class DomainPilotFidelityTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=Path(__file__).parent/'domain-data/v2/candidates.jsonl'
        cls.wifi=next(r for r in map(json.loads,path.read_text().splitlines())
            if r['task']=='REVIEW_DEFENSIVE_EVIDENCE' and r['family']=='clock_invalid')

    def test_unknown_times_stay_unknown(self):
        record=transform_record(self.wifi); audit_record(record,self.wifi)
        facts={f['factId']:f['value'] for f in json.loads(record['messages'][1]['content'].split('\n',1)[1])['facts']}
        self.assertIsNone(facts['capture_summary']['observedSequenceSpanMs'])
        self.assertIn('configuredFollowOnWindowMs',facts['capture_context']['configuration'])

    def test_count_corruption_is_rejected(self):
        record=copy.deepcopy(transform_record(self.wifi))
        query,body=record['messages'][1]['content'].split('\n',1); context=json.loads(body)
        context['facts'][0]['value']['uniqueDisconnectRequests']+=1
        record['messages'][1]['content']=query+'\n'+json.dumps(context)
        with self.assertRaisesRegex(AssertionError,'Count changed'):audit_record(record,self.wifi)


if __name__=='__main__':unittest.main()
