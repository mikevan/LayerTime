"""Pinned decoder facts plus explicitly authored defensive interpretation lessons."""
import hashlib
import json
import shutil
from pathlib import Path
from grounded_contract import canonical
from compact_domain_contract import schema,validate
from build_domain_pilot_v3 import LEGEND

H='epan/dissectors/packet-eapol.h';C='epan/dissectors/packet-eapol.c';W='epan/dissectors/packet-ieee80211.c'
FACTS=[
 ('eap-packet',H,'#define EAPOL_EAP ',1,'EAPOL packet type zero is named EAP in the pinned decoder.','An EAPOL observation is not automatically an EAPOL-Key packet.'),
 ('start',H,'#define EAPOL_START ',1,'EAPOL packet type one is Start.','A Start packet alone does not establish a key exchange or a completed connection.'),
 ('logoff',H,'#define EAPOL_LOGOFF ',1,'EAPOL packet type two is Logoff.','Do not rename this packet as an 802.11 deauthentication frame or a key message.'),
 ('key',H,'#define EAPOL_KEY ',1,'EAPOL packet type three is Key.','Its packet type does not establish the descriptor, message number, complete exchange or credential loss.'),
 ('asf',H,'#define EAPOL_ENCAP_ASF_ALERT ',1,'EAPOL packet type four is an encapsulated ASF alert.','Do not force every EAPOL packet into a four-message Wi-Fi key-exchange narrative.'),
 ('mka',H,'#define EAPOL_MKA ',1,'EAPOL packet type five is MKA.','MKA is not the packet-type label for an RSN key message.'),
 ('announcement-generic',H,'#define EAPOL_ANNOUNCEMENT_GENERIC ',1,'EAPOL packet type six is a generic announcement.','The EAPOL label alone is insufficient to identify a key message.'),
 ('announcement-specific',H,'#define EAPOL_ANNOUNCEMENT_SPECIFIC ',1,'EAPOL packet type seven is a specific announcement.','Report the decoded type instead of assuming a key exchange.'),
 ('announcement-request',H,'#define EAPOL_ANNOUNCEMENT_REQUEST ',1,'EAPOL packet type eight is an announcement request.','This packet-type name is distinct from a Request flag within a key descriptor.'),
 ('descriptor-rc4',H,'#define EAPOL_RC4_KEY ',1,'The pinned source names key-descriptor type one RC4 and marks it deprecated.','Descriptor type and EAPOL packet type are different fields; retain the source version.'),
 ('descriptor-rsn',H,'#define EAPOL_RSN_KEY ',1,'The pinned source assigns value two to the RSN key-descriptor type.','A descriptor value does not identify a whole captured exchange or prove its outcome.'),
 ('descriptor-wpa',H,'#define EAPOL_WPA_KEY ',1,'The pinned source assigns value 254 to the WPA key-descriptor type.','Do not treat WPA and RSN descriptor values as the same observed field value.'),
 ('request-precedence',W,'  if (keyinfo & KEY_INFO_REQUEST_MASK) {',5,'The pinned decoder checks the key Request flag before its pairwise-message classification branch.','A key request should not be forced into a numbered four-way message merely because the pairwise bit is present.'),
 ('pairwise-scope',W,'  } else if (keyinfo & KEY_INFO_KEY_TYPE_MASK) {',7,'The pairwise classification branch considers Install and Key ACK flags and explicitly avoids relying on MIC alone.','The MIC flag alone is not a portable message-number or successful-verification rule.'),
 ('group-scope',W,'      col_set_str(pinfo->cinfo, COL_INFO, "Key (Group Message 1 of 2)");',8,'The pinned decoder distinguishes two-message group-key labels from four-message pairwise labels.','A group message numbered one or two is not automatically a pairwise message with that number.'),
 ('message-heuristics',W,'      /* We check the key length to differentiate between message 2 and 4 and just hope that',14,'The pinned implementation documents special cases and implementation differences when distinguishing pairwise messages two and four.','Treat decoder message labels as versioned interpretations and retain the underlying evidence.'),
 ('generated-number',W,'      msg_type = DOT11DECRYPT_HS_MSG_TYPE_GHS_2;',5,'The decoder marks the generated message-number tree item as generated.','A generated message number is not proof that all other messages in the exchange were captured.'),
 ('declared-length',C,'  len = EAPOL_HDR_LEN + eapol_len;',2,'The decoder derives the EAPOL total length from a fixed header plus the declared body length.','Trailing capture bytes cannot establish that missing declared body bytes were observed.'),
]

