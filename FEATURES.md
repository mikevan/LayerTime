# LayerTime Features

Current-state feature documentation for the LayerTime firmware, targeting the LilyGo T-Watch Ultra. This replaces the earlier per-increment `README-*.md` files, which tracked development stage-by-stage and had gone stale relative to the current build.

## Watch face

- Time, date, and battery percentage.
- Large, colored ALT / TRAVEL / THREATS / GPS data blocks, Montserrat 18.
- **THREATS** (bottom-left, green/red): status text shows what's actively scanning right now - `ALL`, a single detector name (e.g. `DEAUTH`) when manually selected, `EARLY WARNING` when only the background sweep is running, or `OFF` when nothing is scanning at all. Color is separate from that text: red as long as the persistent threat log (see Recon below) holds anything, green when it's empty - so a threat found earlier stays flagged red even after monitoring stops, until the user clears the log. Tap it to jump straight into Recon's All-detectors monitor, skipping the detector-picker menu.
- No seconds or AM/PM shown; the 12/24-hour setting still controls the hour format.
- Long-press anywhere on the face to open **Settings**.
- **Double-tap the watch face** (two taps inside 500 ms) to put the display to sleep immediately instead of waiting out the 15-second timeout. Only the face background counts: taps on the four shortcut buttons or the GPS / THREATS blocks still do their normal job, so the gesture can't fire while working a menu or picking mesh phrases. The owl and the time/date/ALT/TRAVEL labels aren't touch targets, so taps pass through them to the background. A tap anywhere - including on a button - wakes the display again, and that waking tap is never counted as the first half of a new pair. A forced sleep is only ended by an actual touch; a Recon alert arriving while the screen is dark no longer switches it back on.
- Tap the green GPS block to open the **GPS** page.
- Four shortcut buttons in a 2x2 over the lower face: **MTASTIC** (Meshtastic) and **RECON** on the upper row, **MESHCORE** and **MAPPING** on the lower, MESHCORE's bottom edge level with the time.
- Footer reads `LAYERTIME | T-WATCH ULTRA`.
- The panel also blanks on its own after 15 seconds without touch input. Blanking is backlight-off (`setBrightness(0)`) - not panel sleep and not deep sleep - so all services keep running and touch stays live underneath. One consequence: a tap that lands on a button while the screen is dark both wakes it and activates that button. Brightness is restored from Settings on wake.

## GPS

- Uses LilyGoLib's onboard MIA-M10Q GNSS (`instance.gps`), polled continuously.
- Live latitude/longitude (6 decimal places), altitude in the selected unit system, satellite count, HDOP, speed, and course over ground when moving.
- **MGRS grid reference** (Military Grid Reference System) computed on-device from WGS84 - the format used on military maps and in radio traffic, e.g. `18S UJ` / `23383 08450`: grid zone designator, 100 km square letters, then easting and northing within that square at 5+5 digits (1 m precision). No map engine or SD dependency. UTM zone logic (including the Norway/Svalbard special zones) lives in the shared `GeoGrid` module so the Mapping screen's grid-north correction uses the same geodesy.
- Layout: latitude, longitude and the MGRS reference take the full panel width at Montserrat 28 - they are what the page is for. Altitude, satellites, HDOP, speed and direction of travel sit in a two-up grid below.
- GPS can be toggled on/off in Settings; the Ultra's GNSS rail is powered down when off.
- **DIRECTION OF TRAVEL** (shortened to TRAVEL on the watch face) is GPS course-over-ground, not a compass heading - the Ultra's BHI260AP is a 6-axis IMU with no magnetometer, so there is no stationary heading source on this board. It shows the direction you are actually moving, which is not necessarily the way the watch is pointed, and only while moving (>0.5 mph); it reads blank while stationary, since course over ground is undefined at zero speed. The label deliberately avoids the COG abbreviation, which is marine/aviation jargon.

## Mapping

Reached from the **MAPPING** button on the watch face. Currently the home of the compass-correction tool; intended to grow into map tooling generally.

