# Sensor replay harness (BLE detection pipeline)

This is the application integration replay: it follows each observation past
the sensor output into LayerTime's event log and alert policy. The public
replay, which stops at the sensor output, lives with the sensor library in
`sensors/test/test_public_replay`.

A deterministic, host-only harness that replays recorded or synthetic BLE
observations through the **real** LayerTime detection code and checks the
result. It exists because a watch that went quiet in the field could not be
diagnosed: there was no way to prove, off the device, that a given
advertisement does or does not produce the expected detection.

## What it drives (no reimplementation)

- `core/logic/ReconClassification` - core's classification entry point,
  which runs the LayerTime-Sensors BLE classifier and signature tables
  (`sensors/src/lts`) on each observation.
- `core/logic/ReconSelection` - the scan-scope rules that decide which
  detectors a selection enables.
- `core/logic/MonitorEventLog` + `core/logic/AlertPolicy` - the stateful
  history: one record per (detector, sourceId), repeat-in-place, strongest
  confidence kept, capacity eviction, alert policy.

`replay.h` only records an observation, adapts it to the production
`BleAdvertSource` through the same two callbacks the device's NimBLE adapter
uses, routes each emitted `Candidate` into the right receiver's event log with
the platform-supplied time, and reads records back. The detection decisions are
entirely the production code's.

## Design points this harness establishes

- **Explicit time.** Every observation carries `atMs`; nothing sleeps. Windows,
  repeats, and ordering are exercised by the timestamps alone.
- **Per-receiver state.** One `MonitorEventLog` per receiver id, so four wands
  never share one detection history. `two_receivers_keep_independent_histories`
  pins this.
- **Missing vs zero.** `Observation` carries availability flags (`hasRssi`,
  `hasChannel`, `hasName`) so an absent field is not silently a real zero.
- **Acquisition / processing / delivery seam.** The observation is acquisition,
  the classifier + event log is processing, the records read back are delivery.
  This is the boundary the `sensors/` library will formalize at layout step 5.

## Run it

    ./run_harness.sh            # all cases; nonzero exit on any failure
    ./run_harness.sh <case>     # one case in isolation

On failure each line carries the fixture, observation index, simulated time,
and expected vs actual value.

## Coverage (13 cases, 52 checks, all passing on v1.0.1 source)

Valid signatures (Tile FEED High, Flipper 3081 Medium); close-but-invalid
(Apple record with no Find My subtype); truncated manufacturer record; repeated
observations (count in place, no re-alert); changing name at one address (one
record); Low-then-High confidence upgrade that never alerts (pinned KNOWN
DEFECT); scan-selection scope; independent receiver histories; capacity
eviction at 40; sleep-mode suppression; clear() keeps the event serial;
acquisition gap updates lastSeen.

## In-repo home and the pre-flight gate

Lives at `test/replay/`. It stays in LayerTime after layout step 5 because it
tests LayerTime's event log and alert policy, not the sensor library; the
library's own public replay is `sensors/test/test_public_replay`.
`tools/ci/preflight.sh --host`, run locally before every push, builds and runs
it; a failed check fails the pre-flight. LayerTime does not use GitHub Actions
or any hosted CI.

## Hardware validation gaps (not covered here, by design)

- Radio reception: whether the antenna and scan duty cycle actually capture a
  given advertisement over the air. This harness starts from a captured
  observation; it does not prove capture.
- On-screen behavior: what the watch face and alert UI actually show.
- The Wi-Fi promiscuous path and the NimBLE/ESP parsing on the device: the
  adapter here mirrors that parsing but does not run the vendor stack.

These remain device UAT, as recorded in the project's testing constraints.
