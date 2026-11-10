> Documentation reference copy. Original repository path: `tools/ai-training/domain-data/v1/REVIEW_SAMPLES.md`. Historical statements retain their original context; use the collection index and check-in note for current status.

# Blue-team dataset review samples

These are unapproved candidates. Full evidence, source hashes, split assignments and all 336 queued review items are retained.

## concept-observation-0

Task: EXPLAIN_DEFENSIVE_CONCEPT; split: test; source: layertime-blue-team-ontology-v1

Explain observation in plain language.

Expected answer:

```json
{
  "conceptId": "observation",
  "definition": "A value actually present in supplied telemetry, with its origin and visibility limits.",
  "definitionKind": "project-authored-scoped-definition"
}
```

## concept-observation-1

Task: EXPLAIN_DEFENSIVE_CONCEPT; split: test; source: layertime-blue-team-ontology-v1

What does observation mean in LayerTime?

Expected answer:

```json
{
  "conceptId": "observation",
  "definition": "A value actually present in supplied telemetry, with its origin and visibility limits.",
  "definitionKind": "project-authored-scoped-definition"
}
```

## source-false-positive-0

Task: ANSWER_SOURCED_DEFENSIVE_QUESTION; split: train; source: layertime-nist-paraphrases-v1

What is a false warning?

Expected answer:

```json
{
  "answer": "A false positive occurs when normal activity is incorrectly identified as malicious.",
  "pdfPage": 17,
  "scope": "Historical detection concepts; do not treat obsolete technology or configuration advice as current.",
  "sourceId": "nist-sp800-94",
  "sourceSha256": "289c2ed27f7e3aac41624bf2e6fad268642bad2dabe352a5ab28fa169824a96e"
}
```

## source-false-positive-1

Task: ANSWER_SOURCED_DEFENSIVE_QUESTION; split: train; source: layertime-nist-paraphrases-v1

Explain briefly: What is a false warning?

Expected answer:

```json
{
  "answer": "A false positive occurs when normal activity is incorrectly identified as malicious.",
  "pdfPage": 17,
  "scope": "Historical detection concepts; do not treat obsolete technology or configuration advice as current.",
  "sourceId": "nist-sp800-94",
  "sourceSha256": "289c2ed27f7e3aac41624bf2e6fad268642bad2dabe352a5ab28fa169824a96e"
}
```

## synthetic-linked_repeat-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-linked_repeat-000:f0",
    "synthetic-linked_repeat-000:f6"
  ],
  "headline": "Repeated disconnect requests were followed by EAPOL for the same device and network.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 1,
    "links": [
      {
        "delayMs": 1000,
        "exchangeId": "synthetic-linked_repeat-000:f6",
        "requestId": "synthetic-linked_repeat-000:f5"
      }
    ],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Review the cited sequence and compare it with the normal reconnect pattern.",
  "schemaVersion": 1,
  "significance": "This sequence deserves prompt review, but does not establish hostile intent or password recovery.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-other_network-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-other_network-000:f0"
  ],
  "headline": "The observed EAPOL belongs to a different network radio.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Compare both client and BSSID before linking the events.",
  "schemaVersion": 1,
  "significance": "A client match alone does not satisfy a same-client and same-network link.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-clock_invalid-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-clock_invalid-000:f0"
  ],
  "headline": "The sequence has an invalid timestamp.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Inspect recorder timing before applying an elapsed-time rule.",
  "schemaVersion": 1,
  "significance": "The capture cannot support a precise delay or this time-bounded link.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-requests_only-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-requests_only-000:f0"
  ],
  "headline": "Disconnect requests were captured without a linked EAPOL exchange.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Review channel and time coverage before interpreting the missing exchange.",
  "schemaVersion": 1,
  "significance": "No linked exchange in this capture does not prove that none occurred outside its visibility.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-ble_repetition-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-ble_repetition-000:f0"
  ],
  "headline": "Most accepted advertising data in this window repeats.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Compare repetition and rate with an appropriate reference window.",
  "schemaVersion": 1,
  "significance": "Repetition can occur in ordinary advertising and is not an individual-packet attack label.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-aligned_drop-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; source: layertime-blue-team-scenarios-v1

