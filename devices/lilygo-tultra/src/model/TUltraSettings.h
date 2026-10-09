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

// The T-Watch Ultra's own settings: ones that exist because of this watch's
// hardware. Since Phase 0 Step 6 the application settings (clock format,
// units, sleep mode, early warning, mesh advertising, the Meshtastic name)
// are the core's layertime::ApplicationSettings, changed by commands.
struct TUltraSettings {
    uint8_t brightness = 80;
    bool gpsEnabled = true;

    // Mesh/LoRa radio power + listening. Deliberately NOT persisted: every boot
    // starts with the radio off, and the user opts in each session from Settings.
    bool meshEnabled = false;

    // Meshtastic listener - deliberately separate/parallel to MeshCore, kept
    // mutually exclusive with it since both would need the one physical
    // SX1262 radio. Deliberately NOT persisted, same reasoning as meshEnabled.
    bool meshtasticEnabled = false;

    // Append every new (non-duplicate) Recon detection to a CSV file on the
    // SD card. Persisted; defaults off (no card assumed present).
    bool reconSdLoggingEnabled = false;

    // "Squachify": swap the owl watch-face logo for Squachy, read from
    // A:/assets/squach.png on the SD card. Persisted; defaults off. Falls
    // back to the owl silently when the card or file is missing, so the
    // default face never depends on removable storage.
    bool squachify = false;
};
