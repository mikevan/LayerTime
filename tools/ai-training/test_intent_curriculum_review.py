import hashlib
import json
from pathlib import Path
import tempfile
import unittest
from review_intent_curriculum_run import review


class IntentCurriculumReviewTests(unittest.TestCase):
    def make_run(self,folder):
        target=dict(schemaVersion=1,status='DRAFT',followOnMs=250,episodeGapMs=3000,minLinkedEpisodes=1,scope='DEVICE_AP_PAIR')
        record=dict(id='fresh',evaluationRole='fresh_test',task='PLAN_ANALYTIC',labelStatus='authored-intent-native-compiler-checked-development-only',
            expectedIntent=target,messages=[dict(role='system',content='test'),dict(role='user',content='Wait a quarter-second for EAPOL.'),dict(role='assistant',content=json.dumps(target))],
            auditCurriculum=dict(template='Wait {f} for EAPOL.',fields=dict(f='a quarter-second'),spec=dict(follow='a quarter-second')))
        known=dict(record,id='known',evaluationRole='known_development')
        examples=folder/'examples.jsonl';examples.write_text('\n'.join(json.dumps(r) for r in (record,known)),encoding='utf-8')
        rows=[dict(id=r['id'],output=json.dumps(target),schemaAndReferencesValid=True,intentAgreementValid=True,intentShapeValid=True) for r in (record,known)]
        (folder/'responses.json').write_text(json.dumps(rows))
        (folder/'report.json').write_text(json.dumps(dict(examples=2,examplesSha256=hashlib.sha256(examples.read_bytes()).hexdigest(),intentAgreementPasses=2)))
        return examples

    def test_keeps_fresh_and_known_results_separate(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);examples=self.make_run(root)
            result=review(examples,root)
            self.assertEqual(1,result['groups']['fresh_test']['examples'])
            self.assertEqual(1,result['groups']['known_development']['examples'])
            self.assertTrue(result['primaryAuthoredTestGatePassed']);self.assertFalse(result['releaseEligible'])

    def test_missing_responses_and_false_validity_flags_are_rejected(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);examples=self.make_run(root)
            path=root/'responses.json';rows=json.loads(path.read_text())
            path.write_text('[]')
            with self.assertRaisesRegex(ValueError,'Incomplete'):review(examples,root)
            rows[0]['output']='{}';path.write_text(json.dumps(rows))
            with self.assertRaisesRegex(ValueError,'validity'):review(examples,root)


if __name__=='__main__':unittest.main()
