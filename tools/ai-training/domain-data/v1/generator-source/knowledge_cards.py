"""Authored defensive teaching cards grounded in selected primary-source pages.

Answers are paraphrases, not scraped instruction labels. Product applications
are explicitly separated from source facts. No capture outcome labels are made.
"""
import hashlib
import json
import re
from pathlib import Path

# id, source, PDF page, supporting anchor, question, source-backed answer,
# LayerTime application (an authored design interpretation, not a NIST claim).
CARDS=[
('false-positive','94',17,'incorrectly identifies benign activity','What is a false warning?',
 'A false positive occurs when normal activity is incorrectly identified as malicious.',
 'Evaluate warnings on normal recordings as well as recordings containing labelled incidents.'),
('false-negative','94',17,'fails to identify malicious activity','What is a missed warning?',
 'A false negative occurs when malicious activity is not identified.',
 'An empty issue list must be checked against the evidence rather than treated as proof of safety.'),
('detection-tradeoff','94',17,'reducing the occurrences of one','Does reducing missed warnings automatically improve every detection metric?',
 'Reducing false negatives can increase false positives. Detection tuning requires measuring both.',
 'Report missed-warning and false-warning measurements separately; a single accuracy number can hide the tradeoff.'),
('detection-methods','94',17,'multiple detection methodologies','Why combine detection methods?',
 'Detection systems can combine signature-based detection, anomaly detection and stateful protocol analysis to broaden coverage.',
 'Combine exact evidence rules and behavioral models; evaluate each contribution and their combined behavior.'),
('signature-known','94',18,'known threats','What does a signature detector recognize?',
 'Signature detection compares observed events with patterns associated with known threats. Its coverage is limited when unfamiliar or changed patterns do not match.',
 'A failure to match a known analytic is not proof that a recording is harmless.'),
('signature-state','94',18,'multiple events','Why might an isolated packet check miss a sequence?',
 'A simple signature comparison may lack the memory and protocol-state context needed to connect multiple events.',
 'Keep client, network and timing context when reviewing disconnect requests and security exchanges.'),
('anomaly-baseline','94',18,'normal behavior','What does an anomaly detector compare?',
 'An anomaly detector compares observations with a profile of expected activity and identifies significant deviations.',
 'A deviation should be explained as a change from the measured baseline; it is not automatically evidence of malicious intent.'),
('static-drift','94',19,'static profile will eventually','Why can an old normal-activity profile become inaccurate?',
 'Normal system and network behavior changes. A static profile can become inaccurate and require regeneration.',
 'Measure transfer between recording sessions and environments before enabling a baseline model.'),
('profile-contamination','94',19,'including malicious activity','Can a normal-activity training profile contain bad examples?',
 'Malicious activity can accidentally enter the initial or updated normal profile, weakening its usefulness.',
 'Review the provenance and conditions of benign training recordings; do not label them benign solely because no alert fired.'),
('rare-benign','94',19,'maintenance activity','Why can unfamiliar normal activity trigger an anomaly alert?',
 'A legitimate activity absent from baseline training can differ greatly from the profile and generate a false positive.',
 'Include varied benign activity and preserve held-out normal sessions for evaluation.'),
('stateful-pairing','94',19,'pairing requests with responses','How does stateful analysis establish the outcome of an exchange?',
 'Stateful protocol analysis tracks protocol state and pairs requests with corresponding responses to interpret what happened.',
 'Do not equate one security-exchange frame with completed authentication or a successful connection.'),
('model-implementation','94',20,'models need to be updated','Why must protocol models be maintained?',
 'Protocol versions and implementation differences can diverge from a detector\'s model; the model must reflect those changes.',
 'Keep protocol coverage and unsupported variants explicit instead of silently applying older assumptions.'),
('protocol-valid-flood','94',20,'many benign actions','Can harmful traffic consist of individually acceptable protocol actions?',
 'A sequence can cause denial of service even when its individual actions do not violate a protocol model.',
 'Measure frequency and behavior over time as well as the validity of individual frames.'),
('passive-attacker','153',16,'does not generate, alter, or disrupt','Can radio monitoring alone reveal every passive eavesdropper?',
 'A strictly passive listener generates no transmissions for a radio monitor to detect. Observed radio traffic cannot establish the absence of such a listener.',
 'Never claim that no one captured a security exchange solely because no suspicious transmitter was observed.'),
('traffic-patterns','153',16,'patterns of communication','Why can traffic patterns matter beyond packet contents?',
 'Traffic analysis can reveal information through communication patterns, even when message content is not the focus.',
 'Preserve behavioral trends and timing context alongside frame-level inspection.'),
('dos-alternatives','153',16,'occur inadvertently','Does disrupted wireless service always imply intentional interference?',
 'Denial of service can result from unintentional interference as well as deliberate high-rate traffic.',
 'Describe the observed disruption pattern and unresolved cause separately; increased latency alone does not identify an attacker.'),
('rogue-consequence','153',16,'rogue WLAN devices','Why can an unauthorized access point matter?',
 'A rogue access point can create an unauthorized path into a network and expose clients that connect to it.',
 'Explain the possible consequence after checking identity and ownership evidence; a shared network name alone does not establish authorization.'),
('passive-sensor','153',17,'simply sniffing WLAN','Does a dedicated wireless sensor have to transmit?',
 'Dedicated wireless monitoring sensors can operate passively and observe traffic without forwarding user network traffic.',
 'Keep passive observation distinct from active network intervention in the product design.'),
('mobile-sensor','153',17,'portable','Can wireless monitoring use a portable sensor?',
 'A mobile monitoring sensor can be carried between locations rather than installed at one fixed site.',
 'Moving the Hunter can change what it observes; changing capture conditions should accompany behavioral comparisons.'),
('tool-capabilities','153',18,'not the labels','Must all wireless monitoring use the same named tool?',
 'Monitoring capabilities matter more than the labels or categories assigned to the tools providing them.',
 'Wireshark is one evidence source. Timeline analysis and other passive tools can contribute complementary observations.'),
('join-failures','153',18,'many failed attempts','Why might repeated failed network joins deserve attention?',
 'Many failed attempts to join a wireless network in a short period are an unusual usage pattern worth monitoring.',
 'Distinguish observed failed responses from merely seeing security-exchange frames; failure needs supporting evidence.'),
('event-counts','153',18,'counting events','Why count disconnect-related events over time?',
 'Counting events over intervals and comparing them with thresholds can help identify denial-of-service conditions; frequent session terminations can be a warning sign.',
 'Count observed disconnect requests accurately and report the interval. Do not claim that every request terminated a session.'),
('coverage-gaps','153',19,'outside the WIDPS','Why can one monitoring location miss wireless activity?',
 'Monitoring coverage can be incomplete, and mobile assessment can examine areas outside installed sensor coverage.',
 'Missing frames in one recording do not establish that the same activity did not occur elsewhere.'),
('changing-environment','153',19,'physical changes','Can changing surroundings affect observed wireless signals?',
 'Physical changes to a site can affect wireless signal strength and propagation.',
 'A signal-strength change must not be presented as an attacker location or intent without additional evidence.')]


