# LayerTime on the LilyGo T-Watch S3 Plus

The all-in-one LayerTime watch with the SX1262 radio, with the T-Watch Ultra's
functionality. This folder is the whole target, organized like `garmin/`:

| Folder or file | Holds |
|---|---|
| `platformio.ini` | This target's own pioarduino project |
| `partitions.csv`, `lv_conf_s3plus.h` | Flash layout and LVGL configuration |
| `source/` | The S3 Plus hardware adapters behind the core's ports, services, screens, and entry point |
| `resources/` | Images and fonts (created with the first one) |
| `test/` | S3 Plus host tests |
| `bin/` | Build output and libraries (git-ignored, like `garmin/bin/`) |
| `.vscode/` | Editor settings (git-ignored, like `garmin/.vscode/`) |

Outside this folder it uses only: `src/core/` (shared, hardware-independent,
compiled unchanged through the project's source filter), `boards/` and
`variants/lilygo_twatch_s3/` (read only), `contracts/vectors/profile_twatch_s3plus.json`,
and the S3 Plus files in `tools/`.

## Isolation from the T-Watch Ultra and the T-Dongle-C5

- Open `s3plus/` itself in VS Code. The repository root stays the Ultra and C5
  project; its `platformio.ini` does not know this target exists, and the Ultra
  build compiles only `src/`, so it never sees `s3plus/`.
- `core_dir = ~/.platformio-s3plus` in `platformio.ini` puts this target's
  platform link, packages, Python environment, and cache in their own core
  directory, for the IDE and the command line alike. Nothing is installed in
  the shared `~/.platformio`. A `PLATFORMIO_CORE_DIR` environment variable
  would override `core_dir`, so `../tools/s3plus_core_dir_check.py` stops any
  S3 Plus build that is not running in `~/.platformio-s3plus`. The pioarduino
  IDE (1.4.4) only reads that variable to find its own installation; it does
  not set it for build tasks.
- `workspace_dir = bin` keeps build output and libraries in `s3plus/bin/`.
- The platform is `../tools/pioarduino-platform-s3plus-55.03.36-1-lt1`, a
  renamed copy of the Ultra's lt1 platform (same Arduino core 3.3.6, which
  passes the ESP32-S3 BLE gate the newer cores fail). See its `S3PLUS-COPY.md`.
- `test/test_s3plus_boundary` enforces the code boundary in both directions.

## Build

Open `s3plus/` in VS Code and use the normal build task. The first build
downloads the toolchain into `~/.platformio-s3plus`.

LVGL 9.4.0's bundled ThorVG (needed for the owl SVG) calls `memcpy`,
`memset`, and `strchr` without including `<cstring>`, which this GCC 14
toolchain no longer pulls in by other routes. `../tools/s3plus_lvgl_thorvg_cstring.py`
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

## Current firmware: Phase 1 bring-up (0.1.3)

`source/S3PlusMain.cpp` identifies the hardware. Over USB serial (115200) it
prints a report, repeated every 10 seconds, with the chip, MAC, PSRAM, both I2C
buses checked against LilyGo's documented parts, the GNSS module fitted,
battery and charger, RTC time, and the FFat partition and filesystem (the
partition table entry, what is mounted at /fs and /ffat, and one
non-formatting mount retry). The screen shows the same pass and fail results;
tap it to test touch (a dot follows the tap) and haptics (one pulse per tap,
at most once a second). The LoRa radio is never initialised and its power rail
is switched off. LilyGoLib's own `begin()` mounts FFat at `/fs` (formatting it
only if it will not mount at all) and, on the first boot only, writes the
battery fuel-gauge parameters to the AXP2101.

### FFat repair (0.1.2)

On this watch 0.1.1 reported `FFat totalBytes=0` and `ESP_FAIL` from the
free-space query. A read-only capture (`../tools/s3plus_flash_read.py`) showed
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

### Storage reboot test (0.1.3)

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
TinyUSB mode. Getting logs off the watch is decided in the Recon milestone.

## Register the watch before its first upload

`../tools/s3plus_upload_guard.py` refuses every upload, erase, and filesystem
write unless the board is a registered S3 Plus. Register once, after the first
build has created this target's Python environment, with the watch plugged in
while you hold its BOOT button (the script waits for the new port and never
touches any other):

    & "$env:USERPROFILE\.platformio-s3plus\penv\Scripts\python.exe" ..\tools\s3plus_register_mac.py

The record goes to `%USERPROFILE%\.layertime\s3plus_allowed_boards.txt`,
outside the repository.

## Host tests

From `s3plus/test/`:

    g++ -std=c++17 -O0 -Wall -Wextra -I../../test -o tests_s3plus_boundary test_s3plus_boundary/test_s3plus_boundary.cpp
    ./tests_s3plus_boundary
    g++ -std=c++17 -O0 -Wall -Wextra -I../../test -I../../src -I../source -o tests_s3plus_profile test_s3plus_profile/test_s3plus_profile.cpp
    ./tests_s3plus_profile
    g++ -std=c++17 -O0 -Wall -Wextra -I../../test -I../source -o tests_s3plus_bringup test_s3plus_bringup/test_s3plus_bringup.cpp ../source/BringUpCheck.cpp
    ./tests_s3plus_bringup

From the repository root, for the upload guard:

    python tools/test_s3plus_upload_guard.py

Services this target takes over from the T-Ultra are S3 Plus copies in
`source/`, behind the same core interfaces. They are held to account by
interface-behaviour tests and the boundary test, not by equality with the
Ultra's files. Intentional differences are recorded below.

## LVGL configuration

`lv_conf_s3plus.h` is this target's own copy of LilyGoLib's `lv_conf.h` at
the pinned commit, with the vector-graphics settings the Ultra build uses (it
gets them by an edit inside its `.pio/libdeps` copy). Selected with
`-D LV_CONF_PATH=\"lv_conf_s3plus.h\"` and `-I .`. The name is unique on
purpose: with `LV_CONF_INCLUDE_SIMPLE`, include-path order picked LilyGoLib's
own `lv_conf.h` instead. `source/S3PlusMain.cpp` stops the build if any other
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
