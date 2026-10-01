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

// Sunrise and sunset for the watch face's solar ring, from the GNSS position
// and the clock. Pure, so it is tested with g++ (lilygo-s3plus/test/
// test_s3plus_solar).
//
// The equations are NOAA's (the NOAA/GML Solar Calculator spreadsheet, based
// on Jean Meeus, "Astronomical Algorithms"): the sun's apparent longitude,
// declination, and the equation of time for the Julian century, then the
// hour angle at a solar zenith of 90.833 degrees (the disc's upper edge on
// the horizon with standard refraction). NOAA gives the method as accurate to
// within a minute between 72 degrees north and south, and within 10 minutes
// outside that (https://gml.noaa.gov/grad/solcalc/calcdetails.html).
//
// Each event is computed twice: once at the day's solar noon for a first
// estimate, then again at that estimate, so the sun's position is taken at
// the moment of the event rather than at noon.

#include <stdint.h>

#include "GnssRules.h"

namespace layertime {
namespace twatch_s3plus {
namespace solar {

// Minutes since 2000-01-01 00:00 UTC.
int64_t minutesSinceEpoch(const gnss::DateTime &utc);
gnss::DateTime fromMinutesSinceEpoch(int64_t minutes);

// The sunrise and sunset of one UTC date at a place (east longitude
// positive), in minutes since the epoch. False when the sun does not rise or
// does not set that day (polar day or night).
bool sunrise(int year, int month, int day, double latitudeDeg, double longitudeDeg, int64_t &out);
bool sunset(int year, int month, int day, double latitudeDeg, double longitudeDeg, int64_t &out);

struct NextEvent {
    bool known = false;
    bool isSunrise = false;
    int64_t utcMinutes = 0;
};

// The first sunrise or sunset after `utcNow`, searching up to two days ahead.
// Unknown inside a polar day or night longer than that.
NextEvent nextEvent(double latitudeDeg, double longitudeDeg, const gnss::DateTime &utcNow);

} // namespace solar
} // namespace twatch_s3plus
} // namespace layertime
