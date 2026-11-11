import copy
import unittest
from blue_team_ontology import ontology
from blue_team_cases import CASES
from build_blue_team_domain import synthetic_case
from audit_blue_team_domain import expected_facts


class BlueTeamDomainTests(unittest.TestCase):
    def test_all_families_have_independently_recomputed_facts(self):
        for case in CASES:
            for variant in (0,31,63):
                with self.subTest(case=case[0],variant=variant):
                    _,frames,evidence,answer=synthetic_case(case,variant)
                    self.assertEqual(256,len(frames));self.assertEqual(expected_facts(frames),evidence['facts'])
                    self.assertEqual(evidence['facts'],answer['observedFacts'])

    def test_identity_time_and_broadcast_negatives_do_not_link(self):
        for name in ('other_client','other_network','same_name_other_radio','exchange_before','exchange_late','clock_invalid','broadcast_request','requests_only','exchange_only'):
            case=next(c for c in CASES if c[0]==name)
            self.assertEqual(0,synthetic_case(case,0)[2]['facts']['linkedExchanges'],name)
        self.assertEqual(1,synthetic_case(CASES[0],0)[2]['facts']['linkedExchanges'])

    def test_duplicate_key_cannot_hide_changed_evidence(self):
        case=next(c for c in CASES if c[0]=='duplicate_records');_,frames,evidence,_=synthetic_case(case,0)
        self.assertEqual(6,evidence['facts']['duplicateRecords'])
        changed=copy.deepcopy(frames);duplicate=next(f for f in changed[1:] if f['observationKey']==changed[0]['observationKey'])
        duplicate['network']='different'
        with self.assertRaisesRegex(ValueError,'Conflicting duplicate'):expected_facts(changed)

    def test_ontology_ids_and_edges_resolve(self):
        ont=ontology();ids={c['id'] for c in ont['concepts']}
        self.assertEqual(84,len(ids));self.assertEqual(48,len(CASES))
        self.assertTrue(all(r['subject'] in ids and r['object'] in ids for r in ont['relationships']))
