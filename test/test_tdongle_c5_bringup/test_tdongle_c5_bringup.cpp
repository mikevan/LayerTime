// Host tests for the hardware-free logic behind the T-Dongle-C5 bring-up
// firmware (Slice 1, Increment 0): the APA102 frame, the button debouncer,
// the LayerWand button gestures (Increment 2B), the advertised test name, and
// the MiB report.
//
// Build command: see test/README.md, "T-Dongle-C5".

#include "check.h"

#include <cstring>

#include "platform/tdongle_c5/BringUpLogic.h"

using namespace layertime::tdongle_c5;

// --- APA102 --------------------------------------------------------------

void apa102_frame_is_start_led_end()
{
    uint8_t f[kApa102FrameBytes];
    encodeApa102(Rgb{0x11, 0x22, 0x33}, 7, f);
    CHECK_INT(12, kApa102FrameBytes);
    // A whole number of 32-bit words, so consecutive updates stay aligned.
    CHECK_INT(0, static_cast<int>(kApa102FrameBytes % 4));
    // 32-bit start frame of zeros.
    CHECK_INT(0x00, f[0]);
    CHECK_INT(0x00, f[1]);
    CHECK_INT(0x00, f[2]);
    CHECK_INT(0x00, f[3]);
    // LED frame: 111 + 5-bit brightness, then blue, green, red.
    CHECK_INT(0xE7, f[4]);
    CHECK_INT(0x33, f[5]);
    CHECK_INT(0x22, f[6]);
    CHECK_INT(0x11, f[7]);
    // Trailing 32 zero bits (SK9822 reset frame); leaves the data line low.
    CHECK_INT(0x00, f[8]);
    CHECK_INT(0x00, f[9]);
    CHECK_INT(0x00, f[10]);
    CHECK_INT(0x00, f[11]);
}

void apa102_brightness_is_clamped_to_five_bits()
{
    uint8_t f[kApa102FrameBytes];
    encodeApa102(Rgb{0, 0, 0}, 200, f);
    CHECK_INT(0xFF, f[4]);
    encodeApa102(Rgb{0, 0, 0}, 31, f);
    CHECK_INT(0xFF, f[4]);
    encodeApa102(Rgb{0, 0, 0}, 0, f);
    CHECK_INT(0xE0, f[4]);
}

void led_cycle_is_red_green_blue_repeating()
{
    const Rgb r = bringUpCycleColor(0), g = bringUpCycleColor(1), b = bringUpCycleColor(2);
    CHECK_INT(255, r.red);   CHECK_INT(0, r.green);   CHECK_INT(0, r.blue);
    CHECK_INT(0, g.red);     CHECK_INT(255, g.green); CHECK_INT(0, g.blue);
    CHECK_INT(0, b.red);     CHECK_INT(0, b.green);   CHECK_INT(255, b.blue);
    const Rgb again = bringUpCycleColor(3);
    CHECK_INT(255, again.red);
    CHECK_INT(0, again.blue);
}

// --- Button --------------------------------------------------------------

void a_clean_press_counts_once_after_the_debounce_time()
{
    ButtonDebouncer d;
    CHECK_FALSE(d.update(false, 0));
    CHECK_FALSE(d.update(true, 100));                            // edge seen, timer starts
    CHECK_FALSE(d.update(true, 100 + ButtonDebouncer::kDebounceMs - 1));
    CHECK_TRUE(d.update(true, 100 + ButtonDebouncer::kDebounceMs)); // counted once
    CHECK_TRUE(d.isPressed());
    CHECK_FALSE(d.update(true, 500));                            // held: no second count
    CHECK_FALSE(d.update(true, 5000));
}

void a_bounce_shorter_than_the_debounce_time_is_ignored()
{
    ButtonDebouncer d;
    d.update(false, 0);
    CHECK_FALSE(d.update(true, 100));
    CHECK_FALSE(d.update(false, 110)); // bounced back before 30 ms
    CHECK_FALSE(d.update(false, 200));
    CHECK_FALSE(d.isPressed());
}

void release_rearms_and_the_next_press_counts_again()
{
    ButtonDebouncer d;
    d.update(false, 0);
    d.update(true, 100);
    CHECK_TRUE(d.update(true, 130));
    CHECK_FALSE(d.update(false, 400));  // release edge
    CHECK_FALSE(d.update(false, 430));  // release becomes stable, no count
    CHECK_FALSE(d.isPressed());
    d.update(true, 1000);
    CHECK_TRUE(d.update(true, 1030));
}

void the_debouncer_survives_millis_wraparound()
{
    ButtonDebouncer d;
    const uint32_t nearWrap = 0xFFFFFFF0u;
    d.update(false, nearWrap);
    d.update(true, nearWrap);
    CHECK_TRUE(d.update(true, nearWrap + ButtonDebouncer::kDebounceMs)); // wraps past zero
}

// --- Advertised name -----------------------------------------------------

// --- Button gestures (LayerWand: short press wakes the screen, long press
// dumps the run) ------------------------------------------------------------

int g(ButtonGestures &b, bool pressed, uint32_t now) { return static_cast<int>(b.update(pressed, now)); }
const int kNone = static_cast<int>(ButtonGesture::None);
const int kShort = static_cast<int>(ButtonGesture::Short);
const int kLong = static_cast<int>(ButtonGesture::Long);

