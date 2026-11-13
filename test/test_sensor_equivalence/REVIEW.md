# Sensors 1.1.2 regression oracle review

The old fixture predates deliberate detector-policy updates. Its corpus also
iterated live signature tables, so adding table rows changed seeded addresses,
input indexes, Wi-Fi timestamps and RSSI values. Comparing that moving corpus
with a fixed old output combined input drift with detector-policy differences.

The input keys are now the same ordered 10 UUIDs and 39 OUIs from LayerTime
commit `894b508`, stored in `corpus_894b508.h`. Production classification still
uses the pinned Sensors library. The original `golden_894b508.txt` is preserved.

The active expected output is derived from that historical oracle by the
versioned rules below, rather than copied from the current classifier output.
`review_policy_update.py` reproduces the expected file without invoking or
reading the classifier. Its default check also verifies the historical oracle,
fixed input keys, exact change counts and final expected-output hash.

| Reviewed policy change | Changed expectations |
|---|---:|
| Nineteen non-exclusive Flock OUIs become Low in BLE | 372 |
| The same non-exclusive Flock OUIs become Low in Wi-Fi | 294 |
| Short Find My records lack a device-status byte and become Low/unknown | 6 |
| The obsolete `0x1E` subtype is rejected | 3 removed |

Flock's own `B4:1E:52` prefix keeps its existing grade. Independently name-gated
manufacturer records keep their existing grade. Addresses, signal strength,
channel, timestamps, selection behavior, event order and every other output
remain unchanged under the constrained translation. The translated expectation
was then compared with the classifier output for these fixed inputs and matched
exactly. No detector code or dependency version was changed.

The intended rules are documented in the pinned library's
[versioned changelog](https://github.com/mikevan/LayerTime-Sensors/blob/35e7213874d4577bd9385f226b20e614a65fbfa8/CHANGELOG.md),
sections 1.1.0 and 1.1.2, and exercised by its explicit grading tests. Numeric
values, prefix changes, source provenance and hashes are recorded in
`reviewed-sensors-1.1.2.json`.

This is an AI-assisted source and policy review of synthetic regression inputs.
It is not independent human review, field validation, or proof that an observed
device is hostile.
