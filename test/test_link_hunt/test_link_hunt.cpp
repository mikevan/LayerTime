// HUNT v1 (src/core/link/LinkHunt): the HUNT_START, HUNT_STOP and GET_HUNT
// codec, HuntTracker's readings, and the answers the LayerWand gives. Also
// proves the Node dispatcher without HUNT still answers the HUNT ops with
// ERROR UnknownOp, so a Node that never bound them says so.
//
// Build command: see test/README.md, "HUNT v1".

#include "check.h"

#include <cstdint>
#include <cstring>

#include "core/link/LinkCodec.h"
#include "core/link/LinkFrames.h"
#include "core/link/LinkHunt.h"

using namespace layertime::link;
using namespace layertime::link::hunt;

void hunt_start_round_trips()
{
    HuntStart s;
    s.reqId = 9; s.targetKind = kTargetAnyVixen; s.channel = 6;
    const uint8_t addr[6] = {0x38, 0x44, 0xBE, 0xBC, 0xF4, 0x10};
    std::memcpy(s.address, addr, 6);
    uint8_t b[kMaxFrame];
    CHECK_INT(10, encodeHuntStart(s, b));
    const uint8_t expected[] = {0x08, 9, 0xF0, 0x38, 0x44, 0xBE, 0xBC, 0xF4, 0x10, 6};
    CHECK_TRUE(std::memcmp(b, expected, 10) == 0);
    HuntStart d;
    CHECK_TRUE(decodeHuntStart(b, 10, d));
    CHECK_INT(9, d.reqId);
    CHECK_INT(0xF0, d.targetKind);
    CHECK_TRUE(std::memcmp(d.address, addr, 6) == 0);
    CHECK_INT(6, d.channel);
    CHECK_FALSE(decodeHuntStart(b, 9, d));
    CHECK_FALSE(decodeHuntStart(b, 11, d));
    b[0] = 0x09;
    CHECK_FALSE(decodeHuntStart(b, 10, d));
}

void stop_and_get_are_two_bytes()
{
    uint8_t b[kMaxFrame];
    CHECK_INT(2, encodeHuntStop(4, b));
    CHECK_INT(0x09, b[0]);
    CHECK_INT(4, b[1]);
    CHECK_INT(2, encodeGetHunt(5, b));
    CHECK_INT(0x0A, b[0]);
    CHECK_INT(5, b[1]);
}

void hunt_reading_layout_is_little_endian()
{
    HuntReading r;
    r.reqId = 3; r.readings = 0x0102; r.latest = -40; r.strongest = -35; r.weakest = -60; r.mean = -45;
    r.msSinceHeard = 0x0304; r.channel = 0; r.flags = kReadingHunting;
    uint8_t b[kMaxFrame];
    CHECK_INT(14, encodeHuntReading(r, b));
    const uint8_t expected[] = {0x8A, 3, 0, 0x02, 0x01, 0xD8, 0xDD, 0xC4, 0xD3, 0x04, 0x03, 0, 0x01, 0};
    CHECK_TRUE(std::memcmp(b, expected, 14) == 0);
    HuntReading d;
    CHECK_TRUE(decodeHuntReading(b, 14, d));
    CHECK_INT(0x0102, d.readings);
    CHECK_INT(-40, d.latest);
    CHECK_INT(-35, d.strongest);
    CHECK_INT(-60, d.weakest);
    CHECK_INT(-45, d.mean);
    CHECK_INT(0x0304, d.msSinceHeard);
    CHECK_INT(1, d.flags);
    CHECK_FALSE(decodeHuntReading(b, 13, d));
}

void tracker_reports_one_window_per_take()
{
    HuntTracker t;
    t.start(1000);
    t.sighting(-50, 1010);
    t.sighting(-40, 1100);
    t.sighting(-61, 1200);
    HuntReading r = t.take(1, 1220);
    CHECK_INT(3, r.readings);
    CHECK_INT(-61, r.latest);
    CHECK_INT(-40, r.strongest);
    CHECK_INT(-61, r.weakest);
    CHECK_INT(-50, r.mean); // -151 / 3 = -50.33
    CHECK_INT(20, r.msSinceHeard);
    CHECK_INT(kReadingHunting, r.flags);
    // The next window is empty: no signal fields, time keeps counting.
    r = t.take(2, 1440);
    CHECK_INT(0, r.readings);
    CHECK_INT(kNoReading, r.latest);
    CHECK_INT(kNoReading, r.mean);
    CHECK_INT(240, r.msSinceHeard);
    CHECK_INT(3, t.totalSightings());
}

void mean_rounds_to_nearest()
{
    HuntTracker t;
    t.start(0);
    t.sighting(-50, 1);
    t.sighting(-51, 2);
    CHECK_INT(-51, t.take(0, 3).mean); // -50.5 rounds away from zero
    t.sighting(-50, 4);
    t.sighting(-50, 5);
    t.sighting(-51, 6);
    CHECK_INT(-50, t.take(0, 7).mean); // -50.33
}

