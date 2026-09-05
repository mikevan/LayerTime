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
#include <stdio.h>

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kWgs84A = 6378137.0;
constexpr double kWgs84EccSquared = 0.00669437999014;
constexpr double kUtmScaleFactor = 0.9996;
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

char latitudeBand(double latitude)
{
    static const char bands[] = "CDEFGHJKLMNPQRSTUVWXX";
    if (latitude < -80.0 || latitude > 84.0) {
        return '-';
    }

    const int index = static_cast<int>((latitude + 80.0) / 8.0);
    return bands[index];
}

UtmCoordinate toUtm(double latitude, double longitude)
{
    UtmCoordinate result;
    if (latitude < -80.0 || latitude > 84.0 || longitude < -180.0 || longitude > 180.0) {
        return result;
    }

    const int zone = GeoGrid::utmZoneFor(latitude, longitude);
    const char band = latitudeBand(latitude);
    if (band == '-') {
        return result;
    }

    const double latRad = toRadians(latitude);
    const double lonRad = toRadians(longitude);
    const double lonOriginRad = toRadians(centralMeridian(zone));

    const double eccPrimeSquared =
        kWgs84EccSquared / (1.0 - kWgs84EccSquared);
    const double sinLat = sin(latRad);
    const double cosLat = cos(latRad);
    const double tanLat = tan(latRad);

    const double n = kWgs84A / sqrt(1.0 - kWgs84EccSquared * sinLat * sinLat);
    const double t = tanLat * tanLat;
    const double c = eccPrimeSquared * cosLat * cosLat;
    const double a = cosLat * (lonRad - lonOriginRad);

    const double ecc2 = kWgs84EccSquared;
    const double ecc3 = ecc2 * ecc2;
    const double ecc4 = ecc3 * ecc2;

    const double m = kWgs84A * (
        (1.0 - ecc2 / 4.0 - 3.0 * ecc3 / 64.0 - 5.0 * ecc4 / 256.0) * latRad
        - (3.0 * ecc2 / 8.0 + 3.0 * ecc3 / 32.0 + 45.0 * ecc4 / 1024.0) * sin(2.0 * latRad)
        + (15.0 * ecc3 / 256.0 + 45.0 * ecc4 / 1024.0) * sin(4.0 * latRad)
        - (35.0 * ecc4 / 3072.0) * sin(6.0 * latRad));

    const double a2 = a * a;
    const double a3 = a2 * a;
    const double a4 = a2 * a2;
    const double a5 = a4 * a;
    const double a6 = a3 * a3;

    double easting = kUtmScaleFactor * n * (
        a
        + (1.0 - t + c) * a3 / 6.0
        + (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * eccPrimeSquared) * a5 / 120.0)
        + 500000.0;

    double northing = kUtmScaleFactor * (
        m
        + n * tanLat * (
            a2 / 2.0
            + (5.0 - t + 9.0 * c + 4.0 * c * c) * a4 / 24.0
            + (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * eccPrimeSquared) * a6 / 720.0));

    if (latitude < 0.0) {
        northing += 10000000.0;
    }

    result.valid = true;
    result.zone = zone;
    result.band = band;
    result.easting = easting;
    result.northing = northing;
    return result;
}

bool toMgrs(const UtmCoordinate &utm, char *out, size_t outSize)
{
    if (!utm.valid || out == nullptr) return false;

    // 100km column letters repeat every three zones; row letters run A-V
    // with I and O omitted, offset by half the set on even zones so
    // neighbouring zones never show the same pair.
    static const char *const kColumns[3] = {"ABCDEFGH", "JKLMNPQR", "STUVWXYZ"};
    static const char kRowsOdd[] = "ABCDEFGHJKLMNPQRSTUV";
    static const char kRowsEven[] = "FGHJKLMNPQRSTUVABCDE";

    const int columnIndex = static_cast<int>(utm.easting / 100000.0) - 1;
    if (columnIndex < 0 || columnIndex > 7) return false;
    const char column = kColumns[(utm.zone - 1) % 3][columnIndex];

    const int rowIndex = static_cast<int>(fmod(utm.northing / 100000.0, 20.0));
    if (rowIndex < 0 || rowIndex > 19) return false;
    const char row = (utm.zone % 2 == 1) ? kRowsOdd[rowIndex] : kRowsEven[rowIndex];

    const long localEasting = static_cast<long>(fmod(utm.easting, 100000.0));
    const long localNorthing = static_cast<long>(fmod(utm.northing, 100000.0));
    snprintf(out, outSize, "%02d%c %c%c\n%05ld %05ld",
             utm.zone, utm.band, column, row, localEasting, localNorthing);
    return true;
}

}
