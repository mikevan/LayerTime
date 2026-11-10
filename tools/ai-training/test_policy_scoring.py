import json
import unittest
from build_policy_curriculum import sample
from score_policy_predictions import score


class PolicyScoringTests(unittest.TestCase):
    def test_coverage_does_not_approve_prose(self):
        record=sample('same_identity',0,'train')
        answer=json.loads(record['messages'][-1]['content']);answer['issues'][0]['explanation']='Deliberately unsupported prose.'
        report=score([record],[dict(id=record['id'],output=json.dumps(answer))])
        self.assertEqual(1,report['coveredTriggerGroups'])
        self.assertFalse(report['results'][0]['proseInterpretationReviewed'])
        self.assertFalse(report['releaseEligible'])

    def test_empty_response_misses_both_targets(self):
        record=sample('two_targets',0,'test')
        report=score([record],[dict(id=record['id'],output='{"schemaVersion":1,"issues":[]}')])
        self.assertEqual(2,report['missingTriggerGroups'])

    def test_placeholders_cannot_be_used_as_gold(self):
        record=sample('ordinary_exchange',0,'train');record['labelStatus']='placeholder'
        with self.assertRaisesRegex(ValueError,'placeholders'):score([record],[])

    def test_partial_outputs_remain_incomplete(self):
        records=[sample('same_identity',i,'train') for i in range(2)]
        report=score(records,[dict(id=records[0]['id'],output=records[0]['messages'][-1]['content'])])
        self.assertFalse(report['allExamplesEvaluated'])
        self.assertEqual(1,report['examplesEvaluated'])


if __name__=='__main__':unittest.main()
