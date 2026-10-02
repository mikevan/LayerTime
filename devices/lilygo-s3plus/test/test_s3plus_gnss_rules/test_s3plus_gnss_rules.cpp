// The S3 Plus GNSS decisions (devices/lilygo-s3plus/src/gnss/GnssRules): when a fix is a
// usable position and how long it stays current, the date arithmetic, the
// time-zone offset, and when GNSS UTC may set the clock.
//
// Run from devices/lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_gnss_rules test_s3plus_gnss_rules/test_s3plus_gnss_rules.cpp ../src/gnss/GnssRules.cpp
//   ./tests_s3plus_gnss_rules

#include "check.h"

#include "gnss/GnssRules.h"

using namespace layertime::twatch_s3plus::gnss;

namespace {
DateTime dt(int y, int mo, int d, int h, int mi, int s)
{
    DateTime t;
    t.year = y;
    t.month = mo;
    t.day = d;
    t.hour = h;
    t.minute = mi;
    t.second = s;
    return t;
}

bool same(const DateTime &a, const DateTime &b)
{
    return a.year == b.year && a.month == b.month && a.day == b.day && a.hour == b.hour &&
           a.minute == b.minute && a.second == b.second;
}
}

void usable_position_needs_fixok_and_a_position_fix_type()
{
    CHECK_TRUE(pvtIsUsablePosition(true, 2));
    CHECK_TRUE(pvtIsUsablePosition(true, 3));
    CHECK_TRUE(pvtIsUsablePosition(true, 4));
    CHECK_FALSE(pvtIsUsablePosition(true, 0));
    CHECK_FALSE(pvtIsUsablePosition(true, 1)); // dead reckoning only
    CHECK_FALSE(pvtIsUsablePosition(true, 5)); // time only: no position
    CHECK_FALSE(pvtIsUsablePosition(true, 6));
    CHECK_FALSE(pvtIsUsablePosition(false, 3)); // receiver says not usable
}

void a_position_goes_stale_by_age_not_by_coordinates()
{
    CHECK_FALSE(positionIsCurrent(false, 0)); // never had one
    CHECK_TRUE(positionIsCurrent(true, 0));
    CHECK_TRUE(positionIsCurrent(true, kFreshFixMs - 1));
    CHECK_FALSE(positionIsCurrent(true, kFreshFixMs));
    CHECK_FALSE(positionIsCurrent(true, 3600000));
    CHECK_INT(5000, kFreshFixMs);
}

void course_counts_only_when_really_moving()
{
    // Walking pace, good accuracies: real travel.
    CHECK_TRUE(courseIsMeaningful(1.4f, 0.3f, 8.0f));
    // A watch on a desk: speed just over 0.5 mph, but no better than its own
    // accuracy, and the heading accuracy is huge.
    CHECK_FALSE(courseIsMeaningful(0.3f, 0.4f, 120.0f));
    // Over the speed bar but inside twice its accuracy.
    CHECK_FALSE(courseIsMeaningful(0.5f, 0.3f, 10.0f));
    CHECK_TRUE(courseIsMeaningful(0.6f, 0.3f, 10.0f));
    // Heading accuracy right at and past the 30 degree limit.
    CHECK_TRUE(courseIsMeaningful(2.0f, 0.2f, 30.0f));
    CHECK_FALSE(courseIsMeaningful(2.0f, 0.2f, 30.5f));
    // Under 0.5 mph is never travel, however good the estimates.
    CHECK_FALSE(courseIsMeaningful(0.2f, 0.01f, 1.0f));
    CHECK_NEAR(0.22352, kMinTravelMps, 1e-5);
}

void leap_years_and_month_lengths()
{
    CHECK_TRUE(isLeapYear(2024));
    CHECK_FALSE(isLeapYear(2026));
    CHECK_FALSE(isLeapYear(2100));
    CHECK_TRUE(isLeapYear(2000));
    CHECK_INT(29, daysInMonth(2028, 2));
    CHECK_INT(28, daysInMonth(2026, 2));
    CHECK_INT(31, daysInMonth(2026, 1));
    CHECK_INT(30, daysInMonth(2026, 4));
    CHECK_INT(31, daysInMonth(2026, 12));
    CHECK_INT(0, daysInMonth(2026, 0));
    CHECK_INT(0, daysInMonth(2026, 13));
}

void datetime_validity_rejects_out_of_range_fields()
{
    CHECK_TRUE(dateTimeIsValid(dt(2026, 10, 1, 14, 10, 3)));
    CHECK_TRUE(dateTimeIsValid(dt(2028, 2, 29, 23, 59, 59)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 2, 29, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2023, 12, 31, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2100, 1, 1, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 0, 1, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 13, 1, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 4, 31, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 1, 0, 0, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 1, 1, 24, 0, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 1, 1, 0, 60, 0)));
    CHECK_FALSE(dateTimeIsValid(dt(2026, 1, 1, 0, 0, 60))); // leap second refused
    CHECK_FALSE(dateTimeIsValid(dt(2026, 1, 1, -1, 0, 0)));
}

