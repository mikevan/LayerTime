import hashlib
import json
import unittest
from pathlib import Path
from compact_domain_contract import validate


class EapolTeachingTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.root=Path(__file__).parent;cls.folder=cls.root/'domain-data/eapol-semantic-teaching-v1'

    def test_excerpts_match_pinned_licensed_source(self):
        cards=[json.loads(s) for s in (self.folder/'cards.jsonl').read_text().splitlines()]
        self.assertEqual(18,len(cards))
        for card in cards:
            ref=card['sourceReference'];raw=(self.root/'external-data/wireshark-teaching-reference-v1'/ref['path']).read_bytes()
            self.assertEqual(ref['fileSha256'],hashlib.sha256(raw).hexdigest())
            excerpt='\n'.join(raw.decode().splitlines()[ref['firstLine']-1:ref['lastLine']])
            self.assertEqual(excerpt,ref['excerpt'])
            self.assertEqual(ref['excerptSha256'],hashlib.sha256(excerpt.encode()).hexdigest())
            if card['id']=='generated-number':self.assertIn('proto_item_set_generated(ti)',excerpt)

    def test_source_scope_metadata_and_reserved_cases(self):
        records=[json.loads(s) for s in (self.folder/'candidates.jsonl').read_text().splitlines()]
        self.assertEqual(34,len(records));self.assertEqual(34,len({r['messages'][-1]['content'] for r in records}))
        for record in records:
            validate(record['messages'][-1]['content'],record)
            self.assertFalse(record['trainingApproved'])
            context=json.loads(record['messages'][1]['content'].split('\n',1)[1])
            self.assertTrue(context['facts'][1]['value']['definitions'])
            self.assertNotIn('labelStatus',record['messages'][1]['content'])
        self.assertEqual(['case-group-vs-pairwise'],[r['id'] for r in records if r['split']=='validation'])
        self.assertEqual(['case-declared-truncation'],[r['id'] for r in records if r['split']=='test'])


if __name__=='__main__':unittest.main()
