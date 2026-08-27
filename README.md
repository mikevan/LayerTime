<div align="center" markdown="1">
  <img src=".github/LilyGo_logo.png" alt="LilyGo logo" width="100"/>
</div>

<h1 align="center">🌟 LayerTime 🌟</h1>
<p align="center">Custom field/survivalism firmware for the LilyGo T-Watch Ultra</p>

# Overview

LayerTime is a PlatformIO-based firmware for the [LilyGo T-Watch Ultra](https://lilygo.cc/products/t-watch-ultra) (ESP32-S3, SX1262 LoRa, AXP2101 PMU, MIA-M10Q GNSS), built on top of [LilyGoLib](https://github.com/Xinyuan-LilyGO/LilyGoLib). It turns the watch into a field companion: a live GPS page, passive Wi-Fi/BLE recon and threat detection, two independent LoRa mesh protocols (MeshCore and Meshtastic), SD-card logging and utilities, and a settings screen to tune all of it.

See [FEATURES.md](./FEATURES.md) for a full walkthrough of every screen and setting.

# Highlights

- **Watch face** — time/date, battery, GPS status, and quick-launch buttons for MeshCore, Meshtastic, and Recon.
- **GPS** — live lat/long, altitude, satellites, HDOP, speed, and course from the onboard GNSS.
- **Recon** — passive Wi-Fi/BLE survey and monitoring, with dedicated detectors for deauth attacks, Pwnagotchi/Flipper/Flock/AirTag/Meta devices, rogue APs, and more. Optional always-on early-warning sweep and SD-card CSV logging of every detection.
- **MeshCore** — the original LoRa mesh integration (public-channel chat, signed node adverts, heard-node list). Kept in the tree but deprecated in favor of Meshtastic for this project's use case.
- **Meshtastic** — a parallel, actively-developed LoRa mesh implementation for the default US/LongFast public channel: node/telemetry/position decoding, public-channel chat (send + receive), and optional identity advertising. MeshCore and Meshtastic are mutually exclusive, since both drive the same physical radio.
- **SD card** — format/recover a card in the field (including one left in a non-standard filesystem state by tools like Pwnagotchi), plus CSV logging of Recon detections.
- **Settings** — brightness, clock format, units, GPS/mesh/recon toggles, date & time, SD card tools.

# Hardware target

This project targets the **T-Watch Ultra** specifically (`default_envs = twatch_ultra` in `platformio.ini`). The `platformio.ini` also carries environments for T-Watch-S3, T-LoRa-Pager, and the LilyGoLib emulator targets inherited from the base template, but LayerTime's own screens and services (Recon, MeshCore, Meshtastic, GPS, SD logging) are written against the Ultra's hardware and are not verified on the other boards.

# Build & flash (PlatformIO)

1. Install [Visual Studio Code](https://code.visualstudio.com/) and the `PlatformIO` extension (or use the PlatformIO Core CLI directly).
2. Open this project folder in VS Code and let PlatformIO fetch dependencies.
3. Confirm `default_envs = twatch_ultra` is uncommented in `platformio.ini` (it is, by default, in this repo).
4. Build, then upload over USB. If the port won't open ("Access is denied" on Windows), make sure nothing else — most commonly an open Serial Monitor — has it locked.
5. Use the Serial Monitor to watch boot/log output once flashed.

> [!IMPORTANT]
> ⚠️ USB port not enumerating, or the board won't enter download mode?
> See LilyGoLib's [T-Watch-Ultra download-mode notes](https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/lilygo-t-watch-ultra.md#t-watch-s3-ultra-enter-download-mode).
> For hardware diagnosis, LilyGo also provides stock [factory firmware](https://github.com/Xinyuan-LilyGO/LilyGoLib/tree/master/firmware).

# Project layout

- `src/app/WatchApp.*` — top-level app: wires every service and screen together, owns the settings-changed/mutual-exclusion logic.
- `src/services/` — hardware/protocol logic (Battery, Clock, GPS, Recon, MeshService (MeshCore), MeshtasticService, SdCardService, SettingsService, WeatherBleService).
- `src/ui/` — LVGL screens (WatchFace, GpsScreen, MeshScreen, MeshtasticScreen, ReconScreen, SettingsScreen).
- `src/model/` — shared state/settings structs (`AppSettings`, `WatchState`).
- `variants/lilygo_twatch_ultra/` — board pin definitions.
- `assets/` — source SVG assets (owl logo).

# Attribution

Based on LilyGo's [LilyGoLib-PlatformIO](https://github.com/Xinyuan-LilyGO/LilyGoLib) starter template.
