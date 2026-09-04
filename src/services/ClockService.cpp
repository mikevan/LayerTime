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

#include "ClockService.h"

#include <LilyGoLib.h>
#include <time.h>

void ClockService::update(WatchState &state)
{
    struct tm now = {};
    instance.rtc.getDateTime(&now);

    state.hour = now.tm_hour;
    state.minute = now.tm_min;
    state.second = now.tm_sec;

    state.year = now.tm_year + 1900;
    state.month = now.tm_mon + 1;
    state.day = now.tm_mday;
    state.weekday = now.tm_wday;
}

void ClockService::setDateTime(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second)
{
    instance.rtc.setDateTime(year, month, day, hour, minute, second);
}
