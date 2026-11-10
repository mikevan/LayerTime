# EAPOL semantic teaching review

Candidates only; no model training. Definitions and scenarios are not independent field incidents.

## definition-eap-packet

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"eap-packet"}},{"factId":"capture_context","value":{"definitions":[{"id":"eap-packet","projectInterpretationLimit":"An EAPOL observation is not automatically an EAPOL-Key packet.","sourceFact":"EAPOL packet type zero is named EAP in the pinned decoder.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type zero is named EAP in the pinned decoder.

An EAPOL observation is not automatically an EAPOL-Key packet.

## definition-start

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"start"}},{"factId":"capture_context","value":{"definitions":[{"id":"start","projectInterpretationLimit":"A Start packet alone does not establish a key exchange or a completed connection.","sourceFact":"EAPOL packet type one is Start.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type one is Start.

A Start packet alone does not establish a key exchange or a completed connection.

## definition-logoff

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"logoff"}},{"factId":"capture_context","value":{"definitions":[{"id":"logoff","projectInterpretationLimit":"Do not rename this packet as an 802.11 deauthentication frame or a key message.","sourceFact":"EAPOL packet type two is Logoff.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type two is Logoff.

Do not rename this packet as an 802.11 deauthentication frame or a key message.

## definition-key

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"key"}},{"factId":"capture_context","value":{"definitions":[{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type three is Key.

Its packet type does not establish the descriptor, message number, complete exchange or credential loss.

## definition-asf

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"asf"}},{"factId":"capture_context","value":{"definitions":[{"id":"asf","projectInterpretationLimit":"Do not force every EAPOL packet into a four-message Wi-Fi key-exchange narrative.","sourceFact":"EAPOL packet type four is an encapsulated ASF alert.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type four is an encapsulated ASF alert.

Do not force every EAPOL packet into a four-message Wi-Fi key-exchange narrative.

## definition-mka

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"mka"}},{"factId":"capture_context","value":{"definitions":[{"id":"mka","projectInterpretationLimit":"MKA is not the packet-type label for an RSN key message.","sourceFact":"EAPOL packet type five is MKA.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type five is MKA.

MKA is not the packet-type label for an RSN key message.

## definition-announcement-generic

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"announcement-generic"}},{"factId":"capture_context","value":{"definitions":[{"id":"announcement-generic","projectInterpretationLimit":"The EAPOL label alone is insufficient to identify a key message.","sourceFact":"EAPOL packet type six is a generic announcement.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type six is a generic announcement.

The EAPOL label alone is insufficient to identify a key message.

## definition-announcement-specific

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"announcement-specific"}},{"factId":"capture_context","value":{"definitions":[{"id":"announcement-specific","projectInterpretationLimit":"Report the decoded type instead of assuming a key exchange.","sourceFact":"EAPOL packet type seven is a specific announcement.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type seven is a specific announcement.

Report the decoded type instead of assuming a key exchange.

## definition-announcement-request

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"announcement-request"}},{"factId":"capture_context","value":{"definitions":[{"id":"announcement-request","projectInterpretationLimit":"This packet-type name is distinct from a Request flag within a key descriptor.","sourceFact":"EAPOL packet type eight is an announcement request.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

EAPOL packet type eight is an announcement request.

This packet-type name is distinct from a Request flag within a key descriptor.

## definition-descriptor-rc4

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"descriptor-rc4"}},{"factId":"capture_context","value":{"definitions":[{"id":"descriptor-rc4","projectInterpretationLimit":"Descriptor type and EAPOL packet type are different fields; retain the source version.","sourceFact":"The pinned source names key-descriptor type one RC4 and marks it deprecated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned source names key-descriptor type one RC4 and marks it deprecated.

Descriptor type and EAPOL packet type are different fields; retain the source version.

## definition-descriptor-rsn

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"descriptor-rsn"}},{"factId":"capture_context","value":{"definitions":[{"id":"descriptor-rsn","projectInterpretationLimit":"A descriptor value does not identify a whole captured exchange or prove its outcome.","sourceFact":"The pinned source assigns value two to the RSN key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned source assigns value two to the RSN key-descriptor type.

A descriptor value does not identify a whole captured exchange or prove its outcome.

## definition-descriptor-wpa

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"descriptor-wpa"}},{"factId":"capture_context","value":{"definitions":[{"id":"descriptor-wpa","projectInterpretationLimit":"Do not treat WPA and RSN descriptor values as the same observed field value.","sourceFact":"The pinned source assigns value 254 to the WPA key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned source assigns value 254 to the WPA key-descriptor type.

Do not treat WPA and RSN descriptor values as the same observed field value.

## definition-request-precedence

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"request-precedence"}},{"factId":"capture_context","value":{"definitions":[{"id":"request-precedence","projectInterpretationLimit":"A key request should not be forced into a numbered four-way message merely because the pairwise bit is present.","sourceFact":"The pinned decoder checks the key Request flag before its pairwise-message classification branch.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned decoder checks the key Request flag before its pairwise-message classification branch.

A key request should not be forced into a numbered four-way message merely because the pairwise bit is present.

## definition-pairwise-scope

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"pairwise-scope"}},{"factId":"capture_context","value":{"definitions":[{"id":"pairwise-scope","projectInterpretationLimit":"The MIC flag alone is not a portable message-number or successful-verification rule.","sourceFact":"The pairwise classification branch considers Install and Key ACK flags and explicitly avoids relying on MIC alone.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pairwise classification branch considers Install and Key ACK flags and explicitly avoids relying on MIC alone.

The MIC flag alone is not a portable message-number or successful-verification rule.

## definition-group-scope

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"group-scope"}},{"factId":"capture_context","value":{"definitions":[{"id":"group-scope","projectInterpretationLimit":"A group message numbered one or two is not automatically a pairwise message with that number.","sourceFact":"The pinned decoder distinguishes two-message group-key labels from four-message pairwise labels.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned decoder distinguishes two-message group-key labels from four-message pairwise labels.

A group message numbered one or two is not automatically a pairwise message with that number.

## definition-message-heuristics

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"message-heuristics"}},{"factId":"capture_context","value":{"definitions":[{"id":"message-heuristics","projectInterpretationLimit":"Treat decoder message labels as versioned interpretations and retain the underlying evidence.","sourceFact":"The pinned implementation documents special cases and implementation differences when distinguishing pairwise messages two and four.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The pinned implementation documents special cases and implementation differences when distinguishing pairwise messages two and four.

Treat decoder message labels as versioned interpretations and retain the underlying evidence.

## definition-generated-number

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"generated-number"}},{"factId":"capture_context","value":{"definitions":[{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The decoder marks the generated message-number tree item as generated.

A generated message number is not proof that all other messages in the exchange were captured.

## definition-declared-length

Split: unassigned

Explain the supplied EAPOL decoder fact and its reporting limit.
{"facts":[{"factId":"capture_summary","value":{"topic":"declared-length"}},{"factId":"capture_context","value":{"definitions":[{"id":"declared-length","projectInterpretationLimit":"Trailing capture bytes cannot establish that missing declared body bytes were observed.","sourceFact":"The decoder derives the EAPOL total length from a fixed header plus the declared body length.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Keep the EAPOL field or message scope explicit.

The decoder derives the EAPOL total length from a fixed header plus the declared body length.

Trailing capture bytes cannot establish that missing declared body bytes were observed.

## case-non-key-start

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":1}},{"factId":"capture_context","value":{"definitions":[{"id":"start","projectInterpretationLimit":"A Start packet alone does not establish a key exchange or a completed connection.","sourceFact":"EAPOL packet type one is Start.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

This is an EAPOL Start packet, not a Key packet.

The decoded type does not support reporting a captured RSN key exchange.

A later exchange or connection outcome is not supplied.

## case-non-key-eap

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":0}},{"factId":"capture_context","value":{"definitions":[{"id":"eap-packet","projectInterpretationLimit":"An EAPOL observation is not automatically an EAPOL-Key packet.","sourceFact":"EAPOL packet type zero is named EAP in the pinned decoder.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

An EAP packet was observed within EAPOL.

The EAPOL wrapper alone does not make this a Key packet.

The supplied type does not establish a complete authentication process.

## case-logoff-vs-deauth

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":2,"ieee80211DeauthObserved":false}},{"factId":"capture_context","value":{"definitions":[{"id":"logoff","projectInterpretationLimit":"Do not rename this packet as an 802.11 deauthentication frame or a key message.","sourceFact":"EAPOL packet type two is Logoff.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The supplied event is EAPOL Logoff.

No 802.11 deauthentication observation is supplied, so those event types must remain distinct.

A Logoff observation alone does not establish the endpoint outcome or hostile intent.

## case-mka-vs-rsn

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":5}},{"factId":"capture_context","value":{"definitions":[{"id":"mka","projectInterpretationLimit":"MKA is not the packet-type label for an RSN key message.","sourceFact":"EAPOL packet type five is MKA.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"descriptor-rsn","projectInterpretationLimit":"A descriptor value does not identify a whole captured exchange or prove its outcome.","sourceFact":"The pinned source assigns value two to the RSN key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The decoded EAPOL type is MKA.

This is not the EAPOL packet-type label for an RSN Key packet.

Do not infer a four-way Wi-Fi handshake from the generic EAPOL label.

## case-packet-vs-descriptor

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":3,"keyDescriptorType":2}},{"factId":"capture_context","value":{"definitions":[{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"descriptor-rsn","projectInterpretationLimit":"A descriptor value does not identify a whole captured exchange or prove its outcome.","sourceFact":"The pinned source assigns value two to the RSN key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

A Key packet with an RSN descriptor was observed.

The outer packet type is three and the separate descriptor value is two.

Neither value is a handshake-message count or evidence of credential recovery.

## case-descriptor-difference

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"observationA":{"eapolTypeCode":3,"keyDescriptorType":2},"observationB":{"eapolTypeCode":3,"keyDescriptorType":254}}},{"factId":"capture_context","value":{"definitions":[{"id":"descriptor-rsn","projectInterpretationLimit":"A descriptor value does not identify a whole captured exchange or prove its outcome.","sourceFact":"The pinned source assigns value two to the RSN key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"descriptor-wpa","projectInterpretationLimit":"Do not treat WPA and RSN descriptor values as the same observed field value.","sourceFact":"The pinned source assigns value 254 to the WPA key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Both are Key packets, but their descriptor types differ.

One supplied descriptor is RSN and the other is WPA; a shared outer type does not erase that distinction.

Different descriptor values alone do not establish two attacks or two complete sessions.

## case-single-numbered-message

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"decodedMessageLabel":"pairwise message 3 of 4","otherExchangeMessagesObserved":false}},{"factId":"capture_context","value":{"definitions":[{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Only one message from a numbered exchange is supplied.

A message-three label does not mean messages one, two and four were also captured.

Exchange completeness, endpoint acceptance and credential loss are unresolved.

## case-four-records-not-four-messages

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"decodedLabels":["pairwise message 1 of 4","pairwise message 1 of 4","pairwise message 1 of 4","pairwise message 1 of 4"]}},{"factId":"capture_context","value":{"definitions":[{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

Four records share the same message-one label.

Four captured records are not evidence of four distinct numbered handshake messages.

Retries, repeated attempts and capture duplicates are not resolved by the count alone.

## case-all-labels-not-outcome

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"decodedLabels":["pairwise message 1 of 4","pairwise message 2 of 4","pairwise message 3 of 4","pairwise message 4 of 4"],"singleExchangeIdentityVerified":false}},{"factId":"capture_context","value":{"definitions":[{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

All four labels appear, but one coherent exchange has not been established.

The supplied labels still need identity, order and exchange-context checks before grouping them as one exchange.

Labels alone do not establish cryptographic validity, successful endpoint processing or password compromise.

## case-group-vs-pairwise

Split: validation

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"decodedLabel":"group message 1 of 2"}},{"factId":"capture_context","value":{"definitions":[{"id":"group-scope","projectInterpretationLimit":"A group message numbered one or two is not automatically a pairwise message with that number.","sourceFact":"The pinned decoder distinguishes two-message group-key labels from four-message pairwise labels.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The decoder reports a group-key message.

Its message-one label belongs to a two-message group sequence, not the four-message pairwise sequence.

The matching second message and endpoint result are not supplied.

## case-request-not-numbered

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"keyRequestFlag":true,"pairwiseFlag":true}},{"factId":"capture_context","value":{"definitions":[{"id":"request-precedence","projectInterpretationLimit":"A key request should not be forced into a numbered four-way message merely because the pairwise bit is present.","sourceFact":"The pinned decoder checks the key Request flag before its pairwise-message classification branch.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The key Request flag takes priority in this decoder branch.

The pairwise flag does not override the earlier Request handling in the pinned implementation.

No numbered four-way classification or successful operation follows from these two flags alone.

## case-mic-only-number

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"keyMicFlag":true,"otherKeyFieldsUnavailable":true}},{"factId":"capture_context","value":{"definitions":[{"id":"pairwise-scope","projectInterpretationLimit":"The MIC flag alone is not a portable message-number or successful-verification rule.","sourceFact":"The pairwise classification branch considers Install and Key ACK flags and explicitly avoids relying on MIC alone.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"message-heuristics","projectInterpretationLimit":"Treat decoder message labels as versioned interpretations and retain the underlying evidence.","sourceFact":"The pinned implementation documents special cases and implementation differences when distinguishing pairwise messages two and four.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The MIC flag is insufficient to assign a reliable message number.

The pinned decoder uses additional fields and documented special cases rather than MIC alone.

The flag also does not establish successful MIC verification.

## case-declared-truncation

Split: test

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"capturedEapolBodyBytes":50,"declaredEapolBodyBytes":95}},{"factId":"capture_context","value":{"definitions":[{"id":"declared-length","projectInterpretationLimit":"Trailing capture bytes cannot establish that missing declared body bytes were observed.","sourceFact":"The decoder derives the EAPOL total length from a fixed header plus the declared body length.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The captured body is shorter than its declared length.

Forty-five declared body bytes are unavailable in this supplied record.

Unavailable bytes cannot be interpreted as zero, valid key data or a complete message.

## case-unrecognised-type

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"eapolTypeCode":250,"typeRecognisedByPinnedDecoder":false}},{"factId":"capture_context","value":{"definitions":[{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"start","projectInterpretationLimit":"A Start packet alone does not establish a key exchange or a completed connection.","sourceFact":"EAPOL packet type one is Start.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The supplied EAPOL type is unrecognised by this decoder context.

It must not be silently converted into a familiar Key or Start type.

Unsupported decoding is a visibility limit, not independent evidence of hostile behavior.

## case-late-rekey-context

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"earlierDeauthObserved":false,"keyPacketObserved":true,"operatorContext":"Periodic rekeying is a possible explanation, not verified"}},{"factId":"capture_context","value":{"definitions":[{"id":"key","projectInterpretationLimit":"Its packet type does not establish the descriptor, message number, complete exchange or credential loss.","sourceFact":"EAPOL packet type three is Key.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

A key packet was observed without a supplied preceding deauthentication.

The observation cannot be presented as a verified deauth-followed-by-exchange sequence.

Neither periodic rekeying nor an attack is established by the supplied packet alone.

## case-header-not-crypto

Split: train

Explain what this evidence supports, leading with the important distinction.
{"facts":[{"factId":"capture_summary","value":{"cryptographicVerificationPerformed":false,"typeDecoderAgreement":true}},{"factId":"capture_context","value":{"definitions":[{"id":"descriptor-rsn","projectInterpretationLimit":"A descriptor value does not identify a whole captured exchange or prove its outcome.","sourceFact":"The pinned source assigns value two to the RSN key-descriptor type.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"},{"id":"generated-number","projectInterpretationLimit":"A generated message number is not proof that all other messages in the exchange were captured.","sourceFact":"The decoder marks the generated message-number tree item as generated.","sourceScope":"Pinned decoder behavior, not a complete normative protocol specification"}]}}]}

The decoders agree on header fields; cryptographic verification was not performed.

Header agreement supports the decoded observation, not the authenticity or success of an exchange.

This agreement is not a verified password-loss, incident or endpoint-outcome result.
