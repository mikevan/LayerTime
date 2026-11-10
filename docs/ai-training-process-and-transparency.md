# LayerTime AI training: process and public inspection

This document records the process being used and the disclosure policy requested by the project owner on November 10, 2026. Training examples and their history are product development artifacts that users should be able to inspect and critique when the product goes live. This document does not announce a production-qualified model or publish private captures.

## What we are training

The current Builder experiments adapt an existing local Qwen3-4B-Instruct-2507 model. They do not pretrain a new language model from scratch, and they do not yet constitute comprehensive wireless security domain training. The immediate task is interpreting a natural-language request for a supported passive disconnect/EAPOL analytic.

The planner identifies an exchange window, request-episode separation, minimum episode count, and whether the count applies to one device/access-point pair or the recording. It also identifies requests that need clarification or fall outside this planner. That small settings contract is not a complete blue-team ontology. Other app tools are not made unsupported merely because this planner cannot express them.

## How a training item becomes an update

1. Author a request, its intended meaning, and the correct response. Preserve source identity, example identity, family, split, label status, and task. A template with sixteen parameter variations is sixteen examples, not sixteen independent users or capture environments.
2. Validate the labels. Check source permission, identifiers, schema, split separation, source text, and numerical targets. For the quantity task, an independently implemented Python arithmetic checker and the Kotlin app atom must agree. Compiled drafts are executed against deterministic fixture scenarios as an additional check.
3. Freeze the dataset, prompt, and evaluation protocol. Hash the actual bytes, preserve the source code and model ancestry, and keep training, validation, and test assignments separate. Previously examined tests are labelled known development/regression cases when reused.
4. Tokenize the request and correct answer. Training labels mask the input instructions/request; cross-entropy loss is calculated on the assistant answer tokens. The model receives feedback about which answer tokens it predicted incorrectly. Backpropagation updates the trainable adapter weights.
5. Keep the base model frozen and train LoRA adapters. The current recipe uses NF4 double-quantized base weights, FP16 computation, rank-eight adapters on query/value projections, alpha 16, dropout 0.05, learning rate 0.0001, seed 42, and batch size one with one-example gradient accumulation. The current follow-up is one pass over 768 examples, meaning 768 requested updates. Continued experiments inherit adapter weights but start with a fresh optimizer unless explicitly recorded as checkpoint recovery.
6. Verify the run. Record applied versus skipped updates, finite gradient checks, loss scaling, checkpoint weight hashes, optimizer/scheduler state, runtime versions, and saved-weight changes. A requested update count or a low loss does not establish successful training or useful behavior.
7. Evaluate once using the frozen questions. Use deterministic greedy decoding, fixed token budgets, and no repairs or retries. Record exact outputs, including malformed responses and refusals. Score requested meaning separately from valid JSON or runnable workflows. Replay actual outputs through the app boundary. Keep independent qualification and release eligibility separate from synthetic benchmark scores.

Validation examples are evaluated without weight updates. Test examples are not supplied to the trainer. Token budgets fail rather than silently truncate. Current context/output limits are 2048/128 tokens; full examples fit. Runtime manifests record the actual recipe for each run and take precedence over this description if a later recipe changes.

## Why the quantity conversion is a tool

The numerical-output experiment improved clarification but retained arithmetic and role-assignment errors. Its preserved results motivated a source-extraction task: the AI copies the original quantity text and chooses its setting role. A native atom performs exact conversion and routes the numeric result through the existing analytic compiler/flow engine.

The atom rejects invented units, partial numeric matches, unassigned quantities, out-of-range values, and fractional milliseconds. Defaults apply to unspecified settings, not an explicit number with a missing unit. It does not prove that every English request was understood. Correct source extraction, correct roles/scope, final intent, and executed behavior remain separate checks. Exact arithmetic performed by this atom must not be represented as arithmetic learned by the neural model.

## Relationship to ontology-driven training

An ontology describes concepts and relationships: devices, networks, frames, observations, temporal sequences, evidence, uncertainty, defensive interpretations, and available analytic atoms. Supervised examples can be derived from that ontology and source-checked captures. LoRA is one technique for teaching those examples to a pretrained model; rented GPUs supply computing capacity. Those are complementary choices, not alternative definitions of training.

The current settings schema and synthetic curriculum do not replace a reviewed blue-team ontology or a substantial source-traceable domain corpus. A broader phase should document the ontology, source coverage, environment diversity, conflicting evidence, benign explanations, capture visibility limits, and independent evaluations before expanding compute. More repetitions of a small synthetic corpus do not create that missing coverage.

## What users will be able to inspect

The publication materials should include:

- This process, the task contracts, the ontology when developed, and the actual training/evaluation recipes.
- Every project-authored training item, its expected response/meaning, origin, label status, family, and split; retain superseded or rejected items with their status.
- Permitted derived examples from external sources, with attribution, licence, transformations, and a clear distinction between permission to train and permission to redistribute. Where redistribution is restricted, publish the provenance, acquisition/transformation instructions, and hashes rather than the prohibited source bytes.
- Dataset manifests, hashes, generator/checker source, prompt versions, model ancestry and runtime versions, evaluation criteria, complete evaluation outputs where publishable, and results including failures.
- Explicit distinctions between synthetic variants, real independent captures, known regression questions, unseen controlled tests, and independent user/field qualification.

Private field captures, credentials, personal identifiers, restricted source material, and model weights are not automatically part of a public training-item archive. Their treatment must be recorded explicitly. Preserve private originals locally where authorized; use consented/redacted public examples with a transformation record if they are admitted to training. Do not substitute an undisclosed redacted example for a claimed exact training item.

## Persistence and revision policy

Prepared corpora and frozen seeds remain in their version directories. Trainer run directories retain exact corpus/source snapshots and ancestry. The transparency exporter creates an additional append-only snapshot of the project data directories and source/manifests outside ignored weight/cache directories. It records hashes and identifies draft paths; an existing archive destination is never overwritten. Source notices remain attached to derived data.

Keep corrections as new versions with a change rationale and links to superseded items. Do not overwrite the historical record to remove a failed experiment or change an exposed test back into an unseen test. A hash detects later changes; it does not provide independent timestamping or prove that a label is true. Public repository/release history should supply the publication record when release occurs.

The archive is prepared locally for review; it has not been uploaded or published. External-source redistribution and private-data review must be completed before publishing each snapshot. A product release should link its model/data card to the exact archived dataset and training recipe used, not merely to the latest changing files. No current experimental model is release eligible.

## How to critique an item

Identify the snapshot and example ID, the source/label you dispute, the observed versus claimed behavior, and a proposed correction with supporting evidence. Useful critiques include missing benign explanations, mistaken device/network identity, incorrect temporal relationships, invented units, wrong count scope, false certainty, inaccessible wording, source-rights problems, and gaps in environment coverage. Record accepted and rejected corrections with reasons and add regression cases. Publishing data enables scrutiny; it is not independent validation by itself.

Current experiment details are in [the unit/clarification cycle](ai-unit-clarification-cycle-2026-11-10.md) and [the quantity-extraction cycles](ai-quantity-extraction-cycle-2026-11-10.md). The transparency catalog is generated under `tools/ai-training/transparency/`.
