// LayerTime - the operator interface on the wrist. Connect IQ Device App for
// the Garmin tactix 7 AMOLED (epix2pro51mm).
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

import Toybox.Lang;
import Toybox.Math;
import Toybox.Position;
import Toybox.System;
import Toybox.WatchUi;

// The watch's own navigation data for the home screen: GPS fix quality,
// altitude, and distance travelled since the app started. This is the
// Garmin's domain under the LayerTime separation of duties (the watch owns
// GPS, position and navigation; LayerWand owns RF). Travel is the sum of
// straight-line legs between usable fixes, in metres; it is not a Garmin
// activity and is not stored.
class NavTracker {

    public var quality as Number = Position.QUALITY_NOT_AVAILABLE;
    // Metres above sea level, or null before the first fix carried one.
    public var altitudeM as Float? = null;
    public var travelM as Float = 0.0;
    public var fixes as Number = 0;

    private var _last as Position.Location? = null;

    public function start() as Void {
        Position.enableLocationEvents(Position.LOCATION_CONTINUOUS, method(:onPosition));
    }

    public function stop() as Void {
        Position.enableLocationEvents(Position.LOCATION_DISABLE, method(:onPosition));
    }

    // The latest usable position, or null.
    public function lastLocation() as Position.Location? { return _last; }

    public function hasFix() as Boolean {
        return quality == Position.QUALITY_USABLE || quality == Position.QUALITY_GOOD;
    }

    public function onPosition(info as Position.Info) as Void {
        var q = info.accuracy;
        quality = q != null ? q : Position.QUALITY_NOT_AVAILABLE;
        if (!hasFix()) { return; }
        var alt = info.altitude;
        if (alt != null) { altitudeM = alt; }
        var here = info.position;
        if (here == null) { return; }
        fixes += 1;
        var last = _last;
        if (last != null) {
            var leg = distanceM(last, here);
            // Fixes jitter while standing still; legs under 3 m are noise at
            // consumer-GPS accuracy and are not counted.
            if (leg >= 3.0) { travelM += leg; }
        }
        _last = here;
        WatchUi.requestUpdate();
    }

    // Haversine on a spherical Earth, radius 6371 km. Adequate for the
    // walking distances this screen shows.
    public static function distanceM(a as Position.Location, b as Position.Location) as Float {
        var p = a.toRadians();
        var q = b.toRadians();
        var dLat = (q[0] - p[0]).toFloat();
        var dLon = (q[1] - p[1]).toFloat();
        var sLat = Math.sin(dLat / 2);
        var sLon = Math.sin(dLon / 2);
        var h = sLat * sLat + Math.cos(p[0].toFloat()) * Math.cos(q[0].toFloat()) * sLon * sLon;
        return (2.0 * 6371000.0 * Math.asin(Math.sqrt(h))).toFloat();
    }
}
