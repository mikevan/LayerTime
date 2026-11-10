# LayerTime local AI training

The current objective is the broader blue-team ontology and dataset, **before more model training**. See [the concrete dataset and proposed plan](../../docs/blue-team-ontology-dataset-and-training-plan-2026-11-10.md) and `domain-data/v1/README.md`. This candidate pool has 84 concepts and 13,756 examples with explicit source/variant limits. Admission flags remain false. Do not resume narrow Builder training or concatenate candidate split files as a substitute for the reviewed domain plan.

The subsequent [review and real Wi-Fi source intake](../../docs/blue-team-domain-review-and-source-intake-2026-11-10.md) is in `domain-data/v2` and `domain-data/wifi-reference-v1`: 98 concepts, 13,834 candidates, corrected model-visible identities/times with shortcut controls removed, explicit task contracts, and a 925-example proposed pilot. The separate real Wi-Fi campaign remains external-review-only. No model training occurred. Use only each record's messages as model input; reviewer/generator metadata is not model context.

Training process and public-inspection policy: [process and transparency](../../docs/ai-training-process-and-transparency.md). Preserve authored items, source notices, splits, corrections, prompts and failed evaluations. `export_training_transparency.py --out transparency/<new-snapshot-name>` creates a hash-verified, append-only local review archive with a per-item index. `--verify <snapshot>` detects changes. Do not count copied/prepared rows as independent examples or treat archive inclusion as training admission/publication clearance. Raw external downloads and private field evidence are excluded; redistribution rights must be recorded separately before publication.

The current narrow Builder work is documented in [unit and clarification training](../../docs/ai-unit-clarification-cycle-2026-11-10.md) and [source-bound quantity extraction](../../docs/ai-quantity-extraction-cycle-2026-11-10.md). These experiments do not constitute comprehensive domain/ontology training or a release-qualified model.

Current results and remaining qualification work are recorded in [the November 10 progress report](../../docs/ai-training-progress-2026-11-10.md). The BLE specialist pilots and bounded Qwen3-4B adapter experiments have completed. The latest policy-reporting run used 128 training variants from eight original synthetic families; its saved weights passed finiteness checks. Matched adapted/base/SmolLM3 comparisons are running under [the frozen protocol](../../docs/ai-policy-comparison-protocol-2026-11-10.md). Early reviewed answers still contain timing and unresolved-evidence errors. No experimental candidate is release eligible, and nothing from this overnight work has been deployed to the phone.

`python wireless_curriculum.py` writes twelve explicit corrective training candidates and a readable `corrective-data/review.md`. The registry deliberately excludes those candidates from training pending review. Their provenance distinguishes synthetic authored examples from field evidence. The separate, reviewed policy curriculum records controlled variants and original-family counts explicitly; they are not independent field recordings. Current check counts and retained test evidence are in the progress report.

## Evidence-grounded generation experiment

`evidence_facts.py` produces a JSON summary of observed disconnect-frame counts, reason values, client/BSSID identity and page-local timing. It accepts EAPOL in either direction, keeps other BSSIDs separate, distinguishes broadcast targets, excludes malformed or unresolved frames, and rejects duplicate frame IDs. Invalid timestamps prevent chronological links. Counts include retransmissions and never imply successful disconnects, completed handshakes, password loss or authenticated sender identity. Its fixed 30-second link window is an experimental parameter, not evidence of causation. Episode segmentation and cross-page linkage remain separate work.

Pass `--evidence-facts` to `evaluate_local_model.py` to supply these summaries alongside the original observations. This changes inference context; it does not update model weights. Compare actual interpretations with the frozen baseline rather than using valid JSON as the success criterion. Do not add this flag to a matched experiment whose frozen examples already contain its prescribed facts.

## Larger-model training support

Additional source intakes: `fetch_ble_connections.py` pins and verifies the CC BY 4.0 InSecTT connection measurements; `audit_ble_connections.py` checks publisher counts, header ranges and timestamp ordering without inventing attack labels or time units. `fetch_defensive_ontology.py` retains MITRE D3FEND v1.6.0 and its full reuse notice; `extract_defensive_concepts.py` cross-checks 25 selected detection definitions between the source CSV and ontology. These source packs did not enter the policy-verbalization training job. The 25 authored D3FEND teaching cards have a hash-bound same-agent source review but remain outside training intake.

