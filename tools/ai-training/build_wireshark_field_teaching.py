"""Source-linked passive field interpretation candidates, not incident ground truth."""
import hashlib
import json
import shutil
from pathlib import Path
from grounded_contract import canonical
from build_domain_pilot_v3 import LEGEND
from compact_domain_contract import schema,validate

# The source establishes field names and decoded roles. Limits are separately
# authored project guidance, never attributed to Wireshark as incident conclusions.
CARDS=[
 ('receiver','wlan.ra','Receiver address','The decoded receiver address is a distinct address role.','A destination in a captured frame does not prove receipt or identify a physical owner.'),
 ('transmitter','wlan.ta','Transmitter address','The decoded transmitter address is distinct from the receiver role.','A claimed transmitting address does not authenticate a physical sender.'),
 ('source','wlan.sa','Source address','The decoder exposes a source-address role separately from its transmitter-address role.','Preserve both decoded roles rather than treating every source address as the radio transmitter.'),
 ('destination','wlan.da','Destination address','The decoder exposes a destination-address role separately from its receiver-address role.','Preserve the decoder roles rather than replacing every destination with the receiver field.'),
 ('bssid','wlan.bssid','BSS Id','The decoder exposes a BSS identifier as its own field.','Match the client and BSSID together; a shared displayed network name alone is insufficient for that pair.'),
 ('to-ds','wlan.fc.tods','To DS','The frame-control decoder has a To DS flag.','Interpret address roles in frame context rather than applying one address-position rule universally.'),
 ('from-ds','wlan.fc.fromds','From DS','The frame-control decoder has a From DS flag distinct from To DS.','Do not collapse the two flags into one assumed traffic direction.'),
 ('retry','wlan.fc.retry','Retry','The decoder exposes a Retry flag in frame control.','This flag alone does not establish deliberate disruption; preserve sequence and capture context.'),
 ('protected','wlan.fc.protected','Protected flag','The decoder exposes the Protected flag.','A decoded flag alone is not a verified authentication result or proof that every management frame is protected.'),
 ('sequence','wlan.seq','Sequence number','The decoder exposes a sequence-number field.','Sequence values need transmitter, subtype and capture context before deduplication or anomaly claims.'),
 ('fragment','wlan.frag','Fragment number','The decoder exposes a fragment number separately from the sequence number.','Do not discard fragmentation context when comparing records.'),
 ('reason','wlan.fixed.reason_code','Reason code','The decoder distinguishes a reason-code field from a status-code field.','A supplied reason code is not independent verification of the claimed cause or sender.'),
 ('status','wlan.fixed.status_code','Status code','The decoder exposes status codes separately from reason codes.','Preserve the message context; a status value is not a universal explanation for a disconnect.'),
 ('pmf-required','wlan.rsn.capabilities.mfpr','Management Frame Protection Required','The RSN capability decoder exposes a management-frame-protection-required flag.','Do not substitute this advertisement for evidence about the negotiated session or the handling of a particular frame.'),
 ('pmf-capable','wlan.rsn.capabilities.mfpc','Management Frame Protection Capable','The RSN capability decoder distinguishes protection capability from protection requirement.','Capable and required are different claims; neither field alone proves a particular request was accepted.'),
 ('message-number','wlan_rsna_eapol.keydes.msgnr','Message number','The RSNA EAPOL decoder exposes a message-number interpretation.','One decoded message does not establish a complete exchange or a recovered network password.'),
 ('key-type','wlan_rsna_eapol.keydes.key_info.key_type','Key Type','The RSNA key-information decoder exposes a key-type field.','Keep the descriptor and protocol context; an EAPOL label alone does not specify all key-exchange details.'),
 ('key-mic-flag','wlan_rsna_eapol.keydes.key_info.key_mic','Key MIC','The key-information decoder exposes a Key MIC flag.','A flag indicating MIC handling is not itself a successful cryptographic verification result.'),
 ('secure-flag','wlan_rsna_eapol.keydes.key_info.secure','Secure','The key-information decoder exposes a Secure flag.','The field name is not an overall assurance that the network is safe or free from interference.'),
 ('install-flag','wlan_rsna_eapol.keydes.key_info.install','Install','The key-information decoder exposes an Install flag.','An instruction-bearing flag does not prove the endpoint completed the requested operation.'),
 ('key-length','eapol.keydes.key_len','Key Length','The EAPOL key descriptor includes a key-length field.','Do not interpret this as the length of a user-entered network password.'),
 ('replay-counter','eapol.keydes.replay_counter','Replay Counter','The EAPOL key descriptor exposes a replay counter.','The presence of a counter is not proof of a replay attack; assess related messages and context.'),
 ('eapol-type','eapol.type','Type','The EAPOL decoder uses a packet-type field.','Check the decoded type before claiming that every EAPOL record is a key-exchange message.'),
 ('eapol-version','eapol.version','Version','The EAPOL decoder exposes a protocol-version field.','Do not treat this version number as a Wi-Fi signal measurement or a password-strength score.'),
]


