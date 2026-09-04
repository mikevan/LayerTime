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

#include "GeoGrid.h"

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;
double toRadians(double degrees) { return degrees * kPi / 180.0; }
}

namespace GeoGrid {

int utmZoneFor(double latitudeDeg, double longitudeDeg)
{
    int zone = static_cast<int>((longitudeDeg + 180.0) / 6.0) + 1;

    // UTM special-zone rules for southwest Norway.
    if (latitudeDeg >= 56.0 && latitudeDeg < 64.0 && longitudeDeg >= 3.0 && longitudeDeg < 12.0) {
        zone = 32;
    }

    // UTM special-zone rules for Svalbard.
    if (latitudeDeg >= 72.0 && latitudeDeg < 84.0) {
        if (longitudeDeg >= 0.0 && longitudeDeg < 9.0) zone = 31;
        else if (longitudeDeg < 21.0) zone = 33;
        else if (longitudeDeg < 33.0) zone = 35;
        else if (longitudeDeg < 42.0) zone = 37;
    }

    if (zone < 1) zone = 1;
    if (zone > 60) zone = 60;
    return zone;
}

double centralMeridian(int zone)
{
    return (zone - 1) * 6.0 - 180.0 + 3.0;
}

double convergenceDegrees(double latitudeDeg, double longitudeDeg)
{
    const int zone = utmZoneFor(latitudeDeg, longitudeDeg);
    const double deltaLon = toRadians(longitudeDeg - centralMeridian(zone));
    const double latRad = toRadians(latitudeDeg);
    // Standard spherical approximation. The full ellipsoidal series adds
    // well under a hundredth of a degree at these latitudes - far below what
    // anyone can set on a baseplate compass.
    return atan(tan(deltaLon) * sin(latRad)) * 180.0 / kPi;
}

}
