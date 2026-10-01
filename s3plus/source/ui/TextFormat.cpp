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

#include "TextFormat.h"

#include <stdio.h>

#include "core/logic/GeoGrid.h"
#include "core/logic/ReconSelection.h"

namespace layertime {
namespace twatch_s3plus {
namespace text {

namespace {
// The same figures the Ultra's face and GPS page use.
constexpr float kFeetPerMetre = 3.280839895f;
constexpr float kMphPerMps = 2.2369363f;
constexpr float kKmhPerMps = 3.6f;

bool moving(const NavigationState &nav)
{
    return nav.courseValid && nav.speedValid && nav.speedMps > gnss::kMinTravelMps;
}

int roundedDegrees(float deg)
{
    int d = static_cast<int>(deg + 0.5f) % 360;
    return d < 0 ? d + 360 : d;
}
}

int dayOfWeek(int year, int month, int day)
{
    // Sakamoto's method, Gregorian calendar.
    static const int kOffsets[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 3) year -= 1;
    return (year + year / 4 - year / 100 + year / 400 + kOffsets[month - 1] + day) % 7;
}

void formatTime(const gnss::DateTime &t, bool use24Hour, char *out, size_t outSize)
{
    int hour = t.hour;
    if (!use24Hour) {
        hour = t.hour % 12;
        if (hour == 0) hour = 12;
    }
    snprintf(out, outSize, "%02d:%02d", hour, t.minute);
}

void formatDate(const gnss::DateTime &t, char *out, size_t outSize)
{
    static const char *const kDays[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
    static const char *const kMonths[12] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                            "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    if (t.month < 1 || t.month > 12 || t.day < 1) {
        snprintf(out, outSize, "--- | --- --");
        return;
    }
    snprintf(out, outSize, "%s | %s %02d", kDays[dayOfWeek(t.year, t.month, t.day)], kMonths[t.month - 1],
             t.day);
}

void formatAge(uint32_t ms, char *out, size_t outSize)
{
    const uint32_t s = ms / 1000;
    if (s < 60) snprintf(out, outSize, "%lu s", static_cast<unsigned long>(s));
    else if (s < 3600) snprintf(out, outSize, "%lu min", static_cast<unsigned long>(s / 60));
    else snprintf(out, outSize, "%lu h", static_cast<unsigned long>(s / 3600));
}

void formatShortAge(uint32_t ms, char *out, size_t outSize)
{
    const uint32_t s = ms / 1000;
    if (s < 60) snprintf(out, outSize, "%luS", static_cast<unsigned long>(s));
    else if (s < 3600) snprintf(out, outSize, "%luM", static_cast<unsigned long>(s / 60));
    else snprintf(out, outSize, "%luH", static_cast<unsigned long>(s / 3600));
}

void formatBattery(int percent, char *out, size_t outSize)
{
    if (percent < 0) snprintf(out, outSize, "BAT --");
    else snprintf(out, outSize, "BAT %d%%", percent);
}

namespace {
// Nearest whole number, halves away from zero, without libm.
int roundToInt(double v) { return static_cast<int>(v >= 0 ? v + 0.5 : v - 0.5); }
} // namespace

bool watchCelsius(uint8_t reg, int &celsius)
{
    // BMA423 TEMPERATURE (0x22): two's complement, 1 K per bit, 0 = 23 C,
    // 0x80 = no valid reading. SensorLib's getTemperature() treats the byte
    // as unsigned, so anything below 23 C comes out near 280 C; this decodes
    // it correctly.
    if (reg == 0x80) return false;
    celsius = static_cast<int8_t>(reg) + 23;
    return true;
}

bool watchIsHot(int celsius)
{
    return celsius >= kWatchHotCelsius;
}

void formatWatchTemperature(bool valid, int celsius, bool metric, char *out, size_t outSize)
{
    if (!valid) snprintf(out, outSize, "--");
    else if (metric) snprintf(out, outSize, "%d\xC2\xB0" "C", celsius);
    else snprintf(out, outSize, "%d\xC2\xB0" "F", roundToInt(celsius * 9.0 / 5.0 + 32.0));
}

int batteryDotsLit(int percent, int dots)
{
    if (percent <= 0 || dots <= 0) return 0;
    if (percent >= 100) return dots;
    const int lit = (percent * dots + 50) / 100;
    return lit < 1 ? 1 : lit;  // any charge at all shows a dot
}

bool batteryIsLow(int percent)
{
    return percent >= 0 && percent <= kBatteryLowPercent;
}

void formatReconMode(const ReconState &state, char *out, size_t outSize)
{
    // The Garmin face's wording (garmin/source/ReconNames.mc modeText), so
    // every LayerTime device says the same thing.
    if (state.monitoring) snprintf(out, outSize, "RECON %s", recon::detectorShortName(state.selected));
    else if (state.earlyWarningEnabled)
        snprintf(out, outSize, "%s", state.earlyWarningResting ? "EARLY WARN  REST" : "EARLY WARN");
    else snprintf(out, outSize, "RECON OFF");
}

void formatDetections(unsigned count, char *out, size_t outSize)
{
    // garmin/source/ReconNames.mc countText.
    if (count == 1) snprintf(out, outSize, "1 DETECTION");
    else snprintf(out, outSize, "%u DETECTIONS", count);
}

void formatSolar(const solar::NextEvent &next, int offsetMinutes, bool use24Hour, char *title,
                 size_t titleSize, char *value, size_t valueSize)
{
    if (!next.known) {
        snprintf(title, titleSize, "SUN");
        snprintf(value, valueSize, "--");
        return;
    }
    snprintf(title, titleSize, "%s", next.isSunrise ? "SUNRISE" : "SUNSET");
    const gnss::DateTime local = solar::fromMinutesSinceEpoch(next.utcMinutes + offsetMinutes);
    formatTime(local, use24Hour, value, valueSize);
}

void formatGpsStatus(const NavigationState &nav, char *out, size_t outSize)
{
    if (!nav.receiverEnabled) {
        snprintf(out, outSize, "GPS OFF");
    } else if (nav.fixUsable) {
        snprintf(out, outSize, "%s", nav.fixType == FixType::Fix3D   ? "3D FIX"
                                     : nav.fixType == FixType::Fix2D ? "2D FIX"
                                                                     : "FIX");
    } else if (nav.everHadFix) {
        char age[12];
        formatShortAge(nav.fixAgeMs, age, sizeof(age));
        snprintf(out, outSize, "FIX LOST %s AGO", age);
    } else {
        snprintf(out, outSize, "ACQUIRING");
    }
}

void formatLatitude(const NavigationState &nav, char *out, size_t outSize)
{
    if (nav.fixUsable) snprintf(out, outSize, "LAT  %.6f", nav.latitudeDeg);
    else snprintf(out, outSize, "LAT  --");
}

void formatLongitude(const NavigationState &nav, char *out, size_t outSize)
{
    if (nav.fixUsable) snprintf(out, outSize, "LON  %.6f", nav.longitudeDeg);
    else snprintf(out, outSize, "LON  --");
}

void formatMgrs(const NavigationState &nav, char *out, size_t outSize)
{
    if (!nav.fixUsable) {
        snprintf(out, outSize, "WAITING FOR FIX");
        return;
    }
    const GeoGrid::UtmCoordinate utm = GeoGrid::toUtm(nav.latitudeDeg, nav.longitudeDeg);
    char mgrs[32];
    if (!GeoGrid::toMgrs(utm, mgrs, sizeof(mgrs))) {
        snprintf(out, outSize, "OUTSIDE GRID RANGE");
        return;
    }
    // GeoGrid writes zone and band on one line and easting and northing on
    // the next, for the Ultra's two-line block. This page has one line for
    // it, so the break becomes a space: "15S UA 92025 15918".
    for (char *c = mgrs; *c; ++c) {
        if (*c == '\n') *c = ' ';
    }
    snprintf(out, outSize, "%s", mgrs);
}

void formatAltitudeLine(const NavigationState &nav, bool metric, char *out, size_t outSize)
{
    if (!nav.altitudeValid) snprintf(out, outSize, metric ? "ALT  -- M" : "ALT  -- FT");
    else if (metric) snprintf(out, outSize, "ALT  %d M", static_cast<int>(nav.altitudeM));
    else snprintf(out, outSize, "ALT  %d FT", static_cast<int>(nav.altitudeM * kFeetPerMetre));
}

void formatSatellites(const NavigationState &nav, char *out, size_t outSize)
{
    snprintf(out, outSize, "SATS  %u", static_cast<unsigned>(nav.satellites));
}

void formatAccuracy(const NavigationState &nav, bool metric, char *out, size_t outSize)
{
    if (!nav.horizontalAccuracyValid) snprintf(out, outSize, metric ? "ACC  -- M" : "ACC  -- FT");
    else if (metric) snprintf(out, outSize, "ACC  %.1f M", static_cast<double>(nav.horizontalAccuracyM));
    else snprintf(out, outSize, "ACC  %.1f FT", static_cast<double>(nav.horizontalAccuracyM * kFeetPerMetre));
}

void formatSpeed(const NavigationState &nav, bool metric, char *out, size_t outSize)
{
    // Held to the same rule as DIRECTION OF TRAVEL: a watch at rest reports
    // speed noise (0.2.2 showed "SPD 1.9 MPH" on a desk), so a speed is shown
    // only while gnss::courseIsMeaningful says the watch is really moving.
    // The navigation data underneath still carries the raw speed.
    if (!moving(nav)) snprintf(out, outSize, metric ? "SPD  -- KM/H" : "SPD  -- MPH");
    else if (metric) snprintf(out, outSize, "SPD  %.1f KM/H", static_cast<double>(nav.speedMps * kKmhPerMps));
    else snprintf(out, outSize, "SPD  %.1f MPH", static_cast<double>(nav.speedMps * kMphPerMps));
}

void formatCourse(const NavigationState &nav, char *out, size_t outSize)
{
    if (moving(nav)) snprintf(out, outSize, "DIRECTION OF TRAVEL  %03d DEG", roundedDegrees(nav.courseDeg));
    else snprintf(out, outSize, "DIRECTION OF TRAVEL  --");
}

void formatEvent(const MonitorEvent &d, char *out, size_t outSize)
{
    const char *conf = recon::confidenceLabel(d.confidence);
    const char *category = recon::detectorName(d.detector);
    if (d.channel) {
        snprintf(out, outSize, "%s  [%s]\n%s\n%s  %d dBm  CH %u  x%lu\n\n", category, conf, d.detail,
                 d.sourceId, static_cast<int>(d.rssi), static_cast<unsigned>(d.channel),
                 static_cast<unsigned long>(d.count));
    } else {
        snprintf(out, outSize, "%s  [%s]\n%s\n%s  %d dBm  x%lu\n\n", category, conf, d.detail, d.sourceId,
                 static_cast<int>(d.rssi), static_cast<unsigned long>(d.count));
    }
}

} // namespace text
} // namespace twatch_s3plus
} // namespace layertime
