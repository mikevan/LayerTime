import unittest
import tempfile
import shutil
import json
from pathlib import Path
from domain_training_profile import verify,digest,rows


class DomainAdmissionTest(unittest.TestCase):
    def test_admitted_corpus_is_valid_and_acceptance_is_excluded(self):
        folder=Path(__file__).parent/'domain-data/admitted-development-v1'
        manifest=verify(folder)
        self.assertEqual(1090,manifest['splits']['train']['examples'])
        self.assertEqual(28,manifest['splits']['validation']['examples'])

    def test_acceptance_identity_cannot_be_smuggled_with_resealed_file(self):
        root=Path(__file__).parent.resolve()
        with tempfile.TemporaryDirectory(prefix='.admission-check-',dir=root) as temporary:
            folder=Path(temporary);self.assertTrue(folder.resolve().is_relative_to(root))
            shutil.copytree(root/'domain-data/admitted-development-v1',folder,dirs_exist_ok=True)
            values=rows(folder/'train.jsonl');values[0]['id']='acceptance-01'
            (folder/'train.jsonl').write_text(''.join(json.dumps(v)+'\n' for v in values),encoding='utf-8')
            manifest=json.loads((folder/'manifest.json').read_text());manifest['splits']['train']['sha256']=digest(folder/'train.jsonl')
            (folder/'manifest.json').write_text(json.dumps(manifest))
            with self.assertRaisesRegex(ValueError,'Acceptance'):verify(folder)


if __name__=='__main__':unittest.main()
