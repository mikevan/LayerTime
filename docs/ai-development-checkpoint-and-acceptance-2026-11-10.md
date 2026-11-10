# AI development checkpoint and acceptance loop

## Scope

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

The unchanged local Qwen3-4B-Instruct-2507 model runs without an adapter. The evaluator saves source snapshots, hashes, generated outputs, elapsed time, output length and memory data in `runs/qwen4b-domain-acceptance-v1-base`. No new domain weights are trained by this test.

Run the grader tests with the project Python environment: `python -m unittest test_acceptance -v` from `tools/ai-training`. Run inference using `evaluate_local_model.py` with the frozen examples and protocol budgets, then `score_acceptance.py` with the suite, run and explicit review ledger. Scoring without a ledger is useful for structural checks but cannot yield acceptance. Preserve every original response and record any failure; do not edit the frozen suite to fit the output.

Once the baseline has been reviewed, checkpoint its results. Next work is a bounded fresh-base training candidate using admitted teaching data, followed by this same acceptance suite. No further dataset expansion is part of this checkpoint task. Training itself and Android deployment are separate steps; acceptance failures determine the next correction.
