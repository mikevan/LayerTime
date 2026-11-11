# AI development documentation

Start with [CURRENT-AI-STATUS.md](CURRENT-AI-STATUS.md) for the latest training outcome and phone measurements. [CHECKIN-NOTES.md](CHECKIN-NOTES.md) preserves the earlier checkpoint.
The current source/data checkpoint is described in [CHECKIN-BUILD122.md](CHECKIN-BUILD122.md).
The first trained candidate and the completed correction candidate each passed 2/12 overall development acceptance cases. Build 122 retains the first candidate with the faster runtime. No domain adapter or Android deployment is qualified.
The owner controls all subsequent commits and pushes; the check-in script only acts when they run it with `-Execute`.

## Current reading path

1. [Check-in note](CHECKIN-NOTES.md).
2. [Checkpoint and acceptance process](records/ai-development-checkpoint-and-acceptance-2026-11-10.md).
3. [Acceptance results](acceptance/v2/RESULTS.md).
4. [Ontology and training plan](records/blue-team-ontology-dataset-and-training-plan-2026-11-10.md).
5. [Training transparency process](records/ai-training-process-and-transparency.md).

## How this collection works

This directory contains explanatory documents and readable teaching reviews. Earlier research, proposals and narrow training reports are historical; their use of “current” does not override the check-in note. Original documents remain in place so established code/document references keep working. These are reference copies; document-manifest.json records original paths and both hashes. Edit the original, then explicitly refresh the collection. Word documents are copied byte-for-byte; Markdown links are adjusted. Machine-readable fixtures, protocols and model evidence remain in their canonical locations below.

## Evaluation evidence

- [Frozen v2 protocol](../../tools/ai-training/acceptance/v2/protocol.json).
- [Case rubrics](../../tools/ai-training/acceptance/v2/rubrics.jsonl).
- [Exact baseline outputs](../../tools/ai-training/acceptance/v2/results/responses.json).
- [Per-criterion semantic review](../../tools/ai-training/acceptance/v2/results/semantic-review.json).
- [Acceptance score](../../tools/ai-training/acceptance/v2/results/acceptance-score.json).

## All collected documents

