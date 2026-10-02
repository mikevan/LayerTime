// Unit tests for src/core/logic/ReconStageLog, the ring of stage timestamps
// and the interval histogram behind the Slice 1 Increment 2A no-link
// baseline on the T-Dongle-C5. The C5 keeps the ring in PSRAM and stamps
// esp_timer_get_time(); here the buffer is an array and the stamps are
// whatever the case says.

#include "check.h"

#include "core/logic/ReconStageLog.h"

using namespace layertime::recon;

void a_detached_log_counts_and_keeps_nothing()
{
    ReconStageLog log;
    log.record(Stage::WifiHop, 100, 3);
    CHECK_INT(0, log.count());
    CHECK_INT(1, log.total());
    CHECK_INT(1, log.lost());
    CHECK_INT(0, log.capacity());
    CHECK_INT(static_cast<int>(Stage::None), static_cast<int>(log.at(0).stage));
    StageRecord none[1];
    log.attach(none, 0);
    log.record(Stage::WifiHop, 100, 3);
    CHECK_INT(0, log.count());
    CHECK_INT(1, log.lost());
}

void records_come_back_oldest_first_with_every_field()
{
    StageRecord buf[8];
    ReconStageLog log;
    log.attach(buf, 8);
    log.record(Stage::WifiStart, 1000);
    log.record(Stage::WifiHop, 1650000, 2);
    log.record(Stage::Candidate, 1700000, 5, static_cast<uint16_t>(static_cast<int8_t>(-61)), 1);
    log.record(Stage::Heap, 2000000, 0, 120, 300000);
    CHECK_INT(4, log.count());
    CHECK_INT(4, log.total());
    CHECK_INT(0, log.lost());
    CHECK_INT(8, log.capacity());
    CHECK_INT(static_cast<int>(Stage::WifiStart), static_cast<int>(log.at(0).stage));
    CHECK_TRUE(log.at(0).tUs == 1000);
    CHECK_INT(static_cast<int>(Stage::WifiHop), static_cast<int>(log.at(1).stage));
    CHECK_INT(2, log.at(1).a8);
    CHECK_TRUE(log.at(1).tUs == 1650000);
    CHECK_INT(static_cast<int>(Stage::Candidate), static_cast<int>(log.at(2).stage));
    CHECK_INT(5, log.at(2).a8);
    CHECK_INT(-61, static_cast<int8_t>(log.at(2).a16));
    CHECK_INT(1, log.at(2).a32);
    CHECK_INT(120, log.at(3).a16);
    CHECK_INT(300000, log.at(3).a32);
    // Out of range reads as an empty record, never past the ring.
    CHECK_INT(static_cast<int>(Stage::None), static_cast<int>(log.at(4).stage));
}

void a_full_ring_overwrites_the_oldest_and_counts_it_as_lost()
{
    StageRecord buf[3];
    ReconStageLog log;
    log.attach(buf, 3);
    for (uint8_t i = 1; i <= 5; ++i) log.record(Stage::WifiHop, i * 1000, i);
    CHECK_INT(3, log.count());
    CHECK_INT(5, log.total());
    CHECK_INT(2, log.lost());
    CHECK_INT(3, log.at(0).a8);
    CHECK_INT(4, log.at(1).a8);
    CHECK_INT(5, log.at(2).a8);
    CHECK_TRUE(log.at(0).tUs == 3000);
    // Wraps again cleanly.
    log.record(Stage::WifiHop, 6000, 6);
    CHECK_INT(4, log.at(0).a8);
    CHECK_INT(6, log.at(2).a8);
    CHECK_INT(3, log.lost());
}

void clear_empties_the_ring_and_the_counters_but_keeps_the_buffer()
{
    StageRecord buf[4];
    ReconStageLog log;
    log.attach(buf, 4);
    for (int i = 0; i < 6; ++i) log.record(Stage::BleAdvert, i);
    log.clear();
    CHECK_INT(0, log.count());
    CHECK_INT(0, log.total());
    CHECK_INT(0, log.lost());
    CHECK_INT(4, log.capacity());
    log.record(Stage::RunStart, 7, 0, 0, 2);
    CHECK_INT(1, log.count());
    CHECK_INT(2, log.at(0).a32);
    CHECK_TRUE(log.at(0).tUs == 7);
}

void timestamps_are_64_bit()
{
    StageRecord buf[1];
    ReconStageLog log;
    log.attach(buf, 1);
    const int64_t hour = 3600LL * 1000000LL * 3; // three hours of microseconds
    log.record(Stage::RunEnd, hour);
    CHECK_TRUE(log.at(0).tUs == hour);
    CHECK_TRUE(log.at(0).tUs > 0x7FFFFFFFLL);
}

void a_record_is_sixteen_bytes()
{
    // Sizing evidence for the PSRAM ring: 65,536 records are 1 MiB.
    CHECK_INT(16, sizeof(StageRecord));
}

void the_histogram_buckets_intervals_by_upper_bound()
{
    IntervalHistogram h;
    CHECK_INT(8, IntervalHistogram::kBuckets);
    const uint32_t samples[] = {0, 5, 6, 10, 11, 20, 21, 50, 51, 100, 101, 250, 251, 1000, 1001, 4000000000u};
    for (uint32_t s : samples) h.add(s);
    CHECK_INT(16, h.count());
    for (uint8_t i = 0; i < 8; ++i) CHECK_INT(2, h.bucket(i));
    CHECK_INT(0, h.bucket(8));
    CHECK_INT(0, h.minMs());
    CHECK_INT(4000000000u, h.maxMs());
    CHECK_TRUE(h.sumMs() == 4000000000ull + 1 + 5 + 6 + 10 + 11 + 20 + 21 + 50 + 51 + 100 + 101 + 250 + 251 + 1000 + 1001 - 1);
}

void the_histogram_min_is_the_smallest_seen_not_zero_by_default()
{
    IntervalHistogram h;
    CHECK_INT(0, h.minMs());
    h.add(7);
    h.add(3);
    h.add(9);
    CHECK_INT(3, h.minMs());
    CHECK_INT(9, h.maxMs());
    CHECK_INT(3, h.count());
    CHECK_TRUE(h.sumMs() == 19);
    h.clear();
    CHECK_INT(0, h.count());
    CHECK_INT(0, h.minMs());
    CHECK_INT(0, h.maxMs());
    CHECK_TRUE(h.sumMs() == 0);
    CHECK_INT(0, h.bucket(1));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(a_detached_log_counts_and_keeps_nothing);
    CASE(records_come_back_oldest_first_with_every_field);
    CASE(a_full_ring_overwrites_the_oldest_and_counts_it_as_lost);
    CASE(clear_empties_the_ring_and_the_counters_but_keeps_the_buffer);
    CASE(timestamps_are_64_bit);
    CASE(a_record_is_sixteen_bytes);
    CASE(the_histogram_buckets_intervals_by_upper_bound);
    CASE(the_histogram_min_is_the_smallest_seen_not_zero_by_default);
    CHECK_SUMMARY();
}
