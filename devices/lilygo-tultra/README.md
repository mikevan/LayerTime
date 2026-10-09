# T-Watch Ultra (LilyGo T-Watch S3 Ultra)

The original LayerTime watch, kept as the historical reference implementation.
It is frozen: no new work lands here unless Michael authorizes it. It is its
own pioarduino project: open this folder, not the repository root, to build
it in the pioarduino IDE. It compiles only the repository's `src/core/` and
this folder's `src/`. Moved here from the repository root in layout step 4
(2026-10-07); the firmware version stays 0.2.3.

## Layout

- `platformio.ini`: one environment, `twatch_ultra`.
- `src/`: the Ultra's platform code. `main.cpp`; `app/WatchApp.*` wires the
  core, every service, and every screen together; `services/` holds the
  hardware and protocol logic; `ui/` holds the LVGL screens; `model/` holds
  the watch's own state and settings structs; the `TUltra*` files are the
  adapters behind the core's ports.
- `test/`: the Ultra's host suites and the `stubs/` headers they compile
  against, built with plain g++.
- `tools/pioarduino-platform-55.03.36-1-lt1/`: the build platform
  (pioarduino 55.03.36-1, Arduino core 3.3.6) carried in-tree with a one-line
  bootstrap patch; see its `LAYERTIME-PATCH.md`.
- `tools/gpsdiag/`: the GPS diagnostic firmware, its own project.
- `tools/make_flasher_bin.ps1`: builds the merged image the browser flasher
  in the repository's `docs/` serves.
- `boards/lilygo-t-watch-ultra.json` and `variants/lilygo_twatch_ultra/`: the
  board definition and pin map.
- `lib/libhelix-mp3/`: the MP3 decoder library the build carries.
- `bin/`: build output (git-ignored).

## Build and upload

In the pioarduino IDE, open this folder (`devices\lilygo-tultra`) as its own
project and run the build for `twatch_ultra`, then the upload, the same way
as before the move. The first build after the move fetches the platform
packages and libraries again into `bin/`.

Every library is pinned to the exact version the last known-good build used
(LilyGoLib `38e6f8d`, the same commit the S3 Plus pins). LilyGoLib 0.3.0 needs
ESP-IDF 5.5.3 or later and SensorLib 0.5.0, and this target stays on Arduino
core 3.3.6 (ESP-IDF 5.5.2) because 3.3.7 and later fail its BLE hardware gate.
The comment at the top of `platformio.ini` has the details.

The LVGL configuration is this folder's `lv_conf_tultra.h`, and
`tools/tultra_lvgl_thorvg_patch.py` repairs three LVGL ThorVG sources that
use `memcpy` without including `<cstring>`. Before layout step 4 both came
from hand edits inside the downloaded library copies, so a fresh fetch could
not build. The repair script makes the same edit, byte for byte, and checks
each file by SHA-256 before and after. It does not force-include `<cstring>`
into every file the way the S3 Plus does: that pulls `sdkconfig.h` into
ESP8266Audio's `AudioOutputULP.cpp`, which then compiles code that was empty
in the known-good build and does not build on ESP-IDF 5.5.

## Host tests

Run each command from `devices/lilygo-tultra/test/`. The shared `check.h`
comes from the repository's `test/` folder.

### Characterization suites

These record what existing code does today, before any of it moves. They
compile the production files unchanged. Where a file needs Arduino, LVGL,
LilyGoLib, NimBLE, or ESP-IDF Wi-Fi, the headers in `stubs/` stand in for
them. The stubs exist only here; the firmware build never sees them.

A case whose name ends in `KNOWN_DEFECT` pins behaviour that looks wrong. It
is recorded, not fixed, so a later fix shows up as a deliberate change.

