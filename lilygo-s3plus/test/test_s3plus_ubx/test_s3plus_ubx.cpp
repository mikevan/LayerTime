// The S3 Plus UBX parser (lilygo-s3plus/src/gnss/S3PlusUbx), held to the same
// behaviour as the T-Ultra's: these are the T-Ultra's characterization cases
// (test/test_ubx), run against the S3 Plus copy, plus cases for what the S3
// Plus adds (NAV-PVT's UTC date, time, and validity flags, ground speed, and
// heading of motion).
//
// Run from lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../test -I../src -o tests_s3plus_ubx test_s3plus_ubx/test_s3plus_ubx.cpp ../src/gnss/S3PlusUbx.cpp
//   ./tests_s3plus_ubx

#include "check.h"

#include <vector>

#include "gnss/S3PlusUbx.h"

namespace Ubx = layertime::twatch_s3plus::ubx;

namespace {

// A complete UBX frame, checksum computed the way the protocol defines it:
// 8-bit Fletcher over class, id, length, and payload.
std::vector<uint8_t> frame(uint8_t cls, uint8_t id, const std::vector<uint8_t> &payload)
{
    std::vector<uint8_t> f = {0xB5, 0x62, cls, id,
                              static_cast<uint8_t>(payload.size() & 0xFF),
                              static_cast<uint8_t>(payload.size() >> 8)};
    f.insert(f.end(), payload.begin(), payload.end());
    uint8_t a = 0, b = 0;
    for (size_t i = 2; i < f.size(); ++i) {
        a = static_cast<uint8_t>(a + f[i]);
        b = static_cast<uint8_t>(b + a);
    }
    f.push_back(a);
    f.push_back(b);
    return f;
}

// Feeds every byte; returns how many times feed() reported a complete frame.
int feedAll(Ubx::Parser &p, const std::vector<uint8_t> &bytes)
{
    int completed = 0;
    for (uint8_t b : bytes)
        if (p.feed(b)) ++completed;
    return completed;
}

void putU16(std::vector<uint8_t> &p, size_t at, uint16_t v)
{
    p[at] = static_cast<uint8_t>(v);
    p[at + 1] = static_cast<uint8_t>(v >> 8);
}

void putU32(std::vector<uint8_t> &p, size_t at, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[at + i] = static_cast<uint8_t>(v >> (8 * i));
}

// A 92-byte NAV-PVT payload with the fields LayerTime reads filled in.
std::vector<uint8_t> navPvt(uint32_t iTow, uint8_t fixType, uint8_t flags, uint8_t numSv,
                            int32_t lonE7, int32_t latE7, int32_t hMslMm,
                            uint32_t hAccMm, uint32_t vAccMm, uint16_t pDop)
{
    std::vector<uint8_t> p(92, 0);
    putU32(p, 0, iTow);
    p[20] = fixType;
    p[21] = flags;
    p[23] = numSv;
    putU32(p, 24, static_cast<uint32_t>(lonE7));
    putU32(p, 28, static_cast<uint32_t>(latE7));
    putU32(p, 36, static_cast<uint32_t>(hMslMm));
    putU32(p, 40, hAccMm);
    putU32(p, 44, vAccMm);
    putU16(p, 76, pDop);
    return p;
}

} // namespace

// ---------------------------------------------------------------- framing

void parser_accepts_a_valid_frame_once()
{
    Ubx::Parser p;
    const auto f = frame(0x01, 0x07, {1, 2, 3});
    CHECK_INT(1, feedAll(p, f));
    CHECK_INT(1, p.framesAccepted());
    CHECK_INT(0, p.framesRejected());
    CHECK_INT(0x01, p.messageClass());
    CHECK_INT(0x07, p.messageId());
    CHECK_INT(3, p.payloadLength());
    CHECK_INT(1, p.payload()[0]);
    CHECK_INT(3, p.payload()[2]);
}