def build(root):
    source=root/'external-data/wireshark-teaching-reference-v1';intake=json.loads((source/'intake-manifest.json').read_text())
    out=root/'domain-data/wireshark-field-teaching-v1';out.mkdir(exist_ok=False)
    files={f['path']:f for f in intake['files']};cards=[];examples=[]
    for item in intake['files']:
        assert hashlib.sha256((source/item['path']).read_bytes()).hexdigest()==item['sha256']
    for ident,field,label,fact,limit in CARDS:
        filename='epan/dissectors/packet-eapol.c' if field.startswith('eapol.') else 'epan/dissectors/packet-ieee80211.c'
        text=(source/filename).read_text(encoding='utf-8'); assert 'SPDX-License-Identifier: GPL-2.0-or-later' in text[:800]
        lines=text.splitlines();needle='"'+field+'"'
        matches=[i for i,line in enumerate(lines) if needle in line and '"'+label+'"' in line]
        assert matches, 'Missing source field: '+field
        start=matches[0];excerpt='\n'.join(lines[start:start+4])
        card=dict(id=ident,sourceId='wireshark-passive-field-semantics-v1',field=field,sourceFact=fact,
            projectInterpretationLimit=limit,sourceFactScope='Pinned decoder field registration, not a full protocol-standard claim',
            sourceReference=dict(repository=intake['repository'],revision=intake['revision'],path=filename,
                fileSha256=files[filename]['sha256'],firstLine=start+1,lastLine=start+4,excerpt=excerpt,
                excerptSha256=hashlib.sha256(excerpt.encode()).hexdigest(),url=files[filename]['url']+f'#L{start+1}'),
            licence='GPL-2.0-or-later',trainingApproved=False,independentExpertReview=False)
        cards.append(card)
        for mode,question in [('meaning',f'What does {field} represent, and what must we avoid concluding from it?'),
                              ('limit',f'What is the important interpretation limit when reporting {field}?')]:
            answer=dict(schemaVersion=1,headline=label+' needs context.',significance=fact,limitations=limit,
                passiveNextStep='Preserve the decoded field and inspect the related passive evidence.',factReferences=['capture_context'])
            model_context={k:card[k] for k in ('field','sourceFact','projectInterpretationLimit','sourceFactScope')}
            model_context['sourceReference']={k:card['sourceReference'][k] for k in ('revision','path','firstLine','url')}
            row=dict(id='wireshark-'+ident+'-'+mode,family='wireshark-field-'+ident,sourceId=card['sourceId'],
                datasetVersion='blue-team-compact-wireshark-teaching-v1',task='EXPLAIN_PASSIVE_DECODER_FIELD',taskContractVersion='1.0.0',
                split='unassigned',trainingApproved=False,releaseEligible=False,licence=card['licence'],
                outputSchema=canonical(schema()),sourceCardId=ident,
                messages=[dict(role='system',content=LEGEND),dict(role='user',content=question+'\n'+canonical(dict(facts=[dict(factId='capture_context',value=model_context)]))),
                    dict(role='assistant',content=canonical(answer))])
            validate(row['messages'][-1]['content'],row);examples.append(row)
    for name,data in [('cards.jsonl',cards),('candidates.jsonl',examples)]:
        (out/name).write_text(''.join(canonical(r)+'\n' for r in data),encoding='utf-8')
    shutil.copy2(source/'COPYING',out/'SOURCE-LICENSE.txt');shutil.copy2(source/'intake-manifest.json',out/'source-intake-manifest.json')
    (out/'ATTRIBUTION.md').write_text('# Source attribution\n\nWireshark, Copyright 1998 Gerald Combs; IEEE 802.11 decoder also Copyright 2000 Axis Communications AB and other contributors. File notices identify GPL-2.0-or-later. Source revision and links, field excerpts and hashes are retained in cards.jsonl. Source licence is preserved in SOURCE-LICENSE.txt.\n\nParaphrases and project interpretation limits are authored teaching candidates. These are not Wireshark incident verdicts. No downloaded code was executed. Candidates are unassigned and unapproved for training; source/license and independent semantic review are still separate from this intake.\n',encoding='utf-8')
    manifest=dict(status='source-linked-teaching-candidates-not-admitted',cards=len(cards),questions=len(examples),
        exactDistinctAnswers=len({r['messages'][-1]['content'] for r in examples}),revision=intake['revision'],
        trainingPerformed=False,trainingApproved=False,releaseEligible=False,
        limitations=['Two question formulations share one target per field; 48 rows are only 24 teaching facts.',
            'Field registrations support decoded labels; interpretation limits are explicitly project-authored.',
            'No raw capture labels, incident detection performance or full protocol-standard coverage are established.'],
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':print(json.dumps(build(Path(__file__).parent),indent=2))
