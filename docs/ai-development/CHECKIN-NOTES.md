Checkpoint blue-team AI development and frozen acceptance baseline

## Purpose and current decision

Preserve the AI development work and close the first measurable acceptance loop before expanding the dataset further. The product is a passive blue-team Wi-Fi/BLE analysis tool intended to help quasi-technical users understand significant observed patterns quickly, examine supporting evidence and decide their own response. Immediate understandable findings must precede technical detail. Requests, observed outcomes, correlations and verified causes must remain distinct.

The current unchanged instruction model is **not ready for Android deployment or operational use**. It passed structure/reference and desktop timing checks on all 12 corrected acceptance fixtures, but only **1/12 cases passed all development acceptance gates** after explicit agent semantic review. This is a result for a small authored acceptance suite, not a general model-accuracy estimate, independent expert evaluation or field qualification.

Dataset expansion is paused. The next learning cycle is a bounded, fresh-base development adapter using admitted teaching data, followed by the same frozen acceptance suite. No such domain adapter was trained during this checkpoint/acceptance work.

## Ownership of check-in and push

- Local commit `3ec4e41` preserves the initial AI source, ontology, selected teaching data and v1 acceptance suite. It was created before the owner instructed that they would control check-ins and pushes.
- No push was performed by the assistant. Work after that instruction remains uncommitted for the owner to review and check in.
- `tools/ai-training/checkpoint-and-push.ps1` previews by default. Only the owner running it with `-Execute` enables staging, committing and a normal push to the current branch on `origin`.
- The `-Acceptance` preset selects the acceptance source, fixtures/results and this documentation collection, including the explanatory originals listed by its manifest. It leaves unrelated Android UI/GATT and Hunter firmware work outside this check-in.
- The script refuses an existing staged index, requires a selected branch and configured remote, and stops on a rejected push. It does not pull, merge, reset, amend or force-push. It rechecks the branch before pushing.
- The comprehensive note can be passed as `-MessageFile docs/ai-development/CHECKIN-NOTES.md` rather than supplying only a one-line commit message.

## What this check-in preserves

### Development source and architecture intent

The initial checkpoint contains 185 files, including training/evaluation tools, ontology and source-review generators, numerical/contract checks, recovery/provenance utilities and tests. Frozen AI source and data use byte-preserving Git attributes so Windows/Unix line-ending conversion does not invalidate their recorded hashes.

The architectural direction remains a bundled, locally controlled AI component with explicit interfaces, portable model integration and bounded orchestration. It is not a phone-vendor AI or a required remote inference service. Source-grounded interpretation and prompt-assisted repeatable analytics are product goals. Historical architecture proposals and earlier engine research are included as historical documents, not claims that every proposal has been implemented or qualified.

Wireshark is one source/tool among several. Timeline correlation, identity grouping, capture visibility and ordinary-code evidence summaries remain complementary responsibilities. Creating teaching examples or running an offline parser is not neural training. The ordinary-code evidence summary must remain available while optional AI interpretation runs.

### Ontology, teaching data and source review

| Material | Recorded scope | Important limit |
|---|---|---|
| Domain v2 ontology | 98 concepts, 42 relationships | Project-scoped ontology; independent expert qualification is not established |
| Domain v2 candidate pool | 13,834 candidate examples | Contains templated variants and many windows from one BLE testbed, not that many independent incidents |
| Compact rewritten pool | 13,450 candidates, recipes excluded | Source-fidelity audit verifies preserved evidence, not prose truth or original labels |
| Compact pilot proposal | 982 items, 176 exact distinct target answers | Proposed development curriculum only; repetition does not constitute broad expert knowledge |
| Evidence contrasts | 16 pairs, 32 distinct answers | One evidence field changes per pair; authored decoded-feature fixtures |
| Wireshark field teaching | 24 source cards, 48 question formulations, 24 distinct answers | Pinned decoder field roles, not a complete protocol standard |
| EAPOL semantic teaching | 18 source facts and 16 authored evidence cases, 34 distinct answers | Packet type, descriptor, message label, exchange grouping and outcome are separate |

