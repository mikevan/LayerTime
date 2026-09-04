# Select the mesh protocol implicitly from the watch face, not from Settings

Today, using a mesh radio takes two steps in two places: turn `MESHCORE` or `MESHTASTIC` on in Settings, then tap the matching button on the watch face. The Settings toggles exist only because the two protocols share one physical SX1262 and cannot both be live.

That is an implementation detail leaking into the interface. Tapping **MTASTIC** or **MESHCORE** on the face should be the selection.

## The work

- [ ] Tapping a mesh button on the watch face enables that protocol and disables the other, then opens its screen
- [ ] Remove the `MESHCORE` and `MESHTASTIC` on/off rows from Settings
- [ ] Keep `MESHCORE ADVERTISE`, `MESHTASTIC ADVERTISE` and `MESHTASTIC NAME` - those are real preferences, not radio arbitration
- [ ] Decide what leaving the screen does: stay on the air, or power down. Staying on is what makes messages arrive while you are elsewhere in the UI; powering down is the battery-safe default. Probably wants to stay on, since the radio being off at boot is already the power guard.

## Where the logic lives now

- `SettingsScreen::meshEnabledThunk` / `meshtasticEnabledThunk` flip one flag off when the other goes on
- `WatchApp::applySettings` (~line 316) sequences the disables before the enables, defensively, so both drivers never touch the radio at once
- `WatchFace::meshEventThunk` / `meshtasticEventThunk` currently just open the screens

The mutual exclusion itself is right and should stay - it just needs to be driven from the face rather than from a settings row.

## Caution

`meshEnabled` and `meshtasticEnabled` are deliberately NEVER persisted - every boot starts with both radios off, and that is a power decision, not an oversight. Implicit selection must not accidentally make a protocol sticky across reboots.
