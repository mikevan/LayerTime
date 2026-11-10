import unittest
import json
from evidence_facts import summarize
from wireless_curriculum import frame, AP, OTHER, CLIENT


class EvidenceFactsTests(unittest.TestCase):
    def test_reverse_direction_eapol_matches_target(self):
        exchange=dict(frame(2,500,32),source=CLIENT,destination=AP)
        group=summarize([frame(0,0),exchange])['groups'][0]
        self.assertEqual([2],group['eapolAfterLastDisconnectWithinWindow'])
        self.assertEqual(1,group['observedDisconnectFrameCount'])

    def test_other_network_does_not_form_same_network_link(self):
        groups=summarize([frame(0,0),frame(1,100,32,OTHER)])['groups']
        self.assertTrue(all(not g['eapolAfterLastDisconnectWithinWindow'] for g in groups))
        self.assertEqual([OTHER],next(g for g in groups if g['bssid']==AP)['sameClientOtherBssids'])

    def test_invalid_clock_prevents_ordering(self):
        row=dict(frame(1,100,32),timestampValid=False)
        group=summarize([frame(0,0),row])['groups'][0]
        self.assertFalse(group['allRelevantTimestampsValid'])
        self.assertEqual([],group['eapolAfterLastDisconnectWithinWindow'])

    def test_broadcast_does_not_identify_one_client(self):
        group=summarize([frame(0,0,client='ff:ff:ff:ff:ff:ff')])['groups'][0]
        self.assertEqual('broadcast',group['targetKind'])
        self.assertEqual([],group['sameClientOtherBssids'])

    def test_duplicate_ids_and_bool_ids_rejected(self):
        for rows in ([frame(0,0),frame(0,10)],[dict(frame(0,0),frameId=False)]):
            with self.assertRaises(ValueError):summarize(rows)

    def test_outside_window_and_equal_time_do_not_link(self):
        group=summarize([frame(0,100),frame(1,100,32),frame(2,30101,32)])['groups'][0]
        self.assertEqual([],group['eapolAfterLastDisconnectWithinWindow'])

    def test_malformed_and_unresolved_identity_excluded(self):
        report=summarize([dict(frame(0,0),malformed=True),dict(frame(1,10),network=None)])
        self.assertEqual([],report['groups'])
        self.assertEqual(2,sum(report['excluded'].values()))

    def test_prompt_enrichment_preserves_original_observations(self):
        from evaluate_local_model import with_evidence_facts
        original=[dict(role='user',content='RECORDING DATA\n'+json.dumps(dict(observations=[frame(0,0)])))]
        before=original[0]['content']
        enriched=with_evidence_facts(original)
        self.assertEqual(before,original[0]['content'])
        data=json.loads(enriched[0]['content'].split('\n',1)[1])
        self.assertEqual([frame(0,0)],data['observations'])
        self.assertEqual(1,data['derivedEvidenceFacts']['groups'][0]['observedDisconnectFrameCount'])

    def test_burst_uses_time_window_not_just_total_count(self):
        group=summarize([frame(i,i*4000) for i in range(8)])['groups'][0]
        self.assertFalse(group['disconnectBurstPolicy']['thresholdMet'])
        self.assertEqual(1,group['disconnectBurstPolicy']['peakObservedFrames'])
        self.assertEqual(0,group['disconnectBurstPolicy']['observedSpanMs'])

    def test_actual_span_is_not_confused_with_the_configured_window(self):
        group=summarize([frame(i,i*100) for i in range(7)])['groups'][0]
        burst=group['disconnectBurstPolicy']
        self.assertEqual(600,burst['observedSpanMs'])
        self.assertEqual(3000,burst['windowMs'])

    def test_invalid_exchange_clock_does_not_erase_valid_disconnect_burst(self):
        rows=[frame(i,i*100) for i in range(6)]+[dict(frame(20,900,32),timestampValid=False)]
        group=summarize(rows)['groups'][0]
        self.assertTrue(group['disconnectBurstPolicy']['thresholdMet'])
        self.assertFalse(group['allRelevantTimestampsValid'])


if __name__ == '__main__':unittest.main()
