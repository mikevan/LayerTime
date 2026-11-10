> Documentation reference copy. Original repository path: `docs/blue-team-ontology-dataset-and-training-plan-2026-11-10.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Blue-team ontology, dataset and proposed training plan

**Status: built for review; no new model training authorized or performed in this phase.** The owner requested the domain ontology and substantial training material before spending more time training. The narrow Builder experiments remain separate. The incomplete Builder quantity-v3 continuation is superseded; it has not trained a model.

## What exists now

`tools/ai-training/domain-data/v1` contains an initial domain ontology and a traceable candidate corpus. This is broader material than unit conversion or Builder parameter extraction. It is a reviewable starting point, not a claim that the complete blue-team knowledge base or production model is finished.

The ontology has **84 concepts, 28 relationships and 14 interpretation rules**, covering:

- Evidence, provenance, unknowns, capture visibility, clock validity, alternatives and corroboration.
- Wi-Fi identities, network names versus radio identities, disconnect requests, EAPOL, episodes, temporal links and count scope.
- BLE advertisements, address behavior, repeated data, service advertisements, connection visibility, GATT evidence boundaries, feature windows and session-label scope.
- Baselines, trends, latency, missing observations, radio-activity drops, cross-radio alignment, model disagreement and distribution changes.
- Plain-language reports, passive analytics, reusable recipes, reproducibility and evaluation.

Definitions are explicitly project-authored application definitions. They are not represented as copied standards, independently reviewed protocol specifications, or verified attack labels. Source-backed NIST facts remain a separate stratum. Existing D3FEND candidates are reference material pending their own semantic/rights review; this build does not silently admit them.

| Candidate stratum | Examples | Actual underlying coverage |
| --- | ---: | --- |
| Ontology explanation questions | 252 | 84 definitions, three question formulations each |
| Source-grounded defensive questions | 72 | 24 previously source-checked NIST paraphrases, three questions each |
| Defensive evidence reasoning | 3,072 | 48 authored families, 64 parameter/identity variants each |
| Passive analytic recipes | 384 | Eight proposals per scenario family; not executable app workflows |
| Real BLE window descriptions | 9,976 | Seven publisher sessions, one previously explored testbed |
| **Total candidate examples** | **13,756** | **Not 13,756 independently reviewed lessons or environments** |

The scenario families cover matching and nonmatching clients/networks, shared SSIDs, ordering, out-of-window exchanges, invalid clocks, broadcast requests, missing events, duplicate records, aggregation scope, unverified sender ownership, untrusted names, BLE identity/advertising interpretation, latency coincidence, recorder loss, baseline mismatch, cross-radio timing, unsupported certainty and repeatable analytics.

Every synthetic case retains 256 typed observations: **786,432 observations across 3,072 synthetic capture variants**. These are observation graphs, **not PCAP packet bytes**. The model input is a bounded summary plus relevant context and source references. It does not receive a silently truncated hundreds-frame prompt. Independently written arithmetic checks recompute summaries from the retained observations. Native packet-decoder validation is not claimed.

The source-backed questions include the retrieved factual context and provenance in their inputs. A model is not expected to invent an unseen page number or source hash. Ontology questions likewise include the declared entry. These tasks teach grounded explanation; they are not evidence that the model has memorized an entire protocol specification.

## Sources and rights

The BLE source is Rauf, Martinez and Mohsen, *Bluetooth Low Energy (BLE) Dataset: Raw-Capture for Intrusion Analysis*, Mendeley Data V2, DOI `10.17632/ntmmcw6jj8.2`, CC BY 4.0. Source notices, capture hashes, feature-matrix hashes, session IDs and feature-row references are retained. Publisher labels describe recording conditions, not the maliciousness of each packet. Those labels are metadata outside the language-model input and answer; this stratum does not fabricate incident verdicts.

The 24 NIST teaching cards are authored factual paraphrases from selected SP 800-94 and SP 800-153 pages, preserving document dates, page positions, anchors and hashes. They are historical detection/monitoring references; do not generalize obsolete configuration advice as current best practice. The larger local reference library, including SP 800-121r2 Update 1, has not been wholesale ingested as supervised answers.

Project-authored material is identified separately from source facts. Preserve repository licensing and individual source notices. Raw external downloads and private field captures are not automatically included in public archives. Quarantined instruction datasets, unresolved AWID intake and unreviewed D3FEND teaching answers remain excluded. No newly rented service, training API or source upload was used.

## Checks completed

The independent corpus audit passed all **13,756 candidates**. It checks hashes, identifiers, ontology endpoints, family/session split isolation, every synthetic evidence summary, frame references, original BLE feature values/source identity, publisher-label scope, source grounding and passive-recipe operator boundaries. It checked all **786,432 typed observations** and **9,976 original BLE feature rows**. Eight focused tests cover ontology consistency, all scenario families, negative identity/time/broadcast cases, conflicting duplicate records and archive integrity.

These checks do **not** independently validate the English interpretation labels, prove an incident occurred, establish user comprehension, or measure model quality. All records retain `trainingApproved: false`; no model was loaded for inference or training in this build. The existing trainer does not yet support these new task contracts.

Tokenization used only the local Qwen3-4B tokenizer. The pool contains **8,713,088 full conversation tokens**; the longest full example is **782 tokens**, without truncation. That is not an approved training-token budget. About 7.35 million tokens come from BLE descriptions; running the whole pool unweighted would teach a badly imbalanced task mix.

## Important gaps before training

1. **Independent Wi-Fi data is missing.** These Wi-Fi cases are synthetic. Acquire licensed or owner-consented passive recordings covering ordinary reconnects, roaming, varied access points/clients, broadcast requests, capture loss and genuinely labelled incidents where the label is justified. Keep whole environments/recordings apart. Do not publish private field evidence without its own review.
2. **BLE diversity is limited.** There is one testbed. Validation sessions in the current split are spam-present only, so benign validation false-warning performance cannot be measured there. Additional independent benign environments are needed before deployment claims.
3. **The sourced knowledge base is still small.** Twenty-four source-backed teaching concepts are not a comprehensive wireless reference. Add reviewed, rights-checked protocol and defensive-operation material with explicit version/section anchors. Review modern BLE/Wi-Fi applicability rather than padding with generic instruction rows.
4. **Interpretations and reporting priorities need domain review.** Agent-authored answers are candidates. No independent expert review is implied. The real BLE stratum teaches descriptive limits, not reliable attack detection. It must not dominate learning or teach blanket abstention in place of useful warnings.
5. **Recipes need executable semantics.** Current outputs are labelled non-executable proposals. Map the approved operators to actual app tools, validate their behavior and preserve evidence before admitting executable-workflow targets.

## Proposed sequence — review this before more training

### 1. Review the ontology and labels

Review all 84 definitions and 14 rules, all 24 sourced cards, at least two examples from each of the 48 scenario families, one recipe from each family, and stratified BLE windows across all seven sessions and four explanation topics. Verify protocol meaning, benign alternatives, meaningful warning language and the separation of observation from inference. The review queue is saved beside the dataset. Record corrections as a new dataset version; keep this draft and its hashes.

This is where the owner/domain reviewers can critique actual material, not approve an abstract claim that a dataset is good. The current ontology is initial project work, not a substitute for that review.

### 2. Improve source coverage and admit a balanced corpus

Add the missing source-backed facts and independent Wi-Fi/BLE cases. Deduplicate and review at the concept, scenario and recording level. Keep source rights, environment, label scope and transformations attached. Separate human/expert labels, publisher labels, rule-derived facts and agent-authored interpretations.

Keep the entire candidate pool for inspection, but do **not** concatenate it into one training job. Begin with scenario reasoning as the largest stratum, sourced/ontology explanation as supporting material, and reviewed analytic recipes as a distinct task. Cap BLE descriptive windows, select diverse training-only examples, and exclude purely numeric-copy tasks if they add no value over ordinary rendering code. Freeze exact selected IDs and sampling weights after review. Report both unique concepts/families/environments and token counts.

### 3. Implement and verify the training task interfaces

Add explicit schemas and validators for the approved domain tasks. Verify source/claim citations, unsupported conclusions, unknown values and passive recipe semantics. Keep exact arithmetic, identity extraction and other reliable ordinary code outside the model. Test adapters against app evidence interfaces before starting a GPU job. The model should interpret significant patterns and explain/reuse the interpretation, rather than learn to replace a calculator.

### 4. Freeze evaluation and establish baselines

Use the clean pinned base model for the domain baseline, not the narrow Builder adapter as presumed domain expertise. Compare a native evidence report, the unadapted model and the eventual adapted model on the same reviewed material. Hold complete recording/environment/scenario families apart. Exposed Builder tests remain separate regression checks, not domain qualification.

Measure supported observations, device/network identity, temporal direction, count scope, evidence citations, meaningful warning coverage, false warnings, unsupported certainty, clarification quality, recipe behavior and plain-language comprehension. Report failures by task and source; do not hide them in aggregate accuracy. The public dataset cannot also serve indefinitely as an unseen qualification set. Reserve separately collected prospective field cases for that purpose.

### 5. Choose the training recipe from the admitted data

Proposed starting candidate: the locally pinned Qwen3-4B base with supervised domain fine-tuning. LoRA/QLoRA remains an implementation option, **not the ontology or dataset**. Compare the required capacity with the task coverage; make rank/target modules configurable rather than silently reuse the earlier rank-eight query/value-only setting. Full fine-tuning is another option if evidence justifies its cost. Do not choose by novelty or assume more parameters/updates automatically help.

Define the run by approved examples, sampled tokens, effective batch size and evaluation intervals—not “let it run for an hour.” Start with a small measured pilot to verify numerical stability, checkpoint recovery and early task behavior. Only then size the full run. Estimate runtime/memory and, if rented compute would help, show the provider/resource, maximum spend, token/update budget and stop criteria before booking anything. No server rental or training starts under this plan until the owner reviews it.

### 6. Qualify, then package for Android

Require identity/time/citation correctness on the critical cases, useful warning behavior on reviewed positives and benign controls, no unsupported password/physical-attack conclusions, and validated recipe execution. Preserve all failed results. If criteria fail, revise data and keep exposed questions as regressions; do not repair outputs to inflate scores.

After domain/task gates pass, export a portable offline model package with its tokenizer, licences, exact dataset/recipe/model hashes and public model/data cards. Then measure actual Android latency, memory, thermals, offline operation and failure behavior. Retain immediate evidence-based reports while the AI runs. Finish with a capture scenario the owner can independently reproduce and verify. No Android deployment is part of the current work.

## Persistence and public critique

The candidate files, generator/checker source, draft history, notices, manifests, review queue and this plan are preserved in a new transparency snapshot. Earlier snapshots are unchanged. Users should be able to identify a version and item ID, inspect the request/evidence/target and its provenance, and propose a correction. Publication and model admission are separate decisions; this archive is prepared for review and has not been publicly published.
