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

// Canonical definition: contracts/models.md, "NavigationState".

#include <stdint.h>

#include "Time.h"

namespace layertime {

// Numeric values are part of the contract. Do not renumber.
// Values 0 to 3 match UBX-NAV-PVT fixType, which is where the T-Ultra
// gets them. A platform without that receiver maps onto the same meanings.
enum class FixType : uint8_t {
    None = 0,
    DeadReckoningOnly = 1,
    Fix2D = 2,
    Fix3D = 3,
};

// Where the wearer is, and how much to trust it.
//
// Canonical units are SI: metres, metres per second, degrees. Display units
// (feet, mph) are a presentation choice made by the UI from settings, never
// stored here.
//
// Every optional quantity carries its own validity flag. A field whose flag
// is false holds 0 and must not be shown. A platform that cannot measure a
// quantity leaves its flag false forever; it never substitutes a guess.
struct NavigationState {
    bool receiverEnabled = false;

    FixType fixType = FixType::None;
    // The receiver's own "this solution is usable" judgement. Position fields
    // are only meaningful while this is true.
    bool fixUsable = false;
    // False until a usable fix has existed once since boot.
    bool everHadFix = false;
    // Milliseconds since the last usable fix. Keeps growing while blind, so a
    // consumer can show a position getting older instead of showing a stale
    // one as current.
    uint32_t fixAgeMs = 0;

    uint8_t satellites = 0;

    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;

    bool altitudeValid = false;
    float altitudeM = 0.0f;

    // The receiver's own accuracy estimate. This, not HDOP, is what position
    // confidence is sized from.
    bool horizontalAccuracyValid = false;
    float horizontalAccuracyM = 0.0f;

    bool verticalAccuracyValid = false;
    float verticalAccuracyM = 0.0f;

    // Satellite geometry only. Carried because the T-Ultra GPS screen shows
    // it today. It is not an error estimate and must never be used as one.
    bool hdopValid = false;
    float hdop = 0.0f;

    bool speedValid = false;
    float speedMps = 0.0f;

    // Direction of travel over the ground, from the receiver.
    bool courseValid = false;
    float courseDeg = 0.0f;

    // Direction the wearer faces, from a magnetic compass. A different
    // quantity from course. Only a platform with the compass capability ever
    // sets headingValid.
    bool headingValid = false;
    float headingDeg = 0.0f;

    // When the fields above were last refreshed.
    Timestamp updated;
};

} // namespace layertime
