import hashlib
import json
import unittest
from review_evaluation import review_responses


class EvaluationReviewTests(unittest.TestCase):
    def setUp(self):
        self.output = json.dumps(dict(schemaVersion=1, issues=[]))
        self.responses = [dict(id='case', output=self.output)]
        self.records = [dict(id='case', suppliedEvidenceIds=[0])]

    def test_valid_format_does_not_approve_interpretation(self):
        report = review_responses(self.responses, self.records, {})
        self.assertTrue(report['results'][0]['schemaAndReferencesValid'])
        self.assertEqual(0, report['interpretationPasses'])
        self.assertFalse(report['releaseEligible'])

    def test_stale_review_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'match'):
            review_responses(self.responses, self.records, {'case': dict(outputSha256='wrong', status='pass', findings=['Checked'])})

    def test_failed_interpretation_cannot_qualify(self):
        decision = dict(outputSha256=hashlib.sha256(self.output.encode()).hexdigest(),
                        status='fail', findings=['Missed disconnect burst'])
        report = review_responses(self.responses, self.records, {'case': decision})
        self.assertEqual(0, report['interpretationPasses'])


if __name__ == '__main__':
    unittest.main()
