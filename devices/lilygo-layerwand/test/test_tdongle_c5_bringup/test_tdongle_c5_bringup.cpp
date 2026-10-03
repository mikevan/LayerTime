// Host tests for the hardware-free logic behind the T-Dongle-C5 bring-up
// firmware (Slice 1, Increment 0): the APA102 frame, the button debouncer,
// the LayerWand button gestures (Increment 2B), the advertised test name, the
// MiB report, and the LayerWand screen: the owl's eye and lens indicators, the
// event totals by band, and the count of other LayerWands nearby.
//
// Build command: see devices/lilygo-layerwand/README.md, "Host tests".

#include "check.h"

#include <cstring>

#include "BringUpLogic.h"

using namespace layertime::tdongle_c5;
using layertime::Band;
using layertime::SourceKind;

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

// --- LayerWand screen: owl indicators ------------------------------------------

uint16_t to565(uint32_t r8, uint32_t g8, uint32_t b8)
{
    return static_cast<uint16_t>((((r8 * 31u + 127u) / 255u) << 11) | (((g8 * 63u + 127u) / 255u) << 5) |
                                 ((b8 * 31u + 127u) / 255u));
}
uint32_t red8(uint16_t p) { return ((p >> 11) & 0x1F) * 255u / 31u; }
uint32_t green8(uint16_t p) { return ((p >> 5) & 0x3F) * 255u / 63u; }
uint32_t blue8(uint16_t p) { return (p & 0x1F) * 255u / 31u; }

void the_eye_follows_the_link_and_the_lens_the_mesh_role()
{
    CHECK_TRUE(linkIndicatorColor(LinkIndicator::NoWatch) == IndicatorColor::Red);
    CHECK_TRUE(linkIndicatorColor(LinkIndicator::Connected) == IndicatorColor::Green);
    CHECK_TRUE(meshIndicatorColor(MeshIndicator::NotConnected) == IndicatorColor::Red);
    CHECK_TRUE(meshIndicatorColor(MeshIndicator::Node) == IndicatorColor::Blue);
    CHECK_TRUE(meshIndicatorColor(MeshIndicator::Master) == IndicatorColor::Green);
}

void only_the_owls_green_counts_as_green()
{
    CHECK_TRUE(isGreenPixel(to565(123, 222, 16)));  // the owl's green
    CHECK_TRUE(isGreenPixel(to565(0, 20, 0)));      // a dim anti-aliased edge
    CHECK_FALSE(isGreenPixel(to565(255, 182, 32))); // the owl's red
    CHECK_FALSE(isGreenPixel(0));                   // black
    CHECK_FALSE(isGreenPixel(0xFFFF));              // white
}

void a_red_eye_is_pure_red_and_keeps_its_shading()
{
    const uint16_t full = recolorGreenPixel(to565(123, 230, 16), IndicatorColor::Red);
    CHECK_NEAR(255.0, static_cast<double>(red8(full)), 9.0);
    CHECK_INT(0, green8(full));
    CHECK_INT(0, blue8(full));
    CHECK_FALSE(isGreenPixel(full));
    // Brighter than the peak is clamped to the peak.
    CHECK_INT(full, recolorGreenPixel(to565(123, 255, 16), IndicatorColor::Red));
    // Half the green level, half the red.
    const uint16_t half = recolorGreenPixel(to565(60, 115, 8), IndicatorColor::Red);
    CHECK_NEAR(128.0, static_cast<double>(red8(half)), 9.0);
    CHECK_INT(0, green8(half));
}

void a_blue_lens_is_blue()
{
    const uint16_t p = recolorGreenPixel(to565(123, 230, 16), IndicatorColor::Blue);
    CHECK_TRUE(blue8(p) > green8(p));
    CHECK_TRUE(blue8(p) > red8(p));
    CHECK_NEAR(255.0, static_cast<double>(blue8(p)), 9.0);
}

void green_and_non_green_pixels_are_left_alone()
{
    const uint16_t green = to565(123, 222, 16);
    const uint16_t red = to565(255, 182, 32);
    CHECK_INT(green, recolorGreenPixel(green, IndicatorColor::Green));
    CHECK_INT(red, recolorGreenPixel(red, IndicatorColor::Red));
    CHECK_INT(red, recolorGreenPixel(red, IndicatorColor::Blue));
    CHECK_INT(0, recolorGreenPixel(0, IndicatorColor::Red));
}

