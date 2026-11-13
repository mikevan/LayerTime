# LayerTime-C5

Shared LilyGO T-Dongle-C5 board support used by Hunter and the legacy fox-hunt
beacon. This library owns the tested board pin map, SPI display and LED drivers,
logo bitmaps, button/identity logic and the SD chip-select boot precaution.

Hunter remains the sensor and SD-card product: radio acquisition, BLE Link
transport, SD logs, PCAP evidence and card-transfer tooling stay in
`devices/lilygo-hunter`. The beacon calls `holdC5CardDeselected()` directly;
it does not need Hunter's SD logger merely to keep the shared SPI bus idle.

The library retains the existing header names, such as `C5Display.h`,
`C5Led.h`, `BringUpLogic.h` and `TDongleC5Pins.h`. PlatformIO consumers declare
this library and LayerTime-Core in `lib_deps`, alongside the versions of
Arduino, NimBLE and LVGL selected by their device build. Platform/version pins
remain in the product configuration, because the C5 and watches use different
working toolchains. No application `setup()` or `loop()` lives in this library.

Run the existing Hunter host tests using the command in the Hunter README.
Regenerate the logo data using `devices/lilygo-hunter/tools/c5_logo_images.py`;
its output now targets this library. The existing artwork is unchanged.

The old source macro `LAYERTIME_VIXEN_APP` remains temporarily compatible with
the beacon build. Vixen is now the flagship Android/analysis product; the
legacy beacon's product rename is separate from this library extraction.

GPL-3.0-or-later. Original source copyright and attribution are retained.
