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

#include "Solar.h"

#include <math.h>

namespace layertime {
namespace twatch_s3plus {
namespace solar {

namespace {
constexpr double kPi = 3.14159265358979323846;
double rad(double d) { return d * kPi / 180.0; }
double deg(double r) { return r * 180.0 / kPi; }

// Days from 1970-01-01 to a civil date (Howard Hinnant's days_from_civil).
int64_t daysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

const int64_t kEpochDays = daysFromCivil(2000, 1, 1);

struct Sun {
    double declinationDeg;
    double equationOfTimeMin;
};

// NOAA spreadsheet columns G to V, for a moment `t` in minutes since the
// epoch.
Sun sunAt(int64_t t)
{
    // Julian day: 2000-01-01 00:00 UTC is JD 2451544.5.
    const double jd = 2451544.5 + static_cast<double>(t) / 1440.0;
    const double jc = (jd - 2451545.0) / 36525.0;
    const double l0 = fmod(280.46646 + jc * (36000.76983 + jc * 0.0003032), 360.0);
    const double m = 357.52911 + jc * (35999.05029 - 0.0001537 * jc);
    const double e = 0.016708634 - jc * (0.000042037 + 0.0000001267 * jc);
    const double c = sin(rad(m)) * (1.914602 - jc * (0.004817 + 0.000014 * jc)) +
                     sin(rad(2 * m)) * (0.019993 - 0.000101 * jc) + sin(rad(3 * m)) * 0.000289;
    const double trueLong = l0 + c;
    const double omega = 125.04 - 1934.136 * jc;
    const double appLong = trueLong - 0.00569 - 0.00478 * sin(rad(omega));
    const double meanObliq =
        23.0 + (26.0 + (21.448 - jc * (46.815 + jc * (0.00059 - jc * 0.001813))) / 60.0) / 60.0;
    const double obliq = meanObliq + 0.00256 * cos(rad(omega));
    Sun s;
    s.declinationDeg = deg(asin(sin(rad(obliq)) * sin(rad(appLong))));
    const double y = tan(rad(obliq / 2)) * tan(rad(obliq / 2));
    s.equationOfTimeMin =
        4.0 * deg(y * sin(2 * rad(l0)) - 2 * e * sin(rad(m)) + 4 * e * y * sin(rad(m)) * cos(2 * rad(l0)) -
                  0.5 * y * y * sin(4 * rad(l0)) - 1.25 * e * e * sin(2 * rad(m)));
    return s;
}

// The event of one UTC date: rising (sign -1) or setting (+1).
bool event(int year, int month, int day, double lat, double lon, int sign, int64_t &out)
{
    const int64_t midnight = (daysFromCivil(year, month, day) - kEpochDays) * 1440;
    int64_t estimate = midnight + 720;  // first pass: the sun at noon UTC
    for (int pass = 0; pass < 2; ++pass) {
        const Sun s = sunAt(estimate);
        const double cosH = cos(rad(90.833)) / (cos(rad(lat)) * cos(rad(s.declinationDeg))) -
                            tan(rad(lat)) * tan(rad(s.declinationDeg));
        if (cosH < -1.0 || cosH > 1.0) return false;  // no rise or no set
        const double hourAngle = deg(acos(cosH));
        const double solarNoon = 720.0 - 4.0 * lon - s.equationOfTimeMin;
        const double minutes = solarNoon + sign * 4.0 * hourAngle;
        // Nearest minute (halves away from zero), without libm's llround.
        estimate = midnight + static_cast<int64_t>(minutes >= 0 ? minutes + 0.5 : minutes - 0.5);
    }
    out = estimate;
    return true;
}
} // namespace

int64_t minutesSinceEpoch(const gnss::DateTime &utc)
{
    return (daysFromCivil(utc.year, utc.month, utc.day) - kEpochDays) * 1440 + utc.hour * 60 + utc.minute;
}

gnss::DateTime fromMinutesSinceEpoch(int64_t minutes)
{
    int64_t days = minutes / 1440;
    int64_t rem = minutes % 1440;
    if (rem < 0) {
        rem += 1440;
        --days;
    }
    // civil_from_days (Howard Hinnant).
    int64_t z = days + kEpochDays + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    gnss::DateTime t;
    t.day = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    t.month = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    t.year = static_cast<int>(yoe + era * 400 + (t.month <= 2));
    t.hour = static_cast<int>(rem / 60);
    t.minute = static_cast<int>(rem % 60);
    t.second = 0;
    return t;
}

bool sunrise(int year, int month, int day, double lat, double lon, int64_t &out)
{
    return event(year, month, day, lat, lon, -1, out);
}

bool sunset(int year, int month, int day, double lat, double lon, int64_t &out)
{
    return event(year, month, day, lat, lon, +1, out);
}

NextEvent nextEvent(double lat, double lon, const gnss::DateTime &utcNow)
{
    const int64_t now = minutesSinceEpoch(utcNow);
    NextEvent best;
    // A place's local day can straddle two UTC dates, so look from the day
    // before through two days ahead and keep the earliest event after now.
    for (int d = -1; d <= 2; ++d) {
        const gnss::DateTime day = fromMinutesSinceEpoch(now + d * 1440);
        int64_t t = 0;
        if (sunrise(day.year, day.month, day.day, lat, lon, t) && t > now &&
            (!best.known || t < best.utcMinutes)) {
            best = {true, true, t};
        }
        if (sunset(day.year, day.month, day.day, lat, lon, t) && t > now &&
            (!best.known || t < best.utcMinutes)) {
            best = {true, false, t};
        }
    }
    return best;
}

} // namespace solar
} // namespace twatch_s3plus
} // namespace layertime
