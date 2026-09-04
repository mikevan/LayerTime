# Verify the map against real tiles

M5 is written and compiled and has never drawn a real tile.

- [ ] Get the Arkansas / Missouri / Oklahoma bundles onto a FAT32 card at `/maps/osm/<z>/<x>/<y>.png`
- [ ] Confirm XYZ not TMS (TMS flips Y and renders subtly wrong rather than failing)
- [ ] Check tile decode speed - `LV_CACHE_DEF_SIZE=0` means every visible tile re-decodes on every redraw, and this is the real unknown
- [ ] Confirm markers land where they should

Partly de-risked already: `SquatchLogo` uses the identical canvas + `lv_draw_image`-from-file path and works, for opaque palette PNGs - which is exactly what tiles are.

Details in the project notes: bundles beat the polygon downloader, and the FAT32 cluster size decides how much actually fits.
