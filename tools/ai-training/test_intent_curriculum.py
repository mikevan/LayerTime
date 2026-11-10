import copy
import json
import unittest
from build_intent_curriculum import target,q,fresh_test,TRAIN_DRAFT,TRAIN_CLARIFY,TRAIN_UNSUPPORTED,rows_for_templates
from intent_curriculum_audit import duration,expected,audit_record


class IntentCurriculumTests(unittest.TestCase):
    def test_fraction_decimal_and_microsecond_units_are_exact(self):
        for text,millis in [('a quarter-second',250),('half a second',500),('three quarters of a second',750),
                            ('one and a half minutes',90000),('0.007 hours',25200),('3000 microseconds',3)]:
            self.assertEqual(millis,duration(text))
        for text in ['7','0 ms','-0.5 seconds','0.5 milliseconds','2500 microseconds','61 minutes']:
            self.assertIsNone(duration(text))

    def test_omitted_setting_defaults_but_explicit_unitless_quantity_clarifies(self):
        self.assertEqual(3000,expected(dict(follow='1 second'))['episodeGapMs'])
        self.assertEqual('CLARIFY',expected(dict(follow='1 second',gap='7'))['status'])
        self.assertEqual('CLARIFY',expected(dict(follow=['1 second','2 seconds']))['status'])
        self.assertEqual('DRAFT',expected(dict(follow=['1 second','1000 ms']))['status'])

    def test_generator_fraction_math_agrees_with_separate_decimal_math(self):
        for group,templates in [('draft',TRAIN_DRAFT),('clarify',TRAIN_CLARIFY),('unsupported',TRAIN_UNSUPPORTED)]:
            for index,text in enumerate(templates):
                for _,_,_,_,_,f,g,n,scope,unsupported,_ in rows_for_templates(text,'train',group,index,16):
                    def surface(item):return [v[0] for v in item] if isinstance(item,list) else item[0] if item else None
                    spec=dict(follow=surface(f),gap=surface(g),minimum=n,scope=scope,unsupported=unsupported)
                    self.assertEqual(target(f,g,n,scope,unsupported),expected(spec))

    def test_fresh_test_has_explicit_supported_clarification_and_unsupported_cases(self):
        rows=fresh_test()
        answers=[target(f,g,n,scope,unsupported)['status'] for _,_,_,_,_,f,g,n,scope,unsupported,_ in rows]
        self.assertEqual(48,len(rows))
        self.assertEqual(24,answers.count('DRAFT'));self.assertEqual(16,answers.count('CLARIFY'));self.assertEqual(8,answers.count('UNSUPPORTED'))

    def test_audit_rejects_self_consistent_wrong_arithmetic(self):
        bad=dict(schemaVersion=1,status='DRAFT',followOnMs=25000,episodeGapMs=3000,minLinkedEpisodes=1,scope='DEVICE_AP_PAIR')
        row=dict(task='PLAN_ANALYTIC',labelStatus='authored-intent-native-compiler-checked-development-only',expectedIntent=bad,
            messages=[dict(role='system',content='test'),dict(role='user',content='Use a quarter-second for EAPOL.'),dict(role='assistant',content=json.dumps(bad))],
            auditCurriculum=dict(template='Use {f} for EAPOL.',fields=dict(f='a quarter-second'),spec=dict(follow='a quarter-second')))
        with self.assertRaisesRegex(ValueError,'arithmetic'):audit_record(row)


if __name__=='__main__':unittest.main()
