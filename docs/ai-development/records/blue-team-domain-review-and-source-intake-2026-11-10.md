> Documentation reference copy. Original repository path: `docs/blue-team-domain-review-and-source-intake-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Blue-team domain review and source intake

Status: continued pre-training work. No model inference, weight updates, GPU training, server rental or Android deployment occurred in this phase. The original ontology/dataset plan remains preserved; this report records the subsequent review and corrections.

## Material progress

The new `domain-data/v2` review corpus has **98 concepts, 42 relationships, 36 source-backed cards and 13,834 candidate examples**. It preserves the prior version and adds defensive hypotheses, benign alternatives, security-mode/exchange-visibility context and a declared review-posture concept. All training-admission flags remain false; independent expert validation is not claimed.

Twelve new Bluetooth cards are anchored to the locally pinned NIST SP 800-121r2 Update 1. Each preserves source/page hashes, publication date and historical scope. The source fact and the separately authored product interpretation are distinct fields. Because that publication has external coauthors, its notices and selected-material rights review remain explicit admission requirements. These are paraphrase candidates, not wholesale document ingestion or an assertion of coverage of newer Bluetooth versions.

## Corrections made before any baseline or training

1. **Affected identities were missing.** Synthetic summaries now supply the actual target/network groups and their supporting observation references.
2. **Scenario names leaked the intended interpretation.** Model-visible capture/frame IDs are now opaque. A separate audit-only map preserves traceability to the original evidence.
3. **Generator controls leaked the answer.** Wi-Fi authoring fields such as `exchange=other_client` are absent from model inputs. The relevant event times and identity observations are supplied instead. Reviewer metadata retains the generator configuration outside the messages the model receives.
4. **Fixed addresses could become shortcuts.** Synthetic addresses are remapped consistently within each capture, with a distinct mapping per capture. Broadcast destinations retain their meaning. The mapping is reversible for audit; this is shortcut prevention, not a claim of irreversible anonymization.
5. **Duplicate keys selected a later copy's reference.** The corrected summarizer preserves the first declared observation. An independent audit rejected the earlier draft; that draft and its reason are retained.
6. **Missing time was described too strongly.** The relevant headline now says the sequence lacks usable timestamps, rather than asserting that a timestamp is invalid when the supplied value alone cannot distinguish invalid from missing.
7. **Output formats were underspecified.** Every task now includes an explicit portable JSON output contract in the model instructions. The offline checker rejects altered identities/facts, invented references, extra fields, wrong types and executable/unsupported recipe operations.

The checker does not establish that free prose is correct. It explicitly returns `proseSemanticReviewRequired=true`. The application review posture is a supplied draft policy signal, not a learned arithmetic comparison or empirical incident probability. The model's intended work is interpreting and explaining the supplied evidence and its significance. Ordinary code remains responsible for exact facts and calculations.

## New real Wi-Fi reference

The author repository [hsngccu/WPA3-dataset](https://github.com/hsngccu/WPA3-dataset) identifies raw PCAP data for *Empirical Evaluation of Wi-Fi Handshake Attacks: The WPA3 Dataset* and declares an MIT repository licence. Intake is pinned to revision `e07113327e2012a861f3e273e10ed9b02f0491f4`. Only the README, licence and deauthentication capture files were downloaded. No attack programs were downloaded or executed.

All 64 selected files, including 62 capture chunks, matched their pinned Git blob IDs and recorded SHA256 hashes. The source files total 32,822,295 bytes. A network reset was handled by resuming hash-verified files with bounded retries. The files use PCAPNG/radiotap despite their `.pcap` extension; decoding follows their actual format.

The offline extraction found:

- **122,306 captured records**.
- **6,790 disconnect-frame observations** and **748 EAPOL observations**.
- **664 frame-level follow-on links** under the declared same-client/BSSID, nearest-preceding-request, 30-second rule.

These are not counts of attacks, successful disconnects, completed handshakes or recovered credentials. The timestamped source is a public research recording, not the owner's current network. Source-local address aliases appear in derived files; original captures remain in the source intake. Aliases are not a guarantee of irreversible anonymization.

A bounded comparison against Scapy found zero disagreements across **14,886 checked records** for frame type/subtype, receiver/transmitter addresses and EAPOL presence. This is not independent validation of every semantic address mapping, FCS, exposure, timestamps, protected plaintext or Hunter parity. A separately implemented per-exchange query independently reproduced all 664 stored frame links from the extracted observations. Original file hashes and frame indexes support further review.

The entire campaign is **external-review-only**, outside the pilot selection. Adjacent chunks are not independent environments and are not randomly split. Publisher folder/collection labels are not per-packet incident ground truth. There is no independent benign control in this intake. Wi-Fi evidence is therefore newly available, but the original independent-training/benign-control gap is only partially addressed.

## Proposed pilot selection, not a training run

The full pool remains available for critique. The proposed pilot selects **925 examples**, all from the existing training split:

| Task | Selected examples |
| --- | ---: |
| Defensive evidence reasoning | 720 |
| Ontology explanations | 72 |
| Source-grounded questions | 13 |
| Diverse real BLE window descriptions | 120 |

This selection reduces repeated wording/parameter variants and uses training-session-only feature quantiles for BLE diversity. The new Bluetooth source document remains in validation, preserving document-level separation. Recipe proposals remain in the corpus but are deferred from this pilot until their semantics map to actual app tools. The external Wi-Fi campaign is not selected.

Tokenizer-only measurement: **1,117,279 full-conversation tokens**, including **213,382 assistant target tokens**; maximum full example 2,677 tokens, maximum input 2,117, maximum target 571. A proposed 4,096-token context and 1,024-token response budget fit without truncation. These are measurements for a possible pilot, not an approved full-run budget or evidence of model quality. The candidate pool's row count is not independent source diversity or expert labels.

The corpus, identity/time summaries, source links, output contracts and pilot membership pass the independent structural/evidence audit. Prose truth and intended-user comprehension remain distinct evaluations. The previous plan's source/semantic review, task integration, baseline and compute-sizing steps still apply before training; passing a file/schema check does not admit a dataset or enable an app model.

## What remains

Review the corrected ontology, interpretation labels and wording; complete selected-material rights review; add independent benign Wi-Fi and more BLE environments; validate actual tool/recipe semantics; and establish clean-model/native-report baselines with reserved prospective evidence. Do not feed corpus metadata, scenario names, source-condition filenames or publisher labels into model messages. Preserve public benchmark exposure and prior field-source exploration when describing what is unseen.

Every construction draft is retained with a status note. The updated candidate set, source notices, raw-intake references, generator/checker code, review queue, token inventory and this report are preserved in a new transparency snapshot and copied to the OneDrive workspace. No new model has been trained or promoted.
