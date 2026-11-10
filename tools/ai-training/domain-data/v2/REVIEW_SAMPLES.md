# Corrected blue-team review samples

All examples are candidates. Scenario names appear here for reviewers, not in the model messages. Original evidence and visible-ID maps are retained.

## concept-credential_exposure_context-0

Task: EXPLAIN_DEFENSIVE_CONCEPT; split: validation; approval: pending

Explain credential_exposure_context in plain language.

```json
{
  "conceptId": "credential_exposure_context",
  "definition": "Security mode, visible exchange material and credential context needed to assess exposure; observed EAPOL alone does not establish password loss.",
  "definitionKind": "project-authored-scoped-definition"
}
```

## source-ble-rpa-mapping-0

Task: ANSWER_SOURCED_DEFENSIVE_QUESTION; split: validation; approval: pending

Is an observed resolvable private address automatically the underlying identity address?

```json
{
  "answer": "The reference distinguishes a Resolvable Private Address from an Identity Address and describes their mapping through the privacy mechanism.",
  "pdfPage": 39,
  "scope": "Publication covers Bluetooth through 4.2; external coauthors require rights review. Not Bluetooth 5/6 coverage.",
  "sourceId": "nist-sp800-121r2-upd1",
  "sourceSha256": "72eb105cd5ddb77b9adaadb097dfd1b428d86cf7edd81219c27dde0b437ac8b5"
}
```

