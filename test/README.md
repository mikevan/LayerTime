# Running the tests

No IDE, no framework, no platform. One compiler invocation.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests test_geogrid/test_geogrid.cpp ../src/core/logic/GeoGrid.cpp
./tests
```

One case in isolation, which is how the density tooling works:

```
./tests zone_arctic_west_of_prime_meridian_is_not_svalbard
```

## Coverage and density

```
g++ -std=c++17 -O0 -g --coverage -I. -I../src -I../sensors/src -c ../src/core/logic/GeoGrid.cpp -o GeoGrid.o
g++ -std=c++17 -O0 -g --coverage -I. -I../src -I../sensors/src -c test_geogrid/test_geogrid.cpp -o tests.o
g++ --coverage GeoGrid.o tests.o -o test_cov
python3 density.py
```

`density.py` runs every case in its own process, keeps a separate gcov profile
for each, and reports how many distinct cases touched each line.

The deployable firmware build never sees any of this. Nothing in
`platformio.ini` references `test/`, and the test binary is built by a
standalone g++ call that knows nothing about the device.

## Core model conformance

Checks `src/core/model` against `contracts/vectors`, the T-Ultra capability
profile against its JSON, and the new model against the existing T-Ultra
types it will be adapted from. Run from `test/`, because the vectors are read
from `../contracts/vectors/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_core test_core_model/test_core_model.cpp ../src/core/logic/QuickMessages.cpp
./tests_core
```

## T-Watch Ultra

The T-Watch Ultra's characterization and platform adapter suites moved with
it to `devices/lilygo-tultra/test/` in layout step 4 (2026-10-07), together
with the `stubs/` headers they use. The build commands are in
`devices/lilygo-tultra/README.md`, "Host tests". `test_core_model` stays here
until step 5 splits it; it reaches the Ultra's headers by their path under
`devices/lilygo-tultra/src/`.

A case whose name ends in `KNOWN_DEFECT` pins behaviour that looks wrong. It
is recorded, not fixed, so a later fix shows up as a deliberate change.

## Core logic unit suites

Phase 0 Step 3 moved application and detection logic into `src/core/logic`,
Step 4 added the event log (`src/core/logic/MonitorEventLog`), the
application core (`src/core/app/LayerTimeCore`), and the ports it talks to
the platform through (`src/core/ports`), Step 5 added the mesh commands,
the quick-message library, and the conversation table, and Step 6 added the
application settings and their commands. The T-Watch Ultra's characterization
suites (now in `devices/lilygo-tultra/test/`) still prove the moved code
behaves as it did. These suites test each core
module on its own, with no stubs, which is also the proof that core needs
nothing from the platform. `test_layertime_core` uses fake ports. Run from
`test/`.

Since layout step 5 the detectors themselves (the BLE and Wi-Fi classifiers
and their signature tables) are LayerTime-Sensors, the separate library
checked out as the submodule at `sensors/`; their own suites live in
`sensors/test/` and run with `sensors/tools/run_tests.sh`. Every command
below adds `-I../sensors/src`, because core's model uses the library's
Confidence, SourceKind, and Band. `test_recon_classification` covers core's
side of the seam (the detector mapping and the selection-to-detector-set
translation), and `test_sensor_equivalence` proves the extraction changed no
detection: it replays a fixed corpus and compares every candidate against
the output recorded from 894b508, before the move.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_decl_advice test_declination_advice/test_declination_advice.cpp ../src/core/logic/DeclinationAdvice.cpp
./tests_decl_advice

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_recon_selection test_recon_selection/test_recon_selection.cpp ../src/core/logic/ReconSelection.cpp
./tests_recon_selection

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_recon_classification test_recon_classification/test_recon_classification.cpp ../src/core/logic/ReconClassification.cpp ../src/core/logic/ReconSelection.cpp ../sensors/src/lts/*.cpp
./tests_recon_classification

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_sensor_equivalence test_sensor_equivalence/test_sensor_equivalence.cpp ../src/core/logic/ReconClassification.cpp ../src/core/logic/ReconSelection.cpp ../sensors/src/lts/*.cpp
./tests_sensor_equivalence

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_alert_policy test_alert_policy/test_alert_policy.cpp ../src/core/logic/AlertPolicy.cpp
./tests_alert_policy

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_recon_scheduler test_recon_scheduler/test_recon_scheduler.cpp ../src/core/logic/ReconScheduler.cpp ../src/core/logic/ReconSelection.cpp
./tests_recon_scheduler

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_recon_stage_log test_recon_stage_log/test_recon_stage_log.cpp ../src/core/logic/ReconStageLog.cpp
./tests_recon_stage_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_mesh_conversations test_mesh_conversations/test_mesh_conversations.cpp ../src/core/logic/MeshConversations.cpp
./tests_mesh_conversations

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_detection_csv test_detection_csv/test_detection_csv.cpp ../src/core/logic/DetectionCsv.cpp
./tests_detection_csv

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_monitor_event_log test_monitor_event_log/test_monitor_event_log.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/ReconSelection.cpp
./tests_monitor_event_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_layertime_core test_layertime_core/test_layertime_core.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_layertime_core

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_mesh_commands test_mesh_commands/test_mesh_commands.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_mesh_commands

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_settings_commands test_settings_commands/test_settings_commands.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_settings_commands
```

