# Desktop development comparison

This protocol is recorded before predictions from the 128-iteration policy-reporting adapter are available. It evaluates a narrow reporting task, not a general attack detector or release candidate.

## Matched primary comparison

Run the trained Qwen3-4B adapter, its original Qwen3-4B base, and the pinned SmolLM3-3B challenger sequentially. Use all 64 frozen `policy-data/v4/prepared/test.jsonl` records, greedy generation, NF4 double quantization with FP16 compute, 10,240 context tokens and at most 1,024 generated tokens. Do not truncate evidence, add evaluator facts, add policy-guided instructions or include assistant answers in prompts. The examples already supply their configured policy and derived facts. Use each model's native nonthinking chat template. Record exact input, adapter and evaluator hashes. A budget overflow or model-loading failure is a failed experimental condition to report, not permission to silently change the comparison.

There are four held-out synthetic families, with 16 variants each: unresolved exchange identity, two targets, one unknown-reason request, and invalid-clock bursts. There are 32 policy-triggering cases comprising 48 client/access-point groups and 32 cases without a demonstrated policy trigger. These are not 64 independent environments. A missing policy trigger does not establish that the recording is safe.

Before inference, the adapter must have finite saved weights. Report the training ledger and any nonfinite gradient-norm readings separately. A successful save does not erase numerical concerns.

## Measures and review

Report exact-schema and supplied-reference validity across all 64 cases, then evidence-group coverage and nonempty reports on non-trigger cases using the policy scorer. Coverage alone cannot establish that a warning or explanation is correct.

The prespecified initial prose review includes variants `000` and `015` from each of the four families: eight outputs per model. Read the actual responses against the supplied evidence and expected facts. Record decisions bound to each exact output hash. Review other failures and suspicious cases as a separately described diagnostic expansion; do not present that selected expansion as a random sample.

Check the following separately:

- Does the headline put the important implication first in ordinary language?
- Are all triggering devices/access points represented, with correct counts and observed spans rather than the configured maximum window?
- Are direction, order, valid timestamps, unknown identities and exchange linkage described correctly?
- Are cited frames actually evidence for the claims, rather than merely valid identifiers?
- Does the answer avoid asserting a completed disconnect/reconnect, captured usable handshake, password loss, physical attribution or proven attack?
- Does uncertainty remain visible without burying the warning? Do quiet results avoid a claim of general safety?

Record generation latency and allocated GPU memory as desktop measurements only. No phone latency, memory fit, comprehension under stress, or field accuracy is established here.

## Separate Timeline transfer check

Use all 19 frozen `correlation-data/v2/candidates.jsonl` pages from 14 authored scenarios. They have not entered this policy-training run. Test actual linkage behavior, including cases below the six-request rate threshold, cross-page evidence, name observations, broadcast requests and segmentation. Review against native-engine expected facts. Do not apply the burst-only gold scorer to these different labels. Keep this result distinct from the primary policy comparison.

Both model families passed CPU tokenizer preflight for the full set. The 19-page export predates the newer explicit observed-span/delay fields; any later export or enrichment is a separate condition with a new hash, not a replacement of the frozen test.

## Known limitations

The policy corpus sometimes calls a BSSID a “network.” Different BSSIDs can belong to the same multi-access-point Wi-Fi network. Interpret its identity labels as access-point identity only; do not qualify its prose for whole-network identity claims. Correct future teaching material to say “access point” where appropriate without rewriting this running experiment's frozen inputs.

All scenarios and wording remain authored development material, without independent expert review or independent field recordings. The broader objective still requires BLE coverage, trustworthy source material, prompt-to-Builder competence and evidence-based readability evaluation. A successful narrow comparison does not meet those requirements by itself. No deployment is authorized by this protocol.