The production files emit a few compiler warnings of their own, so these
commands do not use `-Werror`. Run each from `devices/lilygo-tultra/test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -I../../../src -I../src -o tests_ubx test_ubx/test_ubx.cpp ../src/UbxParser.cpp
./tests_ubx

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_recon test_recon/test_recon.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_recon

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_chats test_meshtastic_chats/test_meshtastic_chats.cpp ../src/ui/MeshtasticScreen.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_chats

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_decl test_declination_text/test_declination_text.cpp ../src/ui/MappingScreen.cpp ../../../src/core/logic/DeclinationAdvice.cpp ../../../src/core/logic/DeclinationCalculator.cpp ../../../src/core/logic/GeoGrid.cpp
./tests_decl

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_log test_detection_log/test_detection_log.cpp ../src/app/WatchApp.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../src/TUltraEventLog.cpp ../../../src/core/logic/DetectionCsv.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp ../src/TUltraSettingsStore.cpp
./tests_log

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_recon_screen test_recon_screen/test_recon_screen.cpp ../src/ui/ReconScreen.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_recon_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_watch_face_threats test_watch_face_threats/test_watch_face_threats.cpp ../src/ui/WatchFace.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_watch_face_threats

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_mesh_screen test_mesh_screen/test_mesh_screen.cpp ../src/ui/MeshScreen.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_mesh_screen

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_meshtastic_actions test_meshtastic_actions/test_meshtastic_actions.cpp ../src/ui/MeshtasticScreen.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_meshtastic_actions

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_mesh_radio_exclusivity test_mesh_radio_exclusivity/test_mesh_radio_exclusivity.cpp ../src/ui/SettingsScreen.cpp ../src/app/WatchApp.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../src/TUltraEventLog.cpp ../../../src/core/logic/DetectionCsv.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp ../src/TUltraSettingsStore.cpp
./tests_mesh_radio_exclusivity

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_settings_persistence test_settings_persistence/test_settings_persistence.cpp ../src/services/SettingsService.cpp ../src/TUltraSettingsStore.cpp
./tests_settings_persistence

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_settings_flow test_settings_flow/test_settings_flow.cpp ../src/services/SettingsService.cpp ../src/ui/SettingsScreen.cpp ../src/app/WatchApp.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../src/TUltraAlertSink.cpp ../src/TUltraEventLog.cpp ../../../src/core/logic/DetectionCsv.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp ../src/TUltraSettingsStore.cpp
./tests_settings_flow

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_watch_face_units test_watch_face_units/test_watch_face_units.cpp ../src/ui/WatchFace.cpp ../src/services/ReconService.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/ReconSignatures.cpp ../../../src/core/logic/WifiFrameClassifier.cpp ../../../src/core/logic/BleAdvertClassifier.cpp ../../../src/core/app/LayerTimeCore.cpp ../../../src/core/logic/MonitorEventLog.cpp ../../../src/core/logic/AlertPolicy.cpp ../../../src/core/logic/MeshConversations.cpp ../../../src/core/logic/QuickMessages.cpp
./tests_watch_face_units
```

`test_recon.cpp` includes `ReconService.cpp` directly, so it can reach the
service's file-local pieces. Do not also pass `ReconService.cpp` on
that command line.

Since Slice 1 Increment 2A the T-Watch Ultra is frozen reference code:
`ReconService` and the suites above stay as they were and are not part of
the C5/Garmin acceptance path. The Recon schedule `ReconService` runs was
expressed in core as `src/core/logic/ReconScheduler` behind the
`src/core/ports/ReconRadio` port, and the C5 (`devices/lilygo-layerwand/src`) is
its consumer. `test_recon_scheduler` below pins that schedule on its own;
`test_recon` still characterizes the T-Ultra's original.

Since Phase 0 Step 4 the Recon event history, the alert, and the Recon
commands are in core (`src/core/app`, `src/core/logic/MonitorEventLog`),
reached through the T-Ultra adapters in `devices/lilygo-tultra/src`. The
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
`devices/lilygo-tultra/src/TUltraSettingsStore` in the same NVS keys as
before; `SettingsService` keeps the T-Ultra's own. `test_settings_persistence`
and `test_settings_flow` were added in Step 6a, before the split, against the
code as it stood, and only their harnesses changed after it.
`test_watch_face_units` was run against the face before and after the split
with identical results. `stubs/Preferences.h` is an in-memory NVS for these.

### Platform adapter suites

The T-Ultra adapters in `devices/lilygo-tultra/src` that are not already
covered by the characterization suites. Run from `devices/lilygo-tultra/test/`.

```
g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -Istubs -I../../../src -I../src -o tests_navigation_source test_navigation_source/test_navigation_source.cpp
./tests_navigation_source

g++ -std=c++17 -O0 -Wall -Wextra -I. -I../../../test -I../../../src -I../src -o tests_mesh_transports test_mesh_transports/test_mesh_transports.cpp
./tests_mesh_transports
```
