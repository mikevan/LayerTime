#pragma once

// Computes magnetic declination (the angle between true north and magnetic
// north) purely offline, using NOAA's World Magnetic Model (WMM2025,
// coefficients valid 2025.0-2030.0). No network connectivity is required or
// used - the model coefficients are embedded in firmware and the
// spherical-harmonic computation runs entirely on-device from a latitude,
// longitude, and date already available from GPS/RTC.
//
// This implementation is a direct port of NOAA's public-domain legacy
// reference algorithm (the same math used by pygeomag, GPSBabel, and most
// other WMM ports), and its output has been cross-checked against an
// independent reference implementation across several widely separated test
// locations before being embedded here.
namespace DeclinationCalculator {

// Converts a calendar date to the decimal-year format the WMM expects
// (e.g. July 2, 2026 is roughly 2026.5).
double decimalYear(int year, int month, int day);

// Returns the magnetic declination in degrees for a location and date.
// Positive = east of true north, negative = west - i.e. True = Magnetic +
// declination, Magnetic = True - declination.
//
// latitudeDeg: -90..90 (north positive)
// longitudeDeg: -180..180 (east positive)
// decimalYear: from decimalYear() above; accurate for 2025.0-2030.0, and
//   still a reasonable estimate somewhat outside that window since secular
//   change is slow, but increasingly approximate the further out you go.
double declinationDegrees(double latitudeDeg, double longitudeDeg, double decimalYear);

}