void only_green_pixels_inside_the_box_change()
{
    // 4 x 3 image, all owl green except one red pixel at row 1, column 2.
    const uint16_t green = to565(123, 222, 16);
    const uint16_t red = to565(255, 182, 32);
    uint8_t img[4 * 3 * 2];
    for (int i = 0; i < 12; ++i) {
        const uint16_t p = (i == 1 * 4 + 2) ? red : green;
        img[i * 2] = static_cast<uint8_t>(p & 0xFF); // little-endian, as LVGL stores it
        img[i * 2 + 1] = static_cast<uint8_t>(p >> 8);
    }
    recolorOwlIndicator(img, 4, PixelBox{1, 2, 1, 2}, IndicatorColor::Red);
    const uint16_t redEye = recolorGreenPixel(green, IndicatorColor::Red);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < 4; ++x) {
            const int i = y * 4 + x;
            const uint16_t p = static_cast<uint16_t>(img[i * 2] | (img[i * 2 + 1] << 8));
            const bool inside = y >= 1 && y <= 2 && x >= 1 && x <= 2;
            if (i == 1 * 4 + 2) CHECK_INT(red, p);
            else if (inside) CHECK_INT(redEye, p);
            else CHECK_INT(green, p);
        }
    }
}

void the_eye_and_lens_boxes_do_not_overlap()
{
    // Values from C5OwlImage.h, which needs LVGL and is not built here.
    const PixelBox eye{12, 22, 32, 46};
    const PixelBox lens{23, 30, 30, 46};
    CHECK_TRUE(eye.bottom < lens.top);
    CHECK_TRUE(lens.bottom < 56 && eye.right < 56 && lens.right < 56);
}

// --- LayerWand screen: events by band -------------------------------------------

void events_count_toward_their_band()
{
    CHECK_TRUE(eventBand(SourceKind::Ble, Band::Unknown, 0) == EventBand::Ble);
    CHECK_TRUE(eventBand(SourceKind::Ble, Band::Band2_4GHz, 0) == EventBand::Ble);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Band2_4GHz, 6) == EventBand::Wifi2_4GHz);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Band5GHz, 36) == EventBand::Wifi5GHz);
    // Band unknown: the channel decides.
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 1) == EventBand::Wifi2_4GHz);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 14) == EventBand::Wifi2_4GHz);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 32) == EventBand::Wifi5GHz);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 165) == EventBand::Wifi5GHz);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 0) == EventBand::Other);
    CHECK_TRUE(eventBand(SourceKind::Wifi, Band::Unknown, 20) == EventBand::Other);
    CHECK_TRUE(eventBand(SourceKind::Unknown, Band::Band2_4GHz, 6) == EventBand::Other);
    CHECK_TRUE(eventBand(SourceKind::Ieee802154, Band::Unknown, 11) == EventBand::Other);
}

void band_totals_add_up()
{
    BandTotals t;
    t.add(EventBand::Ble);
    t.add(EventBand::Ble);
    t.add(EventBand::Wifi2_4GHz);
    t.add(EventBand::Wifi5GHz);
    t.add(EventBand::Other);
    CHECK_INT(2, t.ble);
    CHECK_INT(1, t.wifi2_4GHz);
    CHECK_INT(1, t.wifi5GHz);
    CHECK_INT(1, t.other);
}

// --- LayerWand screen: other LayerWands nearby -----------------------------------

void a_peer_heard_again_counts_once()
{
    PeerSightings s;
    const uint8_t a[6] = {0xE0, 0xEC, 0xBC, 0xBE, 0x44, 0x38};
    const uint8_t b[6] = {0x10, 0xF4, 0xBC, 0xBE, 0x44, 0x38};
    CHECK_INT(0, s.count(0));
    s.saw(a, 1000);
    s.saw(a, 2000);
    CHECK_INT(1, s.count(2000));
    s.saw(b, 3000);
    CHECK_INT(2, s.count(3000));
}

void a_peer_not_heard_for_five_minutes_is_not_counted()
{
    PeerSightings s;
    const uint8_t a[6] = {1, 2, 3, 4, 5, 6};
    s.saw(a, 10000);
    CHECK_INT(1, s.count(10000 + PeerSightings::kWindowMs - 1));
    CHECK_INT(0, s.count(10000 + PeerSightings::kWindowMs));
    s.saw(a, 10000 + PeerSightings::kWindowMs + 5); // heard again
    CHECK_INT(1, s.count(10000 + PeerSightings::kWindowMs + 5));
}

void when_full_a_new_peer_takes_the_slot_heard_longest_ago()
{
    PeerSightings s;
    uint8_t addr[6] = {0, 0, 0, 0, 0, 0};
    for (uint8_t i = 0; i < PeerSightings::kCapacity; ++i) {
        addr[0] = i;
        s.saw(addr, 1000u + i); // address 0 heard first
    }
    CHECK_INT(PeerSightings::kCapacity, s.count(2000));
    addr[0] = 0;
    s.saw(addr, 2000); // address 0 heard again; now address 1 is oldest
    addr[0] = 200;
    s.saw(addr, 2001); // replaces address 1
    CHECK_INT(PeerSightings::kCapacity, s.count(2001));
    // Address 1 is gone: hearing it again replaces the next oldest (2).
    addr[0] = 1;
    s.saw(addr, 2002);
    CHECK_INT(PeerSightings::kCapacity, s.count(2002));
    // Addresses 3 and up expire first; 0, 200, and 1 remain.
    CHECK_INT(3, s.count(1000u + PeerSightings::kCapacity + PeerSightings::kWindowMs));
}

