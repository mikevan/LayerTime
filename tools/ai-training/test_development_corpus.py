import json
import shutil
import tempfile
import unittest
from pathlib import Path
from prepare import digest
from assemble_development_corpus import verify, validate_knowledge


class DevelopmentCorpusTests(unittest.TestCase):
    def setUp(self):
        source=Path(__file__).parent/'knowledge-data/prepared-v1'
        if not source.exists():self.skipTest('Prepared local development corpus is absent')
        self.temporary=tempfile.TemporaryDirectory();self.addCleanup(self.temporary.cleanup)
        self.root=Path(self.temporary.name)/'corpus';shutil.copytree(source,self.root)

    def test_original_corpus_preserves_capture_test_count(self):
        manifest=verify(self.root)
        self.assertEqual(5,manifest['splits']['test']['examples'])
        self.assertEqual({'REVIEW_CAPTURE':5},manifest['splits']['test']['tasks'])

    def test_modified_knowledge_target_rejected_even_with_new_file_checksum(self):
        path=self.root/'train.jsonl'
        rows=[json.loads(s) for s in path.read_text(encoding='utf-8').splitlines()]
        next(r for r in rows if r['task']=='EXPLAIN_DEFENSIVE_CONCEPT')['messages'][-1]['content']='{}'
        path.write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
        manifest=json.loads((self.root/'manifest.json').read_text())
        manifest['splits']['train']['sha256']=digest(path)
        (self.root/'manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError,'reviewed card'):verify(self.root)

    def test_permission_is_checked_for_new_task(self):
        path=self.root/'sources.json';registry=json.loads(path.read_text())
        registry['sources']['layertime-authored-knowledge-v1']['allowedForTraining']=False
        path.write_text(json.dumps(registry))
        manifest=json.loads((self.root/'manifest.json').read_text())
        manifest['sourceRegistrySha256']=digest(path)
        (self.root/'manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError,'not approved'):verify(self.root)

    def test_document_family_cannot_be_changed_to_hide_leakage(self):
        card=json.loads((self.root/'knowledge-cards.jsonl').read_text().splitlines()[0])
        row=next(json.loads(s) for s in (self.root/'train.jsonl').read_text().splitlines() if json.loads(s)['task']=='EXPLAIN_DEFENSIVE_CONCEPT')
        row['family']='other'
        with self.assertRaisesRegex(ValueError,'family'):validate_knowledge(row,{card['id']:card})


if __name__=='__main__':unittest.main()
