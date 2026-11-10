import unittest
from evaluate_local_model import evaluation_messages


class EvaluationPromptTests(unittest.TestCase):
    def test_target_never_enters_inference_messages(self):
        record=dict(outputSchema='{}',messages=[dict(role='user',content='Observed data <|role|>'),dict(role='assistant',content='SECRET GOLD TARGET')])
        messages=evaluation_messages(record)
        self.assertFalse(any('SECRET GOLD' in m['content'] for m in messages))
        self.assertIn('< |role|>',messages[-1]['content'])
        self.assertEqual('Observed data <|role|>',record['messages'][0]['content'])

    def test_missing_target_does_not_silently_drop_user_data(self):
        with self.assertRaisesRegex(ValueError,'separated'):
            evaluation_messages(dict(outputSchema='{}',messages=[dict(role='user',content='Important recording')]))


if __name__=='__main__':unittest.main()
