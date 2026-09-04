# README promises a Wi-Fi scan that does not exist

Under Recon the README claims:

> Wi-Fi scan: SSID, BSSID, RSSI, channel, security, sorted strongest-first.

There is no such feature. `grep` finds no `scanNetworks` or `WiFi.SSID` anywhere in `src/` - the Recon menu is only the detectors. This is public and is the first thing a new arrival will go looking for after flashing.

Either remove the claim or build the raw-scan mode (see #RAWSCAN). Removing it is minutes.
