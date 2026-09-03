#pragma once

// Shared UTM/MGRS grid geometry. Lives here rather than inside a screen
// because two callers now need it: GpsScreen, to build the MGRS readout, and
// MappingScreen, to correct compass bearings to GRID north rather than true
// north. Duplicating geodesy between them is exactly the kind of thing that
// silently drifts apart.
namespace GeoGrid {

// UTM zone for a position, including the two special-case regions where the
// zone boundaries are not a plain 6-degree grid (southwest Norway, Svalbard).
int utmZoneFor(double latitudeDeg, double longitudeDeg);

// Longitude of a zone's central meridian, where grid north and true north
// coincide.
double centralMeridian(int zone);

// Grid convergence: the angle from true north to GRID north at a position,
// positive when grid north lies east of true north. Zero on the central
// meridian, growing toward the zone edges - up to roughly 3 degrees at
// mid-latitudes, more nearer the poles.
//
// This is what separates a UTM/MGRS map bearing from a true bearing. The
// military G-M angle is declination minus this value.
double convergenceDegrees(double latitudeDeg, double longitudeDeg);

}
