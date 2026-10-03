# LayerTime on the LilyGo T-Watch S3 Plus

The all-in-one LayerTime watch with the SX1262 radio, with the T-Watch Ultra's
functionality. This folder is the whole target, organized like `devices/garmin-tactix/`:

| Folder or file | Holds |
|---|---|
| `platformio.ini` | This target's own pioarduino project |
| `partitions.csv`, `lv_conf_s3plus.h` | Flash layout and LVGL configuration |
| `src/` | The S3 Plus hardware adapters behind the core's ports, services, screens, and entry point |
| `boards/`, `variants/` | This watch's board definition and pin map (copies; LilyGo's leftover `twatchs3` root environments still use the originals) |
| `tools/` | Build checks, the upload guard and its registration, the flash capture, and this target's platform copy |
| `LICENSE` | GPL-3.0, for this area |
| `resources/` | Images and fonts (created with the first one) |
| `test/` | S3 Plus host tests |
| `bin/` | Build output and libraries (git-ignored, like `devices/garmin-tactix/bin/`) |
| `.vscode/` | Editor settings (git-ignored, like `devices/garmin-tactix/.vscode/`) |

Outside this folder it uses only: `src/core/` (shared, hardware-independent,
compiled unchanged through the project's source filter), `boards/` and
`variants/lilygo_twatch_s3/` (read only), `contracts/vectors/profile_twatch_s3plus.json`,
and the S3 Plus files in `tools/`.

## Isolation from the T-Watch Ultra and the T-Dongle-C5

- Open `devices/lilygo-s3plus/` itself in VS Code. The repository root stays the Ultra and C5
  project; its `platformio.ini` does not know this target exists, and the Ultra
  build compiles only `src/`, so it never sees `devices/lilygo-s3plus/`.
- `core_dir = ~/.platformio-s3plus` in `platformio.ini` puts this target's
  platform link, packages, Python environment, and cache in their own core
  directory, for the IDE and the command line alike. Nothing is installed in
  the shared `~/.platformio`. A `PLATFORMIO_CORE_DIR` environment variable
  would override `core_dir`, so `tools/s3plus_core_dir_check.py` stops any
  S3 Plus build that is not running in `~/.platformio-s3plus`. The pioarduino
  IDE (1.4.4) only reads that variable to find its own installation; it does
  not set it for build tasks.
- `workspace_dir = bin` keeps build output and libraries in `devices/lilygo-s3plus/bin/`.
- The platform is `tools/pioarduino-platform-s3plus-55.03.36-1-lt1`, a
  renamed copy of the Ultra's lt1 platform (same Arduino core 3.3.6, which
  passes the ESP32-S3 BLE gate the newer cores fail). See its `S3PLUS-COPY.md`.
- `test/test_s3plus_boundary` enforces the code boundary in both directions.

## Build

Open `devices/lilygo-s3plus/` in VS Code and use the normal build task. The first build
downloads the toolchain into `~/.platformio-s3plus`.

LVGL 9.4.0's bundled ThorVG (needed for the owl SVG) calls `memcpy`,
`memset`, and `strchr` without including `<cstring>`, which this GCC 14
toolchain no longer pulls in by other routes. `tools/s3plus_lvgl_thorvg_cstring.py`
force-includes `<cstring>` for every C++ compile of this target (C and
assembler sources are untouched). The Ultra builds because its `.pio/libdeps`
copy of LVGL was edited by hand (2026-08-23) to add those includes; this target
never edits downloaded libraries. A per-file version that touched only ThorVG
worked on Linux but not on Windows: the platform's Windows-only middleware
integration (`builder/frameworks/arduino.py`, `if IS_WINDOWS:`) drops per-file
flags. The script's header records the details.

Known quirk: a verbose build (`-v`) of this project fails at the final
`firmware.bin` step with "unsupported operand type(s) for +: '_Null' and
'str'" while printing the command line. It happens with any `pre:` extra
script under this platform, compiles nothing differently, and the normal
(non-verbose) build is unaffected.

