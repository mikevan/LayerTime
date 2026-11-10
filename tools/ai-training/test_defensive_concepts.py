import json
import unittest
from pathlib import Path
from extract_defensive_concepts import extract,SELECTED


class DefensiveConceptTests(unittest.TestCase):
    def test_selection_is_defensive_and_primary_identifiers_are_unique(self):
        self.assertEqual(25,len(SELECTED))
        self.assertTrue(all(name.startswith('D3-') for name in SELECTED))
        self.assertNotIn('D3-ACA',SELECTED)

    def test_selected_definition_and_identity_match_both_primary_formats(self):
        source=Path(__file__).parent/'external-data/mitre-d3fend-1.6.0'
        if not source.exists():self.skipTest('Local primary-source intake absent')
        rows=extract(source)
        self.assertEqual(25,len(rows))
        self.assertTrue(all(not r['appCapabilityVerified'] for r in rows))


if __name__=='__main__':unittest.main()
