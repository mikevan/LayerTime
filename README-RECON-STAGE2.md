# LayerTime Recon Stage 2

Adds passive Wi-Fi activity monitoring to Recon.

- ACTIVITY toggles promiscuous receive monitoring.
- Passively hops US 2.4 GHz channels 1-11.
- Counts management, data, and control frames.
- Detects deauthentication and disassociation management frames.
- Shows last detected event source/BSSID, RSSI, and channel.
- No deauthentication, injection, association, or other active Wi-Fi operations are performed.
- Monitoring stops automatically when leaving Recon.
- Existing Wi-Fi/BLE scans and gold footer are preserved.
