import copy
import json
import unittest
from build_policy_curriculum import sample,FAMILIES
from audit_policy_curriculum import audit_row


class PolicyCurriculumTests(unittest.TestCase):
    def test_each_family_passes_independent_raw_frame_arithmetic(self):
        for split,families in FAMILIES.items():
            for family in families:audit_row(sample(family,0,split))

    def test_wrong_narrative_count_is_rejected(self):
        row=sample('same_identity',0,'train')
        answer=json.loads(row['messages'][-1]['content'])
        text=answer['issues'][0]['explanation']
        import re
        answer['issues'][0]['explanation']=re.sub(r'\d+ disconnect requests','999 disconnect requests',text)
        row['messages'][-1]['content']=json.dumps(answer)
        with self.assertRaisesRegex(ValueError,'narrative count'):audit_row(row)

    def test_missing_triggered_group_is_rejected(self):
        row=sample('two_targets',0,'test')
        answer=json.loads(row['messages'][-1]['content']);answer['issues'].pop()
        row['messages'][-1]['content']=json.dumps(answer)
        with self.assertRaisesRegex(ValueError,'missing'):audit_row(row)

    def test_broadcast_explanation_does_not_invent_an_individual_target(self):
        answer=json.loads(sample('broadcast',0,'train')['messages'][-1]['content'])
        self.assertIn('do not identify one exclusive target',answer['issues'][0]['explanation'])
        self.assertNotIn('this individual target',answer['issues'][0]['explanation'])

    def test_fabricated_derived_count_is_rejected(self):
        row=sample('same_identity',0,'train')
        data=json.loads(row['messages'][1]['content'].split('\n',1)[1])
        data['derivedEvidenceFacts']['groups'][0]['disconnectBurstPolicy']['peakObservedFrames']=999
        row['messages'][1]['content']='RECORDING DATA\n'+json.dumps(data)
        with self.assertRaisesRegex(ValueError,'Derived policy'):audit_row(row)


if __name__=='__main__':unittest.main()