void add_minutes_carries_through_days_months_and_years()
{
    CHECK_TRUE(same(dt(2026, 10, 1, 9, 10, 3), addMinutes(dt(2026, 10, 1, 14, 10, 3), -300)));
    CHECK_TRUE(same(dt(2026, 9, 30, 21, 30, 0), addMinutes(dt(2026, 10, 1, 2, 30, 0), -300)));
    CHECK_TRUE(same(dt(2027, 1, 1, 4, 0, 0), addMinutes(dt(2026, 12, 31, 20, 0, 0), 480)));
    CHECK_TRUE(same(dt(2026, 12, 31, 19, 0, 0), addMinutes(dt(2027, 1, 1, 0, 0, 0), -300)));
    CHECK_TRUE(same(dt(2028, 2, 29, 1, 0, 0), addMinutes(dt(2028, 2, 28, 23, 0, 0), 120)));
    CHECK_TRUE(same(dt(2026, 3, 1, 1, 0, 0), addMinutes(dt(2026, 2, 28, 23, 0, 0), 120)));
    CHECK_TRUE(same(dt(2028, 2, 29, 22, 0, 0), addMinutes(dt(2028, 3, 1, 1, 0, 0), -180)));
    CHECK_TRUE(same(dt(2026, 10, 1, 14, 10, 3), addMinutes(dt(2026, 10, 1, 14, 10, 3), 0)));
    CHECK_TRUE(same(dt(2026, 10, 2, 5, 55, 3), addMinutes(dt(2026, 10, 1, 14, 10, 3), 945)));
    CHECK_TRUE(same(dt(2026, 10, 3, 14, 10, 3), addMinutes(dt(2026, 10, 1, 14, 10, 3), 2 * 24 * 60)));
    // Seconds never move.
    CHECK_INT(59, addMinutes(dt(2026, 10, 1, 14, 10, 59), 1).second);
}

void offsets_are_clamped_to_the_real_range_on_15_minute_steps()
{
    CHECK_INT(-720, kMinOffsetMinutes);
    CHECK_INT(840, kMaxOffsetMinutes);
    CHECK_INT(0, clampOffset(0));
    CHECK_INT(-300, clampOffset(-300));
    CHECK_INT(345, clampOffset(345));  // Nepal, +05:45
    CHECK_INT(-720, clampOffset(-9999));
    CHECK_INT(840, clampOffset(9999));
    CHECK_INT(-300, clampOffset(-307)); // onto a step, toward zero
    CHECK_INT(300, clampOffset(307));
}

void stepping_moves_15_minutes_and_stops_at_the_ends()
{
    CHECK_INT(15, stepOffset(0, 1));
    CHECK_INT(-15, stepOffset(0, -1));
    CHECK_INT(-285, stepOffset(-300, 1));
    CHECK_INT(840, stepOffset(840, 1));
    CHECK_INT(-720, stepOffset(-720, -1));
    CHECK_INT(0, stepOffset(0, 0));
}

void offsets_format_as_utc_plus_or_minus_hours_and_minutes()
{
    char text[16];
    formatOffset(-300, text, sizeof(text));
    CHECK_STR("UTC-05:00", text);
    formatOffset(0, text, sizeof(text));
    CHECK_STR("UTC+00:00", text);
    formatOffset(345, text, sizeof(text));
    CHECK_STR("UTC+05:45", text);
    formatOffset(-570, text, sizeof(text));
    CHECK_STR("UTC-09:30", text);
    formatOffset(840, text, sizeof(text));
    CHECK_STR("UTC+14:00", text);
    formatOffset(-720, text, sizeof(text));
    CHECK_STR("UTC-12:00", text);
}

void utc_sets_the_clock_only_when_all_three_flags_and_the_fields_hold()
{
    const DateTime good = dt(2026, 10, 1, 14, 10, 3);
    CHECK_TRUE(utcIsTrustworthy(true, true, true, good));
    CHECK_FALSE(utcIsTrustworthy(false, true, true, good));
    CHECK_FALSE(utcIsTrustworthy(true, false, true, good));
    CHECK_FALSE(utcIsTrustworthy(true, true, false, good));
    // A receiver that has not resolved time yet reports its build date.
    CHECK_FALSE(utcIsTrustworthy(true, true, true, dt(2018, 1, 1, 0, 0, 0)));
    CHECK_FALSE(utcIsTrustworthy(true, true, true, dt(2026, 2, 30, 0, 0, 0)));
}

