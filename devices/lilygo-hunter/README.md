# Hunter (LILYGO T-Dongle-C5)

The Hunter is the LayerTime Node on the LILYGO T-Dongle-C5: it runs Recon
on the C5's Wi-Fi (5 GHz and 2.4 GHz) and BLE radios and serves the watch over
the LayerTime Link (`contracts/link.md`). It is its own pioarduino project:
open this folder, not the repository root, to build it in the pioarduino IDE.
It compiles only the repository's `libraries/LayerTime-Core/src/core/` and this folder's `src/`.

## Layout

- `platformio.ini`: four environments. `tdongle_c5_hunter` (default) is the
  Hunter. `tdongle_c5` is the Link-only Node, `tdongle_c5_test` adds the
  Link Probe characteristic, and `tdongle_c5_recon` is the Increment 2A
  no-link Recon baseline.
- `src/`: the C5 platform code (display, LED, button, Link, Recon radio, and
  the apps).
- `test/`: host tests for the hardware-free logic, built with plain g++.
- `tools/`: flash and log scripts, and the owl bitmap generator.
- `boards/lilygo-t-dongle-c5.json`: the board definition.
- `bin/`: build output (git-ignored).

## Flash and log

Each flash script builds and uploads in one step to the one Hunter whose
USB serial number you name. Run without `-Serial` to list the Hunters that
are plugged in.

```powershell
Set-Location C:\workspace\TUltra-Project\LayerTime
powershell -ExecutionPolicy Bypass -File devices\lilygo-hunter\tools\hunter_flash.ps1 -Serial <serial>
powershell -ExecutionPolicy Bypass -File devices\lilygo-hunter\tools\hunter_log.ps1 -Serial <serial>
```

Logs go to `Claude outputs\wand2\` at the repository root. Keep the logger
open until the Hunter is unplugged: a USB host that holds the port open
without reading it stalls the firmware (D4, lab-only).

## SD-card log

The Hunter looks for a microSD card right after boot. With a card, it
logs every record for the whole run, watch connected or not. The card shares
the status LED's bus, so every card access flashes the LED; the card is
touched only when a write is due: when 512 records (half the ring) are
waiting, or when the watch connects or disconnects. A card inserted after
boot is found at the next write. Without one, events live only in memory
(the newest 40), Link Status carries `noSdLog`, and the watch shows "No SD
card is installed. Limited memory will result in errors when the memory is
full."
Files are named `hunter_0001.log`, `hunter_0002.log`, and so on, after
the highest one already on the card. Records are held in PSRAM until a write, so up to 512
records are lost if power is cut. Each file is CSV with a header
line, then a `boot` line (boot count and reset reason), then `event`, `link`,
`mode`, `repeat`, and `evidence` lines. A `repeat` line (2026-11-04) is an
event seen again: its count, signal, and last seen, at most once every 10 s per
event. An `evidence` line is a detection's evidence window saved as a pcap file
beside the log: the windows from `hunter_0007.log` go in `ev_0007/`, one
file per detection, `event_00042.pcap` for event id 42 (802.11 with radiotap,
no FCS, times since boot). A window is saved once it stops collecting, and
saved again if a repeat sighting revived it and it heard more. Its line gives
the path and the counts kept, seen, dropped, and truncated. Without a card the
windows stay in PSRAM only, and the newest 32 are kept. Text fields are quoted; a quote is doubled, and a backslash
or control byte is written as `\\` or `\xNN`. The `[periodic] ... sd` line
in the serial report shows the card, the file, and the counters. The rules
are in `src/SdLogLogic.h` (host-tested); the card handling is in
`src/C5SdLog.h`.

## Host tests

Run from `devices/lilygo-hunter/test/`:

```
g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../libraries/LayerTime-Core/src -I../../../sensors/src -I../src -I../../../libraries/LayerTime-C5/src -o tests_tdongle_c5_bringup test_tdongle_c5_bringup/test_tdongle_c5_bringup.cpp ../../../libraries/LayerTime-C5/src/BringUpLogic.cpp ../src/SdLogLogic.cpp ../src/CardTransferLogic.cpp ../../../libraries/LayerTime-Core/src/core/link/LinkCard.cpp ../../../libraries/LayerTime-Core/src/core/link/LinkCodec.cpp
./tests_tdongle_c5_bringup
```

The Hunter's scheduler is core's (`libraries/LayerTime-Core/src/core/logic/ReconScheduler`); its
suite, `test_recon_scheduler`, stays with the root suites in `test/`.

## Regenerating the screen heads

Since 2026-11-01 the screen shows the Hunter cat on a Hunter and the Vixen
fox on a Vixen; the owl is the Layers and PRS logo. `tools/c5_logo_images.py`
crops both heads from `assets/hunter-logo.png` and `assets/vixen-logo.png`
into `src/C5LogoImages.cpp`. If a logo changes, rerun it and recheck the eye
and cheek boxes in `src/C5LogoImages.h`; the script prints where the Hunter's
green pixels fall. A green eye means a LayerTime or LayerScope is connected,
and a green cheek means the Hunter is on the mesh; each is red otherwise.

The old owl files (`tools/c5_owl_image.py`, `src/C5OwlImage.*`) are no longer
referenced by any build.