CASES=[
 ('non-key-start',['start','key'],dict(eapolTypeCode=1),
  'This is an EAPOL Start packet, not a Key packet.','The decoded type does not support reporting a captured RSN key exchange.',
  'A later exchange or connection outcome is not supplied.','Inspect the related timeline without relabelling this packet.'),
 ('non-key-eap',['eap-packet','key'],dict(eapolTypeCode=0),
  'An EAP packet was observed within EAPOL.','The EAPOL wrapper alone does not make this a Key packet.',
  'The supplied type does not establish a complete authentication process.','Keep EAPOL type and the observed exchange context separate.'),
 ('logoff-vs-deauth',['logoff'],dict(eapolTypeCode=2,ieee80211DeauthObserved=False),
  'The supplied event is EAPOL Logoff.','No 802.11 deauthentication observation is supplied, so those event types must remain distinct.',
  'A Logoff observation alone does not establish the endpoint outcome or hostile intent.','Report the actual event type and preserve the surrounding evidence.'),
 ('mka-vs-rsn',['mka','descriptor-rsn'],dict(eapolTypeCode=5),
  'The decoded EAPOL type is MKA.','This is not the EAPOL packet-type label for an RSN Key packet.',
  'Do not infer a four-way Wi-Fi handshake from the generic EAPOL label.','Check the packet type before selecting a key-exchange analytic.'),
 ('packet-vs-descriptor',['key','descriptor-rsn'],dict(eapolTypeCode=3,keyDescriptorType=2),
  'A Key packet with an RSN descriptor was observed.','The outer packet type is three and the separate descriptor value is two.',
  'Neither value is a handshake-message count or evidence of credential recovery.','Show the two decoded fields with their names.'),
 ('descriptor-difference',['descriptor-rsn','descriptor-wpa'],dict(observationA=dict(eapolTypeCode=3,keyDescriptorType=2),observationB=dict(eapolTypeCode=3,keyDescriptorType=254)),
  'Both are Key packets, but their descriptor types differ.','One supplied descriptor is RSN and the other is WPA; a shared outer type does not erase that distinction.',
  'Different descriptor values alone do not establish two attacks or two complete sessions.','Preserve descriptor scope when grouping related messages.'),
 ('single-numbered-message',['generated-number'],dict(decodedMessageLabel='pairwise message 3 of 4',otherExchangeMessagesObserved=False),
  'Only one message from a numbered exchange is supplied.','A message-three label does not mean messages one, two and four were also captured.',
  'Exchange completeness, endpoint acceptance and credential loss are unresolved.','Look for separately cited messages with matching identity and exchange context.'),
 ('four-records-not-four-messages',['generated-number'],dict(decodedLabels=['pairwise message 1 of 4']*4),
  'Four records share the same message-one label.','Four captured records are not evidence of four distinct numbered handshake messages.',
  'Retries, repeated attempts and capture duplicates are not resolved by the count alone.','Compare identities, time, sequence context and the original records.'),
 ('all-labels-not-outcome',['generated-number'],dict(decodedLabels=['pairwise message 1 of 4','pairwise message 2 of 4','pairwise message 3 of 4','pairwise message 4 of 4'],singleExchangeIdentityVerified=False),
  'All four labels appear, but one coherent exchange has not been established.','The supplied labels still need identity, order and exchange-context checks before grouping them as one exchange.',
  'Labels alone do not establish cryptographic validity, successful endpoint processing or password compromise.','Verify the grouping against the source records rather than counting labels alone.'),
 ('group-vs-pairwise',['group-scope'],dict(decodedLabel='group message 1 of 2'),
  'The decoder reports a group-key message.','Its message-one label belongs to a two-message group sequence, not the four-message pairwise sequence.',
  'The matching second message and endpoint result are not supplied.','Keep group and pairwise message scope visible in the report.'),
 ('request-not-numbered',['request-precedence'],dict(keyRequestFlag=True,pairwiseFlag=True),
  'The key Request flag takes priority in this decoder branch.','The pairwise flag does not override the earlier Request handling in the pinned implementation.',
  'No numbered four-way classification or successful operation follows from these two flags alone.','Retain the request classification and relevant raw decoded fields.'),
 ('mic-only-number',['pairwise-scope','message-heuristics'],dict(keyMicFlag=True,otherKeyFieldsUnavailable=True),
  'The MIC flag is insufficient to assign a reliable message number.','The pinned decoder uses additional fields and documented special cases rather than MIC alone.',
  'The flag also does not establish successful MIC verification.','Keep the message number unresolved until the required fields are available.'),
 ('declared-truncation',['declared-length'],dict(declaredEapolBodyBytes=95,capturedEapolBodyBytes=50),
  'The captured body is shorter than its declared length.','Forty-five declared body bytes are unavailable in this supplied record.',
  'Unavailable bytes cannot be interpreted as zero, valid key data or a complete message.','Mark the record incomplete and retain capture-length context.'),
 ('unrecognised-type',['key','start'],dict(eapolTypeCode=250,typeRecognisedByPinnedDecoder=False),
  'The supplied EAPOL type is unrecognised by this decoder context.','It must not be silently converted into a familiar Key or Start type.',
  'Unsupported decoding is a visibility limit, not independent evidence of hostile behavior.','Preserve the numeric type and decoder version for review.'),
 ('late-rekey-context',['key','generated-number'],dict(keyPacketObserved=True,earlierDeauthObserved=False,operatorContext='Periodic rekeying is a possible explanation, not verified'),
  'A key packet was observed without a supplied preceding deauthentication.','The observation cannot be presented as a verified deauth-followed-by-exchange sequence.',
  'Neither periodic rekeying nor an attack is established by the supplied packet alone.','Compare the event with matched baseline and session evidence.'),
 ('header-not-crypto',['descriptor-rsn','generated-number'],dict(typeDecoderAgreement=True,cryptographicVerificationPerformed=False),
  'The decoders agree on header fields; cryptographic verification was not performed.','Header agreement supports the decoded observation, not the authenticity or success of an exchange.',
  'This agreement is not a verified password-loss, incident or endpoint-outcome result.','Report decoder agreement and verification scope separately.'),
]


