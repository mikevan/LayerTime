// When the S3 Plus display sleeps and wakes (lilygo-s3plus/src/ui/DisplayGate):
// dark after 15 s without input; a tap on a dark screen only wakes it and
// input comes back when that finger lifts; an alert wakes it with input at
// once.
//
// Run from lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../test -I../src -o tests_s3plus_display test_s3plus_display/test_s3plus_display.cpp ../src/ui/DisplayGate.cpp
//   ./tests_s3plus_display

#include "check.h"

#include "ui/DisplayGate.h"

using layertime::twatch_s3plus::ui::DisplayGate;
using layertime::twatch_s3plus::ui::kDisplayTimeoutMs;
using layertime::twatch_s3plus::ui::DoubleTap;
using layertime::twatch_s3plus::ui::kDoubleTapWindowMs;
using Action = DisplayGate::Action;

void starts_lit_with_input()
{
    DisplayGate g;
    CHECK_TRUE(g.lit());
    CHECK_TRUE(g.inputEnabled());
    CHECK_INT(15000, kDisplayTimeoutMs);
}

void blanks_after_15_seconds_without_input()
{
    DisplayGate g;
    CHECK_TRUE(g.update(0, false) == Action::None);
    CHECK_TRUE(g.update(kDisplayTimeoutMs - 1, false) == Action::None);
    CHECK_TRUE(g.update(kDisplayTimeoutMs, false) == Action::Blank);
    CHECK_FALSE(g.lit());
    CHECK_FALSE(g.inputEnabled());
    CHECK_TRUE(g.update(kDisplayTimeoutMs + 5000, false) == Action::None);
}

void a_tap_on_a_dark_screen_wakes_it_without_pressing_anything()
{
    DisplayGate g;
    g.update(kDisplayTimeoutMs, false);
    CHECK_TRUE(g.update(kDisplayTimeoutMs + 100, true) == Action::Wake);
    CHECK_TRUE(g.lit());
    CHECK_FALSE(g.inputEnabled()); // the waking finger presses nothing
    CHECK_TRUE(g.update(kDisplayTimeoutMs + 200, true) == Action::None);
    CHECK_FALSE(g.inputEnabled());
    CHECK_TRUE(g.update(kDisplayTimeoutMs + 300, false) == Action::Input);
    CHECK_TRUE(g.inputEnabled());
    CHECK_TRUE(g.lit());
}

void an_alert_wakes_a_dark_screen_with_input_at_once()
{
    DisplayGate g;
    g.update(kDisplayTimeoutMs, false);
    // The alert sink marks activity, so inactivity drops below the timeout.
    CHECK_TRUE(g.update(10, false) == Action::WakeAndInput);
    CHECK_TRUE(g.lit());
    CHECK_TRUE(g.inputEnabled());
}

void a_finger_held_down_does_not_leak_into_the_screens()
{
    DisplayGate g;
    g.update(kDisplayTimeoutMs, false);
    g.update(kDisplayTimeoutMs, true);
    // Activity during the hold changes nothing until release.
    CHECK_TRUE(g.update(0, true) == Action::None);
    CHECK_FALSE(g.inputEnabled());
    CHECK_TRUE(g.update(0, false) == Action::Input);
}

void after_waking_it_blanks_again_on_the_next_timeout()
{
    DisplayGate g;
    g.update(kDisplayTimeoutMs, false);
    g.update(kDisplayTimeoutMs, true);
    g.update(kDisplayTimeoutMs, false); // input back; the app marks activity
    CHECK_TRUE(g.update(1000, false) == Action::None);
    CHECK_TRUE(g.update(kDisplayTimeoutMs, false) == Action::Blank);
}

// ---- double-tap sleep --------------------------------------------------------

void two_taps_inside_the_window_are_a_double_tap()
{
    DoubleTap t;
    CHECK_INT(500, kDoubleTapWindowMs);
    CHECK_FALSE(t.tap(1000));
    CHECK_TRUE(t.tap(1000 + kDoubleTapWindowMs));  // edge of the window counts
}

