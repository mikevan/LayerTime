> Documentation reference copy. Original repository path: `docs/ai-grounded-training-cycle-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Grounded reporting training cycle — November 10, 2026

## Frozen experiment protocol

Purpose: measure schema-v2 typed fact agreement, not learned packet interpretation. Native analysis supplies facts; the model must preserve them exactly. Synthetic scenarios are not independent field evidence.

Dataset: grounded-data/v1/prepared. 56 training pages from 12 authored families; 24 validation pages from four families; 52 test pages from eight families. Four relabelings per family are augmentation. Test SHA256: 69cf5ab32d22ed96c4e81d468bfbc9b123c2b41cb7927c0539746c1d4a53573c. All 132 rows pass separate raw-observation arithmetic and identity audits. The development agent reviewed the cases; no independent expert review is claimed.

Comparison fixed before predictions: original local Qwen3-4B-Instruct-2507 versus at most one bounded 56-step QLoRA candidate trained from that original model. Evaluate the same 52 test pages, greedy decoding, NF4 double quantization with FP16 compute, 4096-token context and 1024-token output allowance. No added policy/evidence prompts, no retries or test-driven candidate selection. Every test input fits (maximum 614 tokens). Train only if the original model leaves a measurable gap. Training consumes train and validation only. Assess exact complete fact agreement and rejection reasons; preserve individual outputs and source snapshots.

A candidate must also undergo older Timeline regression checks. Success here qualifies an interface experiment only. BLE is a separate untrained task: two benign recordings cannot supply independent three-way language-model splits. No device installation is authorized in this cycle.

## Verification before inference

Full Python suite: 89 tests, 88 passed, one CUDA-only test skipped. CUDA was explicitly disabled and asserted unavailable. Dataset review and token-budget records are retained under grounded-data/v1.

## Results and decision

The bounded cycle is complete. Do not promote this adapter to normal reporting or package it as a qualified Android model.

| Measure | Original model | Contract-trained adapter |
| --- | --- | --- |
| Exact grounded test pages | 34/52 | 51/52 |
| Fact-bearing pages | 25/40 | 39/40 |
| Authored test families with every page passing | 0/8 | 7/8 |
| Older Timeline interface passes | 10/19 | 3/19 |
| Older Timeline full-task passes | 0/19 | 0/19 |

The remaining grounded failure changes the recording/page envelope and is rejected. Supplying typed facts and teaching the model to preserve them improved a narrow interface task. It did not establish packet interpretation, general defensive knowledge, BLE reasoning, or Builder authoring. The broader custom-model training objective remains unfinished.

The Timeline check replayed the original 19-page dataset and unchanged prompt/validator functions. Sixteen adapter outputs failed the interface. The development agent reviewed the three valid outputs against raw observations: two make identity/outcome errors; one correctly explains invalid timing but omits the requested episode and device/AP explanation. Thus no complete-task pass. These are controlled diagnostics with known fixture limitations, not independent field qualification.

Keep the useful work: exact fact validation, deterministic urgent reports, versioned prompt compatibility, numerical guards, and reproducible native-decoder tests. The next model cycle should target source-reviewed interpretation and prompt-to-analytic tasks with explicit factual and task-coverage measures; further improvements at echoing supplied facts should not be mistaken for that work. Freeze a fresh evaluation before training those skills, retain whole-recording splits, and keep BLE scope separate. Do not tune this candidate against the failed held-out identifiers.

For Android, the diagnostic app and test APKs compile and build. Their packaged native library, test files, hashes, and existing debug-only smoke model were inspected. The new 4B adapter is not included. Four reproducible native-capture checks are prepared, but have not run on a phone. On-device model testing still requires a compatible reviewed development package, conversion/quantization checks, and explicit deployment authorization. No phone install, commit, push, remote model service, or dataset upload occurred.

Final checks: 348 JVM tests passed; 93 Python tests ran with 92 passed and one CUDA-only skip under asserted CPU isolation. Android instrumentation compilation and both diagnostic APK builds passed. All local GPU jobs from this cycle are terminal; failed setup attempts are retained and excluded from comparisons.

## Prospective training safeguards and integration checks

Before any candidate training, the trainer now uses single-device float64 L2 norm calculation on unscaled gradients and stops before an optimizer update if gradients are nonfinite. Each prospective update records its unscaled norm and loss scale. This addresses the possibility of FP32 norm overflow; it does not retrospectively explain the older adapter's nonfinite readings. Resumable training is still not implemented, so this remains a bounded one-pass experiment.

Training plus validation full prompt/answer lengths were checked on CPU: 80 pages, maximum 867 tokens, all fit the 4096-token budget. Updated Python suite: 91 tests, 90 passed and one CUDA-only skipped.

Android integration found a real prompt-format mismatch: the older Qwen3 no-thinking formatter appended think delimiters, whereas the pinned Qwen3-Instruct-2507 tokenizer ends directly at the assistant turn. Added a separately identified manifest-selected prompt format, retaining compatibility with the previous format. A tokenizer-generated golden fixture checks exact prompt parity, including two system messages and escaped control-token text. Full JVM suite passed after this correction (347 tests). No weights, production manifest, or phone were changed.

Baseline completed: 34/52 exact complete-page passes, including 25/40 fact-bearing pages. Zero of eight authored test families passed every page and relabeling. Median generation 12.219 seconds; maximum 22.562 seconds, excluding model load. Independent replay matched every saved validity flag and re-audited all test facts from raw observations. The bounded 56-step adapter experiment was then started from the original pinned model, with the prospective gradient safeguards above. No retries of failed test outputs.

The first candidate training attempt stopped at update zero: the new guard found nonfinite unscaled gradients with the framework's initial FP16 loss scale of 65536. No optimizer update or saved adapter exists from this attempt. Preserve its directory as a failed numerical setup, excluded from model comparisons. Before examining any candidate test output, retry the same seed/data/56-update budget in a new directory with initial loss scale 256, recorded explicitly in the runtime manifest. Gradient checking and stop-on-nonfinite remain enabled. This is correction of a demonstrated numerical setup failure, not selection using held-out performance.

Post-hoc baseline failure localization (not a change to the acceptance rule): 17 of 18 rejected pages had a wrong recording/page envelope but otherwise exact supplied claims; one had malformed JSON. No output was repaired or accepted. This is predominantly an interface reliability problem, not evidence that the model derived new forensic facts. The original exact acceptance score remains 34/52.

Prepared diagnostic-artifacts/android-review-pack-v1 with four labeled, never-transmitted capture files and exact expected facts. Engine checks passed; a separate Scapy reader verified file hashes, counts, and absence of a timestamp on the final PCAPNG packet. Native Android decoding remains unverified. Full checks after these changes: 348 JVM tests passed; 93 Python tests run, 92 passed and one CUDA-only skipped under explicit CPU isolation.

## Prepared automated phone checks

The debug instrumentation now supports `groundedPack=true` to decode and audit the four bundled test captures through the actual Android Wireshark path, without a model. `groundedAi=true` adds development-model generation and exact typed validation, records timing and process memory, and verifies that the unqualified model remains refused by the product coordinator. The four tiny captures and expected hashes are bundled only in androidTest assets. Android test Kotlin compilation passed; neither mode has been run on a device. The existing separate local-AI benchmark retains cancellation and reuse checks.

The initial scale-256 adapter completed 56 applied updates with zero skips; all 56 unscaled-gradient checks were finite at loss scale 256. All 144 saved adapter tensors are finite; all 72 LoRA B tensors are nonzero. Training took 57.394 seconds plus 8.438 seconds validation; peak allocated GPU memory was 5,058,081,792 bytes. The 11,815,504-byte adapter hash is 876ecac856cdcf6dbee29e426b13c911a7bf4b0cce19f58bea5bb9fe93d0db57. Low completion loss is not a qualification result. The matched 52-page evaluation is running separately.

Matched grounded evaluation completed: original 34/52 versus adapter 51/52 exact complete-page agreement; fact-bearing pages 25/40 versus 39/40; fully passing authored families 0/8 versus 7/8. The remaining adapter error is a recording/page envelope mismatch; no failed result was accepted. Observed generation median 15.906 seconds and maximum 33 seconds for the adapter. These are indicative timings, not controlled performance benchmarks: normal local work, including compilation near the end, occurred during the runs. No Android timing has been measured.

The legacy regression's first launch accidentally inherited explicit CPU isolation and exited before loading or generating any output. That empty setup-failure directory is excluded. A separate GPU-enabled run uses the unchanged 19-page legacy dataset (SHA495ee55bfe7325d7496a9fcd68255a3815233737274453030a05ee0dc9712a82), matching the older baseline's 10240 context and 1024 output budgets. CPU AST comparisons confirm that all five legacy prompt/validator functions are unchanged from the original baseline snapshot.

APK inspection found the existing 639,446,880-byte Qwen3-0.6B debug-only smoke model. An initial recursive inventory missed its source files and led to an incorrect stale-cache diagnosis; a direct source-path check resolved that mistake. The files are present under src/debug/assets/ai/development and were preserved unchanged. Asset regeneration/rebuild succeeded without changing this expected package content. The old manifest and actual bundled weight hash were verified; blueTeamTrained and releaseEligible remain false, no production package exists, and the new adapter is not bundled. The old smoke model does not advertise the grounded-review task, so only native capture checks are currently available in the prepared APKs. No APK was installed. build-check.json records the actual final package contents.

## Additional source leads checked during the regression run

The [Uni Bonn Probe Request Data Set](https://bonndata.uni-bonn.de/dataset.xhtml?persistentId=doi:10.60507/FK2/TXOWTX) describes smartphone probe-request captures with and without MAC randomization, under CC BY 4.0. It offers a small published capture archive. Inference: this may help evaluate address/identity limitations and ordinary probe traffic, but it cannot by itself supply labels for ordered disconnect/EAPOL behavior. Metadata only inspected; no files downloaded, admitted, or trained on.

The [D-Link IoT traces dataset](https://data.mendeley.com/datasets/84cc8grtkt/1) describes 14 devices, Ethernet packet traces and IEEE 802.11 traces collected passively in a university laboratory, under CC BY 4.0. The description specifically discusses probe-request traces. Inference: it is a candidate for broader normal-device context, not a verified replacement for missing disconnect/EAPOL recordings. File contents, applicable capture fields, and independent environments remain unverified. No files downloaded, admitted, or trained on.