void peer_sightings_survive_millis_wraparound()
{
    PeerSightings s;
    const uint8_t a[6] = {9, 9, 9, 9, 9, 9};
    const uint32_t nearWrap = 0xFFFFFF00u;
    s.saw(a, nearWrap);
    CHECK_INT(1, s.count(nearWrap + 60000u)); // wraps past zero
    CHECK_INT(0, s.count(nearWrap + PeerSightings::kWindowMs));
}

// --- LayerWand channel plan -------------------------------------------------------

bool acceptAll(uint8_t, void *) { return true; }
bool acceptNone(uint8_t, void *) { return false; }
// The world-safe default as far as it is known: 2.4 GHz 1 to 11 only.
bool accept1To11(uint8_t ch, void *) { return ch >= 1 && ch <= 11; }
// Some 5 GHz channels and 2.4 GHz 1 to 13; counts the probes.
bool acceptSome(uint8_t ch, void *context)
{
    ++*static_cast<int *>(context);
    return ch == 36 || ch == 40 || ch == 149 || ch == 165 || (ch >= 1 && ch <= 13);
}

void the_plan_is_5_ghz_first_then_2_4_ghz_each_lowest_to_highest()
{
    uint8_t plan[64];
    int probes = 0;
    const uint8_t n = buildChannelPlan(acceptSome, &probes, plan, sizeof(plan));
    CHECK_INT(17, n);
    CHECK_INT(42, probes); // every candidate asked once
    const uint8_t expected[] = {36, 40, 149, 165, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13};
    for (uint8_t i = 0; i < 17; ++i) CHECK_INT(expected[i], plan[i]);
}

void every_candidate_accepted_gives_28_then_14_in_order()
{
    uint8_t plan[64];
    CHECK_INT(42, buildChannelPlan(acceptAll, nullptr, plan, sizeof(plan)));
    CHECK_INT(36, plan[0]);
    CHECK_INT(177, plan[27]);
    CHECK_INT(1, plan[28]);
    CHECK_INT(14, plan[41]);
    for (uint8_t i = 1; i < 28; ++i) CHECK_TRUE(plan[i] > plan[i - 1]);
    for (uint8_t i = 29; i < 42; ++i) CHECK_TRUE(plan[i] > plan[i - 1]);
}

void no_5_ghz_accepted_leaves_only_2_4_ghz_and_none_accepted_leaves_nothing()
{
    uint8_t plan[64];
    CHECK_INT(11, buildChannelPlan(accept1To11, nullptr, plan, sizeof(plan)));
    CHECK_INT(1, plan[0]);
    CHECK_INT(11, plan[10]);
    CHECK_INT(0, buildChannelPlan(acceptNone, nullptr, plan, sizeof(plan)));
}

void the_plan_stops_at_capacity()
{
    uint8_t plan[5] = {0, 0, 0, 0, 0};
    CHECK_INT(5, buildChannelPlan(acceptAll, nullptr, plan, 5));
    CHECK_INT(52, plan[4]);
    // The scheduler holds 48, more than the 42 candidates.
    CHECK_TRUE(sizeof(kWifi5GHzCandidates) + sizeof(kWifi2_4GHzCandidates) <= 48);
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
    CASE(the_eye_follows_the_link_and_the_lens_the_mesh_role);
    CASE(only_the_owls_green_counts_as_green);
    CASE(a_red_eye_is_pure_red_and_keeps_its_shading);
    CASE(a_blue_lens_is_blue);
    CASE(green_and_non_green_pixels_are_left_alone);
    CASE(only_green_pixels_inside_the_box_change);
    CASE(the_eye_and_lens_boxes_do_not_overlap);
    CASE(events_count_toward_their_band);
    CASE(band_totals_add_up);
    CASE(a_peer_heard_again_counts_once);
    CASE(a_peer_not_heard_for_five_minutes_is_not_counted);
    CASE(when_full_a_new_peer_takes_the_slot_heard_longest_ago);
    CASE(peer_sightings_survive_millis_wraparound);
    CASE(the_plan_is_5_ghz_first_then_2_4_ghz_each_lowest_to_highest);
    CASE(every_candidate_accepted_gives_28_then_14_in_order);
    CASE(no_5_ghz_accepted_leaves_only_2_4_ghz_and_none_accepted_leaves_nothing);
    CASE(the_plan_stops_at_capacity);
    CHECK_SUMMARY();
}
