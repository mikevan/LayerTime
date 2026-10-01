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

// The S3 Plus's GNSS decisions, kept out of the hardware code so they are
// tested with plain g++ (s3plus/test/test_s3plus_gnss_rules):
//
//   * when a NAV-PVT counts as a usable position, and how long it stays
//     current (stale or invalid fixes are rejected by validity and age, never
//     by looking for 0,0);
//   * when GNSS UTC is trustworthy enough to set the RTC, and how often;
//   * the wearer's time-zone offset and the date arithmetic it needs.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace twatch_s3plus {
namespace gnss {

// ---- position ---------------------------------------------------------------

// A position is current for this long after the last usable NAV-PVT. The
// receiver is polled once a second, so five missed answers in a row means the
// fix is gone, whatever the last frame said. Same figure as the Ultra's
// kFreshFixAgeMs.
constexpr uint32_t kFreshFixMs = 5000;

// The receiver's own fixOk flag, and a solution that carries a GNSS position:
// 2D (2), 3D (3), or GNSS plus dead reckoning (4). Dead reckoning only (1)
// and time only (5) are not positions. Intentional difference from the
// Ultra, which accepts any fixType of 2 or more, time only included.
bool pvtIsUsablePosition(bool fixOk, uint8_t fixType);

// Whether the last usable position is still current.
bool positionIsCurrent(bool everHadFix, uint32_t ageMs);

// ---- direction of travel ----------------------------------------------------

// Below this a heading is not shown, as on the Ultra (0.5 mph).
constexpr float kMinTravelMps = 0.5f * 0.44704f;
// The worst heading accuracy still shown.
constexpr float kMaxHeadingAccuracyDeg = 30.0f;

// Whether NAV-PVT's heading of motion is real travel rather than noise: the
// speed is over 0.5 mph, at least twice its own accuracy estimate, and the
// heading's own accuracy estimate is within 30 degrees. A watch lying on a
// desk passes the Ultra's speed-only test on noise alone (seen on this watch:
// "TRAVEL 223 DEG" while stationary); it does not pass this one.
bool courseIsMeaningful(float speedMps, float speedAccuracyMps, float headingAccuracyDeg);

// ---- date and time ----------------------------------------------------------

struct DateTime {
    int year = 0;
    int month = 0;   // 1..12
    int day = 0;     // 1..31
    int hour = 0;    // 0..23
    int minute = 0;  // 0..59
    int second = 0;  // 0..59
};

bool isLeapYear(int year);
int daysInMonth(int year, int month);

// Every field in range, and a year this firmware can have been built for.
// A leap second (60) is refused: the RTC cannot hold it, and the next
// resync is at most an hour away.
bool dateTimeIsValid(const DateTime &t);

// Adds (or, when negative, subtracts) minutes, carrying through hours, days,
// months, and years.
DateTime addMinutes(const DateTime &t, int minutes);

// The DATE / TIME page's "-" and "+" (the Ultra's manual date and time set).
enum class DateField : uint8_t { Day, Month, Year, Hour, Minute };

// One step of one field (direction > 0 forward, < 0 back). Day wraps within
// the month, month wraps December to January, hour and minute wrap, and year
// stops at 2024 and 2099 (the range dateTimeIsValid accepts). A month or year
// change that leaves the day past the end of the month moves it to the last
// day: intentional difference from the Ultra, which wraps it to the 1st
// (31 JAN, MONTH + gave 1 FEB). Seconds become 0. Out-of-range input is
// first brought into range.
DateTime stepDateField(const DateTime &t, DateField field, int direction);

// ---- time zone --------------------------------------------------------------

// UTC-12:00 through UTC+14:00, in 15-minute steps (which covers every offset
// in use, including +05:45 and +12:45).
constexpr int kMinOffsetMinutes = -12 * 60;
constexpr int kMaxOffsetMinutes = 14 * 60;
constexpr int kOffsetStepMinutes = 15;

// Into range and onto a 15-minute step.
int clampOffset(int minutes);
// One step east (direction > 0) or west (direction < 0), stopping at the ends.
int stepOffset(int minutes, int direction);
// "UTC-05:00", "UTC+00:00", "UTC+05:45". out must hold 10 bytes.
void formatOffset(int minutes, char *out, size_t outSize);

// ---- setting the RTC from GNSS ----------------------------------------------

// How often the RTC is set again once it has been set this boot.
constexpr uint32_t kClockResyncMs = 60UL * 60UL * 1000UL;

// GNSS UTC is used only when the receiver marks the date valid, the time
// valid, and the time fully resolved, and the fields themselves are sane.
bool utcIsTrustworthy(bool validDate, bool validTime, bool fullyResolved, const DateTime &utc);

// At boot (not yet set), then no more than once per kClockResyncMs.
bool clockSetDue(bool setThisBoot, uint32_t msSinceLastSet);

} // namespace gnss
} // namespace twatch_s3plus
} // namespace layertime
