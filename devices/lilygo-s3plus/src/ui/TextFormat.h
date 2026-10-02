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

// The words the S3 Plus screens show, built from core and GNSS state. Pure,
// tested with g++ (devices/lilygo-s3plus/test/test_s3plus_text).
//
// The watch face follows the Garmin face's design (garmin/source/
// HomeView.mc): battery dots, a WATCH temperature ring and the next solar
// event, GPS and DONGLE icons, and the Recon bracket with the Garmin's own
// wording (garmin/source/ReconNames.mc). The GPS page reads as the T-Ultra's.

#include <stddef.h>
#include <stdint.h>

#include "core/model/MonitorEvent.h"
#include "core/model/NavigationState.h"
#include "core/model/ReconState.h"

#include "../gnss/GnssRules.h"
#include "../gnss/Solar.h"

namespace layertime {
namespace twatch_s3plus {
namespace text {

// 0 Sunday .. 6 Saturday.
int dayOfWeek(int year, int month, int day);

// "14:05", or "02:05" on a 12-hour clock (no AM or PM, as on the Ultra).
void formatTime(const gnss::DateTime &t, bool use24Hour, char *out, size_t outSize);
// "THU | OCT 01".
void formatDate(const gnss::DateTime &t, char *out, size_t outSize);
// "45 s", "12 min", "3 h" (the DATE / TIME page).
void formatAge(uint32_t ms, char *out, size_t outSize);
// "45S", "12M", "3H" (the GPS page's "FIX LOST 2M AGO").
void formatShortAge(uint32_t ms, char *out, size_t outSize);

// "BAT 87%".
void formatBattery(int percent, char *out, size_t outSize);
// ---- the watch face --------------------------------------------------------

// The watch's own temperature turns red at this (113 F): a common lithium-ion
// charging limit, used as the "running hot" line. A judgement, not a LilyGo
// figure.
constexpr int kWatchHotCelsius = 45;
// Battery dots and "BAT" turn red at or below this.
constexpr int kBatteryLowPercent = 20;

// Decodes the BMA423's TEMPERATURE register. False for 0x80 (no reading).
bool watchCelsius(uint8_t reg, int &celsius);
bool watchIsHot(int celsius);
// "91\u00b0F", "33\u00b0C", or "--".
void formatWatchTemperature(bool valid, int celsius, bool metric, char *out, size_t outSize);
// How many of `dots` battery dots are lit: rounded, at least one while there
// is any charge, all at 100%.
int batteryDotsLit(int percent, int dots);
bool batteryIsLow(int percent);
// The Recon bracket's mode line, the Garmin's wording: "RECON ALL", "RECON
// INTRUSION", "EARLY WARN", "EARLY WARN  REST", or "RECON OFF".
void formatReconMode(const ReconState &state, char *out, size_t outSize);
// "1 DETECTION", "3 DETECTIONS".
void formatDetections(unsigned count, char *out, size_t outSize);
// The solar ring: "SUNRISE" or "SUNSET" and the local time on the face's
// clock (12-hour without AM or PM, or 24-hour); "SUN" and "--" when unknown.
void formatSolar(const solar::NextEvent &next, int offsetMinutes, bool use24Hour, char *title,
                 size_t titleSize, char *value, size_t valueSize);

// ---- the GPS page (the Ultra's GpsScreen, scaled) ----

// "3D FIX", "2D FIX", "FIX", "FIX LOST 2M AGO", "ACQUIRING".
void formatGpsStatus(const NavigationState &nav, char *out, size_t outSize);
// "LAT  35.123456" and "LON  -94.123456" while the fix is current, else "--".
// A position that is no longer current is never shown as one.
void formatLatitude(const NavigationState &nav, char *out, size_t outSize);
void formatLongitude(const NavigationState &nav, char *out, size_t outSize);
// The MGRS grid reference on one line ("15S UA 92025 15918"; core GeoGrid
// writes it on two), "WAITING FOR FIX", or
// "OUTSIDE GRID RANGE".
void formatMgrs(const NavigationState &nav, char *out, size_t outSize);
// "ALT  1307 FT" or "ALT  398 M".
void formatAltitudeLine(const NavigationState &nav, bool metric, char *out, size_t outSize);
// "SATS  8".
void formatSatellites(const NavigationState &nav, char *out, size_t outSize);
// The receiver's own horizontal accuracy: "ACC  4.9 FT" or "ACC  1.5 M".
// Shown where the Ultra shows HDOP: this watch reads no NMEA, and HDOP is
// satellite geometry, not an error estimate.
void formatAccuracy(const NavigationState &nav, bool metric, char *out, size_t outSize);
// "SPD  3.1 MPH" or "SPD  5.0 KM/H" while the watch is moving (the same
// test as DIRECTION OF TRAVEL), otherwise "SPD  -- MPH" or "SPD  -- KM/H".
void formatSpeed(const NavigationState &nav, bool metric, char *out, size_t outSize);
// "DIRECTION OF TRAVEL  045 DEG" or "DIRECTION OF TRAVEL  --".
void formatCourse(const NavigationState &nav, char *out, size_t outSize);

// One entry of the Recon results list, as the T-Ultra prints it:
// "FLOCK  [HIGH]\n<detail>\n<source>  -67 dBm  CH 6  x3\n\n" (no channel for
// BLE).
void formatEvent(const MonitorEvent &event, char *out, size_t outSize);

} // namespace text
} // namespace twatch_s3plus
} // namespace layertime
