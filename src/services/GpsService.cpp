#include "GpsService.h"

#include <LilyGoLib.h>

namespace {
constexpr uint32_t kFreshFixAgeMs = 5000;
constexpr float kMetersToFeet = 3.280839895f;
}

void GpsService::begin(bool enabled)
{
    setEnabled(enabled);
}

void GpsService::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    instance.powerControl(POWER_GPS, enabled);
}

void GpsService::poll(WatchState &state)
{
    state.gpsEnabled = _enabled;

    if (!_enabled) {
        state.gpsFix = false;
        state.gpsSatellites = 0;
        state.gpsAltitudeValid = false;
        state.gpsHdopValid = false;
        state.gpsSpeedValid = false;
        state.gpsCourseValid = false;
        return;
    }

    // LilyGoLib's GPS class derives from TinyGPSPlus and reads the Ultra's
    // MIA-M10Q from Serial1. Poll frequently so the UART buffer cannot back up.
    instance.gps.loop(false);

    const bool locationFresh =
        instance.gps.location.isValid() &&
        instance.gps.location.age() < kFreshFixAgeMs;

    state.gpsFix = locationFresh;

    if (instance.gps.satellites.isValid()) {
        state.gpsSatellites = static_cast<uint8_t>(instance.gps.satellites.value());
    }

    if (locationFresh) {
        state.latitude = instance.gps.location.lat();
        state.longitude = instance.gps.location.lng();
    }

    const bool altitudeFresh =
        instance.gps.altitude.isValid() &&
        instance.gps.altitude.age() < kFreshFixAgeMs;

    state.gpsAltitudeValid = altitudeFresh;
    if (altitudeFresh) {
        state.altitudeFt = static_cast<float>(instance.gps.altitude.meters()) * kMetersToFeet;
    }

    state.gpsHdopValid = instance.gps.hdop.isValid();
    if (state.gpsHdopValid) {
        state.gpsHdop = static_cast<float>(instance.gps.hdop.hdop());
    }

    state.gpsSpeedValid = instance.gps.speed.isValid();
    if (state.gpsSpeedValid) {
        state.gpsSpeedMph = static_cast<float>(instance.gps.speed.mph());
    }

    state.gpsCourseValid = instance.gps.course.isValid();
    if (state.gpsCourseValid) {
        state.gpsCourseDegrees = static_cast<float>(instance.gps.course.deg());
    }
}
