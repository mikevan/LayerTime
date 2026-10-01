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

#include "GnssRules.h"

#include <stdio.h>

namespace layertime {
namespace twatch_s3plus {
namespace gnss {

bool pvtIsUsablePosition(bool fixOk, uint8_t fixType)
{
    return fixOk && (fixType == 2 || fixType == 3 || fixType == 4);
}

bool positionIsCurrent(bool everHadFix, uint32_t ageMs)
{
    return everHadFix && ageMs < kFreshFixMs;
}

bool courseIsMeaningful(float speedMps, float speedAccuracyMps, float headingAccuracyDeg)
{
    return speedMps > kMinTravelMps && speedMps >= 2.0f * speedAccuracyMps &&
           headingAccuracyDeg <= kMaxHeadingAccuracyDeg;
}

bool isLeapYear(int year)
{
    return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

int daysInMonth(int year, int month)
{
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 0;
    return (month == 2 && isLeapYear(year)) ? 29 : kDays[month - 1];
}

bool dateTimeIsValid(const DateTime &t)
{
    return t.year >= 2024 && t.year <= 2099 && t.month >= 1 && t.month <= 12 && t.day >= 1 &&
           t.day <= daysInMonth(t.year, t.month) && t.hour >= 0 && t.hour <= 23 && t.minute >= 0 &&
           t.minute <= 59 && t.second >= 0 && t.second <= 59;
}

DateTime addMinutes(const DateTime &t, int minutes)
{
    DateTime r = t;
    int total = r.hour * 60 + r.minute + minutes;
    int dayShift = 0;
    while (total < 0) {
        total += 24 * 60;
        --dayShift;
    }
    while (total >= 24 * 60) {
        total -= 24 * 60;
        ++dayShift;
    }
    r.hour = total / 60;
    r.minute = total % 60;
    for (; dayShift > 0; --dayShift) {
        if (++r.day > daysInMonth(r.year, r.month)) {
            r.day = 1;
            if (++r.month > 12) {
                r.month = 1;
                ++r.year;
            }
        }
    }
    for (; dayShift < 0; ++dayShift) {
        if (--r.day < 1) {
            if (--r.month < 1) {
                r.month = 12;
                --r.year;
            }
            r.day = daysInMonth(r.year, r.month);
        }
    }
    return r;
}

DateTime stepDateField(const DateTime &t, DateField field, int direction)
{
    DateTime r = t;
    r.second = 0;
    if (r.year < 2024) r.year = 2024;
    if (r.year > 2099) r.year = 2099;
    if (r.month < 1 || r.month > 12) r.month = 1;
    if (r.hour < 0 || r.hour > 23) r.hour = 0;
    if (r.minute < 0 || r.minute > 59) r.minute = 0;
    if (r.day < 1) r.day = 1;
    if (r.day > daysInMonth(r.year, r.month)) r.day = daysInMonth(r.year, r.month);
    const int step = direction > 0 ? 1 : (direction < 0 ? -1 : 0);
    switch (field) {
    case DateField::Day: {
        const int days = daysInMonth(r.year, r.month);
        r.day = (r.day - 1 + step + days) % days + 1;
        return r;
    }
    case DateField::Month:
        r.month = (r.month - 1 + step + 12) % 12 + 1;
        break;
    case DateField::Year:
        r.year += step;
        if (r.year < 2024) r.year = 2024;
        if (r.year > 2099) r.year = 2099;
        break;
    case DateField::Hour:
        r.hour = (r.hour + step + 24) % 24;
        return r;
    case DateField::Minute:
        r.minute = (r.minute + step + 60) % 60;
        return r;
    }
    if (r.day > daysInMonth(r.year, r.month)) r.day = daysInMonth(r.year, r.month);
    return r;
}

int clampOffset(int minutes)
{
    if (minutes < kMinOffsetMinutes) minutes = kMinOffsetMinutes;
    if (minutes > kMaxOffsetMinutes) minutes = kMaxOffsetMinutes;
    // Toward zero onto a step: the ends are themselves on a step.
    return (minutes / kOffsetStepMinutes) * kOffsetStepMinutes;
}

int stepOffset(int minutes, int direction)
{
    const int current = clampOffset(minutes);
    if (direction > 0) return clampOffset(current + kOffsetStepMinutes);
    if (direction < 0) return clampOffset(current - kOffsetStepMinutes);
    return current;
}

void formatOffset(int minutes, char *out, size_t outSize)
{
    if (out == nullptr || outSize == 0) return;
    const int m = clampOffset(minutes);
    const int a = m < 0 ? -m : m;
    snprintf(out, outSize, "UTC%c%02d:%02d", m < 0 ? '-' : '+', a / 60, a % 60);
}

bool utcIsTrustworthy(bool validDate, bool validTime, bool fullyResolved, const DateTime &utc)
{
    return validDate && validTime && fullyResolved && dateTimeIsValid(utc);
}

bool clockSetDue(bool setThisBoot, uint32_t msSinceLastSet)
{
    return !setThisBoot || msSinceLastSet >= kClockResyncMs;
}

} // namespace gnss
} // namespace twatch_s3plus
} // namespace layertime
