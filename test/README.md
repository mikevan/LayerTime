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

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_chats test_meshtastic_chats/test_meshtastic_chats.cpp ../src/ui/MeshtasticScreen.cpp ../src/core/logic/MeshConversations.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/QuickMessages.cpp
./tests_chats

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_decl test_declination_text/test_declination_text.cpp ../src/ui/MappingScreen.cpp ../src/core/logic/DeclinationAdvice.cpp ../src/core/logic/DeclinationCalculator.cpp ../src/core/logic/GeoGrid.cpp
./tests_decl

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_log test_detection_log/test_detection_log.cpp ../src/app/WatchApp.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/platform/twatch_ultra/TUltraEventLog.cpp ../src/core/logic/DetectionCsv.cpp ../src/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_recon_screen test_recon_screen/test_recon_screen.cpp ../src/ui/ReconScreen.cpp ../src/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_recon_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_watch_face_threats test_watch_face_threats/test_watch_face_threats.cpp ../src/ui/WatchFace.cpp ../src/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_watch_face_threats

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_mesh_screen test_mesh_screen/test_mesh_screen.cpp ../src/ui/MeshScreen.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_mesh_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_meshtastic_actions test_meshtastic_actions/test_meshtastic_actions.cpp ../src/ui/MeshtasticScreen.cpp ../src/core/logic/MeshConversations.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/QuickMessages.cpp
./tests_meshtastic_actions

g++ -std=c++17 -O0 -Wall -Wextra -I. -Istubs -I../src -o tests_mesh_radio_exclusivity test_mesh_radio_exclusivity/test_mesh_radio_exclusivity.cpp ../src/ui/SettingsScreen.cpp ../src/app/WatchApp.cpp ../src/core/app/LayerTimeCore.cpp ../src/core/logic/MonitorEventLog.cpp ../src/platform/twatch_ultra/TUltraAlertSink.cpp ../src/platform/twatch_ultra/TUltraEventLog.cpp ../src/core/logic/DetectionCsv.cpp ../src/services/ReconService.cpp ../src/core/logic/ReconSelection.cpp ../src/core/logic/ReconSignatures.cpp ../src/core/logic/WifiFrameClassifier.cpp ../src/core/logic/BleAdvertClassifier.cpp ../src/core/logic/AlertPolicy.cpp ../src/core/logic/MeshConversations.cpp ../src/core/logic/QuickMessages.cpp
./tests_mesh_radio_exclusivity
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

## Core logic unit suites

Phase 0 Step 3 moved application and detection logic into `src/core/logic`,
Step 4 added the event log (`src/core/logic/MonitorEventLog`), the
application core (`src/core/app/LayerTimeCore`), and the ports it talks to
the platform through (`src/core/ports`), and Step 5 added the mesh commands,
the quick-message library, and the conversation table. The characterization suites above
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
