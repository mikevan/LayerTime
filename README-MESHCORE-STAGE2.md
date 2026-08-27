# LayerTime MeshCore Stage 2

This overlay replaces the raw Meshtastic frame listener with a passive MeshCore listener modeled on the working WDGWatch direction.

Current behavior:
- SX1262 listens on the current USA/Canada MeshCore narrow preset: 910.525 MHz, SF7, BW62.5, CR5.
- Parses MeshCore packet headers.
- Parses plaintext node advertisements.
- Builds an in-memory heard-node list (up to 8 nodes).
- Shows node name/type, RSSI/SNR, age, and advertised GPS coordinates when present.
- Does not yet send advertisements or messages.
- Does not yet decrypt public-channel messages.

The LayerTime footer remains gold.