What matters in this capture, and what can we conclude?

Expected answer:

```json
{
  "evidenceReferences": [
    "synthetic-aligned_drop-000:f0"
  ],
  "headline": "Wi-Fi and BLE observations fell in aligned intervals.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Retain clock alignment, exposure and alternative explanations.",
  "schemaVersion": 1,
  "significance": "Aligned changes support a correlation, not proof of one cause or hostile intent.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-linked_repeat-000-recipe

Task: PROPOSE_PASSIVE_ANALYTIC_RECIPE; split: test; source: layertime-blue-team-scenarios-v1

Describe a reusable passive analytic for this interpretation. Do not claim it is already executable in the app.

Expected answer:

```json
{
  "countScope": "DEVICE_AP_PAIR",
  "executable": false,
  "followOnWindowMs": 30000,
  "goal": "Repeated disconnect requests were followed by EAPOL for the same device and network.",
  "kind": "passive-training-recipe-proposal-not-app-workflow",
  "limitations": "This sequence deserves prompt review, but does not establish hostile intent or password recovery.",
  "linkIdentity": "DEVICE_AP_PAIR",
  "operators": [
    "assess_visibility",
    "filter",
    "group_by",
    "order_by_time",
    "temporal_join",
    "aggregate",
    "explain"
  ],
  "schemaVersion": 1
}
```

## synthetic-linked_repeat-001-recipe

Task: PROPOSE_PASSIVE_ANALYTIC_RECIPE; split: test; source: layertime-blue-team-scenarios-v1

Describe a reusable passive analytic for this interpretation. Do not claim it is already executable in the app.

Expected answer:

```json
{
  "countScope": "DEVICE_AP_PAIR",
  "executable": false,
  "followOnWindowMs": 30000,
  "goal": "Repeated disconnect requests were followed by EAPOL for the same device and network.",
  "kind": "passive-training-recipe-proposal-not-app-workflow",
  "limitations": "This sequence deserves prompt review, but does not establish hostile intent or password recovery.",
  "linkIdentity": "DEVICE_AP_PAIR",
  "operators": [
    "assess_visibility",
    "filter",
    "group_by",
    "order_by_time",
    "temporal_join",
    "aggregate",
    "explain"
  ],
  "schemaVersion": 1
}
```

## ble-reference-window-00000

Task: EXPLAIN_BLE_OBSERVATIONS; split: train; source: ble-ntmmcw6jj8-v2

Explain the repetition observation and its limits without predicting an attack.

Expected answer:

```json
{
  "evidenceReference": {
    "captureSha256": "d23b657b546664067b9c4ddfa14e7c4ff97152743d27476f193c73947cbb566d",
    "featureRow": 0,
    "sessionId": "24ab42e345f728ff",
    "sourceId": "ble-ntmmcw6jj8-v2"
  },
  "explanation": "The accepted-record repeated-data fraction is 0.9560. Repetition alone does not establish hostile intent.",
  "incidentVerdict": "NOT_ESTABLISHED",
  "schemaVersion": 1,
  "scope": "Accepted legacy advertisements; one source testbed."
}
```

## ble-reference-window-00001

Task: EXPLAIN_BLE_OBSERVATIONS; split: train; source: ble-ntmmcw6jj8-v2

Explain the address_format observation and its limits without predicting an attack.

Expected answer:

```json
{
  "evidenceReference": {
    "captureSha256": "d23b657b546664067b9c4ddfa14e7c4ff97152743d27476f193c73947cbb566d",
    "featureRow": 1,
    "sessionId": "24ab42e345f728ff",
    "sourceId": "ble-ntmmcw6jj8-v2"
  },
  "explanation": "The accepted-record random-address fraction is 0.9167. That flag does not classify an individual packet as malicious.",
  "incidentVerdict": "NOT_ESTABLISHED",
  "schemaVersion": 1,
  "scope": "Accepted legacy advertisements; one source testbed."
}
```