void the_clock_sets_at_boot_then_hourly()
{
    CHECK_TRUE(clockSetDue(false, 0));
    CHECK_TRUE(clockSetDue(false, 12345));
    CHECK_FALSE(clockSetDue(true, 0));
    CHECK_FALSE(clockSetDue(true, kClockResyncMs - 1));
    CHECK_TRUE(clockSetDue(true, kClockResyncMs));
    CHECK_INT(3600000, kClockResyncMs);
}

// ---- DATE / TIME page stepping -----------------------------------------------

void day_steps_wrap_within_the_month()
{
    CHECK_TRUE(same(stepDateField(dt(2026, 2, 28, 12, 30, 45), DateField::Day, 1), dt(2026, 2, 1, 12, 30, 0)));
    CHECK_TRUE(same(stepDateField(dt(2028, 2, 28, 12, 30, 45), DateField::Day, 1), dt(2028, 2, 29, 12, 30, 0)));
    CHECK_TRUE(same(stepDateField(dt(2026, 10, 1, 12, 30, 45), DateField::Day, -1), dt(2026, 10, 31, 12, 30, 0)));
}

void month_steps_wrap_and_keep_the_day_in_the_month()
{
    CHECK_TRUE(same(stepDateField(dt(2026, 12, 15, 12, 30, 45), DateField::Month, 1), dt(2026, 1, 15, 12, 30, 0)));
    CHECK_TRUE(same(stepDateField(dt(2026, 1, 15, 12, 30, 45), DateField::Month, -1), dt(2026, 12, 15, 12, 30, 0)));
    // 31 JAN, MONTH +: the last day of February, not 1 FEB as on the Ultra.
    CHECK_TRUE(same(stepDateField(dt(2026, 1, 31, 12, 30, 45), DateField::Month, 1), dt(2026, 2, 28, 12, 30, 0)));
    CHECK_TRUE(same(stepDateField(dt(2028, 1, 31, 12, 30, 45), DateField::Month, 1), dt(2028, 2, 29, 12, 30, 0)));
}

void year_steps_stop_at_the_valid_range()
{
    CHECK_TRUE(same(stepDateField(dt(2024, 6, 1, 12, 30, 45), DateField::Year, -1), dt(2024, 6, 1, 12, 30, 0)));
    CHECK_TRUE(same(stepDateField(dt(2099, 6, 1, 12, 30, 45), DateField::Year, 1), dt(2099, 6, 1, 12, 30, 0)));
    // 29 FEB in a leap year, YEAR +: 28 FEB.
    CHECK_TRUE(same(stepDateField(dt(2028, 2, 29, 12, 30, 45), DateField::Year, 1), dt(2029, 2, 28, 12, 30, 0)));
}

void hour_and_minute_wrap()
{
    CHECK_TRUE(same(stepDateField(dt(2026, 6, 1, 23, 0, 45), DateField::Hour, 1), dt(2026, 6, 1, 0, 0, 0)));
    CHECK_TRUE(same(stepDateField(dt(2026, 6, 1, 0, 0, 45), DateField::Hour, -1), dt(2026, 6, 1, 23, 0, 0)));
    CHECK_TRUE(same(stepDateField(dt(2026, 6, 1, 5, 59, 45), DateField::Minute, 1), dt(2026, 6, 1, 5, 0, 0)));
    CHECK_TRUE(same(stepDateField(dt(2026, 6, 1, 5, 0, 45), DateField::Minute, -1), dt(2026, 6, 1, 5, 59, 0)));
}

void stepping_starts_from_a_sane_date()
{
    // The factory RTC can hold anything: 2000-00-00 becomes 2024-01-01 first.
    const DateTime junk = dt(2000, 0, 0, 25, 61, 45);
    const DateTime r = stepDateField(junk, DateField::Day, 1);
    CHECK_TRUE(same(r, dt(2024, 1, 2, 0, 0, 0)));
    CHECK_TRUE(dateTimeIsValid(r));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(usable_position_needs_fixok_and_a_position_fix_type);
    CASE(a_position_goes_stale_by_age_not_by_coordinates);
    CASE(course_counts_only_when_really_moving);
    CASE(leap_years_and_month_lengths);
    CASE(datetime_validity_rejects_out_of_range_fields);
    CASE(add_minutes_carries_through_days_months_and_years);
    CASE(offsets_are_clamped_to_the_real_range_on_15_minute_steps);
    CASE(stepping_moves_15_minutes_and_stops_at_the_ends);
    CASE(offsets_format_as_utc_plus_or_minus_hours_and_minutes);
    CASE(utc_sets_the_clock_only_when_all_three_flags_and_the_fields_hold);
    CASE(the_clock_sets_at_boot_then_hourly);
    CASE(day_steps_wrap_within_the_month);
    CASE(month_steps_wrap_and_keep_the_day_in_the_month);
    CASE(year_steps_stop_at_the_valid_range);
    CASE(hour_and_minute_wrap);
    CASE(stepping_starts_from_a_sane_date);
    CHECK_SUMMARY();
}