Builder authoring candidates are exported by the Android JVM test `AiAuthoringTrainingSeedTest` when `LAYERTIME_AI_AUTHORING_EXPORT` names a writable destination. All eight proposals are decoded by `AnalyticAuthoring`, validated with the real atom catalog and executed against the existing practice recording. The current eight candidates span five related goal families; they are not independently labelled field analytics and have not entered training. The export and engine test evidence are in `builder-data/v1`.

`knowledge_cards.py` validates 24 authored factual teaching cards against retained primary-document hashes and exact supporting page anchors. It separates source facts from product-design interpretations. `assemble_development_corpus.py` combines the original capture pages with factual concept answers using document-separated knowledge training/validation. `--verify` checks integrity and task-specific labels. `train_lora.py --multitask` uses that verifier. This small corpus has no independent knowledge test set and is not a substantial domain corpus. Product interpretations and the twelve corrective candidates are excluded.

`build_ble_teaching.py` derives six candidate explanations from exact saved feature rows in two publisher-labelled benign recordings. Three question types cover repeated data, address-format interpretation and disagreement between held-out classifier/anomaly flags. Source attribution, hashes and selected-row review are retained in `ble-teaching/v1`. These are selected teaching examples, not a performance sample or six independent recordings; their distinct task is not admitted to current training or production decoding.

`train_lora.py --qlora` loads the frozen base in NF4 double quantization with FP16 compute and prepares it for adapter training. `--max-steps` requests training-loop iterations and `--gradient-accumulation` controls examples accumulated per iteration. Mixed-precision updates can be skipped; consult the step ledger and numerical diagnostics rather than treating the requested count as proof of applied updates. The original two-iteration default remains a compatibility smoke test. More updates on the original fixtures do not create a substantial domain model. Prepared corpus integrity, source permission, family splits and token budgets are checked; the test split is never supplied to the trainer. Experimental adapters remain ineligible for release.

`audit_ble_decoder.py` compares accepted advertisement fields with Scapy. The first 128 accepted records in each of seven sessions (896 total) matched PDU type, address type, advertiser address and advertising data. This bounded prefix comparison does not check rejected records, Nordic wrapper offsets, CRC flags, timestamps or Hunter parity.

This pipeline prepares source-checked development examples, bounded local training experiments and offline evaluations. It does not produce a qualified product model merely by completing a run. Training and evaluation use locally present model weights; capture data and inference requests are not sent to a remote model service. Separate source-intake tools download explicitly selected public material.

The source registry records permission and provenance. Existing synthetic fixtures are authorized for development use. Selected authored factual paraphrases from NIST SP 800-94 and SP 800-153 entered the small multitask pilot with source-document-separated splits; entire PDFs were not ingested as supervised answers. Additional sources require explicit provenance, permission and task-specific review before training intake.

## Reporting evaluation and Timeline candidates

The original `run_policy_comparison.ps1` queue waited for exact training-process exit and reread actual saved tensors on CPU. It completed both Qwen policy conditions, then was intentionally stopped after discovering that SmolLM3's native template discards additional system messages. Its three partial Smol outputs are excluded, not counted as model failures. `continue_policy_comparison.ps1` runs the corrected Smol condition and the three pending Timeline conditions. Queue completion still requires interpretation review. Do not restart over existing experiment directories or launch a competing GPU model.

`render_evaluation_prompt` now checks that every supplied message survives the native chat template. `--merge-system-messages` explicitly joins leading system instructions for templates accepting only one system message; use the same flag in CPU preflight. The retention audit verifies that completed Qwen prompts are byte-for-byte unchanged and contain all instructions. Corrected Smol maximum inputs are 6,388 policy tokens and 885 Timeline tokens, fitting the original budgets. This is a documented input-adapter correction, not a change to the frozen examples or outcome criteria.

`score_policy_predictions.py` measures schema/reference validity and evidence-group coverage against audited policy labels. `review_evaluation.py` binds prose-review decisions to exact output hashes. Neither valid identifiers nor group coverage establish that an explanation is true.

`audit_duration_bounds.py --examples <frozen-test.jsonl> --run <completed-run>` adds a separately labelled post-hoc diagnostic. It checks only uniquely matched literal request-count/within-seconds claims for the entire supplied group. A duration below the observed span, allowing decimal rounding, is contradicted. Larger bounds can be true; unsupported wording, subsets, ambiguous groups and invalid timestamps are left unchecked. This is not a general prose score or a release gate.

