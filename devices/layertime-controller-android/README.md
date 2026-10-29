# LayerTime Controller (Android)

An Android client for the LayerTime ecosystem. It is a LayerTime Link 0.1
**interface** (a "controller"), the same role the Garmin tactix plays: it
connects to a LayerWand (the ESP32-C5 Node) over BLE, pulls real detection
events, and records them. It is passive. It never transmits attacks and holds
no offensive RF capability, consistent with LT-ARCH-001 sections 2.6, 14, and
16.7.

Current build: **versionName 0.1.9, versionCode 10** (installs as
`0.1.9-diag`, application id `com.projectrevivesolutions.layertime.controller.diag`).

Primary target device: the **TCL K24**. LOKMAT APPLLP 5 MAX compatibility is a
later objective and is not assumed. One wand at a time: simultaneous multi-wand
operation is not implemented, so every count and view in this build is written
for a single wand.

## Milestone history: platform validation

The first milestone was platform validation on one phone, and it still defines
the diagnostic core. That scope was: report device model, Android version and
API level, and BLE capability; discover and connect to a LayerWand by service
UUID; display real incoming detection events; record connection changes and
timestamped events; support a screen-off reception test; and export the
results. That core is implemented and, except where noted under Validation
below, verified on a TCL T434D running Android 14 (API 34).

## Current UI

The app is orientation-aware. In **landscape** it shows the round LayerTime
face on the left and a steel-framed data box on the right. In **portrait** it
stacks the face on top and the data box below. Scrolling happens inside the
data box. Rotation follows the device's rotation setting.

The round face uses the Garmin design language: a segmented battery arc, the
owl with a BLE glyph and a wand-link glyph by its ears, a stacked clock, two
ticked rings, two hex buttons, and the hex Recon bracket with the live mode and
detection count.

Navigation (implemented):

- **RECON** (right face button) opens the **Mission Configuration** page. It
  lists the detectors as connectors with a checkbox each. A checkbox sets that
  connector in the mission profile as local app state. It does not yet command
  the wand per connector, because Link 0.1 starts one target at a time; the
  selection is UI state for the coming multi-wand work.
- **COUNTER** (left face button) opens the **Detected Attacks** list. It lists
  the current detections. Tapping one opens the **Attack Detail** screen.
- The **Attack Detail** screen has the six sections the analysis spec calls for:
  detection summary, addresses and targets, frame details, radio and capture
  metadata, timeline and multi-wand, and decoded evidence. It works in both
  orientations and scrolls.

Honest field display: the Attack Detail screen **shows every field, including
the ones it does not have, and marks each unavailable field UNAVAILABLE with a
specific reason.** Fields that are present carry a provenance tag so decoded
facts, receiver metadata, calculated statistics, and inferred conclusions are
never confused. Nothing on the live screen is a demonstration value.

Implemented but not yet verified on the device for the current build: the
COUNTER and RECON navigation, the Mission Configuration checkboxes, the Detected
Attacks list, and the Attack Detail screen were added in build 10 and compile,
but their on-device behavior has not been confirmed on hardware yet. The
connection, detection, logging, export, and screen-off paths were verified on
earlier builds (see Validation).

## Not implemented: proposed, awaiting review

The following are **proposed, not implemented**. See
`FRAME_EVIDENCE_PROPOSAL.md` in this folder.

- Raw-frame retention on the LayerWand.
- Evidence transfer over the link (a backward-compatible Link 0.2 op).
- Wireshark decoding of captured frames on Android.
- PCAP or PCAPNG export.
- Multiple-wand correlation.

The blocker is verified in `devices/lilygo-layerwand/src/C5ReconRadio.cpp`
(`promiscuousThunk`, lines 231 to 253): the wand receives the raw 802.11 frame
and the receiver metadata, classifies them, and discards the bytes. It retains
no frames, and Link 0.1 carries only the normalized `EVENT_SUMMARY`. Android
therefore receives no raw frame bytes, so the Attack Detail screen marks every
frame, address, Radiotap, and raw-hex field UNAVAILABLE with that reason.

## Fields the UI shows from Link 0.1

From `EVENT_SUMMARY` and GET_TEXT: detector (including DEAUTH), confidence,
source kind, band, channel, RSSI, sighting count, age in seconds, and the
`sourceId`/`detail` text. From Status: monitoring flags, event count, active
target, change sequence, and heartbeat.

Timestamps: the exported `firstSeenAt`, `lastUpdatedAt`, and every log entry are
**controller receipt and log times, taken from the phone's wall clock when the
report arrived.** They are not the wand's capture times. Link 0.1 sends only
ages in seconds, not capture timestamps, so no capture timestamp is available
until the evidence transfer in the proposal is implemented.

## Building the APK

