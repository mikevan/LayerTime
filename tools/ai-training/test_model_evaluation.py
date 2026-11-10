import unittest
import json
from evaluate_local_model import validate_assessment

class ModelEvaluationTests(unittest.TestCase):
    def test_reference_validation_is_strict_and_preserves_frame_zero(self):
        issue=dict(headline='Review',explanation='Observed behavior.',frameIds=[0])
        text=json.dumps(dict(schemaVersion=1,issues=[issue]))
        self.assertEqual(0,validate_assessment(text,dict(suppliedEvidenceIds=[0]))['issues'][0]['frameIds'][0])
        with self.assertRaisesRegex(ValueError,'Fabricated'):
            validate_assessment(text,dict(suppliedEvidenceIds=[1]))
        issue['frameIds']=[False]
        with self.assertRaisesRegex(ValueError,'Fabricated'):
            validate_assessment(json.dumps(dict(schemaVersion=1,issues=[issue])),dict(suppliedEvidenceIds=[0]))
    def test_capture_index_alone_does_not_authorize_unsupplied_frames(self):
        data={'observations':[{'frameId':0}],'relatedEpisodes':[{'disconnectFrames':[0],'eapolFrames':[2]}]}
        record=dict(evidenceIds=[0,1,2],messages=[dict(role='user',content='RECORDING DATA\n'+json.dumps(data))])
        issue=dict(headline='Review',explanation='Observed behavior.',frameIds=[2])
        validate_assessment(json.dumps(dict(schemaVersion=1,issues=[issue])),record)
        issue['frameIds']=[1]
        with self.assertRaisesRegex(ValueError,'Fabricated'):
            validate_assessment(json.dumps(dict(schemaVersion=1,issues=[issue])),record)
    def test_duplicate_keys_are_rejected(self):
        with self.assertRaisesRegex(ValueError,'Duplicate'):
            validate_assessment('{"schemaVersion":1,"schemaVersion":1,"issues":[]}',dict(suppliedEvidenceIds=[0]))
    def test_cited_network_name_context_authorizes_only_its_supplied_frames(self):
        data=dict(observations=[dict(frameId=2)],relatedEpisodes=[],
                  networkContext=[dict(bssid='02:00:00:00:00:01',ssid='CampNet',frameIds=[0])])
        record=dict(evidenceIds=[0,1,2],messages=[dict(role='user',content='RECORDING DATA\n'+json.dumps(data))])
        issue=dict(headline='Observed name',explanation='A cited beacon supplied the name.',frameIds=[0])
        validate_assessment(json.dumps(dict(schemaVersion=1,issues=[issue])),record)
        issue['frameIds']=[1]
        with self.assertRaisesRegex(ValueError,'Fabricated'):
            validate_assessment(json.dumps(dict(schemaVersion=1,issues=[issue])),record)

if __name__=='__main__':unittest.main()