## T-Dongle-C5 (LayerWand)

The LayerWand's host tests moved with it to `devices/lilygo-layerwand/test/`
in layout step 2 (2026-10-02). The build command is in
`devices/lilygo-layerwand/README.md`, "Host tests".

## LayerTime Link

The LayerTime Link 0.1 wire format of `contracts/link.md`, added in Slice 1
Increment 1. `test_link_codec` runs the C++ codec and the Node dispatcher in
`src/core/link` against every byte-exact vector in
`contracts/vectors/link_frames.json`. `test_link_vectors_mc` reads the
generated Monkey C copy of those vectors, `devices/garmin-tactix/source/link/LinkVectors.mc`,
back with its own parser and checks it against the same JSON, so the Connect
IQ conformance tests (`devices/garmin-tactix/test/LinkCodecTests.mc`, run in the Connect IQ
simulator) are always working from the same bytes; a stale copy fails here
first. `tools/gen_link_vectors_mc.py` regenerates that file (`--check` reports
whether it is current). Both suites share `link_vectors_json.h`. Run from
`test/`, because the vectors are read from `../contracts/vectors/` and
`../devices/garmin-tactix/`.

`test_link_server` covers the Recon Node's side, added with the Recon
integration (Increment 2B): the change sequence (`ChangeTracker`), the
COMMAND, GET_CHANGED and GET_TEXT answers and the live Status fields
(`LinkServer`) against `LayerTimeCore` with a fake monitor source, including
the gap across the 40-event wrap, a clear, text fragments, and the event-log
lock never being held around a command that reaches the radios; and rule 2's
one outstanding request on the Node (`LinkPipe`).

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_link_codec test_link_codec/test_link_codec.cpp ../src/core/link/LinkCodec.cpp
./tests_link_codec

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_link_vectors_mc test_link_vectors_mc/test_link_vectors_mc.cpp
./tests_link_vectors_mc

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_link_server test_link_server/test_link_server.cpp ../src/core/link/LinkServer.cpp ../src/core/link/ChangeTracker.cpp ../src/core/link/LinkCodec.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_link_server
```

## Core and platform boundary

Checks the source tree itself. Every `#include` under `src/` and under the
T-Watch Ultra's `devices/lilygo-tultra/src/` is resolved the way the compiler
resolves it (the including file's directory, then the `src/` include root) and
judged by the file it reaches, not by how its path is spelled. `src/core` may
reach only `src/core` and the C and C++ standard library, and `src/` holds
nothing but core. Only the Ultra's own files (its `main.cpp` included) reach
the Ultra's platform code. Added in Phase 0 Step 7; since layout step 4 the
Ultra's platform code is its own folder instead of `src/platform/twatch_ultra/`.
The LayerWand and the S3 Plus check their own boundaries in their own test
folders. Run from `test/`, because it reads `../src` and
`../devices/lilygo-tultra/src`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -I../sensors/src -o tests_boundary test_boundary/test_boundary.cpp
./tests_boundary
```
