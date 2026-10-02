// Phase 1 bring-up decisions (devices/lilygo-s3plus/src/BringUpCheck): the expected I2C
// parts, the GNSS module classification, the PSRAM check, and (0.1.2) the
// FFat repair gate: the partition check and the three-second hold, and
// (0.1.3) the storage reboot test's outcome.
//
// Run from devices/lilygo-s3plus/test/:
//   g++ -std=c++17 -O0 -Wall -Wextra -I../../../test -I../src -o tests_s3plus_bringup test_s3plus_bringup/test_s3plus_bringup.cpp ../src/BringUpCheck.cpp
//   ./tests_s3plus_bringup

#include "check.h"

#include "BringUpCheck.h"

#include <string.h>

using namespace layertime::twatch_s3plus::bringup;

void expected_parts_follow_lilygos_address_table()
{
    // LilyGoLib docs/hardware/lilygo-t-watch-s3-plus.md, "I2C Devices Address".
    CHECK_INT(4, sizeof(kMainBus) / sizeof(kMainBus[0]));
    CHECK_INT(0x19, kMainBus[0].address);
    CHECK_INT(0x34, kMainBus[1].address);
    CHECK_INT(0x51, kMainBus[2].address);
    CHECK_INT(0x5A, kMainBus[3].address);
    CHECK_INT(1, sizeof(kTouchBus) / sizeof(kTouchBus[0]));
    CHECK_INT(0x38, kTouchBus[0].address);
}

void a_full_main_bus_passes_and_one_missing_part_fails()
{
    Found f;
    for (const auto &e : kMainBus) f.add(e.address);
    CHECK_TRUE(allPresent(kMainBus, f));
    Found g;
    g.add(0x19); g.add(0x34); g.add(0x5A); // RTC missing
    CHECK_FALSE(allPresent(kMainBus, g));
}

void unexpected_addresses_are_reported_and_expected_ones_are_not()
{
    Found f;
    for (const auto &e : kMainBus) f.add(e.address);
    f.add(0x28); // a BHI260AP would answer here; the S3 Plus has none
    f.add(0x20);
    uint8_t out[8] = {0};
    CHECK_INT(2, unexpected(kMainBus, f, out, sizeof(out)));
    CHECK_INT(0x20, out[0]);
    CHECK_INT(0x28, out[1]);
    uint8_t small[1] = {0};
    CHECK_INT(1, unexpected(kMainBus, f, small, sizeof(small)));
}

void found_ignores_out_of_range_addresses()
{
    Found f;
    f.add(200);
    CHECK_FALSE(f.has(200));
    CHECK_FALSE(f.has(0x38));
}

void gnss_models_classify()
{
    CHECK_TRUE(classifyGnss("MIA-M10Q") == GnssModule::MiaM10Q);
    CHECK_TRUE(classifyGnss("LS550G") == GnssModule::Ls550g);
    CHECK_TRUE(classifyGnss("") == GnssModule::None);
    CHECK_TRUE(classifyGnss(nullptr) == GnssModule::None);
    CHECK_TRUE(classifyGnss("NEO-M8N") == GnssModule::Other);
    CHECK_STR("u-blox MIA-M10Q", gnssName(GnssModule::MiaM10Q));
    CHECK_STR("Quectel LS550G", gnssName(GnssModule::Ls550g));
    CHECK_STR("none answered", gnssName(GnssModule::None));
}

void psram_must_be_the_8_mb_part()
{
    CHECK_TRUE(psramAsExpected(true, 8u * 1024u * 1024u));
    CHECK_TRUE(psramAsExpected(true, 8u * 1024u * 1024u - 65536u)); // heap overhead
    CHECK_FALSE(psramAsExpected(false, 8u * 1024u * 1024u));
    CHECK_FALSE(psramAsExpected(true, 0));
    CHECK_FALSE(psramAsExpected(true, 2u * 1024u * 1024u));
    CHECK_FALSE(psramAsExpected(true, 16u * 1024u * 1024u));
}

