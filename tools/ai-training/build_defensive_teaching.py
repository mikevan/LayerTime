"""Authored plain-language definitions and explicit telemetry limits.

Definitions paraphrase the pinned primary-source concepts. Prerequisites and
limits are LayerTime design interpretations, kept separate from source claims.
No case labels, executable actions or app capabilities are invented.
"""
import hashlib
import json
from pathlib import Path

# Plain definition; required observations; authored interpretation limit.
TEACHING={
'D3-RTA':('Review remote procedure-call traffic to recognize expected operations and suspicious activity.',
    ['Decoded remote procedure-call messages'], 'Wireless headers alone do not reveal an encrypted remote procedure call.'),
'D3-IPCTA':('Compare messages between software processes with expected protocol activity to find unusual behavior.',
    ['Inter-process communication records'], 'These records are not normally provided by a radio packet capture.'),
'D3-RTSD':('Look for an unauthorized live remote terminal session by examining host network traffic.',
    ['Network traffic identifying terminal sessions','Authorization context'], 'A remote session is not proven unauthorized merely because it exists.'),
'D3-RPA':('Look for a host forwarding traffic between an internal network and an external network.',
    ['Traffic across relevant network boundaries','Known internal and external network roles'], 'A single observed wireless endpoint does not establish a relay path.'),
'D3-PMAD':('Collect details about network messages and look for statistical outliers.',
    ['Protocol metadata','A comparison population or baseline'], 'An unusual pattern alone does not identify malicious intent.'),
'D3-PHDURA':('Compare how much data a host downloads with how much it uploads to look for unusual behavior.',
    ['Directional byte counts for the same host','A defined observation interval'], 'Incomplete channel coverage can distort the observed ratio.'),
'D3-DNSTA':('Examine domain names and DNS records to assess whether they may lead to undesirable hosts.',
    ['DNS names and records'], 'A Wi-Fi name or MAC address is not a DNS record.'),
'D3-APCA':('Review remote commands carried by application protocols for unauthorized activity.',
    ['Decoded application commands','Authorization context'], 'Encrypted payloads cannot be described as known commands without supporting decryption or endpoint evidence.'),
'D3-ISVA':('Examine the volume of inbound sessions or connection attempts.',
    ['Inbound session or attempt records','A defined interval'], 'Connection attempts are not automatically successful connections.'),
'D3-NTCD':('Establish groups of hosts that normally communicate and look for unusual communication between those groups.',
    ['Communication relationships across hosts','A baseline grouping'], 'A new relationship is a deviation to investigate, not proof of an attack.'),
'D3-CSPP':('Compare request and response contents with a normal profile to find outliers.',
    ['Paired request and response payloads','A baseline payload profile'], 'Packet lengths alone do not reveal encrypted request and response contents.'),
'D3-ANAA':('Compare administrative network activity with a baseline to find unauthorized use.',
    ['Administrative protocol activity','Baseline and authorization context'], 'An unfamiliar device address does not establish unauthorized administration.'),
'D3-CAA':('Examine failed network connections for signs of unauthorized activity.',
    ['Connection outcomes that establish failure'], 'An EAPOL frame alone does not establish that authentication or a connection failed.'),
'D3-NTSA':('Compare observed network traffic with known signatures.',
    ['Observed traffic','Documented matching signatures'], 'Failure to match a known signature does not establish safety.'),
'D3-CA':('Examine certificates and related traffic or logs for misconfiguration or impersonation.',
    ['PKI certificate fields','Relevant traffic or logs'], 'A network name or BSSID is not itself a certificate.'),
'D3-PCA':('Review certificates obtained from observed traffic or another passive source for unauthorized activity.',
    ['Passively obtained certificate data'], 'This does not authorize connecting to servers or collecting certificates actively.'),
'D3-SDA':('Examine user-session duration for signs of unauthorized activity.',
    ['Session boundaries tied to the same session'], 'The time between two captured frames is not necessarily the duration of a user session.'),
'D3-AZET':('Compare authorization events with a baseline user profile.',
    ['Authorization events','A baseline profile'], 'Authorization decisions differ from authentication exchanges; do not substitute one for the other.'),
'D3-ANET':('Compare authentication events with a baseline user profile.',
    ['Authentication event records','A baseline profile'], 'Observed security-exchange traffic does not by itself establish a successful login.'),
'D3-RAPA':('Examine which resources a user accesses to identify unauthorized activity.',
    ['Resource access records','Authorization context'], 'A wireless address alone does not identify a person or their authorized resource access.'),
'D3-UDTA':('Examine how much data a user transfers.',
    ['Transfer measurements tied to a user or session'], 'Traffic observed from an address is not automatically attributable to a particular person.'),
'D3-IPRA':('Assess an IP address using reputation information.',
    ['An IP address','A dated reputation source'], 'Reputation is contextual and time-sensitive; it is not proof of what happened in this capture.'),
'D3-DNRA':('Assess a domain name using reputation information.',
    ['A domain name','A dated reputation source'], 'A domain reputation result cannot be assigned directly to a Wi-Fi name or MAC address.'),
'D3-IAA':('Check whether known malicious identifiers are present in a system.',
    ['Documented malicious identifiers','Comparable system observations'], 'An identifier match needs context and does not identify the physical person operating a device.'),
'D3-HD':('Compare strings to identify deceptive lookalike text.',
    ['The actual strings being compared','A legitimate comparison target'], 'Similar Wi-Fi names deserve comparison but do not alone establish malicious ownership.')}


def build(source):
    rows=[json.loads(line) for line in (source/'concepts.jsonl').read_text(encoding='utf-8').splitlines()]
    manifest=json.loads((source/'manifest.json').read_text(encoding='utf-8'))
    if hashlib.sha256((source/'concepts.jsonl').read_bytes()).hexdigest()!=manifest['sha256']:
        raise ValueError('Concept pack changed')
    if {row['id'] for row in rows}!=set(TEACHING):raise ValueError('Teaching identities differ from source pack')
    return [dict(id='teaching-'+row['id'],sourceConceptId=row['id'],sourceName=row['name'],
        sourceDefinition=row['definition'],sourceOntologyNode=row['ontologyNode'],sourceVersion=row['sourceVersion'],
        question=f"What does {row['name']} mean, and what evidence would it need?",
        plainDefinition=TEACHING[row['id']][0],authoredTelemetryPrerequisites=TEACHING[row['id']][1],
        authoredInterpretationLimit=TEACHING[row['id']][2],
        transformation='Plain-language paraphrase and separately identified LayerTime interpretation, authored 2026-11-10.',
        independentExpertReview=False,trainingApproved=False,releaseEligible=False) for row in rows]


if __name__=='__main__':
    source=Path(__file__).parent/'defensive-concepts/v1'
    path=source/'teaching-cards.jsonl'
    if path.exists():raise SystemExit('Use a new version for teaching changes')
    rows=build(source);path.write_text(''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows),encoding='utf-8')
    manifest=dict(cards=len(rows),originalConcepts=len(rows),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        sourcePackSha256=hashlib.sha256((source/'concepts.jsonl').read_bytes()).hexdigest(),
        sourceNotice='TERMS-OF-USE.html and SOURCE-NOTICES.json accompany the pack',trainingApproved=False,
        releaseEligible=False,limitations=['Authored teaching candidates, not incident labels or independent expert-reviewed guidance.',
            'Telemetry prerequisites and limits are product-design interpretations, not verbatim MITRE claims.',
            'No claim that the current Builder implements every source technique.'])
    (source/'teaching-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
    print(json.dumps(manifest))
