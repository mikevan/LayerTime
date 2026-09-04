# Recon: raw scan mode

NOTE: this file previously held a "render the confidence tier" issue. That work is already shipped - `ReconService::confidenceLabel()` is rendered in `ReconScreen` and written to the SD log, and the phase-4 OUI merge is in too. Do not file that one. This is the real next Recon item after the ring buffer.

There is no way to see what the watch is actually hearing - Recon reports signature matches only. When a detector does not fire, neither the user nor the maintainer can tell whether the signal was absent, was heard and not matched, or was matched and suppressed.

- [ ] A raw list of everything seen: Wi-Fi SSID / BSSID / RSSI / channel, BLE name / address / RSSI
- [ ] Reachable from the Recon menu, off by default (it is noisy and costs power)

This is the single biggest diagnostic gap in the firmware. It is also what the README used to claim existed, and what would make every "it didn't detect my X" report answerable.
