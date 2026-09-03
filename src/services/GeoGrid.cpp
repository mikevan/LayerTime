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
