> Documentation reference copy. Original repository path: `docs/ai-training-progress-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# AI training results and next steps — November 10, 2026

## Decision

The overnight research, corpus, bounded training and evaluation cycle is complete. **No tested language-model candidate is ready for normal product reporting.** All training and inference jobs have stopped. Nothing was deployed to the phone.

The custom adapter improved policy-group citation coverage, but still invented timing, mishandled unresolved evidence and failed broader Timeline interpretation. The BLE classifier is promising only within one laboratory dataset. The anomaly ensemble is unsuitable for issuing warnings.

The [next training plan](<ai-next-training-plan-2026-11-10.md>) prioritizes verified facts, explicit task skills, better independent material and reliable longer-run recovery. Full verification is retained at `tools/ai-training/runs/overnight-final-audit-v1/report.json`.

## What was trained

The latest Qwen3-4B development adapter used 128 controlled training variants from eight original synthetic families. Validation and test each contain 64 variants from four separate families; the whole policy corpus has 16 original families. Variants are not additional independent recordings or environments. The test split was never supplied to the trainer.

Training used QLoRA NF4 double quantization, FP16 compute, rank-eight query/value adapters, batch size one, learning rate 0.0001 and seed 42. It took 93.64 minutes plus 2.29 minutes validation. Train/validation losses were 0.4381/0.1197; low loss did not establish correct free-generation answers. Peak allocated GPU memory was about 6.87 GiB.

The framework reports 128 applied updates and zero skips. Gradient-norm logs at iterations 13 and 28 are nonfinite and remain a numerical concern. All 144 saved tensors are finite in repeated checks; all 72 LoRA B tensors are nonzero. The adapter contains 2,949,120 F32 parameters and occupies 11,815,504 bytes. It is a patch requiring the separate frozen base model, not a standalone AI of that size. Its SHA-256 is `9fff76a1e4db5162da3aac2c243d3b922e7a42b3086418276caf0e4de78e0166`.

The earlier two-update compatibility run and 22-iteration source-concept/capture pilot are separate feasibility experiments. Their artifacts and failures remain in the work log; they are not qualified models. The latest policy adapter did not train on the separate Builder, Timeline or BLE teaching candidates.

## Matched policy comparison

All three conditions used the same 64 frozen synthetic cases, greedy generation, NF4/FP16, a 10,240-token context and 1,024-token output cap. Gold answers were excluded. Supplied source instructions and evidence were preserved; no evidence was truncated.

| Condition | Valid interface / 64 | Usable cited groups / 48 | Valid nonempty reports on non-trigger cases / 32 | Full reporting passes in eight prespecified answers |
| --- | --- | --- | --- | --- |
| Original Qwen4B | 63 | 28 | 1 | 3 |
| Qwen4B with custom adapter | 63 | 47 | 7 | 3 |
| SmolLM3 with corrected native input | 5 | 0 | 5 | 0 |

Citation coverage is not detection recall or explanation correctness. Invalid output gets zero usable coverage; Smol's zero does not mean its unparseable prose never mentioned a pattern. Nonempty non-trigger reports were inspected rather than automatically labelled false alarms. The original Qwen report eventually acknowledged invalid clocks but contradicted that qualification in its headline. The seven adapted reports and five interface-valid Smol reports made unsupported temporal claims from invalid clocks.

All passing prespecified policy answers were quiet results. None of the four warning examples per model passed the full reporting criteria. The eight endpoints are not a random accuracy sample. Exact decisions bind to output hashes. A separate post-hoc literal-duration check contradicted ten of 15 checkable adapted-model bounds; it abstains on unsupported wording, mixed groups, subsets and invalid clocks. Zero checkable bounds for another model is not a superior accuracy result.

These tests concern one configured six-requests-in-three-seconds analytic, not every app warning criterion or actual attack ground truth. See `runs/policy-comparison-v2/primary-comparison.json` for full measurements. Generation timings are desktop observations with different answer lengths, not mobile or end-to-end alert latency.

## Timeline transfer

All 19 pages from 14 authored scenarios were reviewed for each condition, separately from rate-policy scoring.

| Condition | Valid interface / 19 | Full reporting passes / 19 |
| --- | --- | --- |
| Original Qwen4B | 10 | 0 |
| Qwen4B with custom adapter | 9 | 2 |
| Corrected SmolLM3 | 9 | 1 |

The adapted model's passes were quiet beacon-only pages; it explained none of the 16 pages containing related observations. Smol's pass was a factual informational comparison of observed shared names. No model produced a fully acceptable related-disconnect-episode explanation in this controlled set.

Failures included wrong AP/client roles, reason-code meanings, timing and authentication conclusions; missing exchanges; treating beacons as client connections; and confusing filtered-out exchanges with absent captured exchanges. Review used actual supplied observations and native-engine expectations, not exact-string agreement with candidate prose. Reviews are by Codex, the same development agent, not independent human experts or intended-user studies.

Frozen v2 name fixtures used synthetic beacons with unicast destinations. This limits realism; a beacon still does not establish a client connection. Separate candidate v4 corrects destinations and retains clearer v3 answers and explicit span/delay inputs. Frozen evaluation files and measured outcomes were not rewritten. Versions reuse the same scenarios rather than adding independent environments.

## Sources and teaching material

- Three NIST publications are retained as reference material: 219 PDF pages and 451 extracted passages. They are not 451 approved supervised examples. Twenty-four authored cards preserve document/page anchors, publication scope and separate product interpretations. Selected factual paraphrases entered the earlier small multitask pilot using source-document-separated splits; entire PDFs were not used as supervised answers.
- MITRE D3FEND v1.6.0: 25 selected definitions cross-checked between CSV and ontology, with full terms and notices retained. Twenty-five authored teaching candidates have same-agent source review and remain outside training intake.
- Builder: eight prompt/workflow candidates across five related goal families passed the real Kotlin decoder, typed validator and practice-frame execution. This does not establish general prompt-authoring competence.
- Timeline: revised candidate v4 contains 19 pages from the same 14 scenarios. It remains outside current model training.
- BLE: six teaching candidates illustrate three questions using exact feature rows from two publisher-labelled benign recordings. They explain repeated data, address-format flags and model disagreement, retain attribution/changes, and remain outside training. These are selected teaching windows in one testbed, not six new recordings. Numeric wording still needs intended-user review.
- InSecTT: 66,739 connection records in six measurement sets and 18 streams passed publisher-count/header/order checks. They are not advertisement-spam labels; native timestamp units and task-specific labels still need clarification before rate-based training.

AWID access remains unresolved, including the user's VPN attempts. The inspected KRACK/Kr00k CSV lacks required client/BSSID/EAPOL fields. Trendyol's 53,201 instruction records and HolyCyber's sampled source material remain excluded because of factual and/or source-rights issues. Do not substitute incompatible or unverified data merely to increase corpus size. Private field recordings were excluded.

## BLE specialist outcomes

Seven licensed recordings produced 9,976 nonoverlapping ten-second windows. Leave-one-session-out CatBoost results were TP 5,156, FN 9, TN 4,731 and FP 80. The 1.66% normal-window false-positive fraction is not an alerts-per-hour measurement. Labels indicate spam presence in a recording, not that every packet was malicious. All recordings originate in one testbed.

The Kitsune-inspired fixed-group autoencoder ensemble incorrectly flagged 4,786 of 4,811 held-out normal windows: 99.48%. It failed normal-session transfer. Two benign sessions cannot provide independent training, calibration and testing environments; retuning against these same results would not establish independent validation.

Saved predictions, feature labels/session identities and aggregate counts were recomputed and matched. A bounded Scapy comparison matched 896 accepted records on selected PDU fields; it does not verify rejected records, wrappers, CRC/timestamps or Hunter feature parity. Neither specialist was deployed or enabled for Scope warnings.

## Implementation and verification

Local AI input now includes cited observed Wi-Fi names across pages, indexed once per recording. The decoder accepts only additional citations actually supplied. Exact observed spans, counts and delays from preceding requests are distinct from configured windows. A timestamp-range horizon does not establish continuous channel coverage.

A harness bug was caught: Smol's native template discarded task instructions after a prepended schema system message. The three partial outputs were stopped and excluded from model-quality conclusions. Corrected role merging preserves every message; all completed Qwen prompt bytes were verified unchanged. Shared training/evaluation guards now reject omitted content, and all 192 current Qwen training/validation prefixes remain unchanged.

The final audit verified four corpus directories, source anchors, all 22 pinned model-intake files, six conditions comprising 249 generations, 24 prespecified policy-review bindings, all 57 Timeline reviews, current candidate hashes and actual adapter finiteness. Selected diagnostic reviews remain separate. Latest Python checks: 83 pass, one deliberate CUDA-only skip. Full Android context/timing regression: 333 pass; later candidate-export targeted checks also pass. No phone installation, model export or deployment occurred.

Independent recordings, expert review, intended-user comprehension, mobile memory/latency and release qualification remain future work. Local readable copies are in the OneDrive workspace's `Overnight AI` folder; cloud synchronization or Android availability has not been verified.