The supplements address configuration versus measurement, missing time, receiver versus sender, both client and BSSID identity, preceding versus follow-on exchanges, activity reduction versus silence, address entropy/newness/type, payload repetition, source scope and incomplete exchanges. Candidate flags remain false; candidate files must not be silently treated as admitted optimizer inputs.

Licences, attribution and source/version limits are retained. Wireshark source revision is `55b5def4b5f19e91f56abfd9621f4ffbca938dce`; selected files identify GPL-2.0-or-later. Source facts and project-authored reporting limits are separately labelled. Historical NIST guidance retains publication and protocol scope; newer Bluetooth cards with outstanding selected-material rights review remain outside training. BLE feature windows retain CC BY 4.0 source/session context. No downloaded reference code was executed.

### Capture-backed evidence processing

The external Wi-Fi campaign is pinned to `hsngccu/WPA3-dataset` revision `e07113327e2012a861f3e273e10ed9b02f0491f4`, with repository-declared MIT notice and downloaded-file integrity checks. The 62 chunks contain 122,306 records, including 6,790 disconnect observations and 748 EAPOL observations. The earlier 664 nearest-preceding same-client/BSSID links within 30 seconds are frame-level analytic links, not complete handshakes or attack counts.

The subsequent header review found all 748 selected EAPOL observations have outer type Key and RSN key descriptor. Scapy agreed on EAPOL presence, version, type, declared body length and descriptor type for all 748. Narrow flag categories were not independently validated as message/session classifications. These counts do not establish credential compromise, completed reconnects or attacks. The source campaign stays external-review-only, with no independent benign control.

The preserved v1 header report used an imprecise body-byte label that could include trailing capture bytes. V2 separates captured declared-body bytes from trailing bytes and records the comparison scope. Neither original outputs nor previous manifests were silently rewritten.

## Acceptance suite and fixture correction

The initial v1 suite froze 12 new authored inputs before baseline inference. Review found a fixture defect: some request objects did not explicitly identify deauthentication, and the BLE fixtures did not explicitly declare their radio domain. Those outputs are preserved as a fixture diagnostic rather than counted as a readiness score.

V2 corrects those domains, frame types, addressed-target roles and time-field definitions. It retains the original rubric and thresholds, was frozen before its own baseline, and is the version to reuse for the next candidate. Exposure to v1 and overlap with teaching concepts are disclosed; v2 is a development acceptance test, not a fresh independent benchmark.

All acceptance fixtures, reference headlines, rubrics, outputs and reviewer labels are evaluation-only. Their split is `acceptance`, with `trainingApproved:false`. They must not be included in training. Generic reference-answer body fields are placeholders; semantic grading follows the explicit rubric and required headline meaning, not exact string matching.

### Frozen gates

- Every case must pass structure and supplied-reference checks.
- Every critical semantic criterion requires an explicit human/agent judgement bound to the exact output hash. Missing, incomplete or stale review cannot pass.
- Headlines: at most 140 characters and 22 whitespace-delimited words.
- Explanations: at most 160 words across the four text fields.
- Complete desktop response: at most 30 seconds for each case, not merely on average.
- One greedy response per input, with no repair or retries; 4,096-token context budget and 384-token output budget.

These are development policy choices. Thirty-second desktop interpretation is not an emergency alert requirement or an Android measurement. Immediate evidence display, on-device latency/memory/heat/cancellation and comprehension require separate qualification. The grader leaves Android qualification and deployment eligibility false.

## Baseline results

The unchanged Qwen3-4B-Instruct-2507 model ran locally with no adapter, NF4 base quantization and FP16 computation. No weights were updated.

| Measure | Result |
|---|---|
| Structure/reference checks | 12/12 passed |
| Complete desktop responses within 30 seconds | 12/12 passed |
| Explanation-length gate | 8/12 passed |
| Headline-length gate | 11/12 passed |
| All gates including explicit semantic review | 1/12 passed |
| Android qualification/deployment eligibility | False |

The passing case retained an activity drop from 200 to 40 records and unresolved capture health. Failures include merging or reversing identities, missing the earlier exchange, burying timing or broadcast scope, unsupported BLE spoofing interpretations, reversing zero address newness, and misclassifying captured instruction-like text as invalid evidence rather than untrusted data.