`AiCorrelationTrainingSeedTest` exports candidate Timeline teaching pages when `LAYERTIME_AI_CORRELATION_EXPORT` is set. Frozen `correlation-data/v2` remains the comparison input. Candidate v4 retains clearer v3 answers, observed-name citations and current span/delay fields, and corrects synthetic beacon destinations to broadcast. All versions use the same 14 scenarios rather than new environments. Do not regenerate over frozen versions. These candidates remain outside current training intake; the old name fixtures' realism limit is documented alongside v2 results.

## Export and verify

Run from the Android project with a writable output path:

```powershell
$env:LAYERTIME_AI_SEED_EXPORT='C:\workspace\TUltra-Project\LayerTime\tools\ai-training\seed-data'
.\gradlew.bat :app:testFullDebugUnitTest --tests '*AiTrainingSeedTest' --offline --console=plain
```

Run from this directory using Python:

```powershell
python -m unittest discover -p 'test_*.py'
python prepare.py --seeds seed-data/seeds.jsonl --out seed-data/prepared
python prepare.py --out seed-data/prepared --verify
python train_lora.py --dataset seed-data/prepared --dry-run
```

Splits are assigned by original scenario family, never by packet page. The manifest records source and data hashes. Validation rejects duplicated conversations across splits, unsupported sources, invalid labels, and fabricated frame references. The Android export validates outputs with the same decoder used by the app.

## Local training smoke

Use a dedicated environment containing the pinned training dependencies. The initial configuration targets a small compatible causal model on a CUDA GPU. It masks the input prompt from the training loss, counts real tokenizer lengths, rejects oversized examples, and never supplies the test split to the trainer.

```powershell
python train_lora.py --dataset seed-data/prepared --base-model models/local-base --output runs/seed-smoke
```

The output is an adapter and a development manifest with release eligibility disabled. Training and generation quality have not been established merely because corpus validation passes. A release still requires reviewed defensive examples, independent captures, prompt/tool/analytic evaluations, a reproducible merged and quantized model, and device measurements. Source-family splits reduce leakage but synthetic fixtures are not independent field evidence.

## Measured development run (November 9, 2026)

`runs/seed-smoke-v4` completed two LoRA updates on the pinned Qwen3-0.6B base using nine fixture-derived training examples. Training took 17.51 seconds; validation on three separate-family examples took 4.74 seconds. Peak allocated CUDA memory was 1,974,919,168 bytes. Training loss 1.7433 and validation loss 1.5585 are smoke diagnostics, not evidence of useful field accuracy. The five-example test split was not supplied to training or validation.

On this RTX 2070, native grouped-query SDPA fell back to a quadratic-memory math kernel. The script now selects the memory-efficient kernel and the pinned Transformers adapter's explicit KV-expansion path. A CUDA regression compares outputs and gradients with the original grouped-query math calculation. Completion-only vocabulary projection is separately checked against full causal loss and gradients. Failed/interrupted trial directories are preserved.

`export_development.py --run runs/seed-smoke-v4` merges the adapter, verifies the pinned llama.cpp revision, converts to Q8_0 GGUF, and stages debug-only assets. The measured weights are 639,446,880 bytes (about 610 MiB). The package always sets `blueTeamTrained=false` and `releaseEligible=false`. Normal app assessments reject it; only explicitly invoked debug instrumentation exercises generation. Original private field recordings were not used in this run.

For the Android runtime probe, build/install the app and test APK, then run `adb shell am instrument -w -e aiRuntime true com.projectrevivesolutions.layerscope.diag.test/com.projectrevivesolutions.layertime.controller.BuilderSmokeInstrumentation`. Measurements are saved inside the app as `files/diagnostics/local-ai-benchmark.json`.

Measured Flip6 probe with optimized portable CPU kernels: load/tokenize 922 ms, first generation 11,643 ms, subsequent generation 1,641 ms, cancellation return 104 ms; reuse after cancellation passed. The input was 59 tokens and the output a tiny JSON object. These timings do not predict capture-review or analytic-authoring latency. The debug APK including Wireshark and the development model is approximately 750 MiB. The benchmark also verifies that the unqualified candidate cannot pass the normal product coordinator.
