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

// Canonical definition: contracts/capabilities.md. Profiles:
// contracts/vectors/profile_*.json.

#include <stdint.h>

namespace layertime {

// Numeric values are part of the contract. Do not renumber; append only.
enum class DisplayClass : uint8_t {
    MonochromeLowRes = 0,  // e.g. Instinct 2 class
    ColorHighRes = 1,      // e.g. tactix 8 AMOLED, T-Watch Ultra
};

enum class DisplayShape : uint8_t {
    Rectangle = 0,
    Round = 1,
};

// What a platform can do. Application behaviour branches on these fields,
// never on which device it is running on.
//
// These are EFFECTIVE capabilities. A flag is true only when LayerTime can
// actually use it on that platform today, proven on that platform. Hardware
// that exists but LayerTime cannot yet reach is false. A feature we intend
// to build is false until it works.
//
// Design intent and verification status are recorded in the profile vectors
// (contracts/vectors/profile_*.json), never in this struct. The application
// only ever sees effective values.
//
// Every field defaults to false, so a platform that forgets to declare
// something loses the feature rather than faking it.
struct DeviceCapabilities {
    static constexpr uint8_t kProfileIdSize = 24;

    // For logs and diagnostics only. Never branch on it.
    char profileId[kProfileIdSize] = {0};

    DisplayClass displayClass = DisplayClass::MonochromeLowRes;
    DisplayShape displayShape = DisplayShape::Rectangle;
    uint16_t displayWidth = 0;
    uint16_t displayHeight = 0;

    bool touch = false;
    bool buttons = false;
    bool vibration = false;

    // Navigation.
    bool gps = false;
    bool compass = false;       // magnetic heading
    bool altitude = false;      // any altitude source, GNSS or barometric
    bool nativeMaps = false;    // platform-provided cartography
    bool removableStorage = false;

    // Recon.
    bool localWifiMonitor = false;
    bool localBleMonitor = false;
    bool externalRecon = false; // a LayerTime Node over LayerTime Link

    // Mesh.
    bool meshUi = false;
    bool localMeshtasticRadio = false;
    bool localMeshCoreRadio = false;
    // Both local mesh stacks share one physical radio, so at most one can be
    // enabled at a time.
    bool meshRadioShared = false;
    bool androidBridge = false;
};

} // namespace layertime