void two_taps_too_far_apart_are_not()
{
    DoubleTap t;
    CHECK_FALSE(t.tap(1000));
    CHECK_FALSE(t.tap(1000 + kDoubleTapWindowMs + 1));
    // The late tap starts a new pair.
    CHECK_TRUE(t.tap(1000 + kDoubleTapWindowMs + 100));
}

void a_third_quick_tap_starts_a_new_pair()
{
    DoubleTap t;
    t.tap(1000);
    CHECK_TRUE(t.tap(1100));
    CHECK_FALSE(t.tap(1200));
    CHECK_TRUE(t.tap(1300));
}

void reset_forgets_a_half_finished_pair()
{
    DoubleTap t;
    t.tap(1000);
    t.reset();
    CHECK_FALSE(t.tap(1100));
}

void a_double_tap_blanks_at_once()
{
    DisplayGate g;
    g.sleepNow();
    CHECK_TRUE(g.update(0, false) == Action::Blank);
    CHECK_FALSE(g.lit());
    CHECK_FALSE(g.inputEnabled());
}

void an_alert_buzzes_but_does_not_light_a_chosen_dark()
{
    DisplayGate g;
    g.sleepNow();
    g.update(0, false);
    // The alert sink marks activity: inactivity is near zero, again and again.
    CHECK_TRUE(g.update(0, false) == Action::None);
    CHECK_TRUE(g.update(5, false) == Action::None);
    CHECK_FALSE(g.lit());
}

void the_double_tap_finger_must_lift_before_a_touch_wakes()
{
    DisplayGate g;
    g.sleepNow();
    g.update(0, false);
    // A touch the gate has not seen begin (the controller still reporting the
    // second tap) does not wake it...
    DisplayGate h;
    h.sleepNow();
    CHECK_TRUE(h.update(0, true) == Action::Blank);
    CHECK_TRUE(h.update(0, true) == Action::None);
    CHECK_FALSE(h.lit());
    // ...a lift and a new touch do, and that finger presses nothing.
    CHECK_TRUE(h.update(0, false) == Action::None);
    CHECK_TRUE(h.update(0, true) == Action::Wake);
    CHECK_TRUE(h.lit());
    CHECK_FALSE(h.inputEnabled());
    CHECK_TRUE(h.update(0, false) == Action::Input);
    CHECK_TRUE(h.inputEnabled());
}

void after_a_chosen_dark_the_timeout_and_alerts_work_as_before()
{
    DisplayGate g;
    g.sleepNow();
    g.update(0, false);
    g.update(0, false);
    g.update(0, true);
    g.update(0, false);  // awake, input back
    CHECK_TRUE(g.update(kDisplayTimeoutMs, false) == Action::Blank);  // timeout sleep
    CHECK_TRUE(g.update(10, false) == Action::WakeAndInput);           // an alert lights it
}

void sleep_now_is_ignored_unless_awake()
{
    DisplayGate g;
    g.update(kDisplayTimeoutMs, false);  // timed out
    g.sleepNow();
    CHECK_TRUE(g.update(10, false) == Action::WakeAndInput);  // still a timeout dark
    CHECK_TRUE(g.update(0, false) == Action::None);           // no stale request left
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(starts_lit_with_input);
    CASE(blanks_after_15_seconds_without_input);
    CASE(a_tap_on_a_dark_screen_wakes_it_without_pressing_anything);
    CASE(an_alert_wakes_a_dark_screen_with_input_at_once);
    CASE(a_finger_held_down_does_not_leak_into_the_screens);
    CASE(after_waking_it_blanks_again_on_the_next_timeout);
    CASE(two_taps_inside_the_window_are_a_double_tap);
    CASE(two_taps_too_far_apart_are_not);
    CASE(a_third_quick_tap_starts_a_new_pair);
    CASE(reset_forgets_a_half_finished_pair);
    CASE(a_double_tap_blanks_at_once);
    CASE(an_alert_buzzes_but_does_not_light_a_chosen_dark);
    CASE(the_double_tap_finger_must_lift_before_a_touch_wakes);
    CASE(after_a_chosen_dark_the_timeout_and_alerts_work_as_before);
    CASE(sleep_now_is_ignored_unless_awake);
    CHECK_SUMMARY();
}
