# Running the tests

No IDE, no framework, no platform. One compiler invocation.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests test_geogrid/test_geogrid.cpp ../src/core/logic/GeoGrid.cpp
./tests
```

One case in isolation, which is how the density tooling works:

```
./tests zone_arctic_west_of_prime_meridian_is_not_svalbard
```

## Coverage and density

```
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c ../src/core/logic/GeoGrid.cpp -o GeoGrid.o
g++ -std=c++17 -O0 -g --coverage -I. -I../src -c test_geogrid/test_geogrid.cpp -o tests.o
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
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_core test_core_model/test_core_model.cpp ../src/core/logic/QuickMessages.cpp
./tests_core
```

## Characterization suites

These record what existing code does today, before any of it moves. They
compile the production files unchanged. Where a file needs Arduino, LVGL,
LilyGoLib, NimBLE, or ESP-IDF Wi-Fi, the headers in `stubs/` stand in for
them. The stubs exist only here; the firmware build never sees them.

A case whose name ends in `KNOWN_DEFECT` pins behaviour that looks wrong. It
is recorded, not fixed, so a later fix shows up as a deliberate change.

The production files emit a few compiler warnings of their own, so these
commands do not use `-Werror`. Run each from `test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_ubx test_ubx/test_ubx.cpp ../src/platform/twatch_ultra/UbxParser.cpp
./tests_ubx

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_recon test_recon/test_recon.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_recon

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_chats test_meshtastic_chats/test_meshtastic_chats.cpp ../src/platform/twatch_ultra/ui/MeshtasticScreen.cpp ../src/core/logic/MeshConversations.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/QuickMessages.cpp
./tests_chats

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_decl test_declination_text/test_declination_text.cpp ../src/platform/twatch_ultra/ui/MappingScreen.cpp ../src/core/logic/DeclinationAdvice.cpp ../src/core/logic/DeclinationCalculator.cpp ../src/core/logic/GeoGrid.cpp
./tests_decl

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_log test_detection_log/test_detection_log.cpp ../src/platform/twatch_ultra/app/WatchApp.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/platform/twatch_ultra/TUltraEventLog.cpp ../src/core/logic/DetectionCsv.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp ../src/platform/twatch_ultra/TUltraSettingsStore.cpp
./tests_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_recon_screen test_recon_screen/test_recon_screen.cpp ../src/platform/twatch_ultra/ui/ReconScreen.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_recon_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_watch_face_threats test_watch_face_threats/test_watch_face_threats.cpp ../src/platform/twatch_ultra/ui/WatchFace.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_watch_face_threats

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_mesh_screen test_mesh_screen/test_mesh_screen.cpp ../src/platform/twatch_ultra/ui/MeshScreen.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_mesh_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_meshtastic_actions test_meshtastic_actions/test_meshtastic_actions.cpp ../src/platform/twatch_ultra/ui/MeshtasticScreen.cpp ../src/core/logic/MeshConversations.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/QuickMessages.cpp
./tests_meshtastic_actions

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_mesh_radio_exclusivity test_mesh_radio_exclusivity/test_mesh_radio_exclusivity.cpp ../src/platform/twatch_ultra/ui/SettingsScreen.cpp ../src/platform/twatch_ultra/app/WatchApp.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/platform/twatch_ultra/TUltraEventLog.cpp ../src/core/logic/DetectionCsv.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp ../src/platform/twatch_ultra/TUltraSettingsStore.cpp
./tests_mesh_radio_exclusivity

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_settings_persistence test_settings_persistence/test_settings_persistence.cpp ../src/platform/twatch_ultra/services/SettingsService.cpp ../src/platform/twatch_ultra/TUltraSettingsStore.cpp
./tests_settings_persistence

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_settings_flow test_settings_flow/test_settings_flow.cpp ../src/platform/twatch_ultra/services/SettingsService.cpp ../src/platform/twatch_ultra/ui/SettingsScreen.cpp ../src/platform/twatch_ultra/app/WatchApp.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/platform/twatch_ultra/TUltraEventLog.cpp ../src/core/logic/DetectionCsv.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp ../src/platform/twatch_ultra/TUltraSettingsStore.cpp
./tests_settings_flow

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_watch_face_units test_watch_face_units/test_watch_face_units.cpp ../src/platform/twatch_ultra/ui/WatchFace.cpp ../src/platform/twatch_ultra/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_watch_face_units
```

`test_recon.cpp` includes `ReconService.cpp` directly, so it can reach the
service's file-local pieces. Do not also pass `ReconService.cpp` on
that command line.

Since Phase 0 Step 4 the Recon event history, the alert, and the Recon
commands are in core (`src/core/app`, `src/core/logic/MonitorEventLog`),
reached through the T-Ultra adapters in `src/platform/twatch_ultra`. The
Recon, screen and detection-log suites wire those together the way
`WatchApp` does, in their harnesses only; the cases themselves are unchanged.
`test_recon_screen` and `test_watch_face_threats` were added in Step 4a,
before the move, against the code as it stood.

Step 5 did the same for mesh. Both mesh screens now send and configure
channels through core commands, take their phrase list from the core
(`src/core/logic/QuickMessages`), and the Meshtastic screen keeps its
conversations and read times in the core; MeshCore and Meshtastic plug in
through `src/core/ports/MeshTransport`. `test_mesh_screen`,
`test_meshtastic_actions` and `test_mesh_radio_exclusivity` were added in
Step 5a, before the move, against the code as it stood; since then only
their harnesses changed, as did `test_meshtastic_chats`'s. The exclusivity
suite drives the real Settings rows into the real `WatchApp`.

Step 6 split the settings. The application settings (clock format, units,
sleep mode, early warning, mesh advertising, the Meshtastic name) are the
core's (`src/core/model/Settings.h`), changed by commands and kept by
`src/platform/twatch_ultra/TUltraSettingsStore` in the same NVS keys as
before; `SettingsService` keeps the T-Ultra's own. `test_settings_persistence`
and `test_settings_flow` were added in Step 6a, before the split, against the
code as it stood, and only their harnesses changed after it.
`test_watch_face_units` was run against the face before and after the split
with identical results. `stubs/Preferences.h` is an in-memory NVS for these.

## Core logic unit suites

Phase 0 Step 3 moved application and detection logic into `src/core/logic`,
Step 4 added the event log (`src/core/logic/MonitorEventLog`), the
application core (`src/core/app/LayerTimeCore`), and the ports it talks to
the platform through (`src/core/ports`), Step 5 added the mesh commands,
the quick-message library, and the conversation table, and Step 6 added the
application settings and their commands. The characterization suites above
still prove the moved code behaves as it did. These suites test each core
module on its own, with no stubs, which is also the proof that core needs
nothing from the platform. `test_layertime_core` uses fake ports. Run from
`test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_decl_advice test_declination_advice/test_declination_advice.cpp ../src/core/logic/DeclinationAdvice.cpp
./tests_decl_advice

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_recon_selection test_recon_selection/test_recon_selection.cpp ../src/core/logic/ReconSelection.cpp
./tests_recon_selection

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_recon_signatures test_recon_signatures/test_recon_signatures.cpp ../src/core/logic/ReconSignatures.cpp
./tests_recon_signatures

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_wifi_classifier test_wifi_classifier/test_wifi_classifier.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/ReconSignatures.cpp
./tests_wifi_classifier

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_ble_classifier test_ble_classifier/test_ble_classifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/ReconSelection.cpp
./tests_ble_classifier

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_alert_policy test_alert_policy/test_alert_policy.cpp ../src/core/logic/AlertPolicy.cpp
./tests_alert_policy

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_mesh_conversations test_mesh_conversations/test_mesh_conversations.cpp ../src/core/logic/MeshConversations.cpp
./tests_mesh_conversations

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_detection_csv test_detection_csv/test_detection_csv.cpp ../src/core/logic/DetectionCsv.cpp
./tests_detection_csv

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_monitor_event_log test_monitor_event_log/test_monitor_event_log.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/ReconSelection.cpp
./tests_monitor_event_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_layertime_core test_layertime_core/test_layertime_core.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_layertime_core

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_mesh_commands test_mesh_commands/test_mesh_commands.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_mesh_commands

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_settings_commands test_settings_commands/test_settings_commands.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_settings_commands
```

## Platform adapter suites

The T-Ultra adapters in `src/platform/twatch_ultra` that are not already
covered by the characterization suites. Run from `test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_navigation_source test_navigation_source/test_navigation_source.cpp
./tests_navigation_source

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_mesh_transports test_mesh_transports/test_mesh_transports.cpp
./tests_mesh_transports
```

## T-Dongle-C5

The hardware-free logic behind the C5 bring-up firmware in
`src/platform/tdongle_c5/` (Slice 1 Increment 0): the APA102 LED frame, the
button debouncer, the advertised test name, and the MiB report. Run from
`test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_tdongle_c5_bringup test_tdongle_c5_bringup/test_tdongle_c5_bringup.cpp ../src/platform/tdongle_c5/BringUpLogic.cpp
./tests_tdongle_c5_bringup
```

## LayerTime Link

The LayerTime Link 0.1 wire format of `contracts/link.md`, added in Slice 1
Increment 1. `test_link_codec` runs the C++ codec and the Node dispatcher in
`src/core/link` against every byte-exact vector in
`contracts/vectors/link_frames.json`. `test_link_vectors_mc` reads the
generated Monkey C copy of those vectors, `garmin/source/link/LinkVectors.mc`,
back with its own parser and checks it against the same JSON, so the Connect
IQ conformance tests (`garmin/test/LinkCodecTests.mc`, run in the Connect IQ
simulator) are always working from the same bytes; a stale copy fails here
first. `tools/gen_link_vectors_mc.py` regenerates that file (`--check` reports
whether it is current). Both suites share `link_vectors_json.h`. Run from
`test/`, because the vectors are read from `../contracts/vectors/` and
`../garmin/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_link_codec test_link_codec/test_link_codec.cpp ../src/core/link/LinkCodec.cpp
./tests_link_codec

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_link_vectors_mc test_link_vectors_mc/test_link_vectors_mc.cpp
./tests_link_vectors_mc
```

## Core and platform boundary

Checks the source tree itself. Every `#include` under `src/` is resolved the
way the compiler resolves it (the including file's directory, then the
`src/` include root) and judged by the file it reaches, not by how its path
is spelled. `src/core` may reach only `src/core` and the C and C++ standard
library. Everything specific to one device lives under
`src/platform/<target>/`; only platform code and `src/main.cpp` reach it, and
one target never reaches into another (`twatch_ultra` and `tdongle_c5` today). Added in Phase 0 Step 7, when the
T-Ultra's app, services, screens, and settings structs moved under
`src/platform/twatch_ultra/`. Run from `test/`, because it reads `../src`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../src -o tests_boundary test_boundary/test_boundary.cpp
./tests_boundary
```
