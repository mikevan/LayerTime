// The watch face's sunrise and sunset (s3plus/source/gnss/Solar).
//
// References are the US Naval Observatory's published times (Astronomical
// Applications Department, "Sun and Moon Data for One Day", aa.usno.navy.mil,
// fetched 2026-10-01), which come from a different method than NOAA's: a
// match within a minute checks both the equations and their use here.
//
// Run from s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../test -I../source -o tests_s3plus_solar test_s3plus_solar/test_s3plus_solar.cpp ../source/gnss/Solar.cpp
//   ./tests_s3plus_solar

#include "check.h"

#include <stdlib.h>

#include "gnss/Solar.h"

using namespace layertime::twatch_s3plus;

namespace {
gnss::DateTime utc(int y, int mo, int d, int h, int mi)
{
    gnss::DateTime t;
    t.year = y; t.month = mo; t.day = d; t.hour = h; t.minute = mi; t.second = 0;
    return t;
}
// Within a minute of the reference (the method's own stated accuracy).
bool near(int64_t got, const gnss::DateTime &want)
{
    return llabs(got - solar::minutesSinceEpoch(want)) <= 1;
}
} // namespace

void epoch_minutes_round_trip()
{
    CHECK_INT(0, static_cast<int>(solar::minutesSinceEpoch(utc(2000, 1, 1, 0, 0))));
    CHECK_INT(1440 * 366, static_cast<int>(solar::minutesSinceEpoch(utc(2001, 1, 1, 0, 0))));  // 2000 was leap
    const gnss::DateTime t = solar::fromMinutesSinceEpoch(solar::minutesSinceEpoch(utc(2028, 2, 29, 23, 59)));
    CHECK_TRUE(t.year == 2028 && t.month == 2 && t.day == 29 && t.hour == 23 && t.minute == 59);
    const gnss::DateTime before = solar::fromMinutesSinceEpoch(-1);
    CHECK_TRUE(before.year == 1999 && before.month == 12 && before.day == 31 && before.hour == 23 &&
               before.minute == 59);
}

void bentonville_matches_usno()
{
    // 36.28 N, 94.20 W, 1 Oct 2026: USNO rise 07:12, set 19:00 (UTC-5).
    int64_t t = 0;
    CHECK_TRUE(solar::sunrise(2026, 10, 1, 36.28, -94.20, t));
    CHECK_TRUE(near(t, utc(2026, 10, 1, 12, 12)));
    CHECK_TRUE(solar::sunset(2026, 10, 1, 36.28, -94.20, t));
    // 19:00 local is 00:00 UTC on 2 Oct: the event belongs to the 1 Oct
    // solar day but falls on the next UTC date.
    CHECK_TRUE(near(t, utc(2026, 10, 2, 0, 0)));
}

void fairbanks_solstice_matches_usno()
{
    // 64.84 N, 147.72 W, 21 Jun 2026: USNO rise 01:58, set 23:48 (UTC-9).
    int64_t t = 0;
    CHECK_TRUE(solar::sunrise(2026, 6, 21, 64.84, -147.72, t));
    CHECK_TRUE(near(t, utc(2026, 6, 21, 10, 58)));
    CHECK_TRUE(solar::sunset(2026, 6, 21, 64.84, -147.72, t));
    CHECK_TRUE(near(t, utc(2026, 6, 22, 8, 48)));
}

void sydney_southern_summer_matches_usno()
{
    // 33.87 S, 151.21 E, 21 Dec 2026: USNO rise 04:41, set 19:05 (UTC+10).
    int64_t t = 0;
    CHECK_TRUE(solar::sunrise(2026, 12, 21, -33.87, 151.21, t));
    CHECK_TRUE(near(t, utc(2026, 12, 20, 18, 41)));
    CHECK_TRUE(solar::sunset(2026, 12, 21, -33.87, 151.21, t));
    CHECK_TRUE(near(t, utc(2026, 12, 21, 9, 5)));
}

void polar_night_and_midnight_sun_have_no_event()
{
    int64_t t = 0;
    // Utqiagvik, 71.29 N: no sunrise at the December solstice, no sunset at
    // the June solstice.
    CHECK_FALSE(solar::sunrise(2026, 12, 21, 71.29, -156.79, t));
    CHECK_FALSE(solar::sunset(2026, 12, 21, 71.29, -156.79, t));
    CHECK_FALSE(solar::sunrise(2026, 6, 21, 71.29, -156.79, t));
    CHECK_FALSE(solar::sunset(2026, 6, 21, 71.29, -156.79, t));
    const solar::NextEvent e = solar::nextEvent(71.29, -156.79, utc(2026, 12, 21, 12, 0));
    CHECK_FALSE(e.known);
}

void next_event_is_the_first_one_after_now()
{
    // Bentonville, 1 Oct 2026. Rise 12:12 UTC, set 00:00 UTC on 2 Oct.
    solar::NextEvent e = solar::nextEvent(36.28, -94.20, utc(2026, 10, 1, 6, 0));   // 01:00 local
    CHECK_TRUE(e.known && e.isSunrise && near(e.utcMinutes, utc(2026, 10, 1, 12, 12)));
    e = solar::nextEvent(36.28, -94.20, utc(2026, 10, 1, 19, 19));                  // 14:19 local
    CHECK_TRUE(e.known && !e.isSunrise && near(e.utcMinutes, utc(2026, 10, 2, 0, 0)));
    e = solar::nextEvent(36.28, -94.20, utc(2026, 10, 2, 3, 47));                   // 22:47 local
    CHECK_TRUE(e.known && e.isSunrise);
    const gnss::DateTime local = solar::fromMinutesSinceEpoch(e.utcMinutes - 5 * 60);
    // Sunrise moves about a minute a day in early October: 07:12 or 07:13.
    CHECK_TRUE(local.day == 2 && local.hour == 7 && (local.minute == 12 || local.minute == 13));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(epoch_minutes_round_trip);
    CASE(bentonville_matches_usno);
    CASE(fairbanks_solstice_matches_usno);
    CASE(sydney_southern_summer_matches_usno);
    CASE(polar_night_and_midnight_sun_have_no_event);
    CASE(next_event_is_the_first_one_after_now);
    CHECK_SUMMARY();
}
