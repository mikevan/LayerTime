import copy
import json
import unittest
from grounded_contract import validate,audit_record
from prepare import validate_records


def fixture():
    claims=dict(target='02:00:00:00:02:02',bssid='02:00:00:00:01:01',disconnectCount=1,linkedEapolCount=0,
        disconnectSpanMs=0,networkMatch='NO_LINKED_EAPOL',linkedDelayMinMs=None,linkedDelayMaxMs=None,untimedEapolContextCount=0)
    data=dict(captureId='unit',page=1,facts=[dict(factId='episode-1',claims=claims,frameIds=[0])])
    target=dict(schemaVersion=2,captureId='unit',page=1,assessments=[dict(factId='episode-1',claims=claims)])
    return dict(id='unit',family='one',split='train',sourceId='unit',task='REVIEW_GROUNDED_CAPTURE',
        labelStatus='native-engine-checked-contract-target-not-attack-ground-truth',outputSchema='{}',
        auditFollowOnMs=30000,auditEpisodeGapMs=3000,auditPageIds=[0],
        auditObservations=[dict(frameId=0,timeNanos=1000000,timestampValid=True,malformed=False,typeSubtype=12,
            source=claims['bssid'],destination=claims['target'],network=claims['bssid'],eapol=False)],
        messages=[dict(role='system',content='Check facts'),dict(role='user',content='VERIFIED FACTS\n'+json.dumps(data)),dict(role='assistant',content=json.dumps(target))])


class GroundedContractTests(unittest.TestCase):
    def test_raw_audit_and_strict_target_agree(self):
        r=fixture();self.assertEqual(1,audit_record(r));validate(r['messages'][-1]['content'],r)

    def test_self_consistent_but_false_model_input_is_rejected(self):
        r=fixture();data=json.loads(r['messages'][1]['content'].split('\n',1)[1]);data['facts'][0]['claims']['disconnectCount']=99
        r['messages'][1]['content']='VERIFIED FACTS\n'+json.dumps(data)
        answer=json.loads(r['messages'][-1]['content']);answer['assessments'][0]['claims']['disconnectCount']=99
        r['messages'][-1]['content']=json.dumps(answer)
        with self.assertRaisesRegex(ValueError,'raw-observation'):audit_record(r)

    def test_boolean_is_not_integer_claim_and_null_is_not_zero(self):
        r=fixture()
        for k,v in [('disconnectCount',True),('linkedDelayMinMs',0),('disconnectSpanMs',0.0)]:
            answer=json.loads(r['messages'][-1]['content']);answer['assessments'][0]['claims'][k]=v
            with self.assertRaises(ValueError):validate(json.dumps(answer),r)

    def test_missing_duplicate_and_extra_prose_are_rejected(self):
        r=fixture();answer=json.loads(r['messages'][-1]['content'])
        for replacement in [[],answer['assessments']*2]:
            bad=copy.deepcopy(answer);bad['assessments']=replacement
            with self.assertRaises(ValueError):validate(json.dumps(bad),r)
        answer['explanation']='An attack was proven'
        with self.assertRaises(ValueError):validate(json.dumps(answer),r)

    def test_new_task_requires_registered_raw_audit_profile(self):
        registry=dict(schemaVersion=1,sources=dict(unit=dict(license='project authored',allowedForTraining=True)))
        with self.assertRaisesRegex(ValueError,'requires'):validate_records([fixture()],registry)
        registry['sources']['unit']['validationProfile']='grounded-contract-v1'
        validate_records([fixture()],registry)


if __name__=='__main__':unittest.main()
