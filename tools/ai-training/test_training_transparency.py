import json
from pathlib import Path
import tempfile
import unittest
from export_training_transparency import export,verify


class TrainingTransparencyTests(unittest.TestCase):
    def fixture(self,base):
        root=base/'tools/ai-training';data=root/'quantity-data/v1';data.mkdir(parents=True)
        rows=[dict(id='approved',split='train',labelStatus='authored'),dict(id='held-out',split='test',evaluationRole='fresh_test')]
        (data/'seeds.jsonl').write_text('\n'.join(json.dumps(r) for r in rows)+'\n')
        (data/'sources.json').write_text(json.dumps({'license':'fixture; inspect rights separately'}))
        (data/'SOURCE-LICENSE.txt').write_text('Fixture source licence notice')
        (root/'external-data').mkdir();(root/'external-data/private.jsonl').write_text('{"secret":"excluded"}')
        return root

    def test_preserves_items_metadata_and_excludes_raw_downloads(self):
        with tempfile.TemporaryDirectory() as d:
            base=Path(d);root=self.fixture(base);out=base/'snapshot';result=export(root,out)
            self.assertEqual(2,result['indexedItems']);self.assertTrue(verify(out)['verified'])
            index=[json.loads(x) for x in (out/'item-index.jsonl').read_text().splitlines()]
            self.assertEqual('held-out',index[1]['id']);self.assertEqual('fresh_test',index[1]['evaluationRole'])
            self.assertFalse((out/'files/external-data').exists())
            self.assertEqual('Fixture source licence notice',(out/'files/quantity-data/v1/SOURCE-LICENSE.txt').read_text())
            self.assertEqual((root/'quantity-data/v1/seeds.jsonl').read_bytes(),(out/'files/quantity-data/v1/seeds.jsonl').read_bytes())
            self.assertIn('not-published',json.loads((out/'manifest.json').read_text())['publicationStatus'])

    def test_existing_archive_and_changed_bytes_are_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            base=Path(d);root=self.fixture(base);out=base/'snapshot';export(root,out)
            with self.assertRaisesRegex(ValueError,'already exists'):export(root,out)
            (out/'files/quantity-data/v1/seeds.jsonl').write_text('{}')
            with self.assertRaisesRegex(ValueError,'hash changed'):verify(out)

    def test_links_exact_items_to_run_training_corpora(self):
        with tempfile.TemporaryDirectory() as d:
            base=Path(d);root=self.fixture(base);run=root/'runs/qwen4b-quantity-v2-adapter/corpus';run.mkdir(parents=True)
            source=json.loads((root/'quantity-data/v1/seeds.jsonl').read_text().splitlines()[0])
            (run/'train.jsonl').write_text(json.dumps(source)+'\n');out=base/'snapshot';export(root,out)
            index=[json.loads(x) for x in (out/'item-index.jsonl').read_text().splitlines()]
            for item in index:
                self.assertEqual(['qwen4b-quantity-v2-adapter'] if item['id']=='approved' else [],item['includedInRunTrainingCorpora'])

    def test_manifest_paths_cannot_escape_snapshot(self):
        with tempfile.TemporaryDirectory() as d:
            base=Path(d);root=self.fixture(base);out=base/'snapshot';export(root,out)
            path=out/'manifest.json';manifest=json.loads(path.read_text());manifest['files'][0]['path']='../escape'
            path.write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'escapes'):verify(out)
