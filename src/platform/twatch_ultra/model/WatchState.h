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

#pragma once

#include <stdint.h>

struct WatchState {
    int hour = 0;
    int minute = 0;
    int second = 0;

    int year = 0;
    int month = 0;
    int day = 0;
    int weekday = 0;

    uint8_t batteryPercent = 0;
    bool batteryConnected = false;

    bool bluetoothConnected = false;
    bool loraActive = false;

    // GNSS state.
    bool gpsEnabled = false;
    bool gpsFix = false;
    uint8_t gpsSatellites = 0;

    double latitude = 0.0;
    double longitude = 0.0;

    bool gpsAltitudeValid = false;
    float altitudeFt = 0.0f;

    bool gpsHdopValid = false;
    float gpsHdop = 0.0f;

    bool gpsSpeedValid = false;
    float gpsSpeedMph = 0.0f;

    bool gpsCourseValid = false;
    float gpsCourseDegrees = 0.0f;

    // Position confidence, taken from the receiver's own navigation solution
    // (UBX-NAV-PVT) rather than inferred from NMEA. HDOP above is satellite
    // geometry, not error, and the two disagree: measured on this watch,
    // HDOP swung 1.49 to 3.04 while the receiver's own estimate never left
    // 1.48 to 1.73 metres. Size confidence from these fields, never from HDOP.
    uint8_t gpsFixType = 0;      // 0 none, 1 DR only, 2 2D, 3 3D
    bool gpsEverHadFix = false;  // false until a usable fix has existed once

    // Milliseconds since the last USABLE fix. Keeps growing while the receiver
    // is blind, which is what lets a consumer show a position getting older
    // instead of showing a stale one as if it were current.
    uint32_t gpsFixAgeMs = 0;

    bool gpsHorizontalAccuracyValid = false;
    float gpsHorizontalAccuracyM = 0.0f;

    bool gpsVerticalAccuracyValid = false;
    float gpsVerticalAccuracyM = 0.0f;
};
