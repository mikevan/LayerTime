// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

// NavigationSource on the S3 Plus: S3PlusGpsService's NAV-PVT in the
// contract's SI units. Like TUltraNavigationSource, a quantity this watch
// does not measure keeps its validity flag false: HDOP (NMEA only, and not an
// error estimate anyway) and heading (no magnetometer on this board). Speed
// over ground comes from NAV-PVT and is valid while the position is current;
// course over ground is valid only when gnss::courseIsMeaningful says the
// receiver is really moving.

#include <Arduino.h>

#include "core/ports/NavigationSource.h"

#include "S3PlusGpsService.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusNavigationSource : public NavigationSource {
public:
    explicit S3PlusNavigationSource(const S3PlusGpsService &gps) : _gps(gps) {}

    void read(NavigationState &out) override
    {
        const uint32_t now = millis();
        out = NavigationState{};
        // False while GPS is switched off in Settings; everything below then
        // stays at "nothing known" (the service forgot the fix).
        out.receiverEnabled = _gps.enabled();
        const ubx::NavPvt &latest = _gps.lastPvt();
        // UBX 4 (GNSS plus dead reckoning) and 5 (time only) have no FixType
        // in the contract; reported as None, as on the Ultra.
        out.fixType = (_gps.havePvt() && latest.fixType <= 3) ? static_cast<FixType>(latest.fixType)
                                                              : FixType::None;
        out.fixUsable = _gps.positionCurrent(now);
        out.everHadFix = _gps.everHadFix();
        out.fixAgeMs = _gps.fixAgeMs(now);
        out.satellites = _gps.havePvt() ? latest.satellites : 0;

        // The last usable position, current or not; fixUsable and fixAgeMs
        // say how much to trust it.
        const ubx::NavPvt &fix = _gps.lastUsablePvt();
        if (out.everHadFix) {
            out.latitudeDeg = fix.latitude;
            out.longitudeDeg = fix.longitude;
            out.altitudeValid = out.fixUsable && fix.fixType >= 3;
            if (out.altitudeValid) out.altitudeM = fix.altitudeMslM;
            out.horizontalAccuracyValid = out.fixUsable && fix.horizontalAccuracyValid;
            if (out.horizontalAccuracyValid) out.horizontalAccuracyM = fix.horizontalAccuracyM;
            out.verticalAccuracyValid = out.fixUsable && fix.verticalAccuracyValid;
            if (out.verticalAccuracyValid) out.verticalAccuracyM = fix.verticalAccuracyM;
            out.speedValid = out.fixUsable;
            if (out.speedValid) out.speedMps = fix.groundSpeedMps;
            out.courseValid = out.fixUsable && gnss::courseIsMeaningful(fix.groundSpeedMps,
                                                                        fix.speedAccuracyMps,
                                                                        fix.headingAccuracyDeg);
            if (out.courseValid) out.courseDeg = fix.headingOfMotionDeg;
        }
        out.updated.uptimeMs = now;
    }

private:
    const S3PlusGpsService &_gps;
};

} // namespace twatch_s3plus
} // namespace layertime
