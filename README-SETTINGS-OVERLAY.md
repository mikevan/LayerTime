# LayerTime Settings Overlay

Extract this ZIP into the LayerTime project root and overwrite matching files.

## What this adds

- Long-press anywhere on the LayerTime watch face to open Settings.
- Functional brightness slider (20-255), applied immediately and saved in ESP32 Preferences.
- Functional 12/24-hour clock toggle, saved persistently.
- Functional Imperial/Metric units toggle, saved persistently.
- Functional Date/Time editor that writes the T-Watch Ultra RTC using `instance.rtc.setDateTime(...)`.
- Back button returns to the LayerTime watch face.
- Bluetooth and GPS are shown as "coming next" so the UI does not pretend they are wired before their hardware services exist.

## Important

Do NOT delete `.pio` before this build. Your current SVG/ThorVG fixes are still inside `.pio` and deleting it will remove them.

Build first. If it succeeds, upload.

## Entry gesture

Long-press the watch face for about one second.
