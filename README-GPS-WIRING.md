# LayerTime GPS Wiring Overlay

Extract this ZIP directly into the LayerTime project root (the folder containing `platformio.ini` and `src/`).
The archive is project-root-relative: it contains `src/...` directly and has no extra wrapper folder.

## What this increment wires

- Uses LilyGoLib's already-initialized MIA-M10Q GNSS (`instance.gps`, TinyGPSPlus-based).
- Polls GNSS continuously so the UART buffer stays drained.
- Shows live satellite count when a fresh fix exists.
- Shows GNSS altitude in feet or meters according to the existing Units setting.
- Adds a persistent GPS ON/OFF control to Settings.
- Powers the Ultra GNSS rail on/off through LilyGoLib `POWER_GPS`.
- Calls `instance.loop()` so LilyGoLib hardware events are serviced.
- Preserves the latest watch-face placement: time Y=360, seconds Y=388, date Y=422.

## Deliberately not wired yet

- HDG remains `---`: T-Watch Ultra's BHI260AP is a 6-axis IMU, not a magnetometer. LayerTime heading is reserved for trueNorth.
- WX remains a placeholder until an off-grid/weather source is selected.
- Bluetooth remains the next settings/radio increment.

## Build note

Do NOT delete `.pio` yet. The working LVGL SVG/ThorVG patches are still there.
Build first; if successful, upload.