The build produces a **debug-signed diagnostic APK** (signed with the Android
debug keystore, installable over USB). It is not a release-signed or UAT
candidate; promotion to a signed UAT candidate is a separate workflow.

Requires JDK 17+ and the Android SDK (platform `android-34`, build-tools
`34.0.0`). Verified present on the development desktop `desktop-hi955uo`:
Android SDK at `%LOCALAPPDATA%\Android\Sdk`, Android Studio (its bundled JBR is
a JDK 17), and Gradle caches.

### VS Code terminal on Windows (gradlew.bat)

How to run: open this folder in VS Code and paste this whole block into the
PowerShell terminal with one Ctrl+V, then press Enter.

```powershell
& {
  $env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
  $env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
  $env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
  Set-Location "C:\workspace\TUltra-Project\LayerTime\devices\layertime-controller-android"
  .\gradlew.bat --no-daemon :app:testDebugUnitTest :app:assembleDebug
}
```

The codec conformance test runs first (`:app:testDebugUnitTest`), then the APK
is written to `app\build\outputs\apk\debug\app-debug.apk`.

### Install and verify the exact version on the TCL K24

How to run: with the TCL K24 plugged in and USB debugging allowed, paste this
whole block with one Ctrl+V, then press Enter. It installs, checks the install
exit code, and prints the installed version.

```powershell
& {
  $adb = "$env:LOCALAPPDATA\Android\Sdk\platform-tools\adb.exe"
  $pkg = "com.projectrevivesolutions.layertime.controller.diag"
  $apk = "C:\workspace\TUltra-Project\LayerTime\devices\layertime-controller-android\app\build\outputs\apk\debug\app-debug.apk"
  & $adb install -r "$apk"
  if ($LASTEXITCODE -ne 0) { "ADB INSTALL FAILED (exit $LASTEXITCODE)"; return }
  $ver = (& $adb shell dumpsys package $pkg | Select-String "versionName" | Select-Object -First 1).ToString().Trim()
  $code = (& $adb shell dumpsys package $pkg | Select-String "versionCode" | Select-Object -First 1).ToString().Trim()
  "installed: $ver | $code"
  if ($ver -notmatch "0\.1\.9") { "VERSION MISMATCH - expected 0.1.9" } else { "VERSION OK" }
}
```

Expect `installed: versionName=0.1.9-diag | versionCode=10` and `VERSION OK`.

## Continuous integration status

The GitHub Actions workflow `android-controller.yml` is **delivered in chat but
is not installed in the repository and has not run.** GitHub runs workflows only
from the repository root `.github/workflows/`, so it must be placed at
`C:\workspace\TUltra-Project\LayerTime\.github\workflows\android-controller.yml`.
It was not written through the device bridge because the bridge protects
`.github/` paths. Until it is committed and a run completes, CI has built and
tested nothing. When installed, it runs the codec conformance test and
assembles the debug APK.

## Validation

### Completed on hardware (TCL T434D, Android 14, API 34)

- Device and BLE capability report, with real values.
- Discover by service UUID and connect to `LT-C5-ECE2`.
- HELLO handshake and capability exchange.
- 18-byte Status snapshot decode.
- COMMAND ReconStart returning RESULT Ok.
- Real `EVENT_SUMMARY` decode (four live AirTag detections) and GET_TEXT detail
  reassembly ("Find My tracker beacon").
- Timestamped connection and event logging.
- Diagnostic export (JSON and text).
- Screen-off window: the foreground service held the link for 21 seconds with 0
  disconnects (link survival through screen-off).
- Link codec conformance, 33 of 33 checks, on the phone's build and on the host.

### Remaining

- Reception of a new event while backgrounded: the 21-second screen-off test
  recorded 0 disconnects, but no new event arrived in that window, so
  delivery-while-backgrounded is not yet exercised. Run a longer window while
  trackers or a deauth source are active.
- On-device verification of the build-10 UI: the COUNTER and RECON navigation,
  the Mission Configuration page, the Detected Attacks list, and the Attack
  Detail screen.
- Simultaneous multi-wand operation (not implemented).
- The raw-frame evidence pipeline (proposed; see `FRAME_EVIDENCE_PROPOSAL.md`).

## Protocol binding

The wire protocol is not reinvented. All UUIDs, frame layouts, and enum numbers
are copied from the canonical contract: `contracts/link.md`,
`contracts/vectors/enums.json`, and `contracts/vectors/link_frames.json`. The
codec (`link/LinkCodec.kt`) is verified against `link_frames.json` by
`app/src/test/.../LinkCodecConformanceTest.kt`, the same vectors the C++ and
Monkey C bindings are checked against. Behavior implemented: pull-not-push, one
outstanding Control request, frames of 20 bytes or less, notification
subscriptions on Status and Data, a 5-second per-request timeout, a 20-second
scan timeout, and capped-backoff reconnection.
