import json
import unittest
from evaluate_local_model import validate_explanation


class ExplanationEvaluationTests(unittest.TestCase):
    def setUp(self):
        self.answer=dict(schemaVersion=1,answer='A false warning mislabels normal activity.',references=[dict(sourceId='nist-sp800-94',pdfPage=17)])
        self.record=dict(messages=[dict(role='assistant',content=json.dumps(self.answer))])

    def test_reference_does_not_prove_answer_is_correct(self):
        self.answer['answer']='A deliberately incorrect explanation.'
        validate_explanation(json.dumps(self.answer),self.record)

    def test_fabricated_page_and_boolean_page_rejected(self):
        for page in [99,True]:
            self.answer['references'][0]['pdfPage']=page
            with self.assertRaisesRegex(ValueError,'Unsupported'):validate_explanation(json.dumps(self.answer),self.record)

    def test_empty_reference_or_blank_answer_rejected(self):
        self.answer['answer']=' '
        with self.assertRaisesRegex(ValueError,'answer'):validate_explanation(json.dumps(self.answer),self.record)


if __name__=='__main__':unittest.main()