void parser_reports_completion_only_on_the_last_checksum_byte()
{
    Ubx::Parser p;
    const auto f = frame(0x01, 0x07, {9});
    for (size_t i = 0; i + 1 < f.size(); ++i) CHECK_FALSE(p.feed(f[i]));
    CHECK_TRUE(p.feed(f.back()));
}

void parser_accepts_zero_length_payload()
{
    Ubx::Parser p;
    CHECK_INT(1, feedAll(p, frame(0x0A, 0x04, {})));
    CHECK_INT(0, p.payloadLength());
}

void parser_accepts_payload_at_the_128_byte_limit()
{
    Ubx::Parser p;
    CHECK_INT(1, feedAll(p, frame(0x01, 0x07, std::vector<uint8_t>(128, 0x5A))));
    CHECK_INT(128, p.payloadLength());
}

void parser_rejects_payload_over_128_bytes_at_the_length_field()
{
    Ubx::Parser p;
    // Only the header: the rejection happens as soon as the length is known.
    const std::vector<uint8_t> header = {0xB5, 0x62, 0x01, 0x07, 129, 0};
    CHECK_INT(0, feedAll(p, header));
    CHECK_INT(1, p.framesRejected());
    CHECK_INT(0, p.framesAccepted());
    // The parser is hunting for sync again, so a following valid frame lands.
    CHECK_INT(1, feedAll(p, frame(0x01, 0x07, {7})));
}

void parser_rejects_bad_checksum_and_counts_it()
{
    Ubx::Parser p;
    auto bad = frame(0x01, 0x07, {1, 2});
    bad.back() ^= 0xFF;
    CHECK_INT(0, feedAll(p, bad));
    CHECK_INT(1, p.framesRejected());
    auto badA = frame(0x01, 0x07, {1, 2});
    badA[badA.size() - 2] ^= 0x01;
    CHECK_INT(0, feedAll(p, badA));
    CHECK_INT(2, p.framesRejected());
    CHECK_INT(0, p.framesAccepted());
}

void parser_skips_leading_garbage_and_nmea()
{
    Ubx::Parser p;
    std::vector<uint8_t> stream;
    const char *nmea = "$GNGGA,,,,,,0,00,99.99,,,,,,*56\r\n";
    for (const char *c = nmea; *c; ++c) stream.push_back(static_cast<uint8_t>(*c));
    const auto f = frame(0x01, 0x07, {4, 5});
    stream.insert(stream.end(), f.begin(), f.end());
    CHECK_INT(1, feedAll(p, stream));
    CHECK_INT(0, p.framesRejected());
}

void parser_repeated_first_sync_byte_keeps_waiting_for_the_second()
{
    // B5 B5 62 ... : the second B5 is taken as a fresh first sync byte.
    Ubx::Parser p;
    auto f = frame(0x01, 0x07, {1});
    f.insert(f.begin(), 0xB5);
    CHECK_INT(1, feedAll(p, f));
}

void parser_sync_byte_followed_by_other_returns_to_hunting()
{
    Ubx::Parser p;
    auto f = frame(0x01, 0x07, {1});
    std::vector<uint8_t> s = {0xB5, 0x00};
    s.insert(s.end(), f.begin(), f.end());
    CHECK_INT(1, feedAll(p, s));
}

void parser_handles_back_to_back_frames()
{
    Ubx::Parser p;
    auto a = frame(0x01, 0x07, {1});
    auto b = frame(0x0A, 0x38, {2, 3});
    a.insert(a.end(), b.begin(), b.end());
    CHECK_INT(2, feedAll(p, a));
    CHECK_INT(0x0A, p.messageClass());
    CHECK_INT(0x38, p.messageId());
    CHECK_INT(2, p.framesAccepted());
}