## Current firmware: Recon milestone R1 (0.2.3)

0.2.x runs Recon on this watch: the T-Ultra's onboard Wi-Fi and BLE
monitoring, threats, and alerts, through the shared core
(`src/core/app/LayerTimeCore`) and S3 Plus adapters behind the core's ports.
Logging is R2; the long run and the side-by-side comparison with the Ultra
are R3. The LoRa radio is never initialised and its power rail is switched
off (mesh is deferred).

| Folder | What it holds |
|---|---|
| `src/S3PlusMain.cpp` | `setup()` and `loop()`, nothing else. |
| `src/app/` | `S3PlusApp`: bring-up, the core and its ports, the screens, the display gate. |
| `src/recon/` | `S3PlusReconService` (copy of the Ultra's `ReconService`), `S3PlusMonitorSource`, `EventLock`. |
| `src/gnss/` | `S3PlusGpsService`, `S3PlusUbx` (copy of the Ultra's `UbxParser`), `S3PlusNavigationSource`, `S3PlusClock`, `GnssRules`. |
| `src/ui/` | Home, Recon, and Time screens; `DisplayGate`; `TextFormat`; the theme. |
| `src/S3PlusAlertSink`, `S3PlusSettingsStore` | The alert and settings ports. |
| `src/BringUpReport`, `BringUpCheck` | The hardware report printed once at boot. |

What the wearer sees:

* **Home (0.2.3):** after the Garmin face's design (`devices/garmin-tactix/source/
  HomeView.mc`), laid out for 240 x 240: 24 battery dots across the top
  over "BAT 87%" (both red at 20% or below); the owl, large, with the GPS
  and DONGLE icons beside its ears; the time and the date flanked by a
  "WATCH" ring
  (the watch's own temperature from the BMA423, red at 45 C / 113 F and
  above, so it shows the watch running hot; it is not air temperature) and
  a "SUNRISE" or "SUNSET" ring (the next one, from the last GNSS position
  and the clock; NOAA's equations, `gnss/Solar`), as on the Garmin; "GPS"
  is green with a usable fix and red without, "DONGLE" red (no dongle link
  on this watch yet; "DONGLE ICON" in Settings hides it); "LOCATION" and
  "RECON"; and the Garmin's Recon bracket with the
  Garmin's wording ("EARLY WARN  REST", "EARLY WARN", "RECON ALL", "RECON
  OFF") over "N DETECTIONS", amber, red on an alert. Tap the bracket for
  the running scan's monitor (or ALL), the GPS icon or "LOCATION" for the
  GPS page, "RECON" for the Recon menu. Long-press the face for Settings,
  as on the Ultra and the Garmin. Until GNSS or the DATE / TIME page has set
  the clock, the date line reads "TIME NOT SET YET".
* **GPS (0.2.3):** the Ultra's GPS page scaled to 240 x 240, in its design:
  filled gold "< BACK", green "GPS" title, fix status, "LAT" and "LON", the
  "MGRS" grid reference, "ALT", "SATS", "ACC", and "SPD", "DIRECTION OF
  TRAVEL", and "LAYERTIME | GPS". "ACC" is the receiver's own horizontal
  accuracy, shown where the Ultra shows HDOP. A position that is no longer
  current is never shown: the status reads "FIX LOST 2M AGO" and the
  coordinates read "--". Every line is one line tall, so a long value
  ends in "..." instead of wrapping onto the next row. Core's GeoGrid writes
  MGRS on two lines; this page joins them ("15S UA 92025 15918") in
  Montserrat 14.
* **Recon:** the Ultra's menu with the Ultra's names and labels: "ALL", the
  three group rows (blue opens a group), each group's page, a monitor page
  with "STOP RECON" and "CLEAR LOG", and the alert overlay with "DISMISS".
  "BACK" on the monitor goes straight to the face and leaves the scan
  running; the face then reads "THREATS / RECON ALL" (or "RECON" for any
  other scan), and tapping "THREATS" reopens that scan's monitor without
  restarting it. "STOP RECON" is the only control that stops a scan. On a
  group or the top level, "BACK" steps back one level. Early warning is
  switched in Settings, as on the Ultra.
* **Settings:** the Ultra's "LAYERTIME SETTINGS" page row for row, in one
  scrolling list: "DATE / TIME", "BRIGHTNESS" (slider), "CLOCK FORMAT" ("12
  H" or "24 H"), "UNITS" ("IMPERIAL" or "METRIC"), "GPS" (the receiver's
  power rail), "MESHCORE", "MESHCORE ADVERTISE", "MESHTASTIC", "MESHTASTIC
  ADVERTISE", "MESHTASTIC NAME", "EARLY WARNING", "LOGGING", "SLEEP MODE",
  and "SQUACHIFY?". Rows with nothing behind them yet (mesh, LOGGING,
  SQUACHIFY?) are greyed and read "LATER". No "SD CARD" row: this watch has
  no SD card. Added for the S3 Plus: "DONGLE ICON" ("ON" or "OFF").
* **DATE / TIME:** "TIME ZONE" "-" and "+" (15-minute steps, UTC-12:00 to
  UTC+14:00, applied at once), then the Ultra's date and time set: "DAY",
  "MONTH", "YEAR", "HOUR", and "MIN" "-" and "+", "CANCEL", and "SAVE". A
  status line says when GNSS last set the clock, or that it was set by hand.
  GNSS still wins: its next set (at boot, then hourly) replaces a time saved
  here.
* **Display:** dark after 15 seconds without input. A tap on a dark screen
  only wakes it; input returns when that finger lifts, so a button can never
  be pressed blind. An alert wakes it with input at once. A double-tap on the
  face (the owl, the time, anywhere but a button or the THREATS and GPS
  blocks) puts it to sleep at once, as on the Ultra; that dark is the
  wearer's choice, so an alert only buzzes and the next touch wakes it.

Over USB serial: the hardware report once at boot, a `[s3plus] status` line
every 30 seconds (free heap, minimum, largest block, events, Recon state,
fix, clock), and one line per Recon start, stop, clear, and early-warning
change.

### Intentional differences from the T-Ultra

| Area | T-Ultra | S3 Plus | Why |
|---|---|---|---|
| Event history | Radio tasks write it while the UI reads it, unlocked (the core documents the race). | `recon/EventLock`: one mutex around classification on the radio tasks, and around alerts, clear, acknowledge, and the screens' copy on the main loop. No radio is driven with the lock held. | Closes the race without changing the core. |
| Position | NMEA through TinyGPSPlus, UBX for accuracy. | UBX NAV-PVT only: position, altitude, accuracy, ground speed, and heading of motion. | One protocol, one parser; NAV-PVT carries everything the face shows. |
| Direction of travel | Shown whenever speed is over 0.5 mph. | Also needs the speed to be at least twice NAV-PVT's own speed accuracy, and NAV-PVT's heading accuracy within 30 degrees (`GnssRules::courseIsMeaningful`). | 0.2.1 showed "TRAVEL 223 DEG" on a watch lying on a desk: speed alone passes on noise. |
| Leaving Recon | "BACK" on the monitor stops the scan, so the face never shows a manual scan. | "BACK" goes to the face and the scan keeps running; "STOP RECON" stops it; the face reads "RECON ALL" or "RECON". | With BACK as the only way out of the monitor, a scan could never run while the face showed it. A scan left running costs battery; the face is the reminder. |
| Watch face | The Ultra's face: ALT, TRAVEL, THREATS, and GPS blocks around the owl. | The Garmin face's design: battery dots, WATCH and SUNRISE/SUNSET rings, GPS and DONGLE icons, and the Recon bracket in the Garmin's wording. ALT and direction of travel are on the GPS page. | Chosen 2026-10-01: colour draws the eye, small text carries the detail, and all LayerTime devices tell the Recon story the same way. The Ultra gets the same face (`claude/face_design.md` in the project). |
| WATCH ring | Not applicable. | The watch's own temperature (BMA423 register, decoded here: SensorLib reads it as unsigned, so anything below 23 C came out near 280 C). | No air sensor on this watch; the Garmin's TEMP is phone weather. Knowing the watch runs hot is useful in itself. |
| Settings rows with nothing behind them | Not applicable (every row works). | Greyed and reading "LATER" (mesh, LOGGING, SQUACHIFY?); no "SD CARD" row. | A switch that does nothing would lie about what the watch is doing. |
| Date stepping | 31 JAN, "MONTH +" gives 1 FEB (the day wraps). | Gives 28 FEB (the day stays in the month; `GnssRules::stepDateField`). | A month change should not also change the day. |
| Speed | Shown whenever NMEA reports a valid speed (TinyGPSPlus `speed.isValid()`). | "SPD" shows a speed only when DIRECTION OF TRAVEL would show a heading (`GnssRules::courseIsMeaningful`); otherwise "SPD -- MPH". The navigation data keeps the raw speed. | 0.2.2 showed "SPD 1.9 MPH" on a watch lying on a desk. |
| GPS page | HDOP from NMEA. | "ACC", the receiver's horizontal accuracy. | No NMEA here, and HDOP is geometry, not error. |
| GPS block | "GPS WAIT" whenever there is no fix. | "GPS 2M AGO" once a fix has been lost; "GPS WAIT" only before the first. | A lost fix shows its age instead of looking like a fresh start. |
| Usable fix | Receiver `fixOk` and fixType 2 or more (time only included); fresh within 5 s. | `fixOk` and fixType 2, 3, or 4; current within 5 s (`GnssRules`). | A time-only solution has no position. Stale fixes are rejected by validity and age, never by 0,0. |
| Clock | Set by hand on the Settings screen. | Set from GNSS UTC plus the time-zone offset, only when the receiver marks the date valid, the time valid, and the time fully resolved; at boot, then hourly. A time saved by hand on DATE / TIME holds until GNSS has the time. | Timestamps that agree with real time, for the side-by-side in R3. |
| Results list | Oldest first. | Newest first. | A new detection is visible on a 240 x 240 screen without scrolling. |
| Display sleep | A tap on a dark screen reaches the screen under it. | The waking tap is withheld (`DisplayGate`). | No blind presses on "CLEAR LOG", a detector, or "DISMISS". |

The NAV-PVT UTC offsets and valid bits follow the u-blox definition as
carried in SparkFun's u-blox GNSS v3 library (`src/u-blox_structs.h`,
`UBX_NAV_PVT_data_t`) and Zephyr's `struct ubx_nav_pvt`; the first clock set
on the watch prints the UTC it used, which is the live check.

### Bring-up history (0.1.x)

0.1.0 to 0.1.3 identified the hardware and repaired FFat. 0.2.0 keeps the
hardware report and still completes a storage reboot test left pending by
0.1.3, but drops the one-time format control (it ran and was locked) and the
"Run storage reboot test" button. Both are in the history of
`src/S3PlusMain.cpp` at 0.1.3.

#### FFat repair (0.1.2)

On this watch 0.1.1 reported `FFat totalBytes=0` and `ESP_FAIL` from the
free-space query. A read-only capture (`tools/s3plus_flash_read.py`) showed
why: the FAT boot sector is intact, so the volume mounts, but the FAT table
and root directory hold an earlier firmware's data, so FatFs returns
`FR_INT_ERR` as soon as it walks the FAT. LilyGoLib never formats a volume
that mounts, so this does not repair itself.

0.1.2 offers one repair, as a red button labeled "Hold to format storage". It
is shown only when all of these hold:

* the filesystem fails its check (no nonzero capacity at `/fs`);
* the partition table has exactly one data/fat partition, and it is `ffat`
  at 0x810000 with 0x7E0000 bytes (`BringUpCheck::ffatPartitionIsExpected`);
* this firmware has never formatted this watch (NVS namespace `lt_s3plus`,
  key `ffmt`).

It fires only after a continuous three-second hold (`BringUpCheck::HoldGate`);
releasing early cancels and changes nothing. The format count is written to
NVS before the format starts, and the partition is checked again just before,
so the control can never be offered twice, even after a power loss. The
format is the Arduino core's `FFat.format(FFAT_WIPE_FULL, "ffat")`: it erases
that partition through its wear-leveling layer and builds a new FAT sized from
it. Nothing else in flash is erased. Afterwards the firmware mounts the volume
at `/fs`, checks for a nonzero capacity, writes `/s3plus_selftest.txt` with a
random token, reads it back, and records the token in NVS. On the next boot it
reads the file again, compares it, removes it, and confirms the removal; the
outcome is stored as described below.

#### Storage reboot test (0.1.3)

0.1.2 kept the reboot check's result only in RAM for one boot, and printed it
only to the serial port. On this watch that result was lost: the serial
monitor's automatic reconnect stalled after the restart, and the watch was
restarted again before anyone read the port. 0.1.3 makes the check
repeatable and its result durable.

When storage passes its check, the screen shows a button labeled "Run storage
reboot test". Tapping it writes `/s3plus_selftest.txt` with a random token,
reads it back, and records the token in NVS (`lt_s3plus/sttok`). The screen
then reads "Test file written. Restart the watch to finish the test." On the
next boot, before anything else uses storage, the firmware checks that the
file exists and matches the token, removes it, confirms the removal, and
stores the outcome (`lt_s3plus/strs`, values in
`BringUpCheck::RebootTestResult`, which never change meaning) and a count of
completed tests (`lt_s3plus/stn`). Every report then prints
`storage reboot test: completed runs=N, last result: ...`, and the status
screen shows "Reboot test: PASS." (or "FAIL.", "pending.", "not run."), so a
missed serial line or another restart no longer loses the result. The test
never formats anything; the format control stays locked.

After any watch restart or cable unplug, close the Monitor terminal and start
Monitor again. Its automatic reconnect has been seen to stall without
reporting anything. Opening the port does not reset the watch.

With `ARDUINO_USB_MODE=1` (this board's setting, USB serial and JTAG),
LilyGoLib does not expose FFat to a PC as a USB drive; that only happens in
TinyUSB mode. Getting logs off the watch is part of R2.

## Register the watch before its first upload

`tools/s3plus_upload_guard.py` refuses every upload, erase, and filesystem
write unless the board is a registered S3 Plus. Register once, after the first
build has created this target's Python environment, with the watch plugged in
while you hold its BOOT button (the script waits for the new port and never
touches any other):

    & "$env:USERPROFILE\.platformio-s3plus\penv\Scripts\python.exe" tools\s3plus_register_mac.py

The record goes to `%USERPROFILE%\.layertime\s3plus_allowed_boards.txt`,
outside the repository.

## Host tests

From `devices/lilygo-s3plus/test/`:

    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -o tests_s3plus_boundary test_s3plus_boundary/test_s3plus_boundary.cpp
    ./tests_s3plus_boundary
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../src -I../src -o tests_s3plus_profile test_s3plus_profile/test_s3plus_profile.cpp
    ./tests_s3plus_profile
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_bringup test_s3plus_bringup/test_s3plus_bringup.cpp ../src/BringUpCheck.cpp
    ./tests_s3plus_bringup
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_ubx test_s3plus_ubx/test_s3plus_ubx.cpp ../src/gnss/S3PlusUbx.cpp
    ./tests_s3plus_ubx
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_gnss_rules test_s3plus_gnss_rules/test_s3plus_gnss_rules.cpp ../src/gnss/GnssRules.cpp
    ./tests_s3plus_gnss_rules
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_display test_s3plus_display/test_s3plus_display.cpp ../src/ui/DisplayGate.cpp
    ./tests_s3plus_display
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../src -I../src -o tests_s3plus_text test_s3plus_text/test_s3plus_text.cpp ../src/ui/TextFormat.cpp ../../../src/core/logic/ReconSelection.cpp ../../../src/core/logic/GeoGrid.cpp ../src/gnss/Solar.cpp
    ./tests_s3plus_text
    g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_solar test_s3plus_solar/test_s3plus_solar.cpp ../src/gnss/Solar.cpp
    ./tests_s3plus_solar

From `devices/lilygo-s3plus/`, for the upload guard:

    python tools/test_s3plus_upload_guard.py

Services this target takes over from the T-Ultra are S3 Plus copies in
`src/`, behind the same core interfaces. They are held to account by
interface-behaviour tests and the boundary test, not by equality with the
Ultra's files. Intentional differences are recorded above (0.2.0) and below
(hardware).

## LVGL configuration

`lv_conf_s3plus.h` is this target's own copy of LilyGoLib's `lv_conf.h` at
the pinned commit, with the vector-graphics settings the Ultra build uses (it
gets them by an edit inside its `.pio/libdeps` copy). Selected with
`-D LV_CONF_PATH=\"lv_conf_s3plus.h\"` and `-I .`. The name is unique on
purpose: with `LV_CONF_INCLUDE_SIMPLE`, include-path order picked LilyGoLib's
own `lv_conf.h` instead. `src/S3PlusMain.cpp` stops the build if any other
configuration is picked up.

## Hardware differences from the T-Watch Ultra (intentional)

| Area | T-Watch Ultra | T-Watch S3 Plus | Consequence here |
|---|---|---|---|
| PSRAM | 8 MB quad (board `qio_qspi`) | 8 MB octal: ESP32-S3-R8 on the schematic; board `qio_opi`. LilyGo's hardware doc says QSPI, which the schematic contradicts. | Board file `lilygo-t-watch-s3.json`. Phase 1 reads the PSRAM size at boot. |
| Display | CO5300 410 x 502 AMOLED, QSPI | ST7789V3 240 x 240 IPS TFT, SPI | Every screen is laid out again for 240 x 240. |
| Touch | CST9217 | FT6336U at 0x38 on Wire1; reset not connected | Never put the touch panel to sleep: without a reset line it does not come back. |
| RTC | PCF85063A | PCF8563 at 0x51 | Same SensorLib `getDateTime`/`setDateTime` calls. |
| Motion sensor | BHI260AP (accel and gyro) | BMA423 (accel only) | LayerTime uses neither today. |
| GNSS | MIA-M10Q, PPS on GPIO13 | MIA-M10Q or Quectel LS550G, UART1 RX 41 / TX 42, PPS not connected | The UBX work applies only to the MIA-M10Q; Phase 1 reads which module is fitted. |
| LoRa | SX1262 | SX1262, CS 5 / RST 8 / BUSY 7 / IRQ 9, rail ALDO4 | Mesh is deferred. The Ultra's 3.0 V TCXO setting is not carried over; Meshtastic uses 1.8 V on this watch. |
| Storage | microSD | None. Internal FFat partition (`partitions.csv`, about 8 MB) | Logging is bounded; map tiles are a later decision. |
| NFC, GPIO expander | ST25R3916, XL9555 | None | LayerTime uses neither. |
| USB ID in firmware | 303A:8227 | 303A:821B | Uploads go through the S3 Plus upload guard, not the USB ID. |

## Sources

- Xinyuan-LilyGO. "LilyGo T-Watch-S3-Plus." *LilyGoLib*, GitHub, 2025,
  https://github.com/Xinyuan-LilyGO/LilyGoLib/blob/master/docs/hardware/lilygo-t-watch-s3-plus.md.
- Xinyuan-LilyGO. *T_WATCH-S3 25-03-24* (schematic). LilyGoLib, commit 38e6f8d, `schematic/`.
- Espressif Systems. "lilygo_twatch_s3/pins_arduino.h." *arduino-esp32*, GitHub,
  https://raw.githubusercontent.com/espressif/arduino-esp32/master/variants/lilygo_twatch_s3/pins_arduino.h.
- Meshtastic. "t-watch-s3/variant.h." *meshtastic/firmware*, GitHub,
  https://raw.githubusercontent.com/meshtastic/firmware/master/variants/esp32s3/t-watch-s3/variant.h.
