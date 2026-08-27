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

    // Reserved/placeholder heading until a real heading source is wired.
    float headingDegrees = 0.0f;

    // Weather received from the BLE phone bridge.
    bool weatherValid = false;
    float temperatureC = 0.0f;
};
