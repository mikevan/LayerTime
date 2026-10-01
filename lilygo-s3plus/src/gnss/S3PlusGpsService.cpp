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

#include "S3PlusGpsService.h"

#include <Arduino.h>
#include <LilyGoLib.h>

namespace layertime {
namespace twatch_s3plus {

namespace {
// One NAV-PVT poll per second: the receiver's default navigation rate is 1 Hz.
constexpr uint32_t kPollIntervalMs = 1000;
}

void S3PlusGpsService::setEnabled(bool enabled)
{
    _enabled = enabled;
    instance.powerControl(POWER_GPS, enabled);
    if (enabled) return;
    // Rail off: nothing known about the fix can be vouched for any more.
    _ubx.reset();
    _lastPvt = ubx::NavPvt{};
    _lastUsablePvt = ubx::NavPvt{};
    _havePvt = false;
    _lastPvtMs = 0;
    _everHadFix = false;
    _lastUsableFixMs = 0;
    _lastPollMs = 0;
    while (Serial1.available()) Serial1.read();  // whatever it sent last
}

void S3PlusGpsService::poll(uint32_t nowMs)
{
    if (!_enabled) return;
    pumpSerial(nowMs);
    pollIfDue(nowMs);
}

void S3PlusGpsService::pumpSerial(uint32_t nowMs)
{
    while (Serial1.available()) {
        const uint8_t byte = static_cast<uint8_t>(Serial1.read());
        // NMEA traffic is dropped here: feed() only completes on a
        // checksum-valid UBX frame.
        if (!_ubx.feed(byte)) continue;
        if (_ubx.messageClass() != ubx::kClassNav || _ubx.messageId() != ubx::kIdNavPvt) continue;

        ubx::NavPvt pvt;
        if (!ubx::decodeNavPvt(_ubx.payload(), _ubx.payloadLength(), pvt)) continue;

        _lastPvt = pvt;
        _havePvt = true;
        _lastPvtMs = nowMs;
        if (gnss::pvtIsUsablePosition(pvt.fixOk, pvt.fixType)) {
            _lastUsablePvt = pvt;
            _lastUsableFixMs = nowMs;
            _everHadFix = true;
        }
    }
}

void S3PlusGpsService::pollIfDue(uint32_t nowMs)
{
    if (nowMs - _lastPollMs < kPollIntervalMs) return;
    _lastPollMs = nowMs;
    uint8_t frame[8];
    const size_t written = ubx::buildPoll(ubx::kClassNav, ubx::kIdNavPvt, frame, sizeof(frame));
    if (written > 0) Serial1.write(frame, written);
}

} // namespace twatch_s3plus
} // namespace layertime
