# LayerTime Mesh Integration — Stage 1

Extract this ZIP into the LayerTime project root and overwrite matching files.

This increment:
- Keeps the LayerTime footer gold.
- Adds a boxed **MESH** button at the lower-left of the owl area.
- Adds a Mesh page with BACK navigation.
- Powers and initializes the Ultra SX1262 for the Meshtastic US LongFast RF profile.
- Listens for real Meshtastic LoRa frames and shows frame count, last packet length, RSSI, and SNR.

Current scope: RF-layer Meshtastic reception only. This does not yet decrypt/parse Meshtastic protobuf payloads into node names/messages/positions. That is the next integration layer.

Do not delete `.pio` yet because the current SVG/ThorVG fixes are still stored there.
