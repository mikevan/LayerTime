> Documentation reference copy. Original repository path: `docs/ai-quantity-extraction-cycle-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Source-bound quantities and exact conversion

## Purpose

The expanded numerical-output experiment improved clarification from 8/16 to 16/16 on its authored fresh questions, but exact time settings remained 14/24. Its adapter failed the gate and stays disabled. This follow-on changes the division of work: the AI identifies roles and copies quantities from the user's request; an exact arithmetic atom converts the original text into the existing numeric analytic intent. This is tool-assisted interpretation, not learned neural arithmetic.

## Frozen experiment

`tools/ai-training/quantity-data/v1/protocol.json` pins the experiment before model predictions. There are 512 training requests, 48 validation requests, and 32 fresh test requests. The 64 previously examined numerical-output questions are separate known development checks. These are project-authored synthetic requests, with wording templates and parameter variations; their count is not a count of independent users or field environments.

Training examples include separate interval roles, reverse ordering, shared quantities, omitted settings, explicit missing units, fractional precision, exact fractions, decimals, per-pair versus recording-wide thresholds, conflicting settings, and unsupported goals. Additional recording-wide requests balance supported training scopes. Original numerical targets remain independently checked; only the model's intermediate output changes to source text.

Seed SHA256: `27ef0664fbe1ba095a8398d4647845a38ae566a3f60dc8b335d90d1bbacaa9cd`.

Evaluation SHA256: `70da5b848d7442279119c1a87780e5601049e3d9cb32c19e55cb85992aed0116`.

The fixed control is the completed v2 adapter, SHA256 `ff115f68288e522cdcb443d698d6f924af68f2efe7e39f36a9322704f9d5cae7`, with the new extraction instructions and the same conversion atom as the candidate. The candidate continues those weights for exactly 512 additional updates, one pass over the training requests, with a fresh optimizer. Qwen3-4B-Instruct-2507 remains the local base; rank-eight q/v LoRA, NF4 base quantization, FP16 compute, seed 42, and initial gradient loss scale 256 are retained. Recovery checkpoints are saved every 128 updates, with two retained. No data downloads or model uploads are part of this experiment.

Evaluation uses one greedy response to each frozen question, without response repairs or retries. Context is 2048 tokens with at most 128 output tokens; overlength prompts fail instead of truncating. Score exact raw proposals separately from normalized intent. The primary authored-test gate requires all 32 fresh normalized intents correct: 16 supported, eight clarification, and eight unsupported. Passing does not establish independent user or field qualification.

## App boundary

`AnalyticQuantityAtom` rejects invented quantity text and partial numeric matches. It requires a count to occur outside the quoted time expressions, so a digit from an interval cannot silently become the episode threshold. Unassigned quantities request clarification. Omitted values alone use defaults. Conversion uses exact decimal/rational arithmetic, applies the unit multiplier before rational division, rejects fractional milliseconds instead of rounding, and enforces existing time/count bounds. The normalized result passes through the existing strict `AnalyticIntent` compiler and flow engine.

The Python checker uses exact rational arithmetic; the Kotlin atom uses BigDecimal. All 592 teacher examples agree across those implementations. Their 328 draft results also passed 1,968 executions against six deterministic fixture scenarios. The full desktop app suite passed 362 tests. Python verification ran 120 tests, with 119 passing and one CUDA-only check skipped under CPU isolation.

The source checks are bounded language checks, not a complete English parser. They cannot prove that the AI selected the correct interval role or scope; those choices are part of the measured end-to-end intent test. A well-formed proposal and a runnable draft are insufficient evidence of correct interpretation.

The new task is not advertised by an enabled product model and is not connected to the normal Builder interface. This phase includes no APK/model packaging, phone installation, Android device testing, or production enablement.

## Results

The completed control scored 21/32 exact normalized intents on the fresh set: 6/16 supported drafts, 7/8 clarification requests, and 8/8 unsupported requests. Raw proposals exactly matched their targets in 17/32 cases. Five fresh proposals failed the strict extraction boundary. The separate known development set scored 38/64. All 96 actual baseline responses were replayed through the native conversion atom and draft engine, agreeing with Python acceptance, normalization reasons, and numeric values. No failed output was repaired or resampled.

The fixed 512-update continuation completed in 723.350 seconds, plus 12.370 seconds final validation. All 512 updates were applied exactly once, with no skipped updates and 512 finite-gradient checks. Independent CPU inspection verified optimizer/scheduler states at 128, 256, 384, and 512. All 144 saved adapter tensors are finite and changed from the initial weights. Peak allocated GPU memory was 4,546,740,224 bytes. The resulting adapter SHA256 is `332237f0ddec38d61e8057584b3483e83b7106a3175b074b915f0753f5e8f5bb`.

The candidate scored 29/32 on the fresh questions, compared with 21/32 for control. Supported drafts improved from 6/16 to 13/16, clarification from 7/8 to 8/8, and unsupported requests remained 8/8. Raw proposal agreement improved from 17/32 to 28/32. The separate known development set improved from 38/64 to 57/64. All 96 actual outputs agreed across Python and native normalization/execution; the full 362-test desktop app suite passed again.

The primary gate failed. The three fresh failures were a malformed repeating response for a rational-hour request, an ordinary default WPA3 request incorrectly declined, and an explicit recording threshold omitted and caught by the unassigned-quantity guard. No response was repaired or retried. The corpus coverage audit found zero fully default supported training requests and zero protocol-marker examples. Exact conversion alone does not resolve those language/output failures.

## Separately frozen coverage follow-up

Keep v1 results unchanged. `quantity-data/v2/protocol.json` freezes the follow-up before its model predictions. It adds 256 training examples covering fully default requests with protocol names, count-first threshold phrasing, rational-hour intervals with default settings, missing units, and unsupported goals. With the original training requests retained, the corpus has 768 training, 64 validation, and 24 fresh test examples. The previous 96 evaluation questions are now known regressions, not fresh evidence. All 856 teacher examples passed the independent Python arithmetic audit and native atom/engine checks. Maximum supervised length is 387 tokens; maximum evaluation prompt length is 458, so no truncation occurs under the unchanged 2048/128 budgets.

The control is the completed v1 extraction adapter. The candidate continues its weights for one 768-update pass with a fresh optimizer and unchanged training settings; save every 192 updates, retaining two recovery checkpoints. The new gate requires all 24 fresh questions and all 96 known regressions correct. Control evaluates the 24 new questions once; its previous 96 outputs remain the fixed regression baseline without resampling. No model/native instructions, arithmetic rules, or scoring rules are changed. No phone testing or production enablement occurs in this follow-up.

## Follow-up results

The 768-update continuation completed in 656.946 seconds, plus 17.847 seconds final validation. All 768 updates were applied once, with 768 finite-gradient checks and no skips. All 144 adapter tensors are finite and changed from the v1 starting weights. Optimizer/scheduler states were independently checked at 192, 384, 576, and 768. Peak allocated GPU memory was 4,546,740,224 bytes. Final adapter SHA256: `fe3e38a218354790b61c3092a0594ceacf233f91a48e196d1343c8fbfba53613`.

| Authored evaluation | Fixed control | Follow-up |
| --- | ---: | ---: |
| Fresh exact intent | 20/24 | 24/24 |
| Fresh supported drafts | 12/16 | 16/16 |
| Fresh time settings | 13/16 | 16/16 |
| Fresh required clarification | 4/4 | 4/4 |
| Fresh unsupported requests | 4/4 | 4/4 |
| Fresh exact raw proposal | 20/24 | 24/24 |
| Known regression exact intent | 86/96 | 90/96 |

The old 96-case control is the already recorded v1 adapted response set, not resampled output. Five previous errors were fixed, five persisted, and one previously correct request regressed. The six remaining errors comprise three wrong count scopes and three overlong/misassigned source spans that trigger clarification for otherwise valid requests. The new regression is `quantity-v1-intent-v2-test-18`: the model copied a whole episode phrase instead of the count word. Exact outputs, expected meanings, and failure identities are retained in the replayed review and completion audit.

All 30 clarification requests across the full 120-case evaluation normalized correctly, with no guessed drafts; all 24 unsupported requests were declined. This does not excuse the three unnecessary clarifications on supported requests. Valid-source normalization succeeded for all 120 outputs, but six meanings remained incorrect. Native replay agreed with Python normalization/acceptance and executed the proposed drafts; it does not convert those six failures into correct interpretations.

The fresh-only metric passed, but the frozen overall protocol gate failed because the known regressions were not all correct. Keep this adapter disabled and Android testing deferred. Further work should address span boundaries and count-scope semantics with a reviewed concept/relationship model and broader request coverage, preserving these exposed cases as regressions. These authored results do not establish independent user or field performance.