def normalize(text):return re.sub(r'\s+',' ',text).lower()


def build(library):
    manifest=json.loads((library/'manifest.json').read_text(encoding='utf-8'))
    sources={d['id']:d for d in manifest['documents']};rows=[]
    for name,short,page,anchor,question,answer,application in CARDS:
        source='nist-sp800-'+short
        doc=sources[source]
        pdf=library/(source+'.pdf')
        if hashlib.sha256(pdf.read_bytes()).hexdigest()!=doc['sha256']:raise ValueError('Changed source document')
        pages=json.loads((library/(source+'.pages.json')).read_text(encoding='utf-8'))
        text=pages[page-1]['text']
        if normalize(anchor) not in normalize(text):raise ValueError(f'Anchor missing for {name}, page {page}')
        rows.append(dict(id=name,question=question,sourceFact=answer,productApplication=application,
            sourceId=source,sourceSha256=doc['sha256'],pdfPage=page,supportAnchor=anchor,
            pageTextSha256=hashlib.sha256(text.encode()).hexdigest(),sourceDate=doc['date'],scope=doc['scope'],
            reviewStatus='agent-authored-and-source-checked; not independently expert reviewed',
            purpose='development teaching material; not field ground truth',releaseEligible=False))
    return rows


if __name__=='__main__':
    root=Path(__file__).parent
    rows=build(root/'external-data/nist-reference-library-v1')
    out=root/'knowledge-data';out.mkdir(exist_ok=True)
    raw=''.join(json.dumps(r,sort_keys=True)+'\n' for r in rows)
    (out/'cards.jsonl').write_text(raw,encoding='utf-8')
    (out/'manifest.json').write_text(json.dumps(dict(cards=len(rows),originalConcepts=len(rows),
        sha256=hashlib.sha256((out/'cards.jsonl').read_bytes()).hexdigest(),releaseEligible=False,
        note='Source anchors establish traceability, not automated entailment or independent review.'),indent=2),encoding='utf-8')
    print(json.dumps(dict(cards=len(rows),originalConcepts=len(rows))))
