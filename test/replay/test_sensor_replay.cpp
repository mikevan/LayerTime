// Behavioral replay tests for the LayerTime BLE detection pipeline.
//
// Each case feeds recorded/synthetic observations through the REAL classifier
// and the REAL per-receiver MonitorEventLog (see replay.h) and checks the
// normalized result. Expected outcomes are derived from documented rules:
//   - the BLE signature table (core/logic/ReconSignatures.h),
//   - the MonitorEventLog contract (its header: one record per
//     (detector, sourceId); repeat updates in place and keeps the strongest
//     confidence; capacity 40, oldest dropped; clear() keeps the serial),
//   - the AlertPolicy contract (Low never alerts; a repeat never alerts, the
//     pinned KNOWN DEFECT).
// They are not copied from the implementation.

#include "check.h"
#include "replay.h"

#include <cstdio>
#include <string>

using namespace layertime;
using namespace layertime::replay;

namespace {

char g_ctx[300];
void ctx(const char *fixture, int obsIndex, uint32_t atMs)
{
    std::snprintf(g_ctx, sizeof g_ctx, "fixture=%s obs=%d t=%ums", fixture, obsIndex, (unsigned)atMs);
}
void expectInt(const char *label, long e, long a, const char *file, int line)
{
    ++check::g_checks;
    if (e != a) {
        char b[512];
        std::snprintf(b, sizeof b, "%s | %s: expected %ld, got %ld", g_ctx, label, e, a);
        check::fail(file, line, b);
    }
}
void expectTrue(const char *label, bool c, const char *file, int line)
{
    ++check::g_checks;
    if (!c) {
        char b[512];
        std::snprintf(b, sizeof b, "%s | %s: expected true, got false", g_ctx, label);
        check::fail(file, line, b);
    }
}
#define EI(label, e, a) expectInt((label), (long)(e), (long)(a), __FILE__, __LINE__)
#define ET(label, c) expectTrue((label), (c), __FILE__, __LINE__)

std::string mb(std::initializer_list<uint8_t> b) { return std::string(b.begin(), b.end()); }

const char *kAddr = "aa:bb:cc:dd:ee:ff";

} // namespace

// FEED is Tile at High confidence; a new High record raises the alert.
void valid_tile_feed_detects_high_and_alerts()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.rssi = -50; o.uuids = {0xFEED};
    ctx("tile_feed", 0, o.atMs);
    r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("count", 1, log.count());
    const MonitorEvent *e = find(log, ReconTarget::Tile, kAddr);
    ET("Tile record present", e != nullptr);
    if (e) {
        EI("confidence High", (int)Confidence::High, (int)e->confidence);
        EI("count", 1, (long)e->count);
        EI("rssi carried", -50, e->rssi);
        EI("lastSeen uptime", 1000, (long)e->lastSeen.uptimeMs);
    }
    ET("alert raised on new High record", log.alertPending());
}

// 0x3081 is Flipper at Medium; Medium still raises (only Low is suppressed).
void valid_flipper_uuid_detects_medium_and_alerts()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 500; o.uuids = {0x3081};
    ctx("flipper_3081", 0, o.atMs);
    r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("count", 1, log.count());
    const MonitorEvent *e = find(log, ReconTarget::Flipper, kAddr);
    ET("Flipper record present", e != nullptr);
    if (e) EI("confidence Medium", (int)Confidence::Medium, (int)e->confidence);
    ET("alert raised on new Medium record", log.alertPending());
}

// An Apple manufacturer record with a company id but no Find My subtype is a
// close-but-invalid signature and must not detect.
void close_but_invalid_apple_record_too_short()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.mfg = {mb({0x4C, 0x00})};
    ctx("apple_no_subtype", 0, o.atMs);
    r.feed(o, ReconTarget::AirTag);
    EI("count", 0, r.logOf("w1").count());
}

// A one-byte manufacturer record is truncated and skipped.
void truncated_single_byte_manufacturer_skipped()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.mfg = {mb({0x4C})};
    ctx("apple_one_byte", 0, o.atMs);
    r.feed(o, ReconTarget::AirTag);
    EI("count", 0, r.logOf("w1").count());
}

