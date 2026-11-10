import copy
import json
import tempfile
import unittest
from pathlib import Path
from prepare import prepare, verify, validate_records

class CorpusTests(unittest.TestCase):
    def setUp(self):
        self.registry = {'schemaVersion': 1, 'sources': {'fixture': {'allowedForTraining': True, 'license': 'project fixtures'}}}
        self.records = [dict(id=split, family=split, split=split, sourceId='fixture', labelStatus='synthetic', task='REVIEW_CAPTURE',
            evidenceIds=[0], messages=[{'role':'system','content':'Review observations.'}, {'role':'user','content':split},
            {'role':'assistant','content':json.dumps({'schemaVersion':1,'issues':[]})}]) for split in ('train','validation','test')]

    def test_family_leakage_is_rejected(self):
        self.records[1]['family']='train'
        with self.assertRaisesRegex(ValueError,'family leaks'):validate_records(self.records,self.registry)
    def test_duplicated_conversation_across_families_is_rejected(self):
        self.records[1]['messages']=copy.deepcopy(self.records[0]['messages'])
        with self.assertRaisesRegex(ValueError,'conversation leaks'):validate_records(self.records,self.registry)
    def test_unapproved_source_is_rejected(self):
        self.registry['sources']['fixture']['allowedForTraining']=False
        with self.assertRaisesRegex(ValueError,'permission'):validate_records(self.records,self.registry)
    def test_fabricated_evidence_is_rejected(self):
        self.records[0]['messages'][-1]['content']=json.dumps({'schemaVersion':1,'issues':[{'headline':'Concern','explanation':'Review','frameIds':[99]}]})
        with self.assertRaisesRegex(ValueError,'evidence'):validate_records(self.records,self.registry)
    def test_boolean_frame_identity_is_rejected(self):
        self.records[0]['evidenceIds']=[True]
        with self.assertRaisesRegex(ValueError,'identity'):validate_records(self.records,self.registry)
    def test_prepared_manifest_detects_changed_data(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)
            seeds=root/'seeds.jsonl';sources=root/'sources.json';out=root/'dataset'
            seeds.write_text('\n'.join(json.dumps(r) for r in self.records),encoding='utf-8')
            sources.write_text(json.dumps(self.registry),encoding='utf-8')
            prepare(seeds,sources,out)
            self.assertFalse(verify(out)['releaseEligible'])
            with (out/'train.jsonl').open('a',encoding='utf-8') as f:f.write('\n')
            with self.assertRaisesRegex(ValueError,'integrity'):verify(out)

if __name__ == '__main__':unittest.main()
