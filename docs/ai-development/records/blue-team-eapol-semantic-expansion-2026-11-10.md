> Documentation reference copy. Original repository path: `docs/blue-team-eapol-semantic-expansion-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# EAPOL knowledge and evidence-specific teaching expansion

## Result

Added **18 pinned-source decoder facts and 16 authored evidence cases**, producing 34 distinct teaching answers. These remain candidates, separate from the frozen 982-item proposal and the earlier 32 contrast examples. No model weights changed and no Android feature was deployed.

The new material distinguishes EAPOL packet type, key-descriptor type, decoder message classification, coherent exchange grouping, cryptographic verification and endpoint outcome. This directly addresses the tendency to interpret any EAPOL event as a completed reconnect or a compromised password.

## What the source establishes

The source is the already-downloaded Wireshark revision `55b5def4b5f19e91f56abfd9621f4ffbca938dce`. The selected source files identify GPL-2.0-or-later. Attribution, licence, exact source excerpts, line references, source-file hashes and excerpt hashes are retained in `tools/ai-training/domain-data/eapol-semantic-teaching-v1`.

The facts cover the nine named EAPOL packet types; the separately named RC4, RSN and WPA key-descriptor values; key Request handling; pairwise versus group classification; documented message-number special cases; generated message-number labels; and declared EAPOL length. They describe this decoder version, not a complete normative protocol standard. Project-authored reporting limits are explicitly separate from source facts. No downloaded source was executed.

Fourteen evidence-case families are training candidates, one is reserved for validation and one for test. The 18 definition examples remain unassigned. All retain `trainingApproved:false`. Reserved cases share supplied concepts and source context with other cases; they are not independent field qualification.

## Capture-backed review

An ordinary offline parser revisited all 122,306 records in the pinned external Wi-Fi campaign. Its EAPOL population exactly matched the earlier 748 source observations. All 748 have outer packet type Key and key-descriptor type RSN. Scapy independently agreed on EAPOL presence, version, packet type, declared body length and descriptor type for all 748 records. No declared EAPOL body was truncated in these selected observations.

Narrow flag categories were also recorded: 538 pairwise ACK-without-Install, 116 pairwise Install-and-ACK, and 94 other/unresolved pairwise records. These categories were **not independently compared** and are not asserted as complete handshake identities. The 94 records are not forced into message two or four using a simplified MIC rule: the pinned decoder documents additional fields and special cases.

These numbers are not counts of complete exchanges, compromised passwords or attacks. The campaign remains one external source campaign, without an independent benign control, and stays outside training. Header agreement is not FCS, cryptographic, sender-identity, timestamp or incident verification.

The current review is `domain-data/wifi-eapol-type-review-v2`. The preserved v1 report counted all bytes following the EAPOL header under a body-byte label, which could include trailing capture bytes. V2 separates captured declared-body bytes from trailing bytes and documents the exact comparison scope. Its type and flag-category totals are unchanged. V1 remains available for inspection rather than being silently overwritten.

## Examples the user can inspect

| Supplied evidence | Expected opening | Why it matters |
|---|---|---|
| EAPOL type 1 | “This is an EAPOL Start packet, not a Key packet.” | The wrapper alone does not establish a key exchange. |
| Four records, all labelled message 1 | “Four records share the same message-one label.” | Four records do not establish four distinct handshake messages. |
| Labels 1, 2, 3 and 4, but exchange identity unverified | “All four labels appear, but one coherent exchange has not been established.” | Identity, order and exchange context still need checks. |
| Group message 1 of 2 | “The decoder reports a group-key message.” | Group and pairwise numbering have different scopes. |
| Declared body 95 bytes, captured body 50 bytes | “The captured body is shorter than its declared length.” | Missing bytes cannot be treated as valid data or zero. |

The complete readable teaching set is `domain-data/eapol-semantic-teaching-v1/REVIEW.md`. These are expected teaching answers, not measured model responses. Existing model behavior has not been qualified by creating them.

## Verification and next work

Tests verify pinned source/excerpt hashes, generated-field support, schema/reference integrity, reserved-case assignments, unknown/incomplete headers, declared-length bounds and refusal to guess pairwise message two versus four from limited flags. The real-capture comparison checks the specific header fields listed above. Neither kind of check independently establishes the correctness of every teaching explanation.

The full Python suite ran with the GPU explicitly excluded: 155 tests, 154 passed and one CUDA-only test skipped. The offline capture review completed separately. No neural training or inference was run in this expansion.

Continue expanding source-grounded BLE privacy and benign reconnect/rekey/capture-loss cases, and review the assembled targets before training admission. The remaining coverage problem cannot be solved by counting repeated numeric variants as new knowledge. Once admitted, run a bounded fresh-base adapter and compare substantive interpretation against a frozen baseline, keeping desktop and Android qualification separate.
