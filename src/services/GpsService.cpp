#include "GpsService.h"

#include <Arduino.h>
#include <LilyGoLib.h>

namespace {
constexpr uint32_t kFreshFixAgeMs = 5000;
constexpr float kMetersToFeet = 3.280839895f;
}

void GpsService::begin(bool enabled)
{
    // Unlike setEnabled(), always run the enable/disable path once at boot -
    // _enabled already defaults to true (matching gpsEnabled's default), so
    // routing through setEnabled() here would silently no-op on the common
    // case and skip the GGA-enable command below.
    _enabled = enabled;
    applyEnabled(enabled);
}

void GpsService::setEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;
    applyEnabled(enabled);
}

void GpsService::applyEnabled(bool enabled)
{
    instance.powerControl(POWER_GPS, enabled);

    if (enabled) {
        // The MIA-M10Q doesn't reliably come up with GGA enabled on its
        // NMEA output set. GGA is the only one of the standard sentences
        // that carries altitude - lat/long/speed/course all come through
        // RMC too, so a fix can look otherwise healthy (location valid,
        // satellites counting) while altitude simply never populates.
        // Explicitly (re)enable GGA on UART1 every time the rail powers up,
        // rather than depending on whatever the module happened to boot
        // into. $PUBX,40,GGA,<ddc,us1,us2,usb,spi rates>*checksum - us1=1
        // means "output every fix" on the UART1 port LayerTime uses.
        delay(300); // Let the module finish booting before it'll ack commands.
        Serial1.print("$PUBX,40,GGA,0,1,0,0,0,0*5A\r\n");
    }
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
