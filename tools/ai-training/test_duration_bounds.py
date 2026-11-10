import json
import unittest
from audit_duration_bounds import inspect


def fixture(text, valid=True):
    frames=[dict(frameId=i,typeSubtype=12,malformed=False,network='ap',source='ap',destination='client',
                 timeNanos=1_700_000_000_000_000_000+i*1_000_000_000,timestampValid=valid) for i in range(2)]
    record=dict(labelStatus='synthetic-policy-derived-not-attack-ground-truth',evidenceIds=[0,1],
        messages=[dict(role='system',content='test'),dict(role='user',content='RECORDING DATA\n'+json.dumps(dict(observations=frames))),
                  dict(role='assistant',content='target')])
    output=json.dumps(dict(schemaVersion=1,issues=[dict(headline='Observed requests',explanation=text,frameIds=[0,1])]))
    return record,output


class DurationBoundTests(unittest.TestCase):
    def test_short_bound_is_contradicted_by_actual_timestamps(self):
        result=inspect(*fixture('2 disconnect requests within 0.100 seconds.'))[0]
        self.assertTrue(result['contradictedByObservedSpan'])
        self.assertEqual(1.0,result['observedSpanSeconds'])

    def test_larger_upper_bound_is_not_declared_false(self):
        result=inspect(*fixture('2 disconnect requests within 3 seconds.'))[0]
        self.assertTrue(result['checked'])
        self.assertFalse(result['contradictedByObservedSpan'])

    def test_invalid_clock_abstains(self):
        self.assertFalse(inspect(*fixture('2 disconnect requests within 0.100 seconds.',False))[0]['checked'])

    def test_subsets_and_unmatched_language_abstain(self):
        for text in ['1 disconnect requests within 0.100 seconds.','Several requests happened quickly.']:
            self.assertFalse(inspect(*fixture(text))[0]['checked'])


if __name__=='__main__':unittest.main()
