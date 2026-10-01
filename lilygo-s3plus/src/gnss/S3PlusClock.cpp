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

#include "S3PlusClock.h"

#include <Arduino.h>
#include <LilyGoLib.h>

namespace layertime {
namespace twatch_s3plus {

namespace {
// A NAV-PVT older than this is not used to set the clock: the second it
// carries would already be wrong.
constexpr uint32_t kMaxUtcAgeMs = 1500;
}

void S3PlusClock::begin(int offsetMinutes)
{
    _offsetMinutes = gnss::clampOffset(offsetMinutes);
}

void S3PlusClock::poll(const S3PlusGpsService &gps, uint32_t nowMs)
{
    if (!gps.havePvt() || nowMs - gps.lastPvtMs() > kMaxUtcAgeMs) return;
    if (!gnss::clockSetDue(_setThisBoot, nowMs - _lastSetMs)) return;

    const ubx::NavPvt &p = gps.lastPvt();
    gnss::DateTime utc;
    utc.year = p.utcYear;
    utc.month = p.utcMonth;
    utc.day = p.utcDay;
    utc.hour = p.utcHour;
    utc.minute = p.utcMinute;
    utc.second = p.utcSecond;
    if (!gnss::utcIsTrustworthy(p.validDate, p.validTime, p.fullyResolved, utc)) return;

    const gnss::DateTime local = gnss::addMinutes(utc, _offsetMinutes);
    writeLocal(local);
    _setThisBoot = true;
    _lastSetMs = nowMs;

    char offset[12];
    gnss::formatOffset(_offsetMinutes, offset, sizeof(offset));
    Serial.printf("[s3plus] clock set from GNSS: UTC %04d-%02d-%02d %02d:%02d:%02d, %s, local "
                  "%04d-%02d-%02d %02d:%02d:%02d\n",
                  utc.year, utc.month, utc.day, utc.hour, utc.minute, utc.second, offset, local.year,
                  local.month, local.day, local.hour, local.minute, local.second);
}

void S3PlusClock::setOffsetMinutes(int minutes)
{
    const int next = gnss::clampOffset(minutes);
    const int delta = next - _offsetMinutes;
    _offsetMinutes = next;
    if (delta == 0) return;
    const gnss::DateTime now = readLocal();
    // Only move a clock that holds a real date; the factory setting is moved
    // by the next GNSS set instead.
    if (gnss::dateTimeIsValid(now)) writeLocal(gnss::addMinutes(now, delta));
}

bool S3PlusClock::setLocalManually(const gnss::DateTime &local)
{
    if (!gnss::dateTimeIsValid(local)) return false;
    writeLocal(local);
    _setManually = true;
    Serial.printf("[s3plus] clock set by hand: local %04d-%02d-%02d %02d:%02d:%02d\n", local.year,
                  local.month, local.day, local.hour, local.minute, local.second);
    return true;
}

gnss::DateTime S3PlusClock::readLocal() const
{
    const RTC_DateTime t = instance.rtc.getDateTime();
    gnss::DateTime d;
    d.year = t.getYear();
    d.month = t.getMonth();
    d.day = t.getDay();
    d.hour = t.getHour();
    d.minute = t.getMinute();
    d.second = t.getSecond();
    return d;
}

void S3PlusClock::writeLocal(const gnss::DateTime &local)
{
    instance.rtc.setDateTime(static_cast<uint16_t>(local.year), static_cast<uint8_t>(local.month),
                             static_cast<uint8_t>(local.day), static_cast<uint8_t>(local.hour),
                             static_cast<uint8_t>(local.minute), static_cast<uint8_t>(local.second));
}

} // namespace twatch_s3plus
} // namespace layertime
