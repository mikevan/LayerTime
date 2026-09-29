// Unit tests for src/core/logic/MonitorEventLog, the Recon event history
// moved out of ReconService in Phase 0 Step 4. The end-to-end behaviour is
// still pinned by test_recon, test_recon_screen and test_detection_log
// through the real radio entry points; these check the log on its own.

#include "check.h"

#include <cstring>
#include <string>
#include <vector>

#include "core/logic/MonitorEventLog.h"
#include "core/logic/ReconSelection.h"

using namespace layertime;
using namespace layertime::recon;

namespace {

struct Recorder : EventLog {
    std::vector<MonitorEvent> rows;
    void append(const MonitorEvent &e) override { rows.push_back(e); }
};

Candidate cand(ReconTarget d, const char *addr, Confidence conf = Confidence::High,
               int8_t rssi = -60, uint8_t ch = 6, uint32_t at = 1000, const char *detail = "d")
{
    Candidate c;
    c.detector = d;
    c.address = addr;
    c.confidence = conf;
    c.rssi = rssi;
    c.channel = ch;
    c.atMs = at;
    c.detail = detail;
    return c;
}

const char *kA = "AA:BB:CC:DD:EE:01";
const char *kB = "AA:BB:CC:DD:EE:02";

} // namespace

void a_new_record_copies_the_candidate()
{
    MonitorEventLog log;
    Candidate c = cand(ReconTarget::Pwnagotchi, kA, Confidence::Medium, -55, 11, 4242, "Pwnagotchi beacon");
    c.sourceKind = SourceKind::Wifi;
    c.band = Band::Band2_4GHz;
    log.add(c);
    CHECK_INT(1, log.count());
    const MonitorEvent &e = log.event(0);
    CHECK_INT(1, e.eventId);
    CHECK_INT(static_cast<int>(ReconTarget::Pwnagotchi), static_cast<int>(e.detector));
    CHECK_INT(static_cast<int>(Confidence::Medium), static_cast<int>(e.confidence));
    CHECK_INT(static_cast<int>(SourceKind::Wifi), static_cast<int>(e.sourceKind));
    CHECK_INT(static_cast<int>(Band::Band2_4GHz), static_cast<int>(e.band));
    CHECK_STR(kA, e.sourceId);
    CHECK_STR("Pwnagotchi beacon", e.detail);
    CHECK_INT(-55, e.rssi);
    CHECK_INT(11, e.channel);
    CHECK_INT(1, e.count);
    CHECK_INT(4242, e.lastSeen.uptimeMs);
    CHECK_FALSE(e.lastSeen.wallClockValid);
    CHECK_INT(1, log.lastEventId());
}

void missing_detail_reads_activity_detected()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Tile, kA, Confidence::High, -60, 0, 1, nullptr));
    CHECK_STR("Activity detected", log.event(0).detail);
}

void long_text_is_truncated_to_the_field()
{
    MonitorEventLog log;
    const std::string longDetail(80, 'x');
    const char *longAddr = "0123456789ABCDEFGHIJKLMNOP";
    log.add(cand(ReconTarget::Axon, longAddr, Confidence::High, -60, 1, 1, longDetail.c_str()));
    CHECK_INT(39, std::strlen(log.event(0).detail));
    CHECK_STR("0123456789ABCDEFGH", log.event(0).sourceId);
}

void a_truncated_address_never_matches_again()
{
    // The stored id is compared against the full incoming address, so an
    // address longer than 18 characters is a new record every time. Same as
    // ReconService before the move. MACs are 17 characters.
    MonitorEventLog log;
    const char *longAddr = "0123456789ABCDEFGHIJ";
    log.add(cand(ReconTarget::Axon, longAddr));
    log.add(cand(ReconTarget::Axon, longAddr));
    CHECK_INT(2, log.count());
}

void a_repeat_updates_in_place()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Pwnagotchi, kA, Confidence::High, -70, 1, 100, "first"));
    log.add(cand(ReconTarget::Pwnagotchi, kA, Confidence::High, -40, 9, 900, "second"));
    CHECK_INT(1, log.count());
    const MonitorEvent &e = log.event(0);
    CHECK_INT(-40, e.rssi);
    CHECK_INT(9, e.channel);
    CHECK_INT(900, e.lastSeen.uptimeMs);
    CHECK_INT(2, e.count);
    CHECK_STR("first", e.detail);  // detail is not refreshed
    CHECK_INT(1, e.eventId);
    CHECK_INT(1, log.lastEventId());
}

void confidence_only_rises()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Flock, kA, Confidence::Medium));
    log.add(cand(ReconTarget::Flock, kA, Confidence::Low));
    CHECK_INT(static_cast<int>(Confidence::Medium), static_cast<int>(log.event(0).confidence));
    log.add(cand(ReconTarget::Flock, kA, Confidence::High));
    CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(log.event(0).confidence));
}

void same_address_under_another_detector_is_a_new_record()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Pwnagotchi, kA));
    log.add(cand(ReconTarget::Pineapple, kA));
    log.add(cand(ReconTarget::Pwnagotchi, kB));
    CHECK_INT(3, log.count());
    CHECK_INT(3, log.lastEventId());
}

