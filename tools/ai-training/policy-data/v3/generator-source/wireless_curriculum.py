"""Small authored corrective curriculum, not independently collected field data.

No variant expansion: each row represents one explicit teaching case. This corpus
must not be advertised as broad domain training or used to score a trained model.
"""
import hashlib
import json
from pathlib import Path
from evaluate_local_model import validate_assessment

AP = '02:00:00:00:01:01'
OTHER = '02:00:00:00:02:02'
CLIENT = '02:00:00:00:03:03'
SECOND = '02:00:00:00:04:04'
SYSTEM = ('Review only the supplied passive wireless observations. Lead with the important finding in plain language, '
          'then give technical evidence and limits. Distinguish a disconnect request from a confirmed disconnect, '
          'and EAPOL from a completed reconnect or a stolen password. Addresses in frames are claimed identities. '
          'A repeated disconnect pattern can warrant increased threat posture without proving an attack. '
          'Do not invent frame counts, reason meanings, unseen traffic, attacker identity, or network ownership. '
          'Return the specified JSON assessment. An empty issue list means no reportable issue established by this sample, not guaranteed safety.')
REFERENCE = 'https://raw.githubusercontent.com/wireshark/wireshark/master/epan/dissectors/packet-ieee80211.c'


def frame(identity, millis, kind=12, network=AP, client=CLIENT, reason=7):
    return dict(frameId=identity, timeNanos=millis*1_000_000, timestampValid=True,
                malformed=False, typeSubtype=kind, source=network, destination=client,
                network=network, eapol=kind == 32, reasonCode=reason if kind in (10, 12) else None,
                sequence=identity, retry=False, rssi=-50, channel=6, ssid=None,
                managementProtectionCapable=None, managementProtectionRequired=None)


def build():
    # Explicit independent cases, not renamed versions of the original fixture pages.
    cases = []
    def add(name, observations, headline=None, explanation=None, **coverage):
        cases.append(dict(name=name, observations=observations,
                          headline=headline, explanation=explanation, coverage=coverage))

    add('same_client_same_bssid', [frame(i, i*100) for i in range(6)] + [frame(6, 900, 32)],
        'Repeated disconnect requests were followed by a security exchange for the same device and network',
        f'Device {CLIENT} was addressed by six deauthentication frames claiming network {AP}, followed by one EAPOL frame for that same device and BSSID. This is a significant disruption pattern; increase your threat posture and examine the surrounding traffic. The frames request disconnection; they do not alone prove the device disconnected. EAPOL shows a security exchange, not a completed reconnect, a complete captured handshake, or password loss. The physical sender is not authenticated by its displayed address.')
    add('same_client_different_bssid', [frame(i, i*90) for i in range(5)] + [frame(5, 800, 32, OTHER)],
        'The later security exchange belongs to a different network',
        f'Five deauthentication frames address {CLIENT} on BSSID {AP}. The later EAPOL frame involves the same client but BSSID {OTHER}. Do not chain these as a reconnect to the targeted network. Roaming or another connection is possible; the recording does not establish the cause. The repeated disconnect requests still deserve attention.')
    add('different_client_same_bssid', [frame(i, i*80) for i in range(4)] + [frame(4, 600, 32, AP, SECOND)],
        'The security exchange involves a different device',
        f'Four deauthentication frames address {CLIENT}; the later EAPOL frame involves {SECOND}. Both name BSSID {AP}, but that network match does not make the client match. This sample does not show the targeted device reconnecting. Review the repeated disconnect requests without assigning the other device\'s exchange to this target.')
    add('eapol_precedes_disconnects', [frame(0, 0, 32)] + [frame(i, 400+i*100) for i in range(1, 5)],
        'The security exchange happened before the disconnect requests',
        'One EAPOL frame precedes four deauthentication frames for the same device and BSSID. It cannot be described as a reconnect caused by those later requests. Repeated requests may indicate disruption, but the observed ordering does not establish a follow-on exchange.')
    add('inactivity_reason', [frame(11, 0, 10, reason=4)],
        'A disconnect request reports inactivity; this sample does not establish an attack',
        'One disassociation frame reports reason code 4, meaning inactivity. That is the sender\'s stated reason, not an independently verified cause. A single such frame does not establish deliberate disruption. No completed disconnect or reconnect is established by this frame alone.')
    add('nonassociated_reason', [frame(7, 0)],
        'A disconnect request reports a device that was not associated',
        'One deauthentication frame reports reason code 7: a class-3 frame received from a nonassociated station. Code 7 does not mean inactivity. This is the stated reason in a frame, not proof of an attack or of the transmitter\'s identity.')
    add('broadcast_target', [frame(i, i*50, client='ff:ff:ff:ff:ff:ff') for i in range(9)],
        'Repeated disconnect requests were broadcast to devices on this network',
        f'Nine deauthentication frames claim BSSID {AP} and the broadcast destination. This is a significant disruption pattern that warrants increased threat posture. There is no single targeted client MAC in these frames, and the recording does not establish how many devices received or acted on the requests.')
    add('same_ssid_two_bssids', [dict(frame(1, 0, 8), ssid='CampNet'), dict(frame(2, 100, 8, OTHER), ssid='CampNet')],
        'Two access points advertise the same network name; ownership is unresolved',
        'Two beacon frames advertise CampNet with different BSSIDs. Legitimate networks can use multiple access points with the same name. These observations justify comparison with known network details, but do not alone establish an evil twin, malicious ownership, or that a device joined either access point.')
    add('partial_capture_no_eapol', [frame(i, i*60) for i in range(8)],
        'Repeated disconnect requests are visible; the later outcome was not captured',
        'Eight deauthentication frames address the same device and BSSID. No EAPOL appears in the supplied observations. The capture ends immediately after the requests, so absence here does not establish that the device failed to reconnect or that no security exchange occurred elsewhere.', endsImmediatelyAfterRequests=True)
    add('invalid_clock', [dict(frame(3, 1000), timestampValid=False), dict(frame(9, 0, 32), timestampValid=False)],
        'The device and network match, but the event order cannot be established',
        'One deauthentication frame and one EAPOL frame name the same device and BSSID. Their timestamps are marked invalid. Frame identifiers are references, not reliable timestamps; do not describe either event as following or causing the other.')
    add('ordinary_exchange', [frame(0, 0, 32), frame(1, 100, 32)])
    add('unknown_reason', [frame(4, 0, reason=65535)],
        'A disconnect request carries an unrecognized reason value',
        'One deauthentication frame carries reason value 65535. No supported meaning is supplied for this value; do not translate it into inactivity or a named attack. The observation warrants technical review, while its cause and whether a device disconnected remain unresolved.')

    schema = dict(type='object', required=['schemaVersion','issues'], properties={
        'schemaVersion':dict(const=1), 'issues':dict(type='array', items=dict(type='object',
        required=['headline','explanation','frameIds'], properties=dict(headline=dict(type='string'),
        explanation=dict(type='string'),frameIds=dict(type='array',items=dict(type='integer'))),additionalProperties=False))},additionalProperties=False)
    rows = []
    for case in cases:
        data = dict(observations=case['observations'], relatedEpisodes=[], coverage=case['coverage'])
        ids = [f['frameId'] for f in case['observations']]
        issues = [] if not case['headline'] else [dict(headline=case['headline'], explanation=case['explanation'], frameIds=ids)]
        answer = json.dumps(dict(schemaVersion=1,issues=issues))
        row = dict(id='corrective-v1-'+case['name'], family='corrective-v1-'+case['name'],
            sourceId='layertime-authored-corrective-v1', task='REVIEW_CAPTURE', split='train',
            labelStatus='authored-rule-checked-not-independent-expert-reviewed', evidenceIds=ids,
            outputSchema=json.dumps(schema), references=[REFERENCE],
            messages=[dict(role='system', content=SYSTEM),dict(role='user',content='RECORDING DATA\n'+json.dumps(data)),
                      dict(role='assistant',content=answer)])
        validate_assessment(answer,row)
        rows.append(row)
    return rows