def build(root):
    source=root/'external-data/wireshark-teaching-reference-v1';intake=json.loads((source/'intake-manifest.json').read_text())
    files={r['path']:r for r in intake['files']};out=root/'domain-data/eapol-semantic-teaching-v1';out.mkdir(exist_ok=False)
    cards=[];examples=[]
    for ident,name,needle,count,fact,limit in FACTS:
        raw=(source/name).read_bytes();assert hashlib.sha256(raw).hexdigest()==files[name]['sha256']
        text=raw.decode();assert 'SPDX-License-Identifier: GPL-2.0-or-later' in text[:800]
        lines=text.splitlines();matches=[i for i,line in enumerate(lines) if line==needle or line.startswith(needle)]
        assert len(matches)==1,(ident,matches)
        first=matches[0];excerpt='\n'.join(lines[first:first+count])
        cards.append(dict(id=ident,sourceFact=fact,projectInterpretationLimit=limit,sourceScope='Pinned decoder behavior, not a complete normative protocol specification',
            sourceId='wireshark-eapol-semantics-v1',licence='GPL-2.0-or-later',trainingApproved=False,
            sourceReference=dict(path=name,revision=intake['revision'],fileSha256=files[name]['sha256'],
                firstLine=first+1,lastLine=first+count,excerpt=excerpt,excerptSha256=hashlib.sha256(excerpt.encode()).hexdigest(),url=files[name]['url']+f'#L{first+1}')))
    byid={r['id']:r for r in cards}
    def add(ident,question,evidence,refs,answer,split):
        context=dict(definitions=[{k:byid[ref][k] for k in ('id','sourceFact','projectInterpretationLimit','sourceScope')} for ref in refs])
        answer.update(schemaVersion=1,factReferences=['capture_summary','capture_context'])
        row=dict(id=ident,family=ident,split=split,sourceId='layertime-source-linked-eapol-lessons-v1',
            datasetVersion='blue-team-compact-eapol-lessons-v1',task='EXPLAIN_EAPOL_EVIDENCE',taskContractVersion='1.0.0',
            outputSchema=canonical(schema()),sourceCardIds=refs,trainingApproved=False,releaseEligible=False,
            labelStatus='agent-authored-source-linked-not-independently-reviewed',
            evidenceKind='authored-decoded-fixture-not-raw-capture',
            messages=[dict(role='system',content=LEGEND),dict(role='user',content=question+'\n'+canonical(dict(facts=[dict(factId='capture_summary',value=evidence),dict(factId='capture_context',value=context)]))),
                      dict(role='assistant',content=canonical(answer))])
        validate(row['messages'][-1]['content'],row);examples.append(row)
    for card in cards:
        add('definition-'+card['id'],'Explain the supplied EAPOL decoder fact and its reporting limit.',dict(topic=card['id']),[card['id']],
            dict(headline='Keep the EAPOL field or message scope explicit.',significance=card['sourceFact'],limitations=card['projectInterpretationLimit'],
                 passiveNextStep='Preserve the decoder version and supporting evidence.'),'unassigned')
    for index,(ident,refs,evidence,headline,meaning,limit,next_step) in enumerate(CASES):
        add('case-'+ident,'Explain what this evidence supports, leading with the important distinction.',evidence,refs,
            dict(headline=headline,significance=meaning,limitations=limit,passiveNextStep=next_step),
            'validation' if ident=='group-vs-pairwise' else 'test' if ident=='declared-truncation' else 'train')
    for name,rows in [('cards.jsonl',cards),('candidates.jsonl',examples)]:
        (out/name).write_text(''.join(canonical(r)+'\n' for r in rows),encoding='utf-8')
    shutil.copy2(source/'COPYING',out/'SOURCE-LICENSE.txt')
    (out/'ATTRIBUTION.md').write_text('Wireshark source: Gerald Combs and contributors; IEEE 802.11 decoder also Axis Communications AB; EAPOL header also Ethan Young. GPL-2.0-or-later notices are preserved in source excerpts/intake and SOURCE-LICENSE.txt. Source facts are paraphrased decoder observations; interpretation limits and synthetic cases are project-authored. No downloaded source was executed.\n')
    review=['# EAPOL semantic teaching review','','Candidates only; no model training. Definitions and scenarios are not independent field incidents.','']
    for row in examples:
        answer=json.loads(row['messages'][-1]['content']);review+=['## '+row['id'],'','Split: '+row['split'],'',row['messages'][1]['content'],'',answer['headline'],'',answer['significance'],'',answer['limitations'],'']
    (out/'REVIEW.md').write_text('\n'.join(review),encoding='utf-8')
    manifest=dict(status='source-linked-semantic-teaching-candidates-not-admitted',sourceFacts=len(cards),examples=len(examples),
        authoredEvidenceCases=len(CASES),exactDistinctAnswers=len({r['messages'][-1]['content'] for r in examples}),
        sourceRevision=intake['revision'],trainingApproved=False,modelTrainingPerformed=False,releaseEligible=False,
        limitations=['Definitions are unassigned; fourteen case families are training candidates, one validation and one test.',
            'Splits share supplied source concepts and are not independent field qualification.',
            'Source behavior is versioned, and project reporting limits are not attributed to Wireshark as incident verdicts.',
            'The captured campaign header review remains separate and external-review-only.'],
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in out.iterdir()})
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2));return manifest


if __name__=='__main__':print(json.dumps(build(Path(__file__).parent),indent=2))