void detector_identity_matches_the_old_category_key()
{
    // Before the move, records were matched on the category NAME, stored in
    // 14 bytes. Matching on the detector value is the same thing only if
    // every single-detector name is distinct and fits in 13 characters.
    std::vector<std::string> names;
    for (int v = static_cast<int>(ReconTarget::Deauth); v <= static_cast<int>(ReconTarget::GoogleTag); ++v) {
        const std::string n = detectorName(static_cast<ReconTarget>(v));
        CHECK_TRUE(n.size() <= 13);
        for (const std::string &m : names) CHECK_TRUE(m != n);
        names.push_back(n);
    }
}

void when_full_the_oldest_is_dropped()
{
    MonitorEventLog log;
    char addr[MonitorEventLog::kCapacity + 1][18];
    for (int i = 0; i <= MonitorEventLog::kCapacity; ++i) {
        snprintf(addr[i], sizeof(addr[i]), "00:00:00:00:00:%02X", i);
        log.add(cand(ReconTarget::Tile, addr[i]));
    }
    CHECK_INT(40, log.count());
    CHECK_STR("00:00:00:00:00:01", log.event(0).sourceId);
    CHECK_STR("00:00:00:00:00:28", log.event(39).sourceId);
    CHECK_INT(41, log.event(39).eventId);
    CHECK_INT(41, log.lastEventId());
    // The dropped emitter comes back as a new record.
    log.add(cand(ReconTarget::Tile, addr[0]));
    CHECK_INT(42, log.lastEventId());
    CHECK_STR("00:00:00:00:00:02", log.event(0).sourceId);
}

void new_records_alert_by_the_core_policy()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Flock, kA, Confidence::Low));
    CHECK_FALSE(log.alertPending());
    log.add(cand(ReconTarget::Flock, kB, Confidence::Medium));
    CHECK_TRUE(log.alertPending());

    MonitorEventLog sleeping;
    sleeping.setSleepMode(true);
    sleeping.add(cand(ReconTarget::Tile, kA, Confidence::High));
    CHECK_FALSE(sleeping.alertPending());
    CHECK_INT(1, sleeping.count());
}

void low_then_high_never_alerts_KNOWN_DEFECT()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Flock, kA, Confidence::Low));
    log.add(cand(ReconTarget::Flock, kA, Confidence::High));
    CHECK_INT(static_cast<int>(Confidence::High), static_cast<int>(log.event(0).confidence));
    CHECK_FALSE(log.alertPending());
}

void acknowledge_keeps_the_history()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Tile, kA));
    log.acknowledgeAlert();
    CHECK_FALSE(log.alertPending());
    CHECK_INT(1, log.count());
}

void clear_empties_history_and_drops_the_alert_but_keeps_the_serial()
{
    MonitorEventLog log;
    log.add(cand(ReconTarget::Tile, kA));
    log.add(cand(ReconTarget::Tile, kB));
    log.clear();
    CHECK_INT(0, log.count());
    CHECK_FALSE(log.alertPending());
    CHECK_INT(2, log.lastEventId());
    log.add(cand(ReconTarget::Tile, kA));
    CHECK_INT(3, log.event(0).eventId);
    CHECK_INT(1, log.event(0).count);
}

void the_recorder_sees_new_records_only()
{
    MonitorEventLog log;
    Recorder r;
    log.setRecorder(&r);
    log.add(cand(ReconTarget::Tile, kA, Confidence::Low));
    log.add(cand(ReconTarget::Tile, kA, Confidence::High));
    log.add(cand(ReconTarget::Tile, kB));
    CHECK_INT(2, r.rows.size());
    CHECK_STR(kA, r.rows[0].sourceId);
    CHECK_INT(static_cast<int>(Confidence::Low), static_cast<int>(r.rows[0].confidence));
    CHECK_INT(2, r.rows[1].eventId);
}

void the_recorder_is_called_after_the_alert_decision()
{
    struct Probe : EventLog {
        const MonitorEventLog *log = nullptr;
        bool pendingSeen = false;
        void append(const MonitorEvent &) override { pendingSeen = log->alertPending(); }
    } probe;
    MonitorEventLog log;
    probe.log = &log;
    log.setRecorder(&probe);
    log.add(cand(ReconTarget::Tile, kA));
    CHECK_TRUE(probe.pendingSeen);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(a_new_record_copies_the_candidate);
    CASE(missing_detail_reads_activity_detected);
    CASE(long_text_is_truncated_to_the_field);
    CASE(a_truncated_address_never_matches_again);
    CASE(a_repeat_updates_in_place);
    CASE(confidence_only_rises);
    CASE(same_address_under_another_detector_is_a_new_record);
    CASE(detector_identity_matches_the_old_category_key);
    CASE(when_full_the_oldest_is_dropped);
    CASE(new_records_alert_by_the_core_policy);
    CASE(low_then_high_never_alerts_KNOWN_DEFECT);
    CASE(acknowledge_keeps_the_history);
    CASE(clear_empties_history_and_drops_the_alert_but_keeps_the_serial);
    CASE(the_recorder_sees_new_records_only);
    CASE(the_recorder_is_called_after_the_alert_decision);
    CHECK_SUMMARY();
}