| Document | Original repository path |
|---|---|
| [RESULTS.md](<acceptance/v2/RESULTS.md>) | `tools/ai-training/acceptance/v2/RESULTS.md` |
| [FEATURE_MEANINGS.md](<datasets/ble-feature-knowledge-v1/FEATURE_MEANINGS.md>) | `tools/ai-training/domain-data/ble-feature-knowledge-v1/FEATURE_MEANINGS.md` |
| [ATTRIBUTION.md](<datasets/eapol-semantic-teaching-v1/ATTRIBUTION.md>) | `tools/ai-training/domain-data/eapol-semantic-teaching-v1/ATTRIBUTION.md` |
| [REVIEW.md](<datasets/eapol-semantic-teaching-v1/REVIEW.md>) | `tools/ai-training/domain-data/eapol-semantic-teaching-v1/REVIEW.md` |
| [REVIEW.md](<datasets/evidence-contrast-v1/REVIEW.md>) | `tools/ai-training/domain-data/evidence-contrast-v1/REVIEW.md` |
| [REVIEW.md](<datasets/pilot-v3-curriculum/REVIEW.md>) | `tools/ai-training/domain-data/pilot-v3-curriculum/REVIEW.md` |
| [README.md](<datasets/v1/README.md>) | `tools/ai-training/domain-data/v1/README.md` |
| [REVIEW_SAMPLES.md](<datasets/v1/REVIEW_SAMPLES.md>) | `tools/ai-training/domain-data/v1/REVIEW_SAMPLES.md` |
| [README.md](<datasets/v2/README.md>) | `tools/ai-training/domain-data/v2/README.md` |
| [REVIEW_SAMPLES.md](<datasets/v2/REVIEW_SAMPLES.md>) | `tools/ai-training/domain-data/v2/REVIEW_SAMPLES.md` |
| [ATTRIBUTION.md](<datasets/wireshark-field-teaching-v1/ATTRIBUTION.md>) | `tools/ai-training/domain-data/wireshark-field-teaching-v1/ATTRIBUTION.md` |
| [toolkit-evaluation-guide.md](<guides/toolkit-evaluation-guide.md>) | `tools/ai-toolkit-evaluation/README.md` |
| [training-guide.md](<guides/training-guide.md>) | `tools/ai-training/README.md` |
| [LayerTime AI architecture and training proposal v2.docx](<records/LayerTime AI architecture and training proposal v2.docx>) | `docs/LayerTime AI architecture and training proposal v2.docx` |
| [LayerTime AI architecture and training proposal.docx](<records/LayerTime AI architecture and training proposal.docx>) | `docs/LayerTime AI architecture and training proposal.docx` |
| [ai-additional-source-audit-2026-11-10.md](<records/ai-additional-source-audit-2026-11-10.md>) | `docs/ai-additional-source-audit-2026-11-10.md` |
| [ai-builder-intent-cycle-2026-11-10.md](<records/ai-builder-intent-cycle-2026-11-10.md>) | `docs/ai-builder-intent-cycle-2026-11-10.md` |
| [ai-comparison-review-provenance-2026-11-10.md](<records/ai-comparison-review-provenance-2026-11-10.md>) | `docs/ai-comparison-review-provenance-2026-11-10.md` |
| [ai-development-checkpoint-and-acceptance-2026-11-10.md](<records/ai-development-checkpoint-and-acceptance-2026-11-10.md>) | `docs/ai-development-checkpoint-and-acceptance-2026-11-10.md` |
| [ai-grounded-reporting-2026-11-10.md](<records/ai-grounded-reporting-2026-11-10.md>) | `docs/ai-grounded-reporting-2026-11-10.md` |
| [ai-grounded-training-cycle-2026-11-10.md](<records/ai-grounded-training-cycle-2026-11-10.md>) | `docs/ai-grounded-training-cycle-2026-11-10.md` |
| [ai-multimodel-research-2026-11-10.md](<records/ai-multimodel-research-2026-11-10.md>) | `docs/ai-multimodel-research-2026-11-10.md` |
| [ai-next-training-plan-2026-11-10.md](<records/ai-next-training-plan-2026-11-10.md>) | `docs/ai-next-training-plan-2026-11-10.md` |
| [ai-overnight-completion-audit-2026-11-10.md](<records/ai-overnight-completion-audit-2026-11-10.md>) | `docs/ai-overnight-completion-audit-2026-11-10.md` |
| [ai-overnight-work-log-2026-11-10.md](<records/ai-overnight-work-log-2026-11-10.md>) | `docs/ai-overnight-work-log-2026-11-10.md` |
| [ai-policy-comparison-protocol-2026-11-10.md](<records/ai-policy-comparison-protocol-2026-11-10.md>) | `docs/ai-policy-comparison-protocol-2026-11-10.md` |
| [ai-quantity-extraction-cycle-2026-11-10.md](<records/ai-quantity-extraction-cycle-2026-11-10.md>) | `docs/ai-quantity-extraction-cycle-2026-11-10.md` |
| [ai-training-process-and-transparency.md](<records/ai-training-process-and-transparency.md>) | `docs/ai-training-process-and-transparency.md` |
| [ai-training-progress-2026-11-10.md](<records/ai-training-progress-2026-11-10.md>) | `docs/ai-training-progress-2026-11-10.md` |
| [ai-unit-clarification-cycle-2026-11-10.md](<records/ai-unit-clarification-cycle-2026-11-10.md>) | `docs/ai-unit-clarification-cycle-2026-11-10.md` |
| [blue-team-domain-baseline-2026-11-10.md](<records/blue-team-domain-baseline-2026-11-10.md>) | `docs/blue-team-domain-baseline-2026-11-10.md` |
| [blue-team-domain-review-and-source-intake-2026-11-10.md](<records/blue-team-domain-review-and-source-intake-2026-11-10.md>) | `docs/blue-team-domain-review-and-source-intake-2026-11-10.md` |
| [blue-team-eapol-semantic-expansion-2026-11-10.md](<records/blue-team-eapol-semantic-expansion-2026-11-10.md>) | `docs/blue-team-eapol-semantic-expansion-2026-11-10.md` |
| [blue-team-evidence-contrast-expansion-2026-11-10.md](<records/blue-team-evidence-contrast-expansion-2026-11-10.md>) | `docs/blue-team-evidence-contrast-expansion-2026-11-10.md` |
| [blue-team-ontology-dataset-and-training-plan-2026-11-10.md](<records/blue-team-ontology-dataset-and-training-plan-2026-11-10.md>) | `docs/blue-team-ontology-dataset-and-training-plan-2026-11-10.md` |
| [builder-behavior-reporting.md](<records/builder-behavior-reporting.md>) | `docs/builder-behavior-reporting.md` |
| [layerscope-visual-design.md](<records/layerscope-visual-design.md>) | `docs/layerscope-visual-design.md` |
