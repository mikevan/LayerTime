import unittest
from audit_instructions import inspect,normalize_text

class InstructionAuditTests(unittest.TestCase):
    def test_preserves_unicode_while_unescaping_source_paragraphs(self):
        self.assertEqual('Wi-Fi café\n\nExample',normalize_text(r'Wi-Fi café\\n\\nExample'))
    def test_missing_answers_and_corrupted_text_are_not_candidates(self):
        self.assertEqual('missing_fields',inspect({'system':'x','user':'Explain Wi-Fi'})[1])
        self.assertEqual('replacement_characters',inspect(dict(system='x',user='Explain Wi-Fi',assistant='a\ufffd'*100))[1])
    def test_operational_request_requires_separate_scope_review(self):
        row=dict(system='x',user='Explain how to launch a wireless attack',assistant='a'*150)
        self.assertEqual('requires_scope_review',inspect(row)[1])
    def test_defensive_candidate_retains_topic_and_stable_question_identity(self):
        row=dict(system='x',user='Explain Wi-Fi disconnect detection.',assistant='Some reviewed explanation. '*10)
        first,reason=inspect(row)
        self.assertIsNone(reason);self.assertIn('wifi',first['topics'])
        second,_=inspect(dict(row,user='Explain Wi-Fi disconnect detection!'))
        self.assertEqual(first['questionHash'],second['questionHash'])

if __name__=='__main__':unittest.main()
