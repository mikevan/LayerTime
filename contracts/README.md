# LayerTime Contracts

Contract version: **0.1** (Phase 0 draft)

This folder is the single canonical definition of the LayerTime application
model. LayerTime is the application. A watch is a configured target that
renders this model and supplies platform services.

Each platform binds to these contracts in its own language. The C++ binding
lives in `src/core/`. A Garmin (Monkey C) binding comes later. Neither binding
is the definition. This folder is.

## Files

| File | What it defines |
|---|---|
| `models.md` | NavigationState, MonitorEvent, ReconState, Mesh (node, message, network status), ApplicationSettings, Alert, QuickMessage, Timestamp |
| `commands.md` | LayerTimeCommand and CommandResult |
| `capabilities.md` | DeviceCapabilities and how profiles are declared |
| `link.md` | LayerTime Link 0.1: the BLE GATT wire format between a Node and an interface (Slice 1 draft) |
| `vectors/enums.json` | Numeric value of every enum, plus canonical display-name tables |
| `vectors/capacities.json` | Fixed sizes and identity lengths |
| `vectors/profile_twatch_ultra.json` | T-Watch Ultra capability profile: effective value, status, and evidence per field |
| `vectors/profile_tactix_amoled.json` | tactix 8 AMOLED capability profile: effective value, target, status, and evidence per field |
| `vectors/profile_tdongle_c5.json` | T-Dongle-C5 (LayerTime Node) capability profile: effective value, target, status, and evidence per field |
| `vectors/quick_messages_default.json` | The default quick-message library |
| `vectors/link_frames.json` | Byte-exact LayerTime Link frames, requests, replies, and dispatch cases |

## Rules every binding follows

1. **Enum numbers are fixed.** The values in `vectors/enums.json` are part of
   the contract. New values are appended. Existing values are never
   renumbered or reused.
2. **Units are SI.** Metres, metres per second, degrees. Feet and miles per
   hour are display choices made by a UI from settings.
3. **Nothing is fabricated.** Every optional value has its own validity flag.
   A value whose flag is false is 0 and is never displayed. A platform that
   cannot measure something leaves the flag false.
4. **Capabilities, not device names.** Behaviour branches on
   `DeviceCapabilities` fields. `profileId` exists for logs only. Those
   fields are effective capabilities: true only when proven usable on that
   platform. Intent lives in the profile's `target`, never in the flags.
5. **Fixed capacities.** Text fields and lists have fixed maximum sizes,
   stated in `models.md`. Bindings truncate at those sizes. They never grow.
6. **Failures are explicit.** Every command returns a `CommandResult`.
   Nothing is silently ignored.

## Conformance

A binding conforms when its enum values, fixed sizes, defaults, and profile
match the vectors in this folder. For the C++ binding, that is checked by
`test/test_core_model/`, and the Link codec by `test/test_link_codec/`; the
Monkey C Link codec is checked against the same frames by
`garmin/test/LinkCodecTests.mc`. Vectors are data. When a vector and a binding
disagree, the binding is wrong unless the contract is deliberately changed.

## Targets

A target is a watch configured to run LayerTime: a binding of this contract,
a capability profile, and the platform code that connects the two. No
target is the definition of LayerTime.

| Target | Profile | Binding | Platform code | State |
|---|---|---|---|---|
| T-Watch Ultra (`twatch-ultra`) | `vectors/profile_twatch_ultra.json` | C++, `src/core/` | `src/platform/twatch_ultra/` | Running. Screens, hardware services, and the adapters behind the core's ports. |
| tactix 8 AMOLED (`tactix-amoled`) | `vectors/profile_tactix_amoled.json` | Monkey C, `garmin/` | Connect IQ Device App, `garmin/` | Skeleton (Slice 1 Increment 0). The binding and the LayerTime Link client are Slice 1 work. |
| T-Dongle-C5 (`tdongle-c5`) | `vectors/profile_tdongle_c5.json` | C++, `src/core/` | `src/platform/tdongle_c5/` | Bring-up firmware (Slice 1 Increment 0). The LayerTime Node: it runs the Recon engine and serves the tactix over LayerTime Link. |
| T-Watch S3 Plus (`twatch-s3plus`) | `vectors/profile_twatch_s3plus.json` | C++, `src/core/` | `s3plus/` (its own pioarduino project, opened like `garmin/`) | Phase 0 build proof. The all-in-one watch with the T-Ultra's functionality: hardware adapters behind the core's ports, services, and screens. Recon first; mesh deferred. |

In the C++ binding, `src/core/` holds the model, the application logic, and
the ports a platform plugs into. It depends on nothing but the C and C++
standard libraries. Everything specific to one device lives under
`src/platform/<target>/`, and one target never reaches into another.
`test/test_boundary/` checks both rules on every include, by the file it
actually resolves to.

## Changing the contract

A change to this folder is an architecture change. It is reviewed before any
binding is changed to match it.

## Not in contract 0.1

These are known and deliberately left out of Phase 0:

- **Severity on MonitorEvent.** LT-ARCH-001 section 6.3 lists it. No existing
  detector produces one, and inventing it would be fabrication. It gets
  added when a detector has a real basis for it.
- **The event families in LT-ARCH-001 section 6.3** (BEACON_FLOOD,
  AUTH_FLOOD, ROGUE_AP, and so on). They do not match the detectors that
  exist today (Pwnagotchi, MultiSSID, Pineapple, Axon, Tile, and others). This
  contract carries the detectors that exist. Reconciling the two lists
  belongs to the C5 detector work.
- **The LayerTime Link wire format beyond Increment 1.** `link.md` binds
  advertising, the service, Status, HELLO and PING; the remaining operations
  are bound as the Slice 1 increments implement them.
