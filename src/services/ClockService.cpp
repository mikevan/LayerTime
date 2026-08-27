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
