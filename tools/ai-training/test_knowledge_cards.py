import json
import hashlib
import tempfile
import unittest
from pathlib import Path
from knowledge_cards import build, CARDS


class KnowledgeCardTests(unittest.TestCase):
    def test_saved_card_manifest_uses_actual_bytes(self):
        root=Path(__file__).parent/'knowledge-data'
        if not (root/'cards.jsonl').exists():self.skipTest('Teaching cards absent')
        manifest=json.loads((root/'manifest.json').read_text())
        self.assertEqual(manifest['sha256'],hashlib.sha256((root/'cards.jsonl').read_bytes()).hexdigest())

    def test_card_identities_are_unique_and_answers_are_not_empty(self):
        self.assertEqual(len(CARDS),len({c[0] for c in CARDS}))
        self.assertTrue(all(c[4].strip() and c[5].strip() and c[6].strip() for c in CARDS))

    def test_source_pages_and_hashes_match_local_primary_documents(self):
        library=Path(__file__).parent/'external-data/nist-reference-library-v1'
        if not library.exists():self.skipTest('Local primary-source intake not present')
        rows=build(library)
        self.assertEqual(24,len(rows))
        self.assertTrue(all(r['sourceDate'] and r['sourceSha256'] and r['scope'] for r in rows))

    def test_a_changed_pdf_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary)
            (root/'manifest.json').write_text(json.dumps(dict(documents=[dict(id='nist-sp800-94',sha256='bad')])) )
            (root/'nist-sp800-94.pdf').write_bytes(b'changed')
            with self.assertRaisesRegex(ValueError,'Changed source'):build(root)


if __name__=='__main__':unittest.main()
