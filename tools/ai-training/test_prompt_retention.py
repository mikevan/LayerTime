import json
import unittest
from evaluate_local_model import evaluation_messages,render_evaluation_prompt


class SingleSystemTemplate:
    def apply_chat_template(self,messages,**kwargs):
        return messages[0]['content']+'\n'+''.join(m['content'] for m in messages if m['role']=='user')


class PromptRetentionTests(unittest.TestCase):
    def record(self):
        return dict(outputSchema='SCHEMA',messages=[dict(role='system',content='REQUIRED POLICY'),
            dict(role='user',content='EVIDENCE'),dict(role='assistant',content='SECRET TARGET')])

    def test_dropped_second_system_is_rejected(self):
        with self.assertRaisesRegex(ValueError,'omitted supplied system'):
            render_evaluation_prompt(SingleSystemTemplate(),self.record())

    def test_merging_preserves_all_instructions_without_gold(self):
        record=self.record();before=json.dumps(record)
        prompt=render_evaluation_prompt(SingleSystemTemplate(),record,merge_system_messages=True)
        self.assertIn('SCHEMA',prompt);self.assertIn('REQUIRED POLICY',prompt);self.assertIn('EVIDENCE',prompt)
        self.assertNotIn('SECRET TARGET',prompt);self.assertEqual(before,json.dumps(record))

    def test_default_messages_remain_unchanged_for_existing_qwen_conditions(self):
        messages=evaluation_messages(self.record())
        self.assertEqual(['system','system','user'],[m['role'] for m in messages])
        self.assertEqual('REQUIRED POLICY',messages[1]['content'])


if __name__=='__main__':unittest.main()
