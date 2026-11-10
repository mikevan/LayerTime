"""Original request cases, frozen before inference; no packet attack labels."""
import hashlib
import json
from pathlib import Path
from analytic_intent import audit_record


def draft(follow=30000,gap=3000,minimum=1,scope='DEVICE_AP_PAIR'):
    return dict(schemaVersion=1,status='DRAFT',followOnMs=follow,episodeGapMs=gap,minLinkedEpisodes=minimum,scope=scope)


def nondraft(status):return dict(schemaVersion=1,status=status,reason='AMBIGUOUS_SETTINGS' if status=='CLARIFY' else 'NO_SUPPORTED_ATOM')


def cases():
    # Split units are authored request cases; two phrasings are augmentation.
    return [
      ('defaults','train',draft(),['Build an analytic for disconnect requests followed by EAPOL for the same device and access point. Use the defaults.','Watch for security messages after disconnect requests for a device on the same AP, using the default settings.']),
      ('pair_two','train',draft(minimum=2),['Look for at least two linked episodes for one device and AP. Use a 30 second follow-on window and a 3 second episode gap.','For any single device/AP pair, flag two or more episodes with later EAPOL within 30 seconds; split episodes after gaps exceeding 3 seconds.']),
      ('recording_three','train',draft(minimum=3,scope='RECORDING'),['Count linked disconnect episodes across the whole recording. Report at least three, with the default timing.','Across all devices together in this recording, require three episodes with subsequent same-pair EAPOL. Keep default intervals.']),
      ('half_second','train',draft(500),['Use a 500 millisecond follow-on window for same-pair EAPOL after disconnect requests, otherwise defaults.','Create the disconnect/security-message analytic, waiting up to half a second for the exchange. Keep other settings at their defaults.']),
      ('five_second','train',draft(5000,1000,2),['For one device and AP, require two linked episodes; allow 5 seconds for EAPOL and split requests after a 1 second gap.','Find two disconnect episodes for the same pair with an exchange within five seconds. Start a new episode when requests are more than one second apart.']),
      ('milliseconds','train',draft(750,250,3),['Use 750 ms for follow-on EAPOL, 250 ms to separate episodes, and at least three linked episodes for one device/AP pair.','Make a per-pair analytic requiring three episodes. Wait 750 milliseconds for each later security exchange; split at gaps greater than 250 milliseconds.']),
      ('one_minute','train',draft(60000),['Use one minute to look for EAPOL after disconnect requests for the same pair, with the other defaults.','Build the same-device and AP disconnect-sequence check with a 60 second exchange window. Leave episode splitting and threshold at defaults.']),
      ('merge_long','train',draft(30000,10000,2),['Require two linked episodes for one device/AP pair. Look for EAPOL for 30 seconds and use a 10 second episode gap.','Two episodes for the same pair should trigger the report. Separate episodes only after a gap over ten seconds, allowing thirty seconds for a security exchange.']),
      ('recording_short','train',draft(1500,200,2,'RECORDING'),['Count two linked episodes across the whole recording, using a 1500 ms follow-on window and 200 ms episode gap.','Combine matching episodes across all devices for the threshold of two; wait 1.5 seconds for EAPOL and split requests after 0.2 seconds.']),
      ('four_pair','train',draft(1000,1000,4),['For one device/AP pair require four episodes with EAPOL within a second; split episodes after gaps longer than one second.','Use four linked episodes per pair as the threshold. Both the follow-on window and episode gap should be 1000 milliseconds.']),
      ('minimum_time','train',draft(1,1,1),['Use 1 ms for both time settings in the disconnect/EAPOL analytic, with one linked episode per pair required.','Require a single same-pair linked episode; allow one millisecond for the exchange and split at request gaps over one millisecond.']),
      ('maximum_time','train',draft(3600000,10000,3),['Require three linked episodes per pair. Allow up to 60 minutes for EAPOL and split episodes at gaps over 10 seconds.','For one device and access point, find three episodes with an exchange within one hour. Use a ten-second episode gap.']),
      ('missing_unit','train',nondraft('CLARIFY'),['Create the disconnect/EAPOL analytic with a follow-on window of 5, and otherwise defaults.','Wait up to 12 after a disconnect request for security messages. Use the other default settings.']),
      ('invalid_zero','train',nondraft('CLARIFY'),['Find linked disconnect episodes, with a follow-on window of zero milliseconds.','Set the episode gap to 0 ms for the same-pair disconnect/security-message analytic.']),
      ('ble_only','train',nondraft('UNSUPPORTED'),['Build an analytic that detects a flood of Bluetooth advertisements.','Make a reusable flow to compare BLE advertising rates between devices.']),
      ('fixed_filter','train',nondraft('UNSUPPORTED'),['Make the disconnect/EAPOL analytic permanently filter device 02:00:00:00:03:03.','Only examine access point 02:00:00:00:01:01 and save that exact address as the analytic default.']),
      ('validation_pair','validation',draft(2000,5000,2),['Look for two linked episodes for a single device and AP, allowing EAPOL after two seconds and separating request groups after five seconds.','Per pair, require two episodes. Follow-on window: 2000 ms. Episode gap: 5000 ms.']),
      ('validation_recording','validation',draft(120000,4000,3,'RECORDING'),['Across the complete recording, require three episodes with later same-pair EAPOL within two minutes; split at four-second gaps.','Aggregate all matching episodes for the threshold of three, using 120 seconds for an exchange and 4000 ms for episode separation.']),
      ('conflict','validation',nondraft('CLARIFY'),['Set the EAPOL follow-on window to exactly 100 ms and exactly 500 ms at the same time.','Create the analytic with the minimum number of linked episodes set to both two and five.']),
      ('password_proof','validation',nondraft('UNSUPPORTED'),['Build an analytic that confirms a network password was stolen after EAPOL.','Make the flow prove that a reconnect completed from a single EAPOL frame.']),
      ('fractional_test','test',draft(250,750,2),['For each device and AP separately, require two linked episodes. Use 0.25 seconds for an exchange and 0.75 seconds to split request groups.','Create the sequence analytic with a quarter-second EAPOL window, a 750 millisecond episode gap, and a threshold of two episodes for one pair.']),
      ('boundary_test','test',draft(500,5000,2),['Require two episodes for one device/AP pair. EAPOL must follow within half a second, and requests stay in one episode unless their gap exceeds five seconds.','Use 500 ms follow-on and 5000 ms episode gap, reporting when one pair has at least two linked episodes.']),
      ('ninety_seconds_test','test',draft(90000,2000,3,'RECORDING'),['Count linked episodes across all devices in the recording: at least three, with a 90 second exchange window and 2 second episode gap.','Require three matching episodes overall, not per pair. Set the EAPOL window to one and a half minutes, with a 2000 millisecond request gap.']),
      ('pair_scope_test','test',draft(1000,3000,2),['One device and the same access point must have two episodes with later EAPOL. Allow a second for each exchange; use the default episode gap.','Do not combine episodes from different device/AP pairs for the threshold of two. Look for EAPOL within 1000 ms; leave episode separation at its default.']),
      ('recording_scope_test','test',draft(1000,3000,2,'RECORDING'),['Combine episodes from different device/AP pairs for a recording-wide threshold of two. Use 1000 ms for EAPOL and the default episode gap.','Report two linked episodes in total across this capture, even if different clients are involved. Wait up to one second for each exchange; keep default episode separation.']),
      ('missing_gap_unit_test','test',nondraft('CLARIFY'),['Use the default follow-on window, but split disconnect episodes when the gap exceeds 7.','Require two linked episodes for a pair, and set the episode gap to 15 without assuming a time unit.']),
      ('burst_only_test','test',nondraft('UNSUPPORTED'),['Count bursts of six deauthentication frames in three seconds, without looking for EAPOL or episodes.','Build a disconnect-rate check only. Report ten disconnect requests in a second and ignore security exchanges.']),
      ('signal_test','test',nondraft('UNSUPPORTED'),['Compare the signal strength of disconnect frames against access-point beacons.','Build a reusable analytic for RSSI differences between deauthentication frames and the observed beacon stream.']),
    ]


def build(root):
    template=json.loads((root/'native-template.json').read_text(encoding='utf-8'))
    output=root/'seeds.jsonl'
    if output.exists():raise ValueError('Preserve frozen request data')
    rows=[]
    for family,split,expected,prompts in cases():
        for variant,prompt in enumerate(prompts):
            row=dict(id=f'intent-v1-{family}-{variant}',family=family,split=split,task='PLAN_ANALYTIC',
                sourceId='layertime-authored-intents-v1',labelStatus='authored-intent-native-compiler-checked-development-only',
                expectedIntent=expected,outputSchema=template['outputSchema'],
                messages=[template['messages'][0],dict(role='user',content=prompt),dict(role='assistant',content=json.dumps(expected,separators=(',',':')))])
            audit_record(row);rows.append(row)
    output.write_text('\n'.join(json.dumps(r,ensure_ascii=False) for r in rows)+'\n',encoding='utf-8')
    print(json.dumps(dict(rows=len(rows),requestCases=len(cases()),splits={s:sum(r['split']==s for r in rows) for s in ('train','validation','test')},sha256=hashlib.sha256(output.read_bytes()).hexdigest())))


if __name__=='__main__':build(Path(__file__).parent/'intent-data/v1')
