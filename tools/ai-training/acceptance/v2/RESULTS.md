# Unchanged-model acceptance baseline

**Development acceptance failed: 1/12 cases passed all frozen gates.** All 12 outputs passed structure/reference checks and the 30-second complete desktop response limit. Four exceeded the 160-word explanation limit; one exceeded the headline limit. Semantic review identified failures beyond formatting and speed.

This is an agent-reviewed development acceptance result, not an independent expert judgement, broad model-accuracy estimate, field evaluation or user-comprehension study. Some cases fail for burying the required finding even when parts of the body are reasonable. The unit-clarification case asks for a unit but fails the strict no-applied-rule criterion because its headline says the window is already set. Reviewers can inspect and challenge every judgement in `results/semantic-review.json`.

| Case | Result | Main finding |
|---|---|---|
| Same client/network | Fail | Misses the follow-on relationship and observed order |
| Different network | Fail | Does not explain the different BSSID; conflates frame types |
| Different client | Fail | Changes the request target to the exchange client |
| Earlier exchange | Fail | Does not lead with the earlier exchange |
| Unknown timestamps | Fail | Does not explain timing as the primary unresolved requirement |
| Broadcast target | Fail | Buries broadcast scope and adds an unsupported sender claim |
| Activity drop | Pass | Retains remaining records and unresolved capture health |
| BLE entropy/type | Fail | Adds unsupported spoofing/covert-behavior interpretations |
| BLE newness/type | Fail | Describes zero new-address fraction as all addresses new |
| Missing unit | Fail | Asks for the unit but says the window is already set |
| Incomplete exchange | Fail | Buries the single-message limitation |
| Captured instruction | Fail | Rejects captured text as non-evidence instead of retaining it as untrusted data |

The original v1 fixtures omitted explicit frame types and radio domains in some inputs. Their run is preserved under `../v1/results` as a fixture diagnostic, not a readiness score. V2 corrected those inputs without changing the original rubric or thresholds, then froze before this run. Concepts and v1 diagnostic exposure overlap; v2 is not a fresh independent benchmark.

The baseline uses Qwen3-4B-Instruct-2507 with no adapter, greedy generation, a 4,096-token context budget and a 384-token output limit. Model weights were unchanged. Exact responses, source snapshots, base intake metadata, hash-bound semantic reviews and the acceptance score are preserved under `results`.

Reproduce grading from `tools/ai-training` using the existing Python environment:

```text
python score_acceptance.py --suite acceptance/v2 --run acceptance/v2/results --review acceptance/v2/results/semantic-review.json --output acceptance/v2/recheck.json
```

The output path must not already exist. Missing semantic review cannot yield acceptance. Android qualification and deployment eligibility remain false regardless of this desktop score.

Validation: 161 Python tests ran, 160 passed and one CUDA-only test was skipped under CPU isolation. The initial committed checkpoint's five acceptance-grader tests also passed from an isolated Git archive without local datasets/models. All work after the user's check-in instruction remains uncommitted; the user controls the supplied check-in/push script.

The next development step is a bounded candidate trained on admitted teaching examples, then a repeat of this unchanged acceptance suite. Dataset expansion remains paused. These acceptance inputs, targets and reviews must stay out of training.
