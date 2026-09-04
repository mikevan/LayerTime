// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <stdint.h>

struct AppSettings {
    uint8_t brightness = 80;
    bool use24Hour = false;
    bool metricUnits = false;
    bool gpsEnabled = true;

    // Mesh/LoRa radio power + listening. Deliberately NOT persisted: every boot
    // starts with the radio off, and the user opts in each session from Settings.
    bool meshEnabled = false;
    // Whether Mesh also transmits signed adverts. Only takes effect while
    // meshEnabled is true. This one *is* persisted.
    bool meshAdvertiseEnabled = false;

    // Meshtastic listener - deliberately separate/parallel to MeshCore, kept
    // mutually exclusive with it since both would need the one physical
    // SX1262 radio. Deliberately NOT persisted, same reasoning as meshEnabled.
    bool meshtasticEnabled = false;

    // Meshtastic node identity, user-configurable in Settings > MESHTASTIC
    // NAME. Used as the NodeInfo long_name (and a derived short_name) when
    // meshtasticAdvertiseEnabled is on. Persisted; empty until the user sets
    // one, in which case a name is auto-generated from the chip ID instead.
    char meshtasticNodeName[20] = {0};

    // Whether Meshtastic also periodically transmits a NodeInfo advert
    // announcing meshtasticNodeName, so others see a name instead of a raw
    // node number. Only takes effect while meshtasticEnabled is true. This
    // one *is* persisted, mirroring meshAdvertiseEnabled.
    bool meshtasticAdvertiseEnabled = false;

    // Background Wi-Fi/BLE early-warning sweep (Deauth/Pwnagotchi/Pineapple/
    // MultiSSID + Flipper/Meta), duty-cycled for low power draw. Persisted;
    // defaults on.
    bool reconEarlyWarningEnabled = true;

    // Append every new (non-duplicate) Recon detection to a CSV file on the
    // SD card. Persisted; defaults off (no card assumed present).
    bool reconSdLoggingEnabled = false;

    // Sleep mode: forces the backlight off immediately (instead of waiting
    // out the normal auto-blank timeout) and silences Recon's vibration/
    // popup alert. Detections still get logged normally while it's on -
    // only the disruptive alert is muted, so nothing is missed overnight.
    // Persisted; defaults off.
    bool sleepModeEnabled = false;

    // "Squatchify": swap the owl watch-face logo for Squatchy, read from
    // A:/assets/squatch.png on the SD card. Persisted; defaults off. Falls
    // back to the owl silently when the card or file is missing, so the
    // default face never depends on removable storage.
    bool squatchify = false;
};
