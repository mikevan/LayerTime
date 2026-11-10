import copy
import json
from pathlib import Path
import unittest
from blue_team_contracts import validate
from wifi_domain_reference import decode


class DomainContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows={}
        path=Path(__file__).parent/'domain-data/v2/candidates.jsonl'
        for line in path.read_text(encoding='utf-8').splitlines():
            r=json.loads(line);cls.rows.setdefault(r['task'],r)

    def test_all_task_teachers_have_explicit_contracts(self):
        self.assertEqual(5,len(self.rows))
        for r in self.rows.values():
            self.assertIn('output contract:',r['messages'][0]['content'])
            self.assertTrue(validate(r['messages'][-1]['content'],r)['proseSemanticReviewRequired'])

    def test_synthetic_inputs_do_not_disclose_class_or_fixed_role_addresses(self):
        path=Path(__file__).parent/'domain-data/v2/candidates.jsonl';checked=set()
        for line in path.read_text(encoding='utf-8').splitlines():
            r=json.loads(line)
            if r['task']!='REVIEW_DEFENSIVE_EVIDENCE' or r['family'] in checked:continue
            checked.add(r['family']);visible=json.dumps(r['messages']);context=json.loads(r['messages'][1]['content'].split('\n',1)[1])
            self.assertNotIn(r['family'],visible);self.assertNotIn(r['id'],visible)
            self.assertNotIn('02:00:00:02:00:01',visible)
            self.assertIn('sequenceObservations',context)
        self.assertEqual(48,len(checked))

    def test_identity_count_and_extra_field_mutations_rejected(self):
        r=self.rows['REVIEW_DEFENSIVE_EVIDENCE'];base=json.loads(r['messages'][-1]['content'])
        for field in ('target','network'):
            v=copy.deepcopy(base);v['observedFacts']['identityGroups'][0][field]='invented'
            with self.assertRaises(ValueError):validate(json.dumps(v),r)
        v=copy.deepcopy(base);v['observedFacts']['linkedExchanges']=True
        with self.assertRaises(ValueError):validate(json.dumps(v),r)
        v=copy.deepcopy(base);v['attackConfirmed']=True
        with self.assertRaises(ValueError):validate(json.dumps(v),r)

    def test_source_and_execution_mutations_rejected(self):
        r=self.rows['ANSWER_SOURCED_DEFENSIVE_QUESTION'];v=json.loads(r['messages'][-1]['content']);v['sourceId']='invented'
        with self.assertRaises(ValueError):validate(json.dumps(v),r)
        r=self.rows['PROPOSE_PASSIVE_ANALYTIC_RECIPE'];v=json.loads(r['messages'][-1]['content']);v['executable']=True
        with self.assertRaises(ValueError):validate(json.dumps(v),r)
        v=json.loads(r['messages'][-1]['content']);v['operators'].append('transmit')
        with self.assertRaises(ValueError):validate(json.dumps(v),r)

    def test_evaluation_preserves_domain_training_interface_without_duplicate_schema(self):
        from evaluate_local_model import evaluation_messages
        for r in self.rows.values():
            messages=evaluation_messages(r)
            self.assertEqual(r['messages'][:-1],messages)
            self.assertNotIn('sourceGenerationContext',json.dumps(messages))
            changed=copy.deepcopy(r);changed['taskContractVersion']='unknown'
            with self.assertRaisesRegex(ValueError,'contract mismatch'):evaluation_messages(changed)

    def test_packet_header_direction_mapping_and_short_headers(self):
        a1=bytes.fromhex('020000000001');a2=bytes.fromhex('020000000002');a3=bytes.fromhex('020000000003')
        for ds,src,dst,bssid in [(0,a2,a1,a3),(0x100,a2,a3,a1),(0x200,a3,a1,a2)]:
            packet=(8|ds).to_bytes(2,'little')+b'\0\0'+a1+a2+a3+b'\0\0'+bytes.fromhex('aaaa03000000888e')
            p,reason=decode(packet,105);self.assertIsNone(reason);self.assertTrue(p['eapol'])
            fmt=lambda x:':'.join(f'{b:02x}' for b in x)
            self.assertEqual((fmt(src),fmt(dst),fmt(bssid)),(p['source'],p['destination'],p['bssid']))
        self.assertIsNone(decode(b'\0',105)[0]);self.assertIsNone(decode(b'\0'*8,127)[0])
