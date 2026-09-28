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

// The T-Watch Ultra's capability profile. Must match the effective values in
// contracts/vectors/profile_twatch_ultra.json field for field; the test in
// test/test_core_model checks that it does.
//
// Header-only and pure on purpose, so it builds with plain g++ and costs the
// firmware nothing until something includes it.

#include <string.h>

#include "../../core/model/DeviceCapabilities.h"

namespace layertime {
namespace twatch_ultra {

inline DeviceCapabilities capabilities()
{
    DeviceCapabilities c;
    strncpy(c.profileId, "twatch-ultra", sizeof(c.profileId) - 1);

    // CO5300 AMOLED, 410 x 502. Screens are laid out at that size.
    c.displayClass = DisplayClass::ColorHighRes;
    c.displayShape = DisplayShape::Rectangle;
    c.displayWidth = 410;
    c.displayHeight = 502;

    c.touch = true;        // CST9217
    c.buttons = false;     // LayerTime reads no physical button
    c.vibration = true;    // DRV2605

    c.gps = true;          // u-blox MIA-M10Q
    c.compass = false;     // no magnetometer: I2C scan and LilyGo's doc agree
    c.altitude = true;     // from GNSS
    c.nativeMaps = false;  // LayerTime draws its own tiles from the SD card
    c.removableStorage = true;

    c.localWifiMonitor = true;
    c.localBleMonitor = true;
    c.externalRecon = false;

    c.meshUi = true;
    c.localMeshtasticRadio = true;
    c.localMeshCoreRadio = true;
    c.meshRadioShared = true;  // one SX1262
    c.androidBridge = false;
    return c;
}

} // namespace twatch_ultra
} // namespace layertime
