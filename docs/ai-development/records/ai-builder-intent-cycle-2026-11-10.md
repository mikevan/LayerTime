> Documentation reference copy. Original repository path: `docs/ai-builder-intent-cycle-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Prompt-to-Builder intent cycle — November 10, 2026

## Protocol fixed before predictions

Purpose: test a useful, bounded language task: translate a request into settings for a repeatable disconnect/EAPOL analytic. The model proposes intent; native code compiles editable wiring and report rules. This does not establish general packet interpretation or dense domain training.

New PLAN_ANALYTIC capability is separate from CREATE_ANALYTIC and the reporting tasks. Supported settings are follow-on milliseconds, episode-gap milliseconds, minimum linked episodes, and counting scope. DEVICE_AP_PAIR counts episodes for one device and AP; RECORDING counts across pairs. Native compilation selects the appropriate metric. Unsupported goals cannot produce wiring; ambiguous/out-of-range settings require clarification. No product model capability or UI route is enabled.

Data: 28 original authored request cases, two phrasings each (56 rows). Train: 32 rows/16 cases; validation: 8 rows/4 cases; test: 16 rows/8 cases. These are case/parameter/phrasing splits within one narrow skill, not independent user populations or field environments. The development agent authored/reviewed labels; no independent expert review. All gold decisions passed the real Kotlin decoder; 38 gold drafts ran through six controlled capture situations each (228 checks). The packet situations distinguish repeated same-pair episodes, different clients, different APs, invalid timing, exchanges before requests, and broadcast targets.

Frozen test SHA256: c0d7f7cc568904e439f2780de4069415072b5e6f4f28ccfe63625fcd5634106f. Native request/template parity was checked for every example. Original seed bytes and source snapshot are retained in intent-data/v1. All test prompts fit: maximum 413 input tokens plus 128 output tokens within a 2048-token context.

Measure the original pinned local Qwen3-4B-Instruct-2507 model first. If it leaves a measurable gap, train at most one new task-specific adapter from that original model: 64 optimizer updates (two passes through 32 rows), seed 42, rank-8 q/v LoRA, NF4 double quantization/FP16 compute, initial loss scale 256, finite-gradient guard and per-update diagnostics. No previous adapter is stacked or silently substituted. Evaluate the same 16 test prompts, greedy generation, 128 output tokens, 2048 context, no prompt additions/retries/test-guided tuning. Training sees train/validation only.

Score exact intended decision, values, and counting scope separately from JSON shape. Recheck actual model decisions through the real native compiler and six capture situations. A correctly compiled draft can still misunderstand the user's request, so native executability does not substitute for meaning agreement. All proposals remain editable drafts. No release qualification, new model package or phone deployment follows automatically.

## Preparation

Gold/native checks passed. Python suite: 97 tests, 96 passed and one CUDA-only skip under asserted CPU isolation. Full JVM suite passed before adding the separate actual-response audit hook; final checks will include that hook. First invalid-setting test exposed an exception-type inconsistency, corrected before any inference. No gold target or test prompt changed in that correction.

## Results and decision

The bounded experiment is complete. Keep this adapter out of normal prompt authoring.

| Measure | Original model | Intent adapter |
| --- | --- | --- |
| Valid typed response | 10/16 | 16/16 |
| Exact requested meaning/settings | 6/16 | 11/16 |
| Supported draft requests | 2/10 | 7/10 |
| Requests that required clarification | 0/2 | 0/2 |
| Unsupported requests correctly declined | 4/4 | 4/4 |
| Request cases with both phrasings passing | 2/8 | 5/8 |

The five remaining errors matter: a quarter-second becomes 25000 ms; two same-pair requests receive incorrect exchange/gap settings; and both missing-unit requests receive guessed units instead of clarification. All five are structurally valid. Native range/wiring validation alone cannot establish that the model understood the request. Saved predictions were independently replayed against the frozen authored labels, with no repairs or retries. This is a clearer measure than completion loss or JSON validity.

Training completed all 64 applied updates with no skips, all gradient checks finite at loss scale 256. All 144 saved tensors are finite and all 72 LoRA B tensors are nonzero. Actual training runtime was 189.802 seconds, validation 9.881 seconds, and peak allocated GPU memory 4,534,354,432 bytes. The 11,815,504-byte adapter requires the separate 4B base; it is not a standalone model. SHA256: 8b3a27ab1d1ecd7c09e8dd4b5a2ffc4191635f2fa81d731b7a596fba6331a8e6.

Observed median generation was 3.641 seconds for the original and 9 seconds for the adapter. These are indicative desktop timings under changing local load, not a controlled speed comparison or Android measurements.

The native counting capability is usable independently of this model: max_linked_episodes_per_pair groups by both client and AP; the rule editor exposes it separately from recording-wide linked episodes. Both model runs' saved proposals were checked through the real native decoder; the final adapted audit is preserved as native-adapted-audit.xml. Structurally valid drafts were executed on six controlled captures. This verifies compiled behavior, not language meaning. A discovered Gradle caching issue was fixed by declaring audit files as test inputs; export modes explicitly rerun. The initial cached no-op was not counted as a native model audit.

Final checks: 354 JVM tests passed; 98 Python tests ran, 97 passed and one CUDA-only skip under asserted CPU isolation. Diagnostic app/test APK builds passed, including the Android native practice check. Packaged test assets were checked byte-for-byte, with SHA256 records. Existing debug-only smoke model assets were preserved; the new adapter is not bundled or enabled. No phone install, model service, dataset upload, commit, or push occurred. All GPU jobs from this cycle are terminal.

## A concrete test for the user

Use diagnostic-artifacts/intent-practice-v2. It contains two generated four-frame captures and two loadable analytics. With one episode for each of two clients, the per-pair threshold of two must not match, while the recording-wide threshold must match. With two episodes for the same client/AP pair, both analytics must match. The positive case prevents an absent counter from passing through an always-zero result.

Native workflow import/engine checks passed for all four combinations. A separate raw-capture read checked frame kinds, counts, endpoint counts, and LLC EAPOL tags. Android native decoding and on-screen operation are not yet tested. The diagnostic test APK includes intentPractice=true for those four native checks without a language model. Stable APK copies and their hashes are under the practice pack's build directory; neither was installed.

## Next model work

This remains 28 authored request cases, not a large or independently reviewed language corpus. Expand coverage of explicit units versus omitted units, fractional language, setting roles, negation, counting scope, and unsupported combinations. Split whole wording/template families and parameter combinations; collect or independently review natural user requests. Preserve this cycle's held-out failures as development diagnostics, then freeze fresh held-out material before any revised training. Do not add steps to this same tiny dataset and call it dense domain learning.

One validation phrasing says EAPOL “after two seconds,” which is less precise than “within two seconds.” Review and clarify that wording before a future corpus version; the frozen example is retained unchanged. Validation was not supplied to gradient updates or used for candidate selection. This reinforces the need to review language labels separately from executable workflow correctness.

Keep AI proposals as explicit editable drafts, with interpreted intervals and counting scope visible for review. The native engine owns execution and observed facts. Other Builder tools remain available; NO_SUPPORTED_ATOM in this narrow planner means no supported plan in its current vocabulary, not absence of those tools throughout the app. The new planner is not wired into the normal UI and no model advertises its capability yet. General packet interpretation, BLE interpretation, multi-atom authoring, and production/mobile model qualification remain unfinished.