void ffat_partition_must_be_exactly_the_one_in_partitions_csv()
{
    // devices/lilygo-s3plus/partitions.csv: ffat, data, fat, 0x810000, 0x7E0000.
    CHECK_STR("ffat", kFfatLabel);
    CHECK_INT(0x810000, kFfatAddress);
    CHECK_INT(0x7E0000, kFfatSize);
    CHECK_TRUE(ffatPartitionIsExpected("ffat", 0x810000u, 0x7E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected(nullptr, 0x810000u, 0x7E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("", 0x810000u, 0x7E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("FFAT", 0x810000u, 0x7E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("ffat2", 0x810000u, 0x7E0000u, 1));
    // LilyGo's other 16 MB layout puts a 9.9 MB FAT at 0x610000.
    CHECK_FALSE(ffatPartitionIsExpected("ffat", 0x610000u, 0x9E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("ffat", 0x610000u, 0x7E0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("ffat", 0x810000u, 0x7F0000u, 1));
    CHECK_FALSE(ffatPartitionIsExpected("ffat", 0x810000u, 0x7E0000u, 0));
    CHECK_FALSE(ffatPartitionIsExpected("ffat", 0x810000u, 0x7E0000u, 2));
}

void hold_fires_only_after_three_continuous_seconds()
{
    HoldGate g(3000);
    CHECK_TRUE(g.update(false, 100) == HoldGate::Event::None);
    CHECK_TRUE(g.update(true, 1000) == HoldGate::Event::Started);
    CHECK_TRUE(g.holding());
    CHECK_INT(0, g.percent(1000));
    CHECK_TRUE(g.update(true, 2500) == HoldGate::Event::None);
    CHECK_INT(50, g.percent(2500));
    CHECK_TRUE(g.update(true, 3999) == HoldGate::Event::None);
    CHECK_FALSE(g.fired());
    CHECK_TRUE(g.update(true, 4000) == HoldGate::Event::Fired);
    CHECK_TRUE(g.fired());
    CHECK_FALSE(g.holding());
}

void releasing_early_cancels_and_the_next_hold_starts_from_zero()
{
    HoldGate g(3000);
    CHECK_TRUE(g.update(true, 0) == HoldGate::Event::Started);
    CHECK_TRUE(g.update(true, 2990) == HoldGate::Event::None);
    CHECK_TRUE(g.update(false, 2995) == HoldGate::Event::Cancelled);
    CHECK_FALSE(g.holding());
    CHECK_FALSE(g.fired());
    CHECK_INT(0, g.percent(2995));
    // A new press restarts the full three seconds; time held before does not count.
    CHECK_TRUE(g.update(true, 3000) == HoldGate::Event::Started);
    CHECK_TRUE(g.update(true, 5999) == HoldGate::Event::None);
    CHECK_FALSE(g.fired());
    CHECK_TRUE(g.update(true, 6000) == HoldGate::Event::Fired);
}

void a_fired_gate_never_fires_again()
{
    HoldGate g(3000);
    g.update(true, 0);
    CHECK_TRUE(g.update(true, 3000) == HoldGate::Event::Fired);
    CHECK_TRUE(g.update(true, 6000) == HoldGate::Event::None);
    CHECK_TRUE(g.update(false, 6100) == HoldGate::Event::None);
    CHECK_TRUE(g.update(true, 6200) == HoldGate::Event::None);
    CHECK_TRUE(g.update(true, 99999) == HoldGate::Event::None);
    CHECK_FALSE(g.holding());
    CHECK_INT(0, g.percent(99999));
}

void hold_survives_the_millis_wraparound()
{
    HoldGate g(3000);
    // Pressed 1000 ms before millis() wraps; 2999 ms held is not enough, 3000 is.
    CHECK_TRUE(g.update(true, 0xFFFFFC18u) == HoldGate::Event::Started);
    CHECK_TRUE(g.update(true, 1999u) == HoldGate::Event::None);
    CHECK_INT(99, g.percent(1999u));
    CHECK_TRUE(g.update(true, 2000u) == HoldGate::Event::Fired);
}

void reboot_test_passes_only_when_every_step_succeeded()
{
    CHECK_TRUE(classifyRebootTest(true, true, true, true) == RebootTestResult::Pass);
    CHECK_TRUE(classifyRebootTest(false, false, false, false) == RebootTestResult::NotMounted);
    // Not mounted wins even if the other flags claim success.
    CHECK_TRUE(classifyRebootTest(false, true, true, true) == RebootTestResult::NotMounted);
    CHECK_TRUE(classifyRebootTest(true, false, false, true) == RebootTestResult::FileMissing);
    CHECK_TRUE(classifyRebootTest(true, false, true, true) == RebootTestResult::FileMissing);
    CHECK_TRUE(classifyRebootTest(true, true, false, true) == RebootTestResult::Mismatch);
    CHECK_TRUE(classifyRebootTest(true, true, false, false) == RebootTestResult::Mismatch);
    CHECK_TRUE(classifyRebootTest(true, true, true, false) == RebootTestResult::NotRemoved);
}

void reboot_test_results_keep_their_stored_values()
{
    // These numbers are in NVS on the watch; they must never change meaning.
    CHECK_INT(0, static_cast<int>(RebootTestResult::None));
    CHECK_INT(1, static_cast<int>(RebootTestResult::Pass));
    CHECK_INT(2, static_cast<int>(RebootTestResult::NotMounted));
    CHECK_INT(3, static_cast<int>(RebootTestResult::FileMissing));
    CHECK_INT(4, static_cast<int>(RebootTestResult::Mismatch));
    CHECK_INT(5, static_cast<int>(RebootTestResult::NotRemoved));
    CHECK_INT(6, static_cast<int>(RebootTestResult::WriteFailed));
    for (uint8_t v = 0; v <= 6; ++v) CHECK_INT(v, static_cast<int>(rebootTestFromStored(v)));
    CHECK_TRUE(rebootTestFromStored(7) == RebootTestResult::None);
    CHECK_TRUE(rebootTestFromStored(255) == RebootTestResult::None);
}

void reboot_test_text_is_a_sentence_and_says_pass_only_for_a_pass()
{
    for (uint8_t v = 0; v <= 6; ++v) {
        const char *t = rebootTestText(static_cast<RebootTestResult>(v));
        const size_t n = strlen(t);
        CHECK_TRUE(n > 0 && t[n - 1] == '.');
        const bool saysPass = strncmp(t, "PASS", 4) == 0;
        CHECK_TRUE(saysPass == (v == 1));
    }
    CHECK_STR("No storage reboot test has completed.", rebootTestText(RebootTestResult::None));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(expected_parts_follow_lilygos_address_table);
    CASE(a_full_main_bus_passes_and_one_missing_part_fails);
    CASE(unexpected_addresses_are_reported_and_expected_ones_are_not);
    CASE(found_ignores_out_of_range_addresses);
    CASE(gnss_models_classify);
    CASE(psram_must_be_the_8_mb_part);
    CASE(ffat_partition_must_be_exactly_the_one_in_partitions_csv);
    CASE(hold_fires_only_after_three_continuous_seconds);
    CASE(releasing_early_cancels_and_the_next_hold_starts_from_zero);
    CASE(a_fired_gate_never_fires_again);
    CASE(hold_survives_the_millis_wraparound);
    CASE(reboot_test_passes_only_when_every_step_succeeded);
    CASE(reboot_test_results_keep_their_stored_values);
    CASE(reboot_test_text_is_a_sentence_and_says_pass_only_for_a_pass);
    CHECK_SUMMARY();
}