void parser_reset_restarts_framing_but_keeps_counters()
{
    Ubx::Parser p;
    feedAll(p, frame(0x01, 0x07, {1}));
    auto bad = frame(0x01, 0x07, {1});
    bad.back() ^= 0xFF;
    feedAll(p, bad);
    // Half a frame, then reset: the half is abandoned.
    const auto f = frame(0x01, 0x07, {1, 2, 3});
    for (size_t i = 0; i < 5; ++i) p.feed(f[i]);
    p.reset();
    CHECK_INT(1, p.framesAccepted());
    CHECK_INT(1, p.framesRejected());
    CHECK_INT(1, feedAll(p, f));
    CHECK_INT(2, p.framesAccepted());
}

// ---------------------------------------------------------------- NAV-PVT

void navpvt_rejects_null_and_short_payloads()
{
    Ubx::NavPvt out;
    const std::vector<uint8_t> p(92, 0);
    CHECK_FALSE(Ubx::decodeNavPvt(nullptr, 92, out));
    CHECK_FALSE(Ubx::decodeNavPvt(p.data(), 91, out));
    CHECK_TRUE(Ubx::decodeNavPvt(p.data(), 92, out));
}

void navpvt_decodes_fields_at_their_offsets()
{
    // Rogers, AR: 36.2822536 N, 94.2021878 W.
    const auto p = navPvt(123456789u, 3, 0x01, 17, -942021878, 362822536, 412345, 1480, 2250, 132);
    Ubx::NavPvt out;
    CHECK_TRUE(Ubx::decodeNavPvt(p.data(), 92, out));
    CHECK_INT(123456789, out.iTowMs);
    CHECK_INT(3, out.fixType);
    CHECK_TRUE(out.fixOk);
    CHECK_INT(17, out.satellites);
    CHECK_NEAR(36.2822536, out.latitude, 1e-9);
    CHECK_NEAR(-94.2021878, out.longitude, 1e-9);
    CHECK_NEAR(412.345, out.altitudeMslM, 1e-3);
    CHECK_TRUE(out.horizontalAccuracyValid);
    CHECK_NEAR(1.480, out.horizontalAccuracyM, 1e-6);
    CHECK_TRUE(out.verticalAccuracyValid);
    CHECK_NEAR(2.250, out.verticalAccuracyM, 1e-6);
    CHECK_NEAR(1.32, out.pdop, 1e-5);
}

void navpvt_fix_ok_is_bit_zero_of_flags_only()
{
    Ubx::NavPvt out;
    auto p = navPvt(0, 3, 0xFE, 0, 0, 0, 0, 0, 0, 0);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_FALSE(out.fixOk);
    p[21] = 0x01;
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_TRUE(out.fixOk);
}

void navpvt_accuracy_sentinel_means_no_estimate()
{
    const auto p = navPvt(0, 0, 0, 0, 0, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, 0);
    Ubx::NavPvt out;
    out.horizontalAccuracyM = 99.0f;
    out.verticalAccuracyM = 99.0f;
    CHECK_TRUE(Ubx::decodeNavPvt(p.data(), 92, out));
    CHECK_FALSE(out.horizontalAccuracyValid);
    CHECK_NEAR(0.0, out.horizontalAccuracyM, 0.0);
    CHECK_FALSE(out.verticalAccuracyValid);
    CHECK_NEAR(0.0, out.verticalAccuracyM, 0.0);
}

void navpvt_largest_real_accuracy_is_still_valid()
{
    const auto p = navPvt(0, 0, 0, 0, 0, 0, 0, 0xFFFFFFFEu, 0, 0);
    Ubx::NavPvt out;
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_TRUE(out.horizontalAccuracyValid);
    CHECK_TRUE(out.verticalAccuracyValid);  // 0 mm is a value, not the sentinel
    CHECK_NEAR(0.0, out.verticalAccuracyM, 0.0);
}

