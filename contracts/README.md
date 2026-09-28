# LayerTime Contracts

Contract version: **0.1** (Phase 0 draft)

This folder is the single canonical definition of the LayerTime application
model. LayerTime is the application. A watch is a configured target that
renders this model and supplies platform services.

Each platform binds to these contracts in its own language. The C++ binding
for the T-Watch Ultra lives in `src/core/model/`. A Garmin (Monkey C) binding
comes later. Neither binding is the definition. This folder is.

## Files

| File | What it defines |
|---|---|
| `models.md` | NavigationState, MonitorEvent, ReconState, Mesh (node, message, network status), Alert, QuickMessage, Timestamp |
| `commands.md` | LayerTimeCommand and CommandResult |
| `capabilities.md` | DeviceCapabilities and how profiles are declared |
| `vectors/enums.json` | Numeric value of every enum, plus canonical display-name tables |
| `vectors/capacities.json` | Fixed sizes and identity lengths |
| `vectors/profile_twatch_ultra.json` | T-Watch Ultra capability profile: effective value, status, and evidence per field |
| `vectors/profile_tactix_amoled.json` | tactix 8 AMOLED capability profile: effective value, target, status, and evidence per field |
| `vectors/quick_messages_default.json` | The default quick-message library |

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
`test/test_core_model/`. Vectors are data. When a vector and a binding
disagree, the binding is wrong unless the contract is deliberately changed.

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
- **Settings.** `AppSettings` still mixes application settings with
  T-Ultra-only ones. It gets a contract when it is split.
- **The LayerTime Link wire format.** Decided in its own task.