- **Magnetic declination**, computed fully offline with NOAA's World Magnetic Model (WMM2025, coefficients embedded in firmware, valid 2025.0-2030.0). The spherical-harmonic math runs on-device from a lat/lon and the RTC date, and only while this screen is actually on display.
- **Presented as an instruction, not a number.** The card reads `SET YOUR COMPASS TO 11.4 DEG EAST` and then says which way to apply it - "Compass reads low. ADD 11.4 deg to a compass bearing to get a grid bearing." - flipping automatically to "reads high / SUBTRACT" for west. The user never has to reason about the sign convention.
- **LOCATION: HERE / ELSEWHERE.** HERE uses the live GPS fix. ELSEWHERE opens a keyboard for a typed lat/lon, for planning somewhere you are not yet or when indoors with no fix. The selected toggle is filled, the other outlined.
- **MAP NORTH: GRID / TRUE.** Grid is the default. On a UTM/MGRS map, map north is *grid* north, which differs from true north by the grid convergence angle - up to about 3 degrees near a zone's edges. GRID applies the military G-M angle (declination minus convergence); TRUE applies plain declination for an ungridded map. Since the GPS page shows MGRS, grid is the north you are actually plotting against.
- A bearing converter (map-to-compass for navigating somewhere, compass-to-map for resection) was prototyped here and removed: it needs its own page with a keyboard and room to explain which of those two situations each field serves. Planned.

## Recon

Passive Wi-Fi/BLE survey and monitoring — no active transmission, injection, or association.

- **Activity monitoring**: passive promiscuous-mode channel hopping across US 2.4 GHz channels 1-11, inspecting beacon and deauth/disassociation frames, alternating with BLE scan bursts when the selected detector needs both radios. Stops automatically when leaving the Recon screen.
- **Grouped detector menu.** Top level is `ALL` plus three blue group rows - blue opens a group, gold/teal starts a scan. Each group's sub-page offers `ALL` (the whole group) and each member individually, so you can focus on one signal without wading through the rest. The single top-left BACK walks monitor -> group -> top menu -> out.
  - **TRACKERS**: AirTag / Find My, Tile, Samsung SmartTag, Google Find My Device.
  - **COUNTER-SURVEIL**: Flock Safety cameras, Axon body cameras, Meta Ray-Ban glasses.
  - **COUNTER-INTRUSION**: deauth floods, Pwnagotchi, multi-SSID rogue APs, Wi-Fi Pineapple, Flipper Zero.
  Detections are always logged under the individual detector that matched, never the group.
- **Confidence tier.** Every detection carries HIGH / MED / LOW, shown as `[HIGH]` beside the category in the log and the alert popup, and written to the SD log. The grade is set per signature, not per detector - Meta on its SIG-assigned UUID is HIGH while two unsourced Meta UUIDs are LOW, both under `META`. **LOW never raises the alert**: it is logged and counted like anything else, it just doesn't buzz or pop, using the same mechanism as sleep mode. This is what makes broad-but-noisy signatures safe to include at all.
- **BLE matching is against parsed advertisement fields** - the manufacturer record's company ID and the 16-bit service UUIDs - never a byte-pattern search across the raw payload, which matched by coincidence often enough to make a wrist alert useless. AirTag matches Apple company `0x004C` with a Find My offline-finding subtype (`0x12` near owner, `0x1E` separated); the `0x07` proximity-pairing subtype is deliberately excluded because it also fires on AirPods.
- **Flock OUI table** merges LayerTime's original 8 registered prefixes with the set from SquachWatch-CYD (which were entirely disjoint) - 20 at HIGH. A further 16 generic Espressif MA-L blocks are included at LOW, labelled `Flock? (ESP32)`, because they match any ESP32 in range. Flock also matches on the XUNTONG BLE company ID gated on a Flock-shaped device name, and on Wi-Fi beacon BSSIDs.
- **Axon** matches three OUIs and the `AB2-` / `AB3-` / `AB4-` / `AXON-` SSID prefixes a body camera advertises in pairing mode.
- **Deauth flood detection**: a single deauth or disassociation frame is ordinary Wi-Fi traffic - a phone leaving a network, an AP restarting, a roaming handoff - so a detection requires a burst rather than one frame: **6 frames within 3 seconds from the same transmitter**, followed by a 15-second cooldown before that transmitter can report again. Rates are tracked per transmitter (6 slots, least-recently-active evicted), so unrelated background deauths from several different APs can't sum into a flood that never happened. The counting window restarts after any gap longer than itself, so isolated frames minutes apart never accumulate.
- **Early Warning**: an optional always-on, duty-cycled background sweep (10 s active / 60 s rest) covering deauth, Pwnagotchi, Pineapple and multi-SSID over Wi-Fi plus Flipper and Meta over BLE, independent of whether the Recon screen is open. Trackers and Flock are deliberately excluded from the always-on set. Persisted; defaults **on**. This is the single largest background power draw in the firmware — disable it (along with Mesh/Meshtastic) if the watch needs to run unplugged for extended stretches.
- **SD logging**: a **SD LOGGING** toggle in Settings appends every new (non-duplicate) detection to `/recon_log.csv` on the SD card (timestamp, category, detail, address, RSSI, channel, confidence). The header is written only when the file is created, so a card carrying a pre-confidence log keeps its old six-column header. Persisted; defaults off.
- **Persistent threat log**: the Recon monitor's detection list is a running log, not a per-session snapshot - it survives stopping and restarting monitoring (starting a new detector no longer wipes it) and only resets when the user taps **CLEAR LOG** on the Recon monitor screen. Each unique `(detector, address)` entry tracks an encounter count so a repeat-seen signal is visible as a real attack indicator, not just a single line. For Deauth the count is the number of separate flood *reports* from that transmitter - cooldown-throttled to roughly one per 15 seconds of sustained attack - rather than a raw frame count. Capped at 40 unique entries (oldest evicted first).

