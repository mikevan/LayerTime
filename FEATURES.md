# LayerTime Features

Current-state feature documentation for the LayerTime firmware, targeting the LilyGo T-Watch Ultra. This replaces the earlier per-increment `README-*.md` files, which tracked development stage-by-stage and had gone stale relative to the current build.

## Watch face

- Time, date, and battery percentage.
- Large, colored ALT / COG / THREATS / GPS data blocks, Montserrat 18.
- **THREATS** (bottom-left, green/red): status text shows what's actively scanning right now - `ALL`, a single detector name (e.g. `DEAUTH`) when manually selected, `EARLY WARNING` when only the background sweep is running, or `OFF` when nothing is scanning at all. Color is separate from that text: red as long as the persistent threat log (see Recon below) holds anything, green when it's empty - so a threat found earlier stays flagged red even after monitoring stops, until the user clears the log. Tap it to jump straight into Recon's All-detectors monitor, skipping the detector-picker menu.
- No seconds or AM/PM shown; the 12/24-hour setting still controls the hour format.
- Long-press anywhere on the face to open **Settings**.
- Tap the green GPS block to open the **GPS** page.
- Quick-launch buttons along the bottom: **MESHCORE**, **MTASTIC** (Meshtastic), and **RECON**.
- Footer reads `LAYERTIME | T-WATCH ULTRA`.
- The AMOLED panel sleeps after 15 seconds without touch input (via the Ultra's `sleepDisplay()`/`wakeupDisplay()`, not deep sleep) and wakes on a touch or a short crown press; all services keep running while the panel is off. Brightness is restored from Settings on wake.

## GPS

- Uses LilyGoLib's onboard MIA-M10Q GNSS (`instance.gps`), polled continuously.
- Live latitude/longitude (6 decimal places), altitude in the selected unit system, satellite count, HDOP, speed, and course over ground when moving.
- In-page WGS84-to-UTM conversion (zone/easting/northing), no map engine or SD dependency.
- GPS can be toggled on/off in Settings; the Ultra's GNSS rail is powered down when off.
- HDG is shown as GPS course-over-ground (COG), not a true compass heading - the Ultra's BHI260AP is a 6-axis IMU, not a magnetometer, so there's no stationary heading source on this board. COG only reflects direction of travel while moving (>0.5 mph); it reads blank while stationary, since course-over-ground is undefined at zero speed.

## Recon

Passive Wi-Fi/BLE survey and monitoring — no active transmission, injection, or association.

- **Wi-Fi scan**: SSID, BSSID, RSSI, channel, security; sorted strongest-first.
- **BLE scan**: name, address, RSSI.
- **Activity monitoring**: passive promiscuous-mode channel hopping across US 2.4 GHz channels 1-11, counting management/data/control frames and detecting deauth/disassociation frames (source, RSSI, channel). Stops automatically when leaving the Recon screen.
- **Detectors**: Deauth, Pwnagotchi, MultiSSID rogue-AP patterns, and Pineapple over Wi-Fi; Flipper Zero, AirTag, and Meta (Flock/other BLE beacon) devices over BLE.
- **Early Warning**: an optional always-on, duty-cycled background sweep across the same detectors, independent of whether the Recon screen is open. Persisted; defaults **on**. This is the single largest background power draw in the firmware — disable it (along with Mesh/Meshtastic) if the watch needs to run unplugged for extended stretches.
- **SD logging**: a **SD LOGGING** toggle in Settings appends every new (non-duplicate) detection to `/recon_log.csv` on the SD card (timestamp, category, detail, address, RSSI, channel). Persisted; defaults off.
- **Persistent threat log**: the Recon monitor's detection list is a running log, not a per-session snapshot - it survives stopping and restarting monitoring (starting a new detector no longer wipes it) and only resets when the user taps **CLEAR LOG** on the Recon monitor screen. Each unique `(detector, address)` entry tracks an encounter count so a repeat-seen signal (e.g. 13,000 deauth frames from the same MAC vs. just one) is visible as a real attack indicator, not just a single line. Capped at 40 unique entries (oldest evicted first).

## Mesh radios

The T-Watch Ultra has one physical SX1262 LoRa radio. LayerTime implements two independent, non-interoperable mesh protocols against it — **MeshCore** and **Meshtastic** — kept mutually exclusive in software: enabling one in Settings automatically disables the other, since both would otherwise fight over the same radio.

### MeshCore (active)

The original mesh integration. Kept in the tree for reference and comparison, but no longer the primary/recommended protocol for this project.

- Radio profile: USA/Canada preset, 910.525 MHz, BW 62.5 kHz, SF7, CR5.
- Passive heard-node list (up to 8 nodes): name/type, RSSI/SNR, age, advertised GPS coordinates.
- Public-channel chat: receives and decrypts the default MeshCore public channel; **CHAT** opens an on-watch keyboard to send.
- **MESHCORE ADVERTISE** (Settings toggle, persisted): transmits a signed node advert (name + GPS when available) immediately on enable and every 15 minutes thereafter. Identity is a persistent Ed25519 keypair generated once and stored in ESP32 NVS; the node name is auto-generated as `LayerTime-XXXX` from the watch's MAC.

### Meshtastic (active)

A parallel, from-scratch implementation for the default US/LongFast public channel — deliberately not sharing code with MeshCore, so the two can be evaluated side by side. Built against the real `meshtastic/firmware`/`meshtastic/protobufs` wire format (hand-rolled protobuf parsing; no nanopb/protoc toolchain in this build environment).

- Radio profile: US region, LongFast preset, 906.875 MHz, BW 250 kHz, SF11, CR5, default public-channel PSK, AES-128-CTR payload encryption.
- Decodes and displays heard nodes: long/short name, hop count, RSSI/SNR, last-heard age, GPS position when present, and battery/voltage/channel-utilization telemetry when present.
- Public-channel chat: receives and decrypts text messages; **CHAT** opens the same on-watch keyboard as MeshCore to send. Sent messages appear in the message list tagged TX, received ones RX.
- **MESHTASTIC ADVERTISE** (Settings toggle, persisted): periodically transmits a NodeInfo packet announcing your name, so other users see a name instead of a raw node number, mirroring MeshCore's advertise behavior.
- **MESHTASTIC NAME** (Settings, persisted): user-configurable identity via an on-watch keyboard. Leave it blank to fall back to an auto-generated name derived from the chip's MAC.
- The watch's Meshtastic node number is synthesized from the ESP32's factory MAC — it is not a registered Meshtastic device, so this identity only matters for packets the watch itself originates.

## SD card

- Mounted automatically at boot (`/sd`) via LilyGoLib, shared SPI bus with the LoRa radio.
- **Settings > SD CARD** shows status (ready / not recognized) and free/used space.
- **Format**: a destructive format utility is available whenever a card is inserted — whether it currently mounts cleanly or not. This exists specifically to recover cards that come back unreadable after use with tools like Pwnagotchi/Ragnar (which leave a filesystem state other devices, including cameras, won't recognize without a low-level reformat first): the format path force-invalidates the existing filesystem (zeroing the first few sectors) before creating a fresh, standard FAT filesystem, so the watch itself can do the "reformat with an old camera" trick rather than requiring separate hardware.
- Format requires an explicit two-step confirmation (it is fully destructive) before proceeding.

## Settings screen

Long-press the watch face (~1 second) to open. Scrollable; rows include:

- **DATE / TIME** — editor that writes the Ultra's RTC directly.
- **BRIGHTNESS** — slider (20-255), applied immediately, persisted.
- **CLOCK FORMAT** — 12/24-hour, persisted.
- **UNITS** — Imperial/Metric, persisted.
- **GPS** — on/off, persisted.
- **MESHCORE** / **MESHCORE ADVERTISE** — see Mesh radios above. `MESHCORE` itself is never persisted (every boot starts with the radio off); `MESHCORE ADVERTISE` is persisted.
- **MESHTASTIC** / **MESHTASTIC ADVERTISE** / **MESHTASTIC NAME** — see Mesh radios above. `MESHTASTIC` itself is never persisted, same reasoning as MeshCore; `MESHTASTIC ADVERTISE` and `MESHTASTIC NAME` are persisted.
- **EARLY WARNING** — Recon background sweep, persisted, defaults on.
- **SD LOGGING** — Recon-to-SD-card CSV logging, persisted, defaults off.
- **SD CARD** — opens the SD card status/format sub-page.

## Known cosmetic issue

The AXP2101 battery-gauge percentage can read inconsistently (e.g. stuck at 0% while otherwise running fine on battery) after repeated brownouts from running Early Warning/Mesh on battery power before those were properly power-managed. This is a coulomb-counter desync, not a hardware fault, and is expected to self-correct after one full, uninterrupted charge cycle. No firmware change is needed for this.
