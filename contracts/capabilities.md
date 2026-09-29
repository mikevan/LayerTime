# LayerTime Device Capabilities

Contract version 0.1. Enum values are in `vectors/enums.json`. The C++
binding is `src/core/model/DeviceCapabilities.h`.

Application behaviour branches on capabilities, never on which device it is
running on.

`DeviceCapabilities` holds **effective** capabilities only. A flag is true
only when LayerTime can actually use it on that platform today, proven on that
platform. Hardware that exists but LayerTime cannot yet reach is false. A
feature we intend to build is false until it works. Every field defaults to
false, so a platform that forgets to declare one loses the feature instead of
faking it.

Design intent and verification status are recorded in the profile vectors
and nowhere else. The application never reads them.

## Fields

| Field | Type | Meaning |
|---|---|---|
| profileId | text, 24 | For logs and diagnostics only. Never branched on. |
| displayClass | DisplayClass | MonochromeLowRes or ColorHighRes |
| displayShape | DisplayShape | Rectangle or Round |
| displayWidth, displayHeight | uint16 | Pixels |
| touch | bool | Touch input |
| buttons | bool | Physical buttons LayerTime reads |
| vibration | bool | Haptic alerts |
| gps | bool | Own position |
| compass | bool | Magnetic heading |
| altitude | bool | Any altitude source, GNSS or barometric |
| nativeMaps | bool | Platform-provided cartography |
| removableStorage | bool | SD card or similar |
| localWifiMonitor | bool | Passive Wi-Fi observation on this device |
| localBleMonitor | bool | Passive BLE observation on this device |
| externalRecon | bool | Events from a LayerTime Node over LayerTime Link |
| meshUi | bool | This device presents the mesh UI |
| localMeshtasticRadio | bool | Meshtastic runs on a radio in this device |
| localMeshCoreRadio | bool | MeshCore runs on a radio in this device |
| meshRadioShared | bool | Both local mesh stacks share one radio, so only one runs at a time |
| androidBridge | bool | Reaches mesh and other services through the Android companion |

## Profiles

A profile is one JSON file in `vectors/`. Each capability field is an object:

| Key | Required | Meaning |
|---|---|---|
| effective | yes | The value `DeviceCapabilities` reports at runtime |
| status | yes | `verified`, `documented`, `unverified`, or `design` |
| evidence | yes | Where the status comes from: a measurement, a code location, or a source URL |
| target | no | Design intent. Recorded for planning. Never read by the application. |

Status values:

| Status | Meaning |
|---|---|
| verified | Proven on this platform for LayerTime, by measurement or working code |
| documented | A vendor source says the hardware has it. LayerTime access is not proven. |
| unverified | Intended, and depends on something not yet proven on the device |
| design | A LayerTime architecture decision, not a hardware fact |

**The rule:** `effective` may differ from the `DeviceCapabilities` default
only when `status` is `verified`. Every other status carries the default:
false, 0, `MonochromeLowRes`, or `Rectangle`. When a field is proven on the
device, its status becomes `verified` and its effective value changes in the
same edit. The conformance test enforces this rule for every profile.

| Profile | File | State |
|---|---|---|
| twatch-ultra | `vectors/profile_twatch_ultra.json` | Every field verified, by measurement or from the code. Bound in C++ by `src/platform/twatch_ultra/TUltraProfile.h`. |
| tactix-amoled | `vectors/profile_tactix_amoled.json` | No LayerTime code runs on it yet, so nothing is verified and every effective value is the default. Intent is in `target`. |
| tdongle-c5 | `vectors/profile_tdongle_c5.json` | Drafted in Slice 1 Increment 0. Nothing is verified yet, so every effective value is the default. Intent is in `target`; `localWifiMonitor` and `localBleMonitor` become verified in Increment 2A. |