// A repeat sighting of the same emitter updates one record in place and,
// per the pinned defect, never re-raises the alert.
void repeat_same_address_increments_count_and_does_not_realert()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.rssi = -40; o.uuids = {0xFEED};
    o.atMs = 1000; ctx("tile_repeat", 0, o.atMs); r.feed(o, ReconTarget::All);
    r.ack("w1");
    o.atMs = 2000; o.rssi = -55; ctx("tile_repeat", 1, o.atMs); r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("still one record", 1, log.count());
    const MonitorEvent *e = find(log, ReconTarget::Tile, kAddr);
    ET("record present", e != nullptr);
    if (e) {
        EI("count incremented", 2, (long)e->count);
        EI("lastSeen advanced", 2000, (long)e->lastSeen.uptimeMs);
        EI("rssi updated", -55, e->rssi);
    }
    ET("repeat does not re-raise alert", !log.alertPending());
}

// A changed advertised name at the same address stays one history entry.
void changing_name_same_address_stays_single_record()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.uuids = {0xFEED}; o.hasName = true;
    o.atMs = 1000; o.name = "Alpha"; ctx("tile_rename", 0, o.atMs); r.feed(o, ReconTarget::All);
    o.atMs = 2000; o.name = "Bravo"; ctx("tile_rename", 1, o.atMs); r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("one record despite name change", 1, log.count());
    const MonitorEvent *e = find(log, ReconTarget::Tile, kAddr);
    if (e) EI("count incremented", 2, (long)e->count);
}

// Low Meta then High Meta at one address upgrades the kept confidence but,
// per the KNOWN DEFECT, never alerts (first sighting was Low).
void low_then_high_upgrades_confidence_but_never_alerts_KNOWN_DEFECT()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.uuids = {0xFEB7}; // Meta, Low
    o.atMs = 1000; ctx("meta_low_then_high", 0, o.atMs); r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    const MonitorEvent *e0 = find(log, ReconTarget::Meta, kAddr);
    ET("Meta record present after Low", e0 != nullptr);
    if (e0) EI("confidence Low", (int)Confidence::Low, (int)e0->confidence);
    ET("Low match does not alert", !log.alertPending());
    o.uuids = {0xFD5F}; // Meta, High
    o.atMs = 2000; ctx("meta_low_then_high", 1, o.atMs); r.feed(o, ReconTarget::All);
    EI("still one Meta record", 1, log.count());
    const MonitorEvent *e1 = find(log, ReconTarget::Meta, kAddr);
    if (e1) {
        EI("confidence upgraded to High", (int)Confidence::High, (int)e1->confidence);
        EI("count", 2, (long)e1->count);
    }
    ET("upgrade still does not alert (defect)", !log.alertPending());
}

// A UUID outside the active scan selection is not detected.
void selection_scope_excludes_out_of_scan_uuid()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.uuids = {0xFEED}; // Tile
    ctx("scope_tile_scan_meta", 0, o.atMs);
    r.feed(o, ReconTarget::Meta);
    EI("count", 0, r.logOf("w1").count());
}

// Two receivers keep independent histories; a repeat on one never touches the
// other (the four-wands independence requirement).
void two_receivers_keep_independent_histories()
{
    SensorReplay r;
    Observation a; a.receiver = "wA"; a.atMs = 1000; a.uuids = {0xFEED};  // Tile on wA
    Observation b; b.receiver = "wB"; b.atMs = 1000; b.uuids = {0x3081};  // Flipper on wB
    ctx("two_receivers", 0, 1000); r.feed(a, ReconTarget::All);
    ctx("two_receivers", 1, 1000); r.feed(b, ReconTarget::All);
    EI("wA count", 1, r.logOf("wA").count());
    EI("wB count", 1, r.logOf("wB").count());
    ET("wA has Tile", find(r.logOf("wA"), ReconTarget::Tile, kAddr) != nullptr);
    ET("wA has no Flipper", find(r.logOf("wA"), ReconTarget::Flipper, kAddr) == nullptr);
    ET("wB has Flipper", find(r.logOf("wB"), ReconTarget::Flipper, kAddr) != nullptr);
    ET("wB has no Tile", find(r.logOf("wB"), ReconTarget::Tile, kAddr) == nullptr);
    a.atMs = 2000; ctx("two_receivers", 2, 2000); r.feed(a, ReconTarget::All); // repeat on wA only
    const MonitorEvent *ea = find(r.logOf("wA"), ReconTarget::Tile, kAddr);
    const MonitorEvent *eb = find(r.logOf("wB"), ReconTarget::Flipper, kAddr);
    if (ea) EI("wA Tile count after repeat", 2, (long)ea->count);
    if (eb) EI("wB Flipper count unchanged", 1, (long)eb->count);
}

