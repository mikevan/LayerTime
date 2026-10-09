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

// NavigationSource on the T-Watch Ultra: WatchState's GNSS fields, as
// GpsService fills them, in the contract's SI units.
//
// Nothing on the T-Ultra reads NavigationState yet. The GPS, mapping and face
// screens stay on WatchState in Phase 0: they display feet and mph, and
// routing them through metres and back could move a displayed value by one
// on a rounding boundary.

#include <Arduino.h>

#include "core/ports/NavigationSource.h"
#include "model/WatchState.h"

namespace layertime {
namespace twatch_ultra {

class TUltraNavigationSource : public NavigationSource {
public:
    explicit TUltraNavigationSource(const WatchState &state) : _state(state) {}

    void read(NavigationState &out) override { convert(_state, millis(), out); }

    // Pure conversion, separate so it can be tested without a clock.
    // A quantity whose validity flag is false is reported as 0.
    static void convert(const WatchState &s, uint32_t nowMs, NavigationState &out)
    {
        out = NavigationState{};
        out.receiverEnabled = s.gpsEnabled;
        // gpsFixType is the raw UBX-NAV-PVT value. UBX 4 (GNSS plus dead
        // reckoning) and 5 (time only) have no FixType in contract 0.1, so
        // they are reported as None. Open question for the contract.
        out.fixType = s.gpsFixType <= 3 ? static_cast<FixType>(s.gpsFixType) : FixType::None;
        out.fixUsable = s.gpsFix;
        out.everHadFix = s.gpsEverHadFix;
        out.fixAgeMs = s.gpsFixAgeMs;
        out.satellites = s.gpsSatellites;
        out.latitudeDeg = s.latitude;
        out.longitudeDeg = s.longitude;

        out.altitudeValid = s.gpsAltitudeValid;
        if (out.altitudeValid) out.altitudeM = s.altitudeFt / kFeetPerMetre;

        out.horizontalAccuracyValid = s.gpsHorizontalAccuracyValid;
        if (out.horizontalAccuracyValid) out.horizontalAccuracyM = s.gpsHorizontalAccuracyM;
        out.verticalAccuracyValid = s.gpsVerticalAccuracyValid;
        if (out.verticalAccuracyValid) out.verticalAccuracyM = s.gpsVerticalAccuracyM;

        out.hdopValid = s.gpsHdopValid;
        if (out.hdopValid) out.hdop = s.gpsHdop;

        out.speedValid = s.gpsSpeedValid;
        if (out.speedValid) out.speedMps = s.gpsSpeedMph * kMetresPerSecondPerMph;
        out.courseValid = s.gpsCourseValid;
        if (out.courseValid) out.courseDeg = s.gpsCourseDegrees;

        // No magnetometer on this board.
        out.headingValid = false;

        out.updated.uptimeMs = nowMs;
    }

    // The same feet-per-metre figure the screens use.
    static constexpr float kFeetPerMetre = 3.280839895f;
    static constexpr float kMetresPerSecondPerMph = 0.44704f;

private:
    const WatchState &_state;
};

} // namespace twatch_ultra
} // namespace layertime
