# LayerTime MeshCore Stage 4

Adds signed MeshCore node advertisements and a persistent Settings toggle.

- OFF by default.
- Settings > MESH ADVERTISE toggles it.
- When ON, LayerTime advertises immediately and every 15 minutes.
- Advert includes the LayerTime node name and GPS coordinates when a valid fix exists.
- A persistent Ed25519 identity is generated once and stored in ESP32 NVS.
- Adds orlp/ed25519 to PlatformIO dependencies.

Important: do not delete `.pio` yet because the working LVGL SVG/ThorVG patches are still there.
