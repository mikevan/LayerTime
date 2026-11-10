import hashlib
import json
import unittest
from pathlib import Path
from compact_domain_contract import validate


class WiresharkTeachingTest(unittest.TestCase):
    def test_pinned_excerpts_and_separate_interpretation(self):
        root=Path(__file__).parent;folder=root/'domain-data/wireshark-field-teaching-v1'
        cards=[json.loads(s) for s in (folder/'cards.jsonl').read_text().splitlines()]
        self.assertEqual(24,len(cards))
        for card in cards:
            ref=card['sourceReference'];raw=(root/'external-data/wireshark-teaching-reference-v1'/ref['path']).read_bytes()
            self.assertEqual(ref['fileSha256'],hashlib.sha256(raw).hexdigest())
            excerpt='\n'.join(raw.decode().splitlines()[ref['firstLine']-1:ref['lastLine']])
            self.assertEqual(ref['excerpt'],excerpt)
            self.assertEqual(ref['excerptSha256'],hashlib.sha256(excerpt.encode()).hexdigest())
            self.assertIn('"'+card['field']+'"',excerpt)
            self.assertTrue(card['projectInterpretationLimit'])
            self.assertFalse(card['trainingApproved'])

    def test_questions_keep_source_fact_and_authored_limit_separate(self):
        folder=Path(__file__).parent/'domain-data/wireshark-field-teaching-v1'
        records=[json.loads(s) for s in (folder/'candidates.jsonl').read_text().splitlines()]
        self.assertEqual(48,len(records))
        for row in records:
            validate(row['messages'][-1]['content'],row)
            self.assertEqual('unassigned',row['split'])
            self.assertFalse(row['trainingApproved'])
            context=json.loads(row['messages'][1]['content'].split('\n',1)[1])['facts'][0]['value']
            answer=json.loads(row['messages'][-1]['content'])
            self.assertEqual(context['sourceFact'],answer['significance'])
            self.assertEqual(context['projectInterpretationLimit'],answer['limitations'])


if __name__=='__main__':unittest.main()
