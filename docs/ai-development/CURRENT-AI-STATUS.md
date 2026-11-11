# Current AI development status

## Result of the correction run

The second 4B candidate completed training but did not improve overall frozen acceptance: **2 of 12 cases passed**, the same total as the first candidate. It still fails the required Wi-Fi client, network-radio and timing distinctions. It must not replace the phone model.

The BLE address-type explanation improved. Missing-unit clarification remained a pass. The explanation of captured instruction-like text regressed. All 12 outputs met the structural/reference checks and the desktop response-time gate; those checks do not establish correct interpretation. The semantic scores are explicit agent-authored judgments, not independent expert validation or field accuracy.

Exact outputs, criterion judgments, scores, and source/receipt hashes are in [the correction evidence folder](../../tools/ai-training/acceptance/v2/trained-domain-v2/). Acceptance fixtures, token budgets and thresholds were unchanged.

## What actually trained

- Base: pinned local Qwen3-4B-Instruct-2507; continuation from the first domain adapter.
- Data: repaired views of existing admitted records, with source provenance; 341 training examples, 13 validation examples and 12 development test examples. The frozen 12-case acceptance suite is a separate evaluation, not optimizer data.
- Updates: 682 applied, none skipped, two passes, learning rate 0.00005, rank-16 QLoRA. Effective ancestry is 1,090 first-stage updates plus 682 correction updates, totaling 1,772. Seven unsaved updates from the abandoned oversubscribed attempt are excluded.
- Resource adjustment: frozen embedding and output matrices on CPU, quantized body on GPU. The completed run took about 61 minutes with shared resources. All 144 saved adapter tensors are finite.
- Adapter SHA-256: `45c75c3ff286a89de4d328f08428f6b441ceb252dd47f6f9d42a0ffd60220e1c`.

An FP16 CPU merge was produced locally for packaging preparation. No correction GGUF was packaged or installed because the comparison failed. No further training run is currently active.

## What is on the phone

Build 122 retains the first actual trained 4B candidate, `domain-v1-1090-q4km`, in the explicit debug AI test screen. It remains unqualified and excluded from ordinary product assessments.

Runtime changes reduced the two selected responses from approximately 74 seconds each to 23–29 seconds; a repeat measured 25–30 seconds. These are short paired experiments, not sustained thermal or field qualification. Cancellation returned in 116 ms and a queued token-count request established worker availability afterward. Memory improvement is not demonstrated.

[Phone measurements and runtime source](../../tools/ai-training/acceptance/v2/trained-domain-v1/runtime-122/README.md) preserve the details. In the app, use **Builder → More → Test trained AI**. The network-comparison answer remains a known failure.

## Next experiment

Before another long run, audit the existing training labels for conflicting instructions and compare representation variants on development data. The repaired training prompt and frozen evaluation prompt differ, and the model gives the same generic answer to meaningfully different cases; neither observation alone proves the cause.

Use a controlled fresh-base adapter comparison to test whether continuation from the first adapter preserves its errors. Select settings on development validation only; do not turn acceptance outputs into training targets or relax the acceptance gates. Keep the phone candidate unchanged until the required Wi-Fi distinctions improve without losing the other required behavior.

The Python suite completed 168 tests with one skipped; 364 Android unit tests passed and the Android validation build succeeded. The owner subsequently authorized committing and pushing this development checkpoint; [CHECKIN-BUILD122.md](CHECKIN-BUILD122.md) records its scope. Model qualification remains unchanged.