## Mesh radios

The T-Watch Ultra has one physical SX1262 LoRa radio. LayerTime implements two independent, non-interoperable mesh protocols against it — **MeshCore** and **Meshtastic**.

Carrying both is a deliberate, permanent feature, not a transitional state. Mesh coverage is local and uneven: the network that exists where you are is whichever one the people around you happened to build. Walk into a Meshtastic area and use Meshtastic; walk into a MeshCore area and use MeshCore — same watch, same interface, no second device. Because they share one radio they are kept mutually exclusive in software: enabling one automatically disables the other.

*Planned: a single abstracted mesh UI serving both protocols, so they look and behave identically and the duplicated screen code goes away; and selecting a protocol implicitly by tapping it on the watch face rather than toggling it in Settings.*

### MeshCore

Bring your own group — a smaller, simpler protocol with a signed identity.

- Radio profile: USA/Canada preset, 910.525 MHz, BW 62.5 kHz, SF7, CR5.
- Passive heard-node list (up to 8 nodes): name/type, RSSI/SNR, age, advertised GPS coordinates.
- Public-channel chat: receives and decrypts the default MeshCore public channel; **CHAT** opens the quick-phrase composer to send.
- **MESHCORE ADVERTISE** (Settings toggle, persisted): transmits a signed node advert (name + GPS when available) immediately on enable and every 15 minutes thereafter. Identity is a persistent Ed25519 keypair generated once and stored in ESP32 NVS; the node name is auto-generated as `LayerTime-XXXX` from the watch's MAC.

### Meshtastic

Reach the larger population — a from-scratch implementation of the real wire format. Built against the real `meshtastic/firmware`/`meshtastic/protobufs` wire format (hand-rolled protobuf parsing; no nanopb/protoc toolchain in this build environment).

- Radio profile: US region, LongFast preset, 906.875 MHz, BW 250 kHz, SF11, CR5, default public-channel PSK, AES-128-CTR payload encryption.
- Decodes and displays heard nodes: long/short name, hop count, RSSI/SNR, last-heard age, GPS position when present, and battery/voltage/channel-utilization telemetry when present.
- **MUI-style interface.** A home grid of tiles - NODES / CHATS, CHANNELS / MAP, INFO - laid out to match Meshtastic's own on-device UI, so anyone who has used a T-Deck is already oriented. One context-sensitive BACK in the top left.
- **Nodes**: name, hop count, age, battery, RSSI and GPS per node. Tap one to open a direct-message thread with it.
- **Chats**: channels and direct-message conversations in one list, unread ones outlined gold with a count.
- **Delivery state on every message.** Bubbles are outlined by what actually happened to them - green acked, red failed, gold relayed, muted still pending. Direct messages request an ack and retry up to three times before being marked failed; broadcasts use the implicit ack, i.e. hearing another node rebroadcast your own packet.
- **Direct messages with PKC encryption.** X25519 key agreement against the peer's published public key, SHA-256 to a 256-bit key, AES-256-CCM with an 8-byte tag - the same scheme as firmware 2.5 and later. Our keypair is generated once and kept in NVS, and our public key rides along in NodeInfo. Peers that publish no key fall back to the channel PSK.
- **Up to eight channels.** Slot 0 is LongFast and fixed; slots 1-7 are user-defined with a name and a base64 PSK (or a bare 0-10 simple-index key), persisted, and shown with their encryption state and hash. Incoming packets decode against the first channel whose hash matches, which is what the firmware itself does.
- **Map from SD-card tiles.** Standard XYZ tiles at `/maps/<style>/<z>/<x>/<y>.png`, drawn to a canvas with your own position in gold and heard nodes in teal. Drag to pan, zoom in and out, and a CENTER button that toggles between your fix and the centroid of heard nodes.
- **Quick-phrase composer**: twenty tappable phrases instead of an on-screen keyboard, because typing on a watch while moving does not work. Tap to fill, tap again to add another, CLEAR to start over, SEND when it reads right.
- **MESHTASTIC ADVERTISE** (Settings toggle, persisted): periodically transmits a NodeInfo packet announcing your name, so other users see a name instead of a raw node number, mirroring MeshCore's advertise behavior. Position and telemetry are also broadcast on a timer, so the watch shows up on other nodes' maps.
- **MESHTASTIC NAME** (Settings, persisted): user-configurable identity via an on-watch keyboard. Leave it blank to fall back to an auto-generated name derived from the chip's MAC.
- The watch's Meshtastic node number is synthesized from the ESP32's factory MAC — it is not a registered Meshtastic device, so this identity only matters for packets the watch itself originates.

