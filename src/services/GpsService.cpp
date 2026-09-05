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

#include "GpsService.h"

#include <Arduino.h>
#include <LilyGoLib.h>

namespace {
constexpr uint32_t kFreshFixAgeMs = 5000;
constexpr float kMetersToFeet = 3.280839895f;
// One NAV-PVT poll per second. The receiver's default navigation rate is 1 Hz,
// so asking faster only adds traffic, not information.
constexpr uint32_t kPollIntervalMs = 1000;
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

void GpsService::resetFixTracking()
{
    _ubx.reset();
    _lastPvt = Ubx::NavPvt();
    _havePvt = false;
    _everHadFix = false;
    _lastUsableFixMs = 0;
    _lastPollMs = 0;
}

void GpsService::applyEnabled(bool enabled)
{
    instance.powerControl(POWER_GPS, enabled);

    // Powering the rail down throws away the fix. Anything remembered about it
    // is now a claim we cannot support, so it goes too.
    if (!enabled) resetFixTracking();

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
        state.gpsFixType = 0;
        state.gpsEverHadFix = false;
        state.gpsFixAgeMs = 0;
        state.gpsHorizontalAccuracyValid = false;
        state.gpsVerticalAccuracyValid = false;
        return;
    }

    // LilyGoLib's GPS class derives from TinyGPSPlus and reads the Ultra's
    // MIA-M10Q from Serial1. We read the port ourselves instead of calling
    // instance.gps.loop() so the same bytes can reach the UBX parser too.
    // Call frequently so the UART buffer cannot back up.
    pumpSerial();
    pollIfDue();

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

    // Everything below comes from UBX, and nothing above depends on it. This
    // block only ADDS information; it deliberately does not change gpsFix,
    // latitude, or longitude, so this can be verified against the previous
    // build by seeing no difference on screen.
    state.gpsFixType = _havePvt ? _lastPvt.fixType : 0;
    state.gpsEverHadFix = _everHadFix;
    state.gpsFixAgeMs = _everHadFix ? (millis() - _lastUsableFixMs) : 0;

    state.gpsHorizontalAccuracyValid = _havePvt && _lastPvt.horizontalAccuracyValid;
    if (state.gpsHorizontalAccuracyValid) {
        state.gpsHorizontalAccuracyM = _lastPvt.horizontalAccuracyM;
    }

    state.gpsVerticalAccuracyValid = _havePvt && _lastPvt.verticalAccuracyValid;
    if (state.gpsVerticalAccuracyValid) {
        state.gpsVerticalAccuracyM = _lastPvt.verticalAccuracyM;
    }
}

void GpsService::pumpSerial()
{
    while (Serial1.available()) {
        const uint8_t byte = static_cast<uint8_t>(Serial1.read());

        // NMEA path. Identical to what GPS::loop() was doing with these bytes.
        instance.gps.encode(static_cast<char>(byte));

        // UBX path. Same bytes, different protocol. feed() returns true only
        // for a frame whose checksum verified, so NMEA traffic that happens to
        // contain the sync pair is rejected rather than misread.
        if (!_ubx.feed(byte)) continue;
        if (_ubx.messageClass() != Ubx::kClassNav) continue;
        if (_ubx.messageId() != Ubx::kIdNavPvt) continue;

        Ubx::NavPvt pvt;
        if (!Ubx::decodeNavPvt(_ubx.payload(), _ubx.payloadLength(), pvt)) continue;

        _lastPvt = pvt;
        _havePvt = true;

        // Only a solution the receiver itself calls usable resets the clock.
        // A 2D or better fix with fixOk set is the bar; anything less leaves
        // the age growing, which is the honest answer.
        if (pvt.fixOk && pvt.fixType >= 2) {
            _lastUsableFixMs = millis();
            _everHadFix = true;
        }
    }
}

void GpsService::pollIfDue()
{
    const uint32_t now = millis();
    if (now - _lastPollMs < kPollIntervalMs) return;
    _lastPollMs = now;

    uint8_t frame[8];
    const size_t written = Ubx::buildPoll(Ubx::kClassNav, Ubx::kIdNavPvt, frame, sizeof(frame));
    if (written > 0) {
        Serial1.write(frame, written);
    }
}
