# LayerTime Recon Stage 1

Passive survey only. Adds a RECON button on the lower-right of the owl and a Recon screen with Wi-Fi and BLE scans.

- Wi-Fi: SSID, BSSID, RSSI, channel, security
- BLE: name, address, RSSI
- Results sorted strongest signal first
- BACK returns to the LayerTime watch face
- Footer remains gold
- Preserves the prior `esp_mac.h` MeshService compile fix

Extract this ZIP into the project root and overwrite matching files. Do not delete `.pio` yet.
