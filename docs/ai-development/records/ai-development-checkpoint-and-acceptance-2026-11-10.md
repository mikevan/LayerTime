> Documentation reference copy. Original repository path: `docs/ai-development-checkpoint-and-acceptance-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# AI development checkpoint and acceptance loop

The central collection is `docs/ai-development/README.md`; the comprehensive commit-message note is `docs/ai-development/CHECKIN-NOTES.md`. Original explanatory documents are retained at their established paths, with hash-recorded reference copies in the collection. The owner can run the script with `-Acceptance -MessageFile docs/ai-development/CHECKIN-NOTES.md` for a preview, then add `-Execute` to check in and push the selected work. No such script execution is performed by the assistant.

## Scope

Local checkpoint `3ec4e41` was created before the user's instruction to leave check-ins and pushes to them. No push was performed. **All subsequent changes are to remain uncommitted until the user runs the supplied script or explicitly directs a check-in.** `tools/ai-training/checkpoint-and-push.ps1` previews by default. The `-Acceptance` switch selects only the acceptance source, fixtures, results and related documentation; adding `-Execute` lets the user perform the normal local commit and push. No force, automatic pull, merge or broad staging is used.

Dataset expansion is paused. This checkpoint preserves the AI development source, current ontology, teaching examples and frozen acceptance suite. It does not claim a trained domain adapter or Android readiness. Unrelated Android UI, GATT and Hunter working-tree changes are deliberately left outside this commit.

Raw downloaded captures, private field evidence, model weights, optimizer state, virtual environments, caches and bulk transparency archives remain outside Git. Complete generated datasets and earlier failed experiments remain in verified local transparency ZIPs. The latest pre-acceptance snapshot is `2026-11-10-blue-team-eapol-semantics-v1.zip`, SHA-256 `ad4f0978f99968b08979c2d8f5c4834d6253b1855fbb0b33a357a49531535b11`, in the LayerTime OneDrive `Training transparency` folder. Backup existence is verified; cloud synchronization is not asserted.

Git contains generators, source/rights metadata, the 98-concept ontology, selected compact curriculum and smaller teaching sets. Bulk intermediate datasets are restored from the hashed archive or rebuilt using the pinned intake/build scripts. Some legacy tests need those generated local datasets; this checkpoint is not a standalone distribution of all data or external dependencies. The new acceptance fixtures and grader tests need no external capture or model downloads.

## Frozen acceptance suite

`tools/ai-training/acceptance/v1/protocol.json` seals 12 new authored inputs and a separate criterion rubric before model inference. Inputs cover same-pair follow-on evidence, different network despite matching SSID, different client, earlier exchange, missing timestamps, broadcast targeting, activity drop, BLE entropy/type, BLE newness/type, a missing time unit, incomplete exchange and captured instruction-like text. Every fixture is evaluation-only with `trainingApproved:false` and split `acceptance`. Do not add these inputs, reference headlines or review answers to optimizer data.

The suite tests familiar product requirements using new fixture values. Its concepts overlap existing teaching, so it is not independent prospective field qualification. Generic reference-answer fields are placeholders, never semantic scoring targets. Semantic review follows each case's explicit criteria and required headline meaning, without requiring exact wording.

Before seeing outputs, the suite freezes these development gates:

- Every case must pass structure and supplied-reference checks.
- Every critical interpretation criterion must pass explicit human/agent review tied to the exact output hash; absent or stale review cannot pass.
- Headline at most 140 characters and 22 whitespace-delimited words; explanation at most 160 words across its four text fields.
- Each complete desktop response at most 30 seconds; slow cases cannot be averaged away.
- One greedy response, no retries or repair, 4,096-token context budget and 384-token output budget.

These thresholds are declared product-development choices, not threat definitions or user-study results. Desktop latency is not Android latency. The immediate ordinary-code evidence summary and phone memory, heat, cancellation, saved-capture behavior and comprehension require separate Android checks. The grader always leaves Android qualification and deployment eligibility false.

## Baseline and reproducibility

The original v1 baseline completed, but review found ambiguous fixture inputs: requests lacked an explicit deauthentication type and BLE fixtures lacked an explicit radio domain. Its results are preserved as a fixture diagnostic, not a readiness score. Corrected v2 fixtures declare the domains, request/exchange kinds, address roles and time-field meaning. V2 retains the original rubric and thresholds and is frozen before the v2 baseline. It is a corrected development acceptance set, not an independent fresh benchmark. Use `acceptance/v2` for subsequent candidate comparisons; do not edit either frozen version in place.

The unchanged local Qwen3-4B-Instruct-2507 model runs without an adapter. The evaluator saves source snapshots, hashes, generated outputs, elapsed time, output length and memory data in `runs/qwen4b-domain-acceptance-v1-base`. No new domain weights are trained by this test.

Run the grader tests with the project Python environment: `python -m unittest test_acceptance -v` from `tools/ai-training`. Run inference using `evaluate_local_model.py` with the frozen examples and protocol budgets, then `score_acceptance.py` with the suite, run and explicit review ledger. Scoring without a ledger is useful for structural checks but cannot yield acceptance. Preserve every original response and record any failure; do not edit the frozen suite to fit the output.

Once the baseline has been reviewed, checkpoint its results. Next work is a bounded fresh-base training candidate using admitted teaching data, followed by this same acceptance suite. No further dataset expansion is part of this checkpoint task. Training itself and Android deployment are separate steps; acceptance failures determine the next correction.

The corrected v2 baseline is complete: 12/12 structure/reference and desktop timing passes, but only 1/12 overall acceptance passes after explicit per-criterion review. The 11 failures include identity/order errors, missed or buried key findings, BLE misinterpretation and unsupported inferences. This is not a general accuracy estimate. See `acceptance/v2/RESULTS.md` and the exact output/review/score files under `acceptance/v2/results`. V1 is retained as an ambiguous-fixture diagnostic. The full test suite ran 161 tests, with 160 passing and one CUDA-only skip. No domain weights changed, nothing was deployed, and no push was performed. These results and the corrected suite are left uncommitted for the user.