## synthetic-linked_repeat-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-520a53b2642d5da9c46f",
    "observation-5bba419aa3e28ac2a5f1"
  ],
  "headline": "Repeated disconnect requests were followed by EAPOL for the same device and network.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [
      {
        "disconnectRequestIds": [
          "observation-520a53b2642d5da9c46f",
          "observation-f92872ef2192d7b080a1",
          "observation-1725b9510eb655dc631a",
          "observation-1f969f2016ed920dc802",
          "observation-07d5d75db44c54e3975a",
          "observation-f3938c90c85f8367b685"
        ],
        "exchangeIds": [
          "observation-5bba419aa3e28ac2a5f1"
        ],
        "network": "02:74:f1:d3:4f:a1",
        "target": "02:16:d5:7c:6e:e3"
      }
    ],
    "linkedExchanges": 1,
    "links": [
      {
        "delayMs": 1000,
        "exchangeId": "observation-5bba419aa3e28ac2a5f1",
        "requestId": "observation-f3938c90c85f8367b685"
      }
    ],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Review the cited sequence and compare it with the normal reconnect pattern.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "PROMPT_SEQUENCE_REVIEW",
  "schemaVersion": 1,
  "significance": "This sequence deserves prompt review, but does not establish hostile intent or password recovery.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-other_network-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-90793fc3c816df403bf0"
  ],
  "headline": "The observed EAPOL belongs to a different network radio.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [
      {
        "disconnectRequestIds": [
          "observation-90793fc3c816df403bf0",
          "observation-90e8677c71899d303f77",
          "observation-8c20c6263f554a40d714",
          "observation-475b3f23512eac2ebb6b",
          "observation-5c3c36850aec9924c4b8",
          "observation-60a44705a402a64dfac6"
        ],
        "exchangeIds": [],
        "network": "02:d5:ae:c6:77:b6",
        "target": "02:a3:86:26:3d:a0"
      },
      {
        "disconnectRequestIds": [],
        "exchangeIds": [
          "observation-dc08a60991380962ba3f"
        ],
        "network": "02:2a:49:dc:48:cf",
        "target": "02:a3:86:26:3d:a0"
      }
    ],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Compare both client and BSSID before linking the events.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_DISCONNECT_PATTERN",
  "schemaVersion": 1,
  "significance": "A client match alone does not satisfy a same-client and same-network link.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-clock_invalid-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-8e711652e42a3c1ec3d1"
  ],
  "headline": "The sequence lacks usable timestamps.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [
      {
        "disconnectRequestIds": [
          "observation-8e711652e42a3c1ec3d1",
          "observation-4f634d7622fb046e2275",
          "observation-539bc460e2731e4b0929",
          "observation-ad950fbf578f7de573a0",
          "observation-f4d8b12503fa5948220e",
          "observation-938d6c3b764da87f61eb"
        ],
        "exchangeIds": [
          "observation-efb1a7d7ef1dbf8e8352"
        ],
        "network": "02:6c:5f:1f:6f:0e",
        "target": "02:24:2e:c8:60:2c"
      }
    ],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Inspect recorder timing before applying an elapsed-time rule.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_DISCONNECT_PATTERN",
  "schemaVersion": 1,
  "significance": "The capture cannot support a precise delay or this time-bounded link.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-broadcast_request-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: validation; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-5ff479088a0c7c8ae531"
  ],
  "headline": "The disconnect requests have a broadcast destination.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [
      {
        "disconnectRequestIds": [],
        "exchangeIds": [
          "observation-180f083449874dc7313e"
        ],
        "network": "02:23:fd:8d:01:14",
        "target": "02:df:1e:bc:a0:70"
      },
      {
        "disconnectRequestIds": [
          "observation-5ff479088a0c7c8ae531",
          "observation-49dd1c4f93759857081b",
          "observation-4c3c7afb981937ec0870",
          "observation-b9b9da3a0cefe4dd5672",
          "observation-8f2301bed14a93c99d4f",
          "observation-0971970d24072d79f09f"
        ],
        "exchangeIds": [],
        "network": "02:23:fd:8d:01:14",
        "target": "ff:ff:ff:ff:ff:ff"
      }
    ],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 1,
    "uniqueDisconnectRequests": 6
  },
  "passiveNextStep": "Report the broadcast destination explicitly.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_DISCONNECT_PATTERN",
  "schemaVersion": 1,
  "significance": "Do not present them as requests uniquely targeting the client in the exchange.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-ble_repetition-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-1c125298f11af16753c6"
  ],
  "headline": "Most accepted advertising data in this window repeats.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Compare repetition and rate with an appropriate reference window.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_CONTEXT_AND_BASELINE",
  "schemaVersion": 1,
  "significance": "Repetition can occur in ordinary advertising and is not an individual-packet attack label.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-latency_coincidence-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: test; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-0dda8d22fff18e77b79a"
  ],
  "headline": "Latency rose near the radio events.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Compare synchronized network measurements and independent context.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_CONTEXT_AND_BASELINE",
  "schemaVersion": 1,
  "significance": "Temporal coincidence alone does not establish that the radio events caused the delay.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-unaligned_drop-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-09c4626693fdb52d7814"
  ],
  "headline": "The radio timelines have not been aligned.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Resolve clock offsets before making a cross-radio temporal claim.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "CHECK_CROSS_RADIO_ALIGNMENT",
  "schemaVersion": 1,
  "significance": "The supplied times cannot establish simultaneous changes.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```

## synthetic-score_uncalibrated-000

Task: REVIEW_DEFENSIVE_EVIDENCE; split: train; approval: pending

What matters in this capture, and what can we conclude?

```json
{
  "evidenceReferences": [
    "observation-b7e76bbebf2cd448699a"
  ],
  "headline": "The model produced a score without demonstrated calibration here.",
  "observedFacts": {
    "capturedRecords": 256,
    "duplicateRecords": 0,
    "followOnWindowMs": 30000,
    "identityGroups": [],
    "linkedExchanges": 0,
    "links": [],
    "observedExchanges": 0,
    "uniqueDisconnectRequests": 0
  },
  "passiveNextStep": "Label the score as experimental and review the supporting features.",
  "postureBasis": "draft-product-review-policy-not-incident-ground-truth",
  "reviewPosture": "REVIEW_CONTEXT_AND_BASELINE",
  "schemaVersion": 1,
  "significance": "The score is not a verified 94 percent chance of an attack.",
  "uncertainty": "The declared synthetic scenario is not independently verified incident ground truth."
}
```
