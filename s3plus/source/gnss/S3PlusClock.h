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

// The S3 Plus clock: the PCF8563 RTC holds local time (UTC plus the wearer's
// offset), set from GNSS UTC.
//
// The RTC is written only when the receiver marks the date valid, the time
// valid, and the time fully resolved, from a NAV-PVT received within the last
// 1.5 s; once at boot as soon as that holds, then no more than once an hour
// (gnss::clockSetDue). Until the first set, the RTC keeps whatever it had
// (the factory setting on this watch looked like UTC+8).

#include <stdint.h>

#include "GnssRules.h"
#include "S3PlusGpsService.h"

namespace layertime {
namespace twatch_s3plus {

class S3PlusClock {
public:
    void begin(int offsetMinutes);
    void poll(const S3PlusGpsService &gps, uint32_t nowMs);

    int offsetMinutes() const { return _offsetMinutes; }
    // Changes the offset and moves the RTC by the difference at once.
    void setOffsetMinutes(int minutes);

    gnss::DateTime readLocal() const;
    // The DATE / TIME page's SAVE: the wearer's local date and time, for when
    // there is no GNSS. GNSS UTC still wins: the next due GNSS set (at boot,
    // then hourly) overwrites it. Refuses an invalid date.
    bool setLocalManually(const gnss::DateTime &local);
    // The clock holds a date someone vouched for this boot: GNSS or the
    // wearer.
    bool trustedThisBoot() const { return _setThisBoot || _setManually; }
    bool setManuallyThisBoot() const { return _setManually; }

    bool setThisBoot() const { return _setThisBoot; }
    // millis() of the last set, valid when setThisBoot().
    uint32_t lastSetMs() const { return _lastSetMs; }

private:
    void writeLocal(const gnss::DateTime &local);

    int _offsetMinutes = 0;
    bool _setThisBoot = false;
    bool _setManually = false;
    uint32_t _lastSetMs = 0;
};

} // namespace twatch_s3plus
} // namespace layertime
