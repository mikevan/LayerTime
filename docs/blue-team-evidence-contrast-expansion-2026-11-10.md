# Evidence-specific teaching expansion

## What changed

The previous 982-item compact pilot contains 176 exact distinct answers. This expansion adds a separate set of **16 evidence contrasts with 32 distinct answers**, rather than generating more numeric variants with identical explanations. These are authored synthetic decoded-field/feature examples, not captured packets, new field environments or independent expert labels. No model weights were updated.

Each pair changes exactly one supplied evidence field, retains the same question and reference definitions, and requires a different headline and explanation. Reviewers can inspect both variants in `tools/ai-training/domain-data/evidence-contrast-v1/REVIEW.md`, or inspect all machine-readable inputs and targets in `candidates.jsonl`.

The frozen 982-item proposal is unchanged. New contrasts, the 24 Wireshark source cards, and their 48 question formulations remain separately versioned candidates. Nothing is silently admitted to training.

## Coverage and remaining gaps

| Baseline failure or requirement | New contrast | Scope |
|---|---|---|
| Linking the wrong client or radio | Client match; BSSID match despite shared SSID | Both identity keys must match |
| Earlier exchange described as follow-on | Event order; negative relative time | Preserve order and the declared time basis |
| Rule setting described as measured duration | Missing timestamp; inclusive window boundary | Unknown stays unknown; rules remain explicit |
| Request described as successful disconnection | Separate request and client-state observations | Additional observed outcome changes the report; causality remains unresolved |
| Target changed into sender | Receiver/transmitter address roles | Values and decoded roles remain distinct |
| Capability confused with a verified session | PMF capability versus requirement | Advertisement is not negotiation or frame-acceptance evidence |
| Entropy/newness confused with random-address type | Entropy/type; newness/type | Use the appropriate extractor definition |
| Repetition confused with uniform data length | Repetition versus length variation | Independent statistics can coexist |
| Advertisement described as a GATT read | Separate service advertisement and read observation | Additional connection evidence is required |
| Activity drop described as total silence | Remaining records versus zero observed records | Recorder health still limits environmental conclusions |
| Raw count compared across unequal intervals | Exposure-normalized rate | Reserved validation lesson |
| MIC flag described as successful verification | Separate flag and verifier result | Reserved test lesson |

The 28 training-candidate examples span 14 lesson families. Two validation examples share the rate/exposure family; two test examples share the MIC-result family. Entire pairs stay in their assigned split. These small reserved families share supplied principles and sources with other lessons; they are not independent field qualification. Do not use the reserved answers as optimizer inputs.

These lessons do not close all gaps. Source-grounded Bluetooth address resolution, fuller EAPOL message semantics, benign roaming/rekeying cases, capture-loss behavior, more environments and independent semantic review still need expansion. The Wireshark field cards establish decoder field roles, not a complete protocol standard. The contrast examples explicitly label their interpretations as authored project guidance.

## Source and review boundaries

Each lesson identifies supporting Wireshark field cards, project extractor definitions or project interpretation principles. Full supporting cards are retained in `source-registry.json`, with source hashes, original excerpts and links where applicable. Wireshark attribution and the GPL-2.0-or-later source notice are preserved in `domain-data/wireshark-field-teaching-v1`. The BLE definitions are linked to the pinned project extractor. Scenario evidence and expected prose are authored teaching labels, not claims that a source document contains these incidents.

The application can eventually render the structured evidence immediately and let the model explain the supported relationship. This dataset preparation does not implement that application change or establish that the existing model can do it reliably.

## Verification and next work

Tests check that only the declared evidence field changes within each pair, the target answer changes, definitions remain identical, pairs stay in their split, review/gold metadata is absent from model input, source references exist, and exposure arithmetic agrees with the examples. Structural checks do not independently prove the prose correct.

The full Python suite ran under explicit CPU isolation: 149 tests, 148 passed and one CUDA-only test skipped. All contrast artifact hashes also verified. No GPU training or inference was run for this expansion.

### A concrete example to inspect

In the `network-match` pair, the disconnect target is `client-a`, its BSSID is `radio-a`, and its displayed network name is `Camp`. Both exchange variants also use `client-a` and the name `Camp`; only the exchange BSSID changes:

| Exchange BSSID | Expected opening | Important limit |
|---|---|---|
| `radio-a` | “The client and network-radio identities match.” | Timing must still be checked; identity matching alone does not establish a follow-on exchange. |
| `radio-b` | “The familiar name hides a different network radio.” | The shared client and name do not satisfy this same-radio rule. |

Compare the two entries in the readable review file to confirm that one field changes and the teaching explanation responds to that change. This verifies the dataset example, not model behavior or an installed Android feature. A later frozen model test must demonstrate whether the model actually follows this distinction.

Next, expand the missing source-grounded topics and review these concrete targets, then assemble a source-approved development curriculum with an explicit coverage and repetition report. Freeze evaluation before the next model run. Run a bounded fresh-base LoRA candidate only after that admission check; retain all failed outputs and compare substantive interpretation separately from JSON validity. Broad training readiness and Android qualification remain unestablished.