void navpvt_before_first_fix_reports_zero_position()
{
    // Measured on this receiver: lat/lon 0,0 with fixType 0 and gnssFixOK 0.
    const auto p = navPvt(0, 0, 0, 0, 0, 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu, 9999);
    Ubx::NavPvt out;
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_INT(0, out.fixType);
    CHECK_FALSE(out.fixOk);
    CHECK_NEAR(0.0, out.latitude, 0.0);
    CHECK_NEAR(0.0, out.longitude, 0.0);
    CHECK_NEAR(99.99, out.pdop, 1e-3);
}

void navpvt_decodes_from_a_parsed_frame()
{
    Ubx::Parser p;
    const auto payload = navPvt(1000, 3, 1, 12, 100000000, -200000000, -5000, 3000, 4000, 250);
    // A 92-byte payload is inside the 128-byte limit, so the frame is kept.
    CHECK_INT(1, feedAll(p, frame(0x01, 0x07, payload)));
    CHECK_INT(1, p.framesAccepted());
    Ubx::NavPvt out;
    CHECK_TRUE(Ubx::decodeNavPvt(p.payload(), p.payloadLength(), out));
    CHECK_NEAR(10.0, out.longitude, 1e-9);
    CHECK_NEAR(-20.0, out.latitude, 1e-9);
    CHECK_NEAR(-5.0, out.altitudeMslM, 1e-6);
}

// ---------------------------------------------------------------- polls

void poll_builds_the_published_nav_pvt_request()
{
    // B5 62 01 07 00 00 08 19 is the standard UBX-NAV-PVT poll.
    uint8_t out[8] = {0};
    CHECK_INT(8, Ubx::buildPoll(0x01, 0x07, out, sizeof(out)));
    const uint8_t expected[8] = {0xB5, 0x62, 0x01, 0x07, 0x00, 0x00, 0x08, 0x19};
    for (int i = 0; i < 8; ++i) CHECK_INT(expected[i], out[i]);
}

void poll_refuses_a_short_or_missing_buffer()
{
    uint8_t out[7] = {0};
    CHECK_INT(0, Ubx::buildPoll(0x01, 0x07, out, sizeof(out)));
    CHECK_INT(0, Ubx::buildPoll(0x01, 0x07, nullptr, 8));
}

void poll_round_trips_through_the_parser()
{
    uint8_t out[8];
    Ubx::buildPoll(0x0A, 0x38, out, sizeof(out));
    Ubx::Parser p;
    CHECK_INT(1, feedAll(p, std::vector<uint8_t>(out, out + 8)));
    CHECK_INT(0x0A, p.messageClass());
    CHECK_INT(0x38, p.messageId());
}

// ---------------------------------------------------------------- UTC (S3 Plus)

namespace {
std::vector<uint8_t> navPvtUtc(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t minute,
                               uint8_t second, uint8_t valid)
{
    std::vector<uint8_t> p = navPvt(0, 3, 0x01, 9, 0, 0, 0, 1500, 2500, 150);
    putU16(p, 4, year);
    p[6] = month;
    p[7] = day;
    p[8] = hour;
    p[9] = minute;
    p[10] = second;
    p[11] = valid;
    return p;
}
}

void navpvt_decodes_utc_date_and_time_at_offsets_4_to_10()
{
    const auto p = navPvtUtc(2026, 10, 1, 14, 10, 3, 0x07);
    Ubx::NavPvt out;
    CHECK_TRUE(Ubx::decodeNavPvt(p.data(), static_cast<uint16_t>(p.size()), out));
    CHECK_INT(2026, out.utcYear);
    CHECK_INT(10, out.utcMonth);
    CHECK_INT(1, out.utcDay);
    CHECK_INT(14, out.utcHour);
    CHECK_INT(10, out.utcMinute);
    CHECK_INT(3, out.utcSecond);
}

