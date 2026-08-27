# LayerTime SVG Enable Fix

Extract this overlay into the LayerTime project root and overwrite matching files.

Changes:
- Enables LVGL 9.4 matrix/vector/ThorVG/SVG support through PlatformIO build defines.
- Does NOT add or shadow `lv_conf.h`.
- Changes the watch-face footer to `LAYERTIME | T-WATCH ULTRA`.

After extracting:
1. Make sure `src/lv_conf.h` is still absent.
2. Delete the project `.pio` folder.
3. Build.
4. If successful, upload to the watch.
