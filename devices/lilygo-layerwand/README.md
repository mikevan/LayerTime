# LayerWand (LILYGO T-Dongle-C5)

The LayerWand is the LayerTime Node on the LILYGO T-Dongle-C5: it runs Recon
on the C5's Wi-Fi (5 GHz and 2.4 GHz) and BLE radios and serves the watch over
the LayerTime Link (`contracts/link.md`). It is its own pioarduino project:
open this folder, not the repository root, to build it in the pioarduino IDE.
It compiles only the repository's `src/core/` and this folder's `src/`.

## Layout

- `platformio.ini`: four environments. `tdongle_c5_wand` (default) is the
  LayerWand. `tdongle_c5` is the Link-only Node, `tdongle_c5_test` adds the
  Link Probe characteristic, and `tdongle_c5_recon` is the Increment 2A
  no-link Recon baseline.
- `src/`: the C5 platform code (display, LED, button, Link, Recon radio, and
  the apps).
- `test/`: host tests for the hardware-free logic, built with plain g++.
- `tools/`: flash and log scripts, and the owl bitmap generator.
- `boards/lilygo-t-dongle-c5.json`: the board definition.
- `bin/`: build output (git-ignored).

## Flash and log

Each flash script builds and uploads in one step to the one LayerWand whose
USB serial number you name. Run without `-Serial` to list the LayerWands that
are plugged in.

```powershell
Set-Location C:\workspace\TUltra-Project\LayerTime
powershell -ExecutionPolicy Bypass -File devices\lilygo-layerwand\tools\layerwand_wand_flash.ps1 -Serial <serial>
powershell -ExecutionPolicy Bypass -File devices\lilygo-layerwand\tools\layerwand_wand_log.ps1 -Serial <serial>
```

Logs go to `Claude outputs\wand2\` at the repository root. Keep the logger
open until the LayerWand is unplugged: a USB host that holds the port open
without reading it stalls the firmware (D4, lab-only).

## SD-card log

The LayerWand looks for a microSD card right after boot. With a card, it
logs every record for the whole run, watch connected or not. The card shares
the status LED's bus, so every card access flashes the LED; the card is
touched only when a write is due: when 512 records (half the ring) are
waiting, or when the watch connects or disconnects. A card inserted after
boot is found at the next write. Without one, events live only in memory
(the newest 40), Link Status carries `noSdLog`, and the watch shows "No SD
card is installed. Limited memory will result in errors when the memory is
full."
Files are named `layerwand_0001.log`, `layerwand_0002.log`, and so on, after
the highest one already on the card. Records are held in PSRAM until a write, so up to 512
records are lost if power is cut. Each file is CSV with a header
line, then a `boot` line (boot count and reset reason), then `event`, `link`,
and `mode` lines. Text fields are quoted; a quote is doubled, and a backslash
or control byte is written as `\\` or `\xNN`. The `[periodic] ... sd` line
in the serial report shows the card, the file, and the counters. The rules
are in `src/SdLogLogic.h` (host-tested); the card handling is in
`src/C5SdLog.h`.

## Host tests

Run from `devices/lilygo-layerwand/test/`:

```
g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../../../src -I../../../sensors/src -I../src -o tests_tdongle_c5_bringup test_tdongle_c5_bringup/test_tdongle_c5_bringup.cpp ../src/BringUpLogic.cpp ../src/SdLogLogic.cpp
./tests_tdongle_c5_bringup
```

The LayerWand's scheduler is core's (`src/core/logic/ReconScheduler`); its
suite, `test_recon_scheduler`, stays with the root suites in `test/`.

## Regenerating the owl

`tools/c5_owl_image.py` renders the owl from `assets/LayerTime-owl.svg` at
the repository root into `src/C5OwlImage.cpp`. If the
owl changes, recheck the eye and lens boxes in `src/C5OwlImage.h`.