void navpvt_valid_bits_are_date_time_and_fully_resolved()
{
    Ubx::NavPvt out;
    auto p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x07);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_TRUE(out.validDate);
    CHECK_TRUE(out.validTime);
    CHECK_TRUE(out.fullyResolved);
    p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x01);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_TRUE(out.validDate);
    CHECK_FALSE(out.validTime);
    CHECK_FALSE(out.fullyResolved);
    p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x02);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_FALSE(out.validDate);
    CHECK_TRUE(out.validTime);
    CHECK_FALSE(out.fullyResolved);
    p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x04);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_FALSE(out.validDate);
    CHECK_FALSE(out.validTime);
    CHECK_TRUE(out.fullyResolved);
    // Bit 3 is validMag: not one of the three.
    p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x08);
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_FALSE(out.validDate || out.validTime || out.fullyResolved);
}

void navpvt_decodes_ground_speed_and_heading_of_motion()
{
    auto p = navPvtUtc(2026, 10, 1, 0, 0, 0, 0x07);
    putU32(p, 60, 1400);                                   // gSpeed, mm/s
    putU32(p, 64, static_cast<uint32_t>(4500000));          // headMot, 1e-5 deg
    Ubx::NavPvt out;
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_NEAR(1.4, out.groundSpeedMps, 1e-6);
    CHECK_NEAR(45.0, out.headingOfMotionDeg, 1e-4);
    // Neighbours untouched: pDOP at 76 still reads.
    CHECK_NEAR(1.5, out.pdop, 1e-6);
    putU32(p, 68, 120);                                     // sAcc, mm/s
    putU32(p, 72, 1250000);                                 // headAcc, 1e-5 deg
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_NEAR(0.12, out.speedAccuracyMps, 1e-6);
    CHECK_NEAR(12.5, out.headingAccuracyDeg, 1e-4);
    putU32(p, 60, static_cast<uint32_t>(-250));             // signed field
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_NEAR(-0.25, out.groundSpeedMps, 1e-6);
}

void navpvt_utc_fields_do_not_disturb_position_fields()
{
    auto p = navPvtUtc(2099, 12, 31, 23, 59, 59, 0x07);
    Ubx::NavPvt out;
    Ubx::decodeNavPvt(p.data(), 92, out);
    CHECK_INT(3, out.fixType);
    CHECK_TRUE(out.fixOk);
    CHECK_INT(9, out.satellites);
    CHECK_NEAR(1.5, out.horizontalAccuracyM, 1e-6);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(parser_accepts_a_valid_frame_once);
    CASE(parser_reports_completion_only_on_the_last_checksum_byte);
    CASE(parser_accepts_zero_length_payload);
    CASE(parser_accepts_payload_at_the_128_byte_limit);
    CASE(parser_rejects_payload_over_128_bytes_at_the_length_field);
    CASE(parser_rejects_bad_checksum_and_counts_it);
    CASE(parser_skips_leading_garbage_and_nmea);
    CASE(parser_repeated_first_sync_byte_keeps_waiting_for_the_second);
    CASE(parser_sync_byte_followed_by_other_returns_to_hunting);
    CASE(parser_handles_back_to_back_frames);
    CASE(parser_reset_restarts_framing_but_keeps_counters);
    CASE(navpvt_rejects_null_and_short_payloads);
    CASE(navpvt_decodes_fields_at_their_offsets);
    CASE(navpvt_fix_ok_is_bit_zero_of_flags_only);
    CASE(navpvt_accuracy_sentinel_means_no_estimate);
    CASE(navpvt_largest_real_accuracy_is_still_valid);
    CASE(navpvt_before_first_fix_reports_zero_position);
    CASE(navpvt_decodes_from_a_parsed_frame);
    CASE(poll_builds_the_published_nav_pvt_request);
    CASE(poll_refuses_a_short_or_missing_buffer);
    CASE(poll_round_trips_through_the_parser);
    CASE(navpvt_decodes_utc_date_and_time_at_offsets_4_to_10);
    CASE(navpvt_valid_bits_are_date_time_and_fully_resolved);
    CASE(navpvt_decodes_ground_speed_and_heading_of_motion);
    CASE(navpvt_utc_fields_do_not_disturb_position_fields);
    CHECK_SUMMARY();
}