if __name__ == '__main__':
    out = Path(__file__).with_name('corrective-data');out.mkdir(exist_ok=True)
    rows=build()
    raw=''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows)
    (out/'candidates.jsonl').write_text(raw,encoding='utf-8')
    manifest=dict(examples=len(rows), families=len(rows), sha256=hashlib.sha256((out/'candidates.jsonl').read_bytes()).hexdigest(),
        purpose='small corrective curriculum; not broad domain qualification', releaseEligible=False,
        trainingApproved=False, provenance='Project-authored synthetic cases; no private capture data.',
        limitations=['No independent expert review.', 'All cases are training candidates; none are an independent evaluation set.',
                     'Reason-code reference does not validate every authored interpretation.',
                     'Some cases deliberately request a coverage clarification rather than an attack warning.'])
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    sections=['# Corrective wireless reporting examples',
              'Twelve authored synthetic training candidates. These are not field evidence or an independent evaluation set. No model has been trained on them.',
              'Review the observations, the proposed wording, and whether its significance is clear. Technical correctness and readability both matter.']
    for row in rows:
        data=json.loads(row['messages'][1]['content'].split('\n',1)[1])
        answer=json.loads(row['messages'][-1]['content'])
        sections.append('## '+row['id'].removeprefix('corrective-v1-').replace('_',' '))
        sections.append('| Frame | Time valid | Time (ms) | Type/subtype | BSSID | Destination | EAPOL | Reason |\n|---|---|---|---|---|---|---|---|')
        sections.append('\n'.join(f"| {f['frameId']} | {f['timestampValid']} | {f['timeNanos']//1_000_000} | {f['typeSubtype']} | {f['network']} | {f['destination']} | {f['eapol']} | {f['reasonCode']} |" for f in data['observations']))
        if data['coverage']:sections.append('Coverage: '+json.dumps(data['coverage']))
        for issue in answer['issues']:
            sections.extend(['**'+issue['headline']+'**',issue['explanation']])
        if not answer['issues']:sections.append('No reportable issue established from these observations. This does not guarantee network safety.')
    (out/'review.md').write_text('\n\n'.join(sections)+'\n',encoding='utf-8')
    print(json.dumps(manifest))