Some cases fail on presentation as well as substantive errors. The missing-unit case requests a unit but fails the strict no-applied-rule criterion because its headline says the window is already set. These are inspectable agent judgements; users/reviewers should challenge them using the case ID, output hash, criterion and supporting reasoning.

Exact outputs, source snapshots, base intake metadata, per-criterion review, score and hash manifest are under `tools/ai-training/acceptance/v2/results`. `acceptance/v2/RESULTS.md` provides the case summary. V1 diagnostics remain under `acceptance/v1/results`.

## Validation

- Full working-copy Python suite: 161 tests, 160 passed, one CUDA-only test skipped under explicit CPU isolation.
- Initial checkpoint grader tests: five passed from an isolated Git archive, without local datasets or model files. This verifies that committed grader/fixture subset, not standalone availability of every legacy dataset.
- Current acceptance grader tests include corrected explicit domains/roles, reserved evaluation splits, rejection of stale reviews, failure on missing semantic review, critical semantic failures despite valid JSON, and per-case latency limits.
- Portable copied results were regraded and reproduced the saved score exactly. Their 13 evidence/source files have a recorded hash manifest.
- The PowerShell check-in script was syntax-checked and its preview tested with Windows PowerShell from `C:\Users\vange`. This caught and fixed the omitted acceptance-document allowlist entry and script-directory resolution during `-File` invocation. The preview completed with HEAD and staged changes unchanged. Its commit/push path has not been run by the assistant.
- No Android build, phone deployment, thermal test or physical field test was performed for this acceptance checkpoint.

## Documentation collection and provenance

`docs/ai-development/README.md` is the starting index. It collects architecture/research proposals, Builder reporting/design rationale, training process/transparency, source audits, earlier training cycles, current domain/data plans, readable teaching examples and acceptance findings. Older documents retain their original context; their use of “current” does not override this note.

Original documents remain at their established paths. Reference copies are gathered here so existing code and historical links remain valid. Word documents are copied byte-for-byte; Markdown copies receive a provenance banner and adjusted links. `document-manifest.json` records original paths and source/copy hashes. `collect_development_docs.py --verify` detects stale or changed copies. Refresh is explicit; edited copies must first be reconciled.

Full generated datasets and prior experiment records remain in verified local transparency archives, outside Git. The latest pre-acceptance snapshot is `2026-11-10-blue-team-eapol-semantics-v1.zip`, SHA-256 `ad4f0978f99968b08979c2d8f5c4834d6253b1855fbb0b33a357a49531535b11`, in the LayerTime OneDrive `Training transparency` folder. Local existence/integrity were verified; cloud synchronization is not asserted. Acceptance evidence is now separately portable in the repository working tree for the owner's check-in.

## Exclusions and limitations

Raw private field captures, downloaded source campaigns, model weights, optimizer binaries, caches and bulk archives are not part of the selected Git check-in. Unrelated Android UI/GATT/Hunter changes remain outside the preset. No external publication or push was performed by the assistant.

Older narrow Builder/quantity adapters and earlier BLE specialist experiments are historical development work, not a qualified broad blue-team model. Low loss, successful optimizer updates, valid JSON, row counts or decoder agreement do not by themselves establish reliable interpretation. The current domain curriculum remains source-limited, partly synthetic, repetitive and without independent expert or field qualification.

## Next steps after owner review

1. Review the frozen cases, exact baseline outputs and criterion judgements; record any disputed judgement without editing the original outputs or silently changing the suite.
2. Run the owner's preview/check-in/push script when ready. Preserve the current baseline before model changes.
3. Select and explicitly admit a bounded development curriculum from existing teaching candidates, retaining source permissions, split boundaries, hashes and token budgets. Keep acceptance data excluded. Dataset expansion remains paused.
4. Run one bounded fresh-base adapter with recorded configuration, actual update/gradient/weight-change evidence and recoverable checkpoints. Do not continue from a narrow Builder adapter as if it were the domain base.
5. Evaluate the candidate against unchanged v2 acceptance. Report structural, semantic, brevity and timing outcomes separately and preserve failures.
6. Only then consider an experimental Android build and saved-capture tests, including immediate evidence display, memory, latency, heat, cancellation and understandable presentation. Field use and operational readiness require evidence beyond this desktop suite.