// At capacity (40) the oldest record is dropped to make room.
void capacity_caps_at_forty_and_drops_oldest()
{
    SensorReplay r;
    char addr[24];
    for (int i = 0; i <= 40; ++i) { // 41 distinct emitters
        Observation o; o.receiver = "w1"; o.atMs = 1000 + i; o.uuids = {0xFEED};
        std::snprintf(addr, sizeof addr, "aa:bb:cc:dd:ee:%02x", i);
        o.address = addr;
        ctx("capacity", i, o.atMs);
        r.feed(o, ReconTarget::All);
    }
    auto &log = r.logOf("w1");
    EI("capped at capacity", 40, log.count());
    ET("oldest (index 0) evicted", find(log, ReconTarget::Tile, "aa:bb:cc:dd:ee:00") == nullptr);
    ET("newest present", find(log, ReconTarget::Tile, "aa:bb:cc:dd:ee:28") != nullptr);
}

// Sleep mode records and counts but suppresses the interruption.
void sleep_mode_records_without_alerting()
{
    SensorReplay r;
    r.setSleep("w1", true);
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.uuids = {0xFEED}; // Tile High
    ctx("sleep_mode", 0, o.atMs);
    r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("record kept", 1, log.count());
    ET("alert suppressed in sleep", !log.alertPending());
}

// clear() empties the history and drops the alert but does not reset the
// event serial; the next record gets a higher id.
void clear_empties_and_drops_alert_but_keeps_serial()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.atMs = 1000; o.uuids = {0xFEED};
    ctx("clear", 0, o.atMs); r.feed(o, ReconTarget::All);
    auto &log = r.logOf("w1");
    EI("serial after first", 1, (long)log.lastEventId());
    ET("alert pending before clear", log.alertPending());
    r.reset("w1");
    EI("empty after clear", 0, log.count());
    ET("alert dropped after clear", !log.alertPending());
    EI("serial not reset by clear", 1, (long)log.lastEventId());
    o.atMs = 2000; ctx("clear", 1, o.atMs); r.feed(o, ReconTarget::All);
    EI("new record after clear", 1, log.count());
    EI("serial advanced to 2", 2, (long)log.lastEventId());
}

// An acquisition gap (a long quiet stretch) is tolerated; a later sighting
// updates lastSeen to the new time.
void acquisition_gap_updates_last_seen()
{
    SensorReplay r;
    Observation o; o.receiver = "w1"; o.uuids = {0xFEED};
    o.atMs = 1000; ctx("gap", 0, o.atMs); r.feed(o, ReconTarget::All);
    o.atMs = 120000; ctx("gap", 1, o.atMs); r.feed(o, ReconTarget::All);
    const MonitorEvent *e = find(r.logOf("w1"), ReconTarget::Tile, kAddr);
    ET("record present", e != nullptr);
    if (e) {
        EI("count across the gap", 2, (long)e->count);
        EI("lastSeen is the later time", 120000, (long)e->lastSeen.uptimeMs);
    }
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(valid_tile_feed_detects_high_and_alerts);
    CASE(valid_flipper_uuid_detects_medium_and_alerts);
    CASE(close_but_invalid_apple_record_too_short);
    CASE(truncated_single_byte_manufacturer_skipped);
    CASE(repeat_same_address_increments_count_and_does_not_realert);
    CASE(changing_name_same_address_stays_single_record);
    CASE(low_then_high_upgrades_confidence_but_never_alerts_KNOWN_DEFECT);
    CASE(selection_scope_excludes_out_of_scan_uuid);
    CASE(two_receivers_keep_independent_histories);
    CASE(capacity_caps_at_forty_and_drops_oldest);
    CASE(sleep_mode_records_without_alerting);
    CASE(clear_empties_and_drops_alert_but_keeps_serial);
    CASE(acquisition_gap_updates_last_seen);
    CHECK_SUMMARY();
}