## SD card

- Mounted automatically at boot (`/sd`) via LilyGoLib, shared SPI bus with the LoRa radio.
- **Settings > SD CARD** shows status (ready / not recognized) and free/used space.
- **Format**: a destructive format utility is available whenever a card is inserted — whether it currently mounts cleanly or not. This exists specifically to recover cards that come back unreadable after use with tools like Pwnagotchi/Ragnar (which leave a filesystem state other devices, including cameras, won't recognize without a low-level reformat first): the format path force-invalidates the existing filesystem (zeroing the first few sectors) before creating a fresh, standard FAT filesystem, so the watch itself can do the "reformat with an old camera" trick rather than requiring separate hardware.
- Format requires an explicit two-step confirmation (it is fully destructive) before proceeding.

## Settings screen

Long-press the watch face (~1 second) to open. Scrollable, with a fixed BACK in the upper left that stays put while the rows scroll under it - on a sub-page it returns to the main list, on the main list it exits. Rows include:

- **DATE / TIME** — editor that writes the Ultra's RTC directly.
- **BRIGHTNESS** — slider (20-255), applied immediately, persisted.
- **CLOCK FORMAT** — 12/24-hour, persisted.
- **UNITS** — Imperial/Metric, persisted.
- **GPS** — on/off, persisted.
- **MESHCORE** / **MESHCORE ADVERTISE** — see Mesh radios above. `MESHCORE` itself is never persisted (every boot starts with the radio off); `MESHCORE ADVERTISE` is persisted.
- **MESHTASTIC** / **MESHTASTIC ADVERTISE** / **MESHTASTIC NAME** — see Mesh radios above. `MESHTASTIC` itself is never persisted, same reasoning as MeshCore; `MESHTASTIC ADVERTISE` and `MESHTASTIC NAME` are persisted.
- **EARLY WARNING** — Recon background sweep, persisted, defaults on.
- **SD LOGGING** — Recon-to-SD-card CSV logging, persisted, defaults off.
- **SLEEP MODE** — for overnight use: forces the backlight off immediately (instead of waiting out the normal 15s auto-blank), and silences Recon's vibration/popup alert. Detections are still logged and counted normally in the background - only the disruptive alert is muted, so nothing is missed while you're asleep. Tapping the screen still wakes it briefly, same as the normal auto-blank behavior, so you can check the time or come back here to turn it off. Persisted, defaults off.
- **SQUATCHIFY?** — swaps the owl watch-face logo for Squatchy, read from `/assets/squatch.png` on the SD card. Reads `NO FILE` when the artwork isn't there, and the owl stays put. Persisted, defaults off. Artwork by [The Talking Sasquach](https://talkingsasquach.com/), used with permission.
- **SD CARD** — opens the SD card status/format sub-page.

## Known cosmetic issue

The AXP2101 battery-gauge percentage can read inconsistently (e.g. stuck at 0% while otherwise running fine on battery) after repeated brownouts from running Early Warning/Mesh on battery power before those were properly power-managed. This is a coulomb-counter desync, not a hardware fault, and is expected to self-correct after one full, uninterrupted charge cycle. No firmware change is needed for this.