void a_short_press_reports_short_once_after_the_release_settles()
{
    ButtonGestures b;
    CHECK_INT(kNone, g(b, true, 100));
    CHECK_INT(kNone, g(b, true, 130));   // pressed and settled
    CHECK_INT(kNone, g(b, true, 400));
    CHECK_INT(kNone, g(b, false, 500));  // release starts
    CHECK_INT(kNone, g(b, false, 529));  // not settled yet
    CHECK_INT(kShort, g(b, false, 530)); // settled: Short
    CHECK_INT(kNone, g(b, false, 600));  // once
}

void a_long_press_reports_long_once_while_held_and_nothing_on_release()
{
    ButtonGestures b;
    CHECK_INT(kNone, g(b, true, 1000));
    CHECK_INT(kNone, g(b, true, 1030));
    CHECK_INT(kNone, g(b, true, 1000 + ButtonGestures::kLongPressMs - 1));
    CHECK_INT(kLong, g(b, true, 1000 + ButtonGestures::kLongPressMs));
    CHECK_INT(kNone, g(b, true, 5000));  // once
    CHECK_INT(kNone, g(b, false, 6000));
    CHECK_INT(kNone, g(b, false, 6100)); // no Short after a Long
    // Re-armed: the next short press counts.
    CHECK_INT(kNone, g(b, true, 7000));
    CHECK_INT(kNone, g(b, true, 7040));
    CHECK_INT(kNone, g(b, false, 7100));
    CHECK_INT(kShort, g(b, false, 7140));
}

void a_bounce_shorter_than_the_debounce_time_is_not_a_press()
{
    ButtonGestures b;
    CHECK_INT(kNone, g(b, true, 100));
    CHECK_INT(kNone, g(b, false, 110));  // released after 10 ms
    CHECK_INT(kNone, g(b, false, 200));
    CHECK_INT(kNone, g(b, false, 300));  // never reported
}

void a_release_bounce_does_not_end_the_press()
{
    ButtonGestures b;
    CHECK_INT(kNone, g(b, true, 100));
    CHECK_INT(kNone, g(b, true, 140));   // down
    CHECK_INT(kNone, g(b, false, 200));  // brief bounce
    CHECK_INT(kNone, g(b, true, 210));   // back down before settling
    CHECK_INT(kNone, g(b, true, 400));
    CHECK_INT(kNone, g(b, false, 500));
    CHECK_INT(kShort, g(b, false, 540)); // one Short for the whole press
}

void gestures_survive_millis_wraparound()
{
    ButtonGestures b;
    const uint32_t nearWrap = 0xFFFFFFF0u;
    CHECK_INT(kNone, g(b, true, nearWrap));
    CHECK_INT(kNone, g(b, true, nearWrap + 40));
    CHECK_INT(kLong, g(b, true, nearWrap + ButtonGestures::kLongPressMs)); // wraps past zero
}

void the_test_name_uses_the_last_two_mac_bytes_in_upper_case_hex()
{
    const uint8_t mac[6] = {0x7C, 0xDF, 0xA1, 0x00, 0x3F, 0xA2};
    char name[kAdvertisedNameSize];
    formatAdvertisedName(mac, name);
    CHECK_STR("LT-C5-3FA2", name);
    CHECK_INT(10, static_cast<int>(strlen(name)));
    CHECK_INT(11, kAdvertisedNameSize);
}

void the_test_name_keeps_leading_zeros()
{
    const uint8_t mac[6] = {0, 0, 0, 0, 0x00, 0x0B};
    char name[kAdvertisedNameSize];
    formatAdvertisedName(mac, name);
    CHECK_STR("LT-C5-000B", name);
}

// --- Units ---------------------------------------------------------------

void whole_mib_rounds_down()
{
    CHECK_INT(8, wholeMiB(8u * 1024u * 1024u));
    CHECK_INT(7, wholeMiB(8u * 1024u * 1024u - 1));
    CHECK_INT(0, wholeMiB(0));
    CHECK_INT(16, wholeMiB(16u * 1024u * 1024u));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(apa102_frame_is_start_led_end);
    CASE(apa102_brightness_is_clamped_to_five_bits);
    CASE(led_cycle_is_red_green_blue_repeating);
    CASE(a_clean_press_counts_once_after_the_debounce_time);
    CASE(a_bounce_shorter_than_the_debounce_time_is_ignored);
    CASE(release_rearms_and_the_next_press_counts_again);
    CASE(the_debouncer_survives_millis_wraparound);
    CASE(a_short_press_reports_short_once_after_the_release_settles);
    CASE(a_long_press_reports_long_once_while_held_and_nothing_on_release);
    CASE(a_bounce_shorter_than_the_debounce_time_is_not_a_press);
    CASE(a_release_bounce_does_not_end_the_press);
    CASE(gestures_survive_millis_wraparound);
    CASE(the_test_name_uses_the_last_two_mac_bytes_in_upper_case_hex);
    CASE(the_test_name_keeps_leading_zeros);
    CASE(whole_mib_rounds_down);
    CHECK_SUMMARY();
}
