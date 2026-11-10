# Expanded unit-conversion and clarification training — November 10, 2026

## Protocol frozen before predictions

Objective: fix Builder request interpretation of time units, setting roles, defaults, and the need to ask for clarification. Android device testing, installation, packaging of a new model, and production enablement are deferred.

The native request instructions now distinguish an omitted setting (use its default) from an explicit number without units (ask). They separate exchange and episode-gap roles, permit explicitly shared units, define time conversion factors, and prohibit rounding fractional milliseconds. Both comparison conditions receive exactly these same revised instructions. No improvement is attributed to training merely by comparing against the old instruction set.

Data: 384 train examples from 24 authored wording templates (16 parameter variants each), 48 validation examples from six different wording templates, and 48 newly authored test questions. Train decisions: 192 drafts, 144 clarifications, 48 unsupported. Fresh test decisions: 24 drafts, 16 clarifications, 8 unsupported. Parameter variants are not independent users or environments. The development agent reviewed the templates and labels; independent expert review is not claimed.

Targets are produced with rational arithmetic and separately audited with decimal arithmetic and lexical checks. All 480 targets passed. Gold drafts underwent 1440 native engine checks (240 drafts × six capture situations), and all 480 gold decisions passed the native decoder/request-template check. During pre-freeze review, duplicate parameter variants, a validation-template scope assignment, and lexical aliases were corrected before any prediction; no test-output-guided label edits.

Pinned seed SHA256: 47c11e557a3c58e10a1ecf85b9f55869b7ab5db2d445fffab1d804d039e944df.
Fresh 48-question test SHA256: d0d371c721ad838a7e2821b871e6586afdb0644dd69dc5933c87aee8faccf9c5.
Combined evaluation SHA256: c9c0ea67a21f0ba7ae066c637543917547c431c2497d2d3f6ba4c6d8bd1351f1.

The combined evaluation includes those 48 primary fresh questions and 16 previously examined v1 requests as secondary development checks. The latter keep their original target decisions and user wording, but receive the same v2 instructions. They are not fresh held-out evidence. No exact evaluation request occurs in the new training set. Results will always be separated by role rather than presented as 64 independent fresh tests.

Control: previous 64-update intent adapter (SHA256 8b3a27ab1d1ecd7c09e8dd4b5a2ffc4191635f2fa81d731b7a596fba6331a8e6), on the pinned original Qwen3-4B-Instruct-2507 base with revised instructions. Candidate: continue from exactly those adapter weights, with a fresh optimizer and 384 additional updates (one pass through 384 train rows), same seed 42, rank-8 q/v LoRA, NF4 double quantization/FP16 compute, initial loss scale 256, unscaled finite-gradient guard. Training sees train/validation only; no early stopping, retries of test outputs, or candidate selection using test results.

Preserve full model/optimizer/scheduler/scaler/RNG checkpoints every 96 updates, retaining two. Validate a controlled save-and-resume at the first checkpoint before completing the remaining 288 updates; count each iteration once. Checkpoint recovery is an engineering check, not an extra candidate. Keep corpus, model ancestry, source snapshots and runtime provenance fixed across recovery.

Evaluate both conditions once on all 64 frozen requests: greedy decoding, 128 output tokens, 2048-token context, no added evidence/policy prompts. All evaluation inputs fit (maximum 516 tokens). Recheck shape separately from exact request meaning. Primary gates: all 24 supported fresh requests correct, all 16 fresh clarification requests ask rather than guess, and all 8 unsupported requests remain declined. Check the older 16 cases separately. Passing this authored test is not production or independent user qualification. Native checks verify executable drafts, not the correctness of language interpretation.

## Preparation

103 Python tests ran with 102 passing and one CUDA-only skip under asserted CPU isolation. Native gold/engine checks passed. New numerical audit rejects self-consistent but wrong conversion targets. No Android device checks have been run.

## Results

| Fresh 48-question measure | Previous adapter, revised prompt | Expanded-data continuation |
| --- | --- | --- |
| Exact requested meaning | 28/48 | 37/48 |
| Supported drafts correct | 12/24 | 13/24 |
| Exact time settings | 14/24 | 14/24 |
| Correct clarification | 8/16 | 16/16 |
| Guessed draft instead of clarification | 7 | 0 |
| Unsupported requests declined | 8/8 | 8/8 |

