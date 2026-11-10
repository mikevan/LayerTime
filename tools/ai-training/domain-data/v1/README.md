# Blue-team domain dataset v1 — review draft

Start with REVIEW_SAMPLES.md, ontology.json and the training plan in the repository docs. review-queue.jsonl contains 336 pending review items. All candidates have trainingApproved=false. No model training occurred.

candidates.jsonl holds 13,756 examples; train/validation/test files are candidate split assignments, not training authorization. The BLE rows are 9,976 feature windows from seven sessions in one testbed. Scenario variants come from 48 authored families. Source-backed questions reuse 24 NIST facts. Do not describe the row count as independent expert lessons or environments.

synthetic-observations.jsonl retains the typed observations behind the scenario summaries. These are not raw packet captures. Recipe proposals are explicitly non-executable. SOURCE-NOTICES.json preserves attribution and limitations. integrity-audit.json checks numeric/provenance consistency, not the truth of every English interpretation. token-inventory.json measures full example lengths without loading model weights.

Run audit_blue_team_domain.py from tools/ai-training to recheck this version against the original local feature matrix. The matrix itself remains under runs/ble-features-v1 and is hash-pinned; the source capture archive remains in external-data. Generate corrections into a new version; never overwrite this draft.

Critiques should name snapshot/version, item ID, disputed evidence or interpretation, supporting source, and proposed correction. Source/semantic review and task-interface implementation must precede model training. The publication archive contains public-source-derived material and project synthetic examples, not private field captures. Publication clearance remains separate from training review.
