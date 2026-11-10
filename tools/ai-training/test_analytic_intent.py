import copy
import json
import unittest
from analytic_intent import check_shape,validate,audit_record
from build_intent_corpus import cases,draft
from prepare import validate_records


class AnalyticIntentTests(unittest.TestCase):
    def test_task_requires_explicit_source_validation_profile(self):
        row=dict(id='unit',family='unit',split='train',task='PLAN_ANALYTIC',sourceId='unit',
            labelStatus='authored-intent-native-compiler-checked-development-only',expectedIntent=draft(),
            messages=[dict(role='system',content='test'),dict(role='user',content='test'),dict(role='assistant',content=json.dumps(draft()))])
        registry=dict(schemaVersion=1,sources=dict(unit=dict(license='project authored',allowedForTraining=True)))
        with self.assertRaisesRegex(ValueError,'requires'):validate_records([row],registry)
        registry['sources']['unit']['validationProfile']='analytic-intent-v1'
        validate_records([row],registry)
    def test_valid_but_wrong_scope_fails_request_meaning(self):
        row=dict(expectedIntent=draft(minimum=2))
        wrong=draft(minimum=2,scope='RECORDING')
        check_shape(json.dumps(wrong))
        with self.assertRaisesRegex(ValueError,'meaning'):validate(json.dumps(wrong),row)

    def test_noninteger_unknown_and_out_of_range_fields_fail(self):
        for patch in [dict(followOnMs=True),dict(episodeGapMs=0),dict(minLinkedEpisodes=2.0),dict(code='anything')]:
            value=draft();value.update(patch)
            with self.assertRaises(ValueError):check_shape(json.dumps(value))

    def test_nondraft_cannot_hide_executable_settings(self):
        value=dict(schemaVersion=1,status='UNSUPPORTED',reason='NO_SUPPORTED_ATOM',followOnMs=30000)
        with self.assertRaises(ValueError):check_shape(json.dumps(value))

    def test_cases_have_disjoint_explicit_request_groups(self):
        rows=cases()
        self.assertEqual(28,len(rows))
        self.assertEqual(56,len({p for _,_,_,ps in rows for p in ps}))
        self.assertEqual(dict(train=16,validation=4,test=8),{s:sum(split==s for _,split,_,_ in rows) for s in ('train','validation','test')})
        for _,_,value,_ in rows:check_shape(json.dumps(value))


if __name__=='__main__':unittest.main()
