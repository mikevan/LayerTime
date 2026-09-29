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

// Canonical definition: contracts/models.md, "ApplicationSettings".

#include <stdint.h>

#include "Mesh.h"

namespace layertime {

// The wearer's application settings: what LayerTime does, on any target.
// Owned by the core and changed only by commands. Settings that exist only
// because of one watch's hardware (its backlight, its SD card, its shared
// LoRa radio) belong to that platform, not here.
//
// The defaults are the T-Ultra's first-boot values.
struct ApplicationSettings {
    // Meshtastic long name as the wearer typed it: up to 19 characters.
    static constexpr uint8_t kMeshtasticNameSize = 20;

    bool use24Hour = false;
    bool metricUnits = false;

    // Alerts are suppressed; every event is still logged and counted.
    bool sleepModeEnabled = false;

    // The duty-cycled background Recon sweep.
    bool earlyWarningEnabled = true;

    // Announce this node on each mesh network. Indexed by MeshNetwork value.
    bool meshAdvertising[kMeshNetworkCount] = {false, false};

    // Empty means the platform generates a name.
    char meshtasticName[kMeshtasticNameSize] = {0};

    bool advertisingOn(MeshNetwork network) const
    {
        return meshAdvertising[static_cast<uint8_t>(network)];
    }
};

} // namespace layertime
