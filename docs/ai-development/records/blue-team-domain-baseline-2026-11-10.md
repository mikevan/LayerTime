> Documentation reference copy. Original repository path: `docs/blue-team-domain-baseline-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Blue-team domain baseline, revised curriculum and next training run

## Decision

The clean instruction model produced misleading interpretations even when its JSON was valid. The original domain-v2 pilot proposal is superseded. A compact, evidence-specific development curriculum is now proposed; no domain model weights have yet been trained from it. The main ontology remains 98 concepts and 42 relationships. Supplemental feature definitions and interpretation principles are separately versioned teaching material, not silently added ontology concepts.

The application must continue showing ordinary-code evidence summaries immediately. Desktop generation timings below are neither Android measurements nor acceptable evidence of emergency response speed.

## What the baseline actually measured

`tools/ai-training/domain-data/baseline-v1/protocol.json` froze 16 authored questions before one greedy response per question from the original Qwen3-4B-Instruct-2507 instruction model, without an adapter. This is a development diagnostic, not a fresh field benchmark. Training-family questions, reserved families and held-out BLE sessions have separate roles.

The full-table interface passed strict structure/evidence checks on 9/16 responses: 1/8 reasoning, 4/4 ontology, 2/2 sourced questions and 2/2 BLE descriptions. The eight reasoning responses had a median total generation time of about 100 seconds on the desktop RTX 2070; two hit the output limit. These are not accuracy scores. Among five parseable reasoning answers, all five retained the typed facts and used grounded references, while only three exactly echoed the requested reference list. Contract failures and invented evidence must not be conflated.

Agent prose review found material errors: confusing targets with senders, treating earlier exchanges as follow-on events, confusing absence of selected patterns with environmental silence, and misinterpreting BLE address statistics. Both BLE descriptions passed structure checks while containing interpretation errors. The review is linked to exact output hashes; it is not an independent expert review or a user comprehension study.

A post-hoc three-question compact diagnostic reduced median total generation time from about 104 to 30 seconds for those same questions and passed structure checks on all three. All three explanations still needed semantic revision. In particular, the model invented a monitoring threshold, buried a different-network finding, and treated a configured window as an observed duration despite missing timestamps. Prompt, interface and output budget changed together; this comparison does not isolate a training effect.

## Revised candidates

`domain-data/pilot-v3` preserves 13,450 compact candidates and the 925-item pilot selection. It excludes the deferred analytic-recipe task. The transformation:

- Separates configured windows from observed timestamps and durations.
- Keeps non-Wi-Fi reports focused on their supplied context rather than irrelevant zero Wi-Fi counts.
- Leaves exact measurements and identity tables for application rendering; the generated explanation cites local fact blocks.
- Keeps source/review metadata outside model messages and uses evidence-specific limits in explanations.
- Retains source actor/event maps outside messages for inspection.

An independent comparison checked all 13,450 transformed records against frozen v2 evidence, and checked all 925 pilot memberships and split assignments. It passed. Deliberately corrupted counts are rejected by the regression test. This is source fidelity, not independent verification of the original labels or prose.

`domain-data/pilot-v3-curriculum` adds 57 teaching items, bringing the proposed training subset to 982:

| Component | Items | Meaning |
|---|---:|---|
| Defensive reasoning | 720 | Authored synthetic variants from training families |
| Ontology explanations | 72 | Project definitions |
| Source-grounded explanations | 13 | Selected historical NIST SP 800-94 paraphrases |
| BLE descriptions | 120 | Windows from three training sessions in one source testbed |
| Feature/evidence teaching | 57 | 25 definitions/principles and 32 paired questions over 16 toy contexts |

The supplements teach distinctions the baseline missed: entropy versus address type, newness versus randomness, exact payload repetition versus similarity, logged versus raw units, configured versus measured time, target versus sender, and correlation versus cause. Paired affirmative and negative questions reduce a blanket “no” response shortcut. Counts are teaching items, not independent incidents or environments.

Candidates remain `trainingApproved:false`. The 982-item file is a proposal, not a claim of model exposure. The real Wi-Fi campaign remains external review only, and new NIST Bluetooth cards remain outside training. Private field captures, quarantined generic instructions and executable recipes are excluded. Source notices and hashes remain attached to the source artifacts.

### Density finding: do not mistake the pilot for substantial training

The exact-answer audit finds **176 distinct target answers across 982 items**. In particular, the 720 reasoning variants share 30 answers, and the 120 real BLE windows share four descriptive answers. Measurement variation is useful for checking invariance, but it does not add the amount of expert knowledge suggested by the row count. Exact answer uniqueness itself is not semantic diversity.

This is a material limitation relative to the requested substantial domain dataset. The pilot can test the compact interface and selected interpretation habits; it must not be presented as broad blue-team learning. Before the substantial run, expand source-grounded protocol knowledge and case-specific reasoning targets, with explicit coverage of frame/address roles, management-frame protection, handshake message scope and incompleteness, roaming and benign alternatives, capture loss/clock/channel effects, BLE privacy and PDU semantics, and counterexamples. Each new teaching claim needs a pinned licensed source or an explicit project-rule label, and an inspectable evidence-backed answer. More numeric variants of the same answer do not satisfy this expansion.

Expansion has begun in `domain-data/wireshark-field-teaching-v1`: 24 source-linked passive field cards and 48 question formulations, representing 24 distinct target answers. These are separate unassigned candidates, not added to the frozen 982-item proposal. The source intake pins Wireshark revision `55b5def4b5f19e91f56abfd9621f4ffbca938dce` and verifies each downloaded file against its Git blob and SHA-256. File notices identify GPL-2.0-or-later; the licence and attribution are retained. Nothing downloaded was executed. Cards link exact field-registration excerpts for address roles, flags, status/reason codes, management-frame-protection capability/requirement and EAPOL fields. Decoder facts and project-authored interpretation limits are separate. A field label is not a full protocol-standard explanation, incident label or verification result.

## Execution plan

1. Freeze a matched compact-interface diagnostic before the next baseline or weight update. Measure complete prompt-plus-target lengths for every proposed training example and reject oversize records; never silently truncate evidence or labels. The 16 previously exposed questions remain a development comparison, not a fresh qualification test.
2. Finish semantic review of the teaching targets and admission rules. Preserve corrections as new versions. Add the relevant pinned feature definitions to any report interface that requires them. Do not teach the model to call unknown measurements zero or to invent a baseline.
3. Prepare a development-only training manifest with explicit source permissions, train-family/session boundaries, reserved validation/test examples, hashes and token budgets. Keep candidate files unchanged. Extend the existing trainer to verify this domain profile and record configurable LoRA settings.
4. Run one bounded fresh-base QLoRA candidate, proposed rank 16/alpha 32 on query/value projections, one pass, seed 42, batch size one, finite-gradient checks and recoverable checkpoints. Start from the clean licensed instruction model, not a Builder quantity adapter. Record actual optimizer updates, weight changes, token exposure, runtime and failures. The configuration is proposed until frozen in the run manifest.
5. Evaluate the candidate against the frozen clean-model comparison. Score structural validity, substantive interpretation, policy preservation, latency and limits separately. Use separately reserved families/sessions for qualification; exposed diagnostics cannot establish generalization. Retain every failed output.
6. Before Android deployment, verify conversion/quantization preserves behavior, model storage and working-memory budgets, cancellation and error handling. Then measure on-device latency, heat and comprehension. No Android enablement is authorized by a desktop schema pass.

The first development run tests whether domain fine-tuning helps. It does not complete the substantial training program: more independently reviewed sources, diverse benign/incident environments, source-specific coverage and user comprehension evaluation are still needed.

The compact-interface protocol is now frozen in `domain-data/pilot-v3-evaluation/protocol.json`. All 982 proposed training items fit without truncation: 660,174 total prompt-plus-answer tokens, including 80,730 target tokens (with end-of-sequence tokens), with a largest example of 1,238 tokens. The proposed context budget is 4,096 tokens. These are tokenized data quantities, not evidence of optimizer updates or learned knowledge.

The matched clean-model run has completed: **16/16 structural/reference passes**, no output-limit hits, median complete response 25.30 seconds overall and 31.27 seconds for the eight reasoning questions. Agent prose review still flags material interpretation problems, including requests presented as completed disconnections, an activity drop presented as no activity, and BLE entropy/newness presented as address randomness. Full outputs, per-task and per-role replay, and hash-linked review notes are in `runs/qwen4b-domain-pilot-v3-base`. This is not a measured semantic-accuracy improvement or an Android qualification. The original run report's generic synthetic-only limitation was inaccurate for this mixed-provenance set; its review ledger records the correction while preserving the original report and source snapshot.

Validation: 145 Python tests ran under explicit CPU isolation, with 144 passing and one CUDA-only test skipped. Local GPU inference completed separately. No new domain adapter was trained. Data density and teaching-target review remain prerequisites for the substantial run; the small pilot must not be substituted for that request.

## Inspection and preservation

Start with `tools/ai-training/domain-data/pilot-v3-curriculum/REVIEW.md` for readable teaching examples. Complete items, parent hashes, feature extractor snapshots, source notices, protocols, failed baseline outputs and prose reviews are retained in the repository artifacts. The transparency exporter now includes domain baseline runs and their prose-review ledgers. Archives remain local and unpublished; duplicate snapshots are not unique training examples. No cloud rental, public upload or Android deployment occurred in this work.