void lost_after_five_seconds_heard_or_not()
{
    HuntTracker t;
    t.start(1000);
    HuntReading r = t.take(0, 5999);
    CHECK_INT(kReadingHunting, r.flags); // still acquiring
    CHECK_INT(0xFFFF, r.msSinceHeard);   // never heard
    r = t.take(0, 6000);
    CHECK_INT(kReadingHunting | kReadingLost, r.flags);
    t.sighting(-70, 6100);
    r = t.take(0, 6200);
    CHECK_INT(kReadingHunting, r.flags);
    r = t.take(0, 11100);
    CHECK_INT(kReadingHunting | kReadingLost, r.flags);
    CHECK_INT(5000, r.msSinceHeard);
}

void time_since_heard_saturates()
{
    HuntTracker t;
    t.start(0);
    t.sighting(-70, 10);
    CHECK_INT(0xFFFF, t.take(0, 10 + 70000).msSinceHeard);
}

void rssi_is_clamped_and_ignored_when_idle()
{
    HuntTracker t;
    t.sighting(-50, 1); // no hunt
    CHECK_INT(0, t.totalSightings());
    CHECK_INT(0, t.take(0, 2).flags);
    t.start(0);
    t.sighting(-200, 1);
    t.sighting(99, 2);
    HuntReading r = t.take(0, 3);
    CHECK_INT(-127, r.weakest);
    CHECK_INT(20, r.strongest);
}

void answer_starts_reads_and_stops()
{
    HuntTracker t;
    uint8_t req[kMaxFrame];
    uint8_t reply[kMaxFrame];
    HuntStart s;
    s.reqId = 1; s.targetKind = kTargetAnyVixen;
    size_t n = answer(t, req, encodeHuntStart(s, req), 100, reply);
    Result res;
    CHECK_TRUE(decodeResult(reply, n, res));
    CHECK_INT(1, res.reqId);
    CHECK_INT(0x08, res.commandType);
    CHECK_INT(kStarted, res.commandResult);
    CHECK_TRUE(t.active());

    t.sighting(-55, 150);
    n = answer(t, req, encodeGetHunt(2, req), 200, reply);
    HuntReading r;
    CHECK_TRUE(decodeHuntReading(reply, n, r));
    CHECK_INT(2, r.reqId);
    CHECK_INT(1, r.readings);
    CHECK_INT(-55, r.latest);

    n = answer(t, req, encodeHuntStop(3, req), 300, reply);
    CHECK_TRUE(decodeResult(reply, n, res));
    CHECK_INT(0x09, res.commandType);
    CHECK_FALSE(t.active());
}

void answer_refuses_other_targets_and_bad_lengths()
{
    HuntTracker t;
    uint8_t req[kMaxFrame];
    uint8_t reply[kMaxFrame];
    HuntStart s;
    s.reqId = 1; s.targetKind = 0x01;
    size_t n = answer(t, req, encodeHuntStart(s, req), 0, reply);
    Result res;
    CHECK_TRUE(decodeResult(reply, n, res));
    CHECK_INT(kUnsupportedTarget, res.commandResult);
    CHECK_FALSE(t.active());

    const uint8_t shortStart[] = {0x08, 7, 0xF0};
    n = answer(t, shortStart, sizeof(shortStart), 0, reply);
    Error e;
    CHECK_TRUE(decodeError(reply, n, e));
    CHECK_INT(7, e.reqId);
    CHECK_INT(static_cast<int>(LinkStatus::BadLength), static_cast<int>(e.status));
    const uint8_t longGet[] = {0x0A, 8, 0};
    n = answer(t, longGet, sizeof(longGet), 0, reply);
    CHECK_TRUE(decodeError(reply, n, e));
    CHECK_INT(static_cast<int>(LinkStatus::BadLength), static_cast<int>(e.status));
}

void a_node_without_hunt_answers_unknown_op()
{
    NodeIdentity node;
    uint8_t req[kMaxFrame];
    uint8_t reply[kMaxFrame];
    const uint8_t ops[] = {kOpHuntStart, kOpHuntStop, kOpGetHunt};
    for (uint8_t op : ops) {
        req[0] = op; req[1] = 5;
        std::memset(req + 2, 0, 8);
        const size_t len = op == kOpHuntStart ? kHuntStartSize : 2;
        const size_t n = dispatch(node, 0, req, len, reply);
        Error e;
        CHECK_TRUE(decodeError(reply, n, e));
        CHECK_INT(static_cast<int>(LinkStatus::UnknownOp), static_cast<int>(e.status));
    }
}

void hunt_ops_are_recognised()
{
    CHECK_TRUE(isHuntOp(0x08));
    CHECK_TRUE(isHuntOp(0x09));
    CHECK_TRUE(isHuntOp(0x0A));
    CHECK_FALSE(isHuntOp(0x07));
    CHECK_FALSE(isHuntOp(0x0B));
    CHECK_FALSE(isHuntOp(0x03));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(hunt_start_round_trips);
    CASE(stop_and_get_are_two_bytes);
    CASE(hunt_reading_layout_is_little_endian);
    CASE(tracker_reports_one_window_per_take);
    CASE(mean_rounds_to_nearest);
    CASE(lost_after_five_seconds_heard_or_not);
    CASE(time_since_heard_saturates);
    CASE(rssi_is_clamped_and_ignored_when_idle);
    CASE(answer_starts_reads_and_stops);
    CASE(answer_refuses_other_targets_and_bad_lengths);
    CASE(a_node_without_hunt_answers_unknown_op);
    CASE(hunt_ops_are_recognised);
    CHECK_SUMMARY();
}
