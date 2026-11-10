# Grounded recording reports — implementation

The recording workflow now uses `REVIEW_GROUNDED_CAPTURE`, with response schema version 2. A model package must explicitly support this task. Older `REVIEW_CAPTURE` packages are not silently treated as compatible. The original version-one interface remains for historical training exports and diagnostics, not the live recording-review path.

## Accepted evidence contract

The whole-recording correlation engine supplies stable episode IDs and typed claims: observed client/BSSID, disconnect count, linked EAPOL count, observed request span, network-match category, minimum/maximum delay from preceding requests, and untimed-exchange context count. Spans and delays use whole milliseconds; configured windows are not substituted for observed durations.

The response must identify the same recording/page and include every supplied episode exactly once. Claims must match their source values, including nulls and types. Missing, invented or duplicated episodes, changed identities/counts/times, arbitrary prose, extra fields and stale recording/page identities are rejected. The app assigns citations from the underlying evidence; the model cannot supply substitute frame references.

Accepted summaries are rendered from checked recording facts, with important observations first and technical details afterward. Observed names are cited separately by BSSID, so a shared name does not merge access points. Untimed EAPOL mentioning the client remains visible as recording-wide context, including across pages; it is not promoted to an ordered same-pair follow-on. “No linked exchange” is explicitly distinguished from “no captured exchange.”

The UI labels this section “RECORDED FACTS.” It does not present the deterministic wording as freely generated AI analysis. The baseline is delivered before model processing and remains available if the model is missing, incompatible, over budget, times out or contradicts the facts.

## What this proves and what remains

This layer validates agreement with decoded recording evidence. It does not authenticate physical senders, prove attacks or make an arbitrary free-form explanation trustworthy. The current grounded task intentionally has no model-prose channel. It establishes the claim boundary for later, separately evaluated interpretation skills; returning the supplied facts is not evidence of learned forensic reasoning.

No existing experimental model was promoted, retrained or installed. New training/evaluation examples must target this explicit contract, with fresh held-out material, before a package advertises support. Broader interpretation and prompt-to-Builder authoring remain separate tasks. Historical version-one corpora and measured model comparisons were not rewritten.

## Verification

All **344 local JVM tests in 56 suites pass**, with no failures/errors/skips. Evidence is retained in `diagnostic-artifacts/grounded-review-2026-11-10`.

Regression coverage includes:

- Wrong client/access-point identity, counts, linked-exchange claims and observed spans.
- Two clients sharing an access point, with only one having a linked exchange.
- Shared Wi-Fi names on different BSSIDs, with exact cross-page name citations.
- Invalid exchange timestamps retained as unresolved context rather than absence.
- An exchange between requests: requests at 0, 100 and 200 ms, exchange at 150 ms. The observed request span is 200 ms and the delay from the preceding request is 50 ms. Replacing that span with the configured 3,000 ms is rejected.
- Exchanges before a request or at an equal timestamp, without inventing a later reconnect.
- Omitted/duplicate/invented facts, unsupported prose, wrong page/recording, and old-schema responses.
- A real workflow test showing baseline delivery before a contradictory model result, and rejection without losing that baseline.
- Early refusal to invoke an older model that supports only the legacy task.

To reproduce locally from `devices/layertime-controller-android`, with the Android SDK configured:

```powershell
./gradlew.bat :app:testFullDebugUnitTest --tests '*GroundedReviewTest' --tests '*KoogIntegrationTest' --offline --console=plain
```

These are code/fixture checks, not an on-device or real-model qualification. Nothing was deployed to the phone.