The known 16-case development check improved from 12/16 to 13/16; both missing-unit cases now ask. It is reported separately and is not fresh test evidence.

The expanded curriculum fixed the measured clarification failures but did not improve exact time settings. Errors remain in fractions/decimal scaling, setting roles, and unnecessary clarification of valid requests. All 64 outputs are structurally valid, so schema correctness cannot resolve the remaining problem. The primary authored-test gate failed. Do not enable this numerical-output adapter or proceed to Android device testing.

Desktop verification passed: 354 JVM tests, including all 64 actual proposals through the native compiler/engine boundary. Python verification ran 111 tests, 110 passed and one CUDA-only skipped under asserted CPU isolation. No device checks, new APK/model packaging, installation, or production enablement occurred.

## Follow-on approach

Retain this completed experiment unchanged. Next, teach the model to extract the original quantities and their roles, then route them to an exact unit-conversion atom. Bind quoted quantities to the user request to reject invented units. Score both extraction and final interpreted settings; do not describe deterministic arithmetic as learned model competence. Establish a new frozen test before the next training run; the exposed v2 questions become development checks.

## Control result and training progress

The previous adapter under the revised instructions scored 28/48 on the fresh questions: 12/24 supported drafts, 8/16 required clarifications, and 8/8 unsupported requests. It guessed a draft in seven clarification cases. The known development set scored 12/16, with both missing-unit requests still guessed. This is the fixed control for the expanded-data continuation, rather than the older prompt's 11/16 figure.

The training and recovery implementation passed six focused recovery checks covering changed arguments/source/data, altered checkpoint weights, incomplete checkpoints, and duplicate ledgers. The summary replayer passed two checks for separating evaluation roles and rejecting incomplete or falsely marked output. The final training/recovery source snapshot was saved and checked against the running code before training. The numerical guard remains enabled at loss scale 256. Training started with the specified prior adapter weights, a fresh optimizer, and a fixed budget of 384 further updates.

## Recovery check

The planned pause completed at update 96. Independent CPU inspection found 144 optimizer states, all at step 96, with finite tensors; the scheduler was at 96 and the loss scale was 256. The checkpoint contained Python, NumPy, CPU and CUDA random states, and a finite adapter with a recorded SHA256. A full checkpoint copy and pre-resume ledgers are archived under recovery-history/1.

Recovery refused changed-code/data/configuration paths in focused tests. The actual run validated the original corpus hash, source hashes, runtime versions, and checkpoint weight hash, then resumed at update 97 with the expected learning rate of 0.000075. This establishes successful checkpoint recovery and continuity, not a claim of bitwise equivalence to a separate uninterrupted run. The final ledger must cover 1..384 exactly once before a candidate is saved.

The halfway checkpoint independently confirmed all 144 optimizer states at step 192, scheduler step 192, loss scale 256, and finite tensors. Optimizer progress therefore continued across the planned recovery rather than resetting at 96. The numerical ledger continues with unique update IDs. New model evaluation remains deferred until the fixed 384-update budget completes.

## Training completed

All 384 further updates completed exactly once, with zero skipped updates and 384 finite gradient checks. Final optimizer states were independently inspected at step 384. All 144 adapter tensors are finite and differ from the starting adapter. The final adapter SHA256 is ff115f68288e522cdcb443d698d6f924af68f2efe7e39f36a9322704f9d5cae7; its starting adapter SHA256 remains the pinned v1 value. The first 96-update phase took 306.874 seconds and the resumed 288-update phase took 1167.830 seconds, plus 22.327 seconds final validation. Raw framework sample-rate/loss fields are retained without treating them as quality measures; paused/resumed phases can use different denominators.

The final trainer state reports globalStep 384, resumedFromStep 96, auditedIterations 384, and peak allocated GPU memory 4,573,334,528 bytes. The candidate is now undergoing the exact same 64-request evaluation as the control, without retries or changes to labels/prompts.

A source coverage audit found 32 follow-on-only draft examples, 32 gap-only draft examples, and 48 missing-unit examples for each role. Positive whole-millisecond microsecond conversions are absent from training draft targets; one fresh question probes that generalization. This limitation is recorded without changing the frozen data or acceptance criteria.
