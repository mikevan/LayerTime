# LayerTime SVG Owl Overlay

This overlay replaces the hand-drawn LVGL owl with the traced LayerTime SVG embedded in firmware.

Files:
- `src/ui/OwlLogo.cpp` - embeds and renders the SVG using LVGL's SVG image decoder.
- `src/ui/OwlLogo.h` - simplified image-backed OwlLogo interface.
- `src/lv_conf.h` - preserves LILYGO's LVGL configuration and enables matrix/vector/ThorVG/SVG support.
- `platformio.ini` - current working pioarduino dependency set plus project LVGL config lookup.
- `assets/LayerTime-owl.svg` - editable/source SVG kept in the project.

Copy these files over the matching locations in the LayerTime project, delete `.pio`, and Build.
