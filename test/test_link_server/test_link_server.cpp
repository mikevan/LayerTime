// LayerTime Link 0.1 on a Node that runs Recon: the change sequence
// (ChangeTracker), the COMMAND, GET_CHANGED and GET_TEXT answers and the
// live Status fields (LinkServer), and rule 2's one outstanding request on
// the Node side (LinkPipe). Against LayerTimeCore with a fake monitor
// source, byte for byte where the contract fixes the bytes.
//
// Build command: see test/README.md, "LayerTime Link".

#include "check.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/app/LayerTimeCore.h"
#include "core/link/ChangeTracker.h"
#include "core/link/LinkCodec.h"
#include "core/link/LinkPipe.h"
#include "core/link/LinkServer.h"

using namespace layertime;
using namespace layertime::link;

namespace {

struct FakeMonitor : MonitorSource {
    AcquisitionStatus status;
    recon::CandidateSink sink = nullptr;
    void *context = nullptr;
    std::vector<std::string> calls;
    ReconTarget started = ReconTarget::None;

    void start(ReconTarget t) override
    {
        started = t;
        calls.push_back("start");
        status.selected = t;
        status.monitoring = true;
    }
    void stopManual() override
    {
        calls.push_back("stopManual");
        status.monitoring = false;
        status.selected = ReconTarget::None;
    }
    void resetDetectorState() override { calls.push_back("resetDetectorState"); }
    void setEarlyWarningEnabled(bool e) override { calls.push_back(e ? "ew1" : "ew0"); }
    void poll() override {}
    AcquisitionStatus acquisition() const override { return status; }
    void setCandidateSink(recon::CandidateSink s, void *c) override { sink = s; context = c; }

    void deliver(ReconTarget d, const char *addr, uint32_t atMs, int8_t rssi = -60,
                 Confidence conf = Confidence::High, const char *detail = "x", uint8_t channel = 6)
    {
        recon::Candidate c;
        c.detector = d;
        c.address = addr;
        c.confidence = conf;
        c.detail = detail;
        c.rssi = rssi;
        c.channel = channel;
        c.sourceKind = SourceKind::Wifi;
        c.band = Band::Band2_4GHz;
        c.atMs = atMs;
        if (sink) sink(c, context);
    }
};

int g_locks = 0;
int g_depth = 0;
int g_maxDepth = 0;
void lockFn(void *) { ++g_locks; ++g_depth; if (g_depth > g_maxDepth) g_maxDepth = g_depth; }
void unlockFn(void *) { --g_depth; }

struct Frames {
    std::vector<std::string> hex;
    std::vector<std::vector<uint8_t>> raw;
    // Every frame must be emitted while no lock is required to be free, but
    // record whether the lock was held, for the sink rule.
    static void sink(const uint8_t *f, size_t n, void *self)
    {
        auto *me = static_cast<Frames *>(self);
        CHECK_TRUE(n > 0 && n <= kMaxFrame);
        me->hex.push_back(toHexStr(f, n));
        me->raw.emplace_back(f, f + n);
    }
    static std::string toHexStr(const uint8_t *p, size_t n)
    {
        static const char *k = "0123456789ABCDEF";
        std::string s;
        for (size_t i = 0; i < n; ++i) { s += k[p[i] >> 4]; s += k[p[i] & 15]; }
        return s;
    }
    void clear() { hex.clear(); raw.clear(); }
};

struct Rig {
    FakeMonitor monitor;
    LayerTimeCore core;
    LinkServer server{core};
    NodeIdentity node;
    Frames frames;

    Rig()
    {
        CorePorts p;
        p.monitor = &monitor;
        core.attach(p);
        node.sessionId = 0x3FA2;
        node.capabilities = 0x1F;
        g_locks = g_depth = g_maxDepth = 0;
        server.setLock(lockFn, unlockFn, nullptr);
    }
    void send(const std::vector<uint8_t> &req, uint32_t nowMs = 0)
    {
        frames.clear();
        server.handle(node, 42, req.data(), req.size(), nowMs, Frames::sink, &frames);
        CHECK_INT(0, g_depth); // every lock released
    }
    void sendHex(const char *hex, uint32_t nowMs = 0)
    {
        std::vector<uint8_t> b;
        for (size_t i = 0; hex[i] && hex[i + 1]; i += 2) {
            char two[3] = {hex[i], hex[i + 1], 0};
            b.push_back(static_cast<uint8_t>(strtoul(two, nullptr, 16)));
        }
        send(b, nowMs);
    }
    void getChanged(uint32_t since, uint32_t nowMs = 0, uint8_t reqId = 9)
    {
        uint8_t b[kMaxFrame];
        GetChanged g;
        g.reqId = reqId;
        g.sinceChangeSeq = since;
        const size_t n = encodeGetChanged(g, b);
        send(std::vector<uint8_t>(b, b + n), nowMs);
    }
    End lastEnd()
    {
        End e;
        CHECK_TRUE(!frames.raw.empty());
        if (frames.raw.empty()) return e;
        CHECK_TRUE(decodeEnd(frames.raw.back().data(), frames.raw.back().size(), e));
        return e;
    }
    std::vector<EventSummary> summaries()
    {
        std::vector<EventSummary> out;
        for (auto &f : frames.raw) {
            EventSummary s;
            if (decodeEventSummary(f.data(), f.size(), s)) out.push_back(s);
        }
        return out;
    }
    StatusSnapshot status()
    {
        StatusSnapshot s;
        server.fillStatus(s);
        return s;
    }
};

char g_addr[64];
const char *addr(int i)
{
    snprintf(g_addr, sizeof(g_addr), "AA:BB:CC:DD:%02X:%02X", (i >> 8) & 0xFF, i & 0xFF);
    return g_addr;
}

MonitorEvent ev(uint32_t id, uint32_t count = 1)
{
    MonitorEvent e;
    e.eventId = id;
    e.count = count;
    return e;
}

} // namespace

// --- ChangeTracker ----------------------------------------------------------

void tracker_starts_at_one_and_stays_while_nothing_changes()
{
    ChangeTracker t;
    CHECK_INT(1, static_cast<long>(t.changeSeq()));
    t.begin();
    t.end(TrackedState{});
    CHECK_INT(1, static_cast<long>(t.changeSeq()));
    CHECK_FALSE(t.gapSince(0));
}

void tracker_bumps_once_per_pass_and_stamps_new_and_changed_events()
{
    ChangeTracker t;
    TrackedState s;
    s.eventCount = 2;
    t.begin(); t.event(ev(1)); t.event(ev(2)); t.end(s);
    CHECK_INT(2, static_cast<long>(t.changeSeq()));
    CHECK_INT(2, static_cast<long>(t.seqOf(1)));
    CHECK_INT(2, static_cast<long>(t.seqOf(2)));
    // A pass with nothing new: no bump, sequences kept.
    t.begin(); t.event(ev(1)); t.event(ev(2)); t.end(s);
    CHECK_INT(2, static_cast<long>(t.changeSeq()));
    // Event 2 sighted again: only it takes the new sequence.
    t.begin(); t.event(ev(1)); t.event(ev(2, 2)); t.end(s);
    CHECK_INT(3, static_cast<long>(t.changeSeq()));
    CHECK_INT(2, static_cast<long>(t.seqOf(1)));
    CHECK_INT(3, static_cast<long>(t.seqOf(2)));
    CHECK_INT(0, static_cast<long>(t.seqOf(99)));
    // A state change alone bumps changeSeq and stamps no event.
    s.flags = kFlagMonitoring;
    t.begin(); t.event(ev(1)); t.event(ev(2, 2)); t.end(s);
    CHECK_INT(4, static_cast<long>(t.changeSeq()));
    CHECK_INT(3, static_cast<long>(t.seqOf(2)));
    CHECK_FALSE(t.gapSince(0));
}

void tracker_records_dropped_events_for_the_gap()
{
    ChangeTracker t;
    TrackedState s;
    t.begin(); t.event(ev(1)); t.event(ev(2)); t.end(s);      // seq 2
    t.begin(); t.event(ev(1)); t.event(ev(2, 5)); t.end(s);   // seq 3, event 2 at 3
    // Event 1 (at 2) dropped, event 3 new.
    t.begin(); t.event(ev(2, 5)); t.event(ev(3)); t.end(s);   // seq 4
    CHECK_INT(4, static_cast<long>(t.changeSeq()));
    CHECK_TRUE(t.gapSince(0));
    CHECK_TRUE(t.gapSince(1));
    CHECK_FALSE(t.gapSince(2));  // an interface at 2 had seen event 1's last change
    // Everything cleared: event 2 (at 3) and 3 (at 4) dropped.
    t.begin(); t.end(s);
    CHECK_INT(5, static_cast<long>(t.changeSeq()));
    CHECK_TRUE(t.gapSince(3));
    CHECK_FALSE(t.gapSince(4));
    CHECK_INT(0, t.heldCount());
    t.reset();
    CHECK_INT(1, static_cast<long>(t.changeSeq()));
    CHECK_FALSE(t.gapSince(0));
}

// --- Status -----------------------------------------------------------------

void status_reports_the_core_state_and_moves_changeseq()
{
    Rig r;
    CHECK_FALSE(r.server.refresh());
    StatusSnapshot s = r.status();
    CHECK_INT(1, static_cast<long>(s.changeSeq));
    CHECK_INT(0, s.flags);
    CHECK_INT(0, s.eventCount);

    r.monitor.status.earlyWarningEnabled = true;
    r.monitor.status.earlyWarningResting = true;
    CHECK_TRUE(r.server.refresh());
    s = r.status();
    CHECK_INT(2, static_cast<long>(s.changeSeq));
    CHECK_INT(kFlagEarlyWarningEnabled | kFlagEarlyWarningResting, s.flags);

    r.monitor.deliver(ReconTarget::Deauth, addr(1), 1000);
    r.core.raiseAlerts(1000);
    CHECK_TRUE(r.server.refresh());
    s = r.status();
    CHECK_INT(3, static_cast<long>(s.changeSeq));
    CHECK_INT(1, s.eventCount);
    CHECK_INT(1, static_cast<long>(s.lastAlertEventId));
    CHECK_INT(kFlagEarlyWarningEnabled | kFlagEarlyWarningResting | kFlagAlertPending, s.flags);
    CHECK_TRUE(g_locks > 0);
    CHECK_INT(0, g_depth);
}

void memory_only_sets_the_no_sd_log_flag_and_moves_changeseq()
{
    Rig r;
    r.server.refresh();
    CHECK_INT(0, r.status().flags & kFlagNoSdLog);
    r.server.setMemoryOnly(true);
    CHECK_TRUE(r.server.refresh());
    StatusSnapshot s = r.status();
    CHECK_INT(kFlagNoSdLog, s.flags & kFlagNoSdLog);
    CHECK_INT(2, static_cast<long>(s.changeSeq));
    CHECK_FALSE(r.server.refresh()); // unchanged: no bump
    r.server.setMemoryOnly(false);
    CHECK_TRUE(r.server.refresh());
    CHECK_INT(0, r.status().flags & kFlagNoSdLog);
    // The flag sits beside the Recon flags, never in place of them.
    r.monitor.status.earlyWarningEnabled = true;
    r.server.setMemoryOnly(true);
    r.server.refresh();
    CHECK_INT(kFlagEarlyWarningEnabled | kFlagNoSdLog, r.status().flags);
}

// --- COMMAND ----------------------------------------------------------------

void recon_start_reaches_the_monitor_and_answers_ok()
{
    Rig r;
    r.sendHex("03400105"); // ReconStart Deauth
    CHECK_INT(1, r.frames.hex.size());
    CHECK_STR("8340000100", r.frames.hex[0].c_str());
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), static_cast<int>(r.monitor.started));
    CHECK_TRUE(r.server.refresh());
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), r.status().selected);
    CHECK_INT(kFlagMonitoring, r.status().flags & kFlagMonitoring);
    // The start is not under the event-log lock: it reaches the radios.
    CHECK_INT(1, g_maxDepth);
}

void recon_start_rejects_none_early_warning_and_beyond()
{
    Rig r;
    r.sendHex("03410100");
    CHECK_STR("8341000102", r.frames.hex[0].c_str()); // None: InvalidArgument
    r.sendHex("03420111");
    CHECK_STR("8342000102", r.frames.hex[0].c_str()); // EarlyWarning 17
    r.sendHex("034301FF");
    CHECK_STR("8343000102", r.frames.hex[0].c_str());
    r.sendHex("03440110");
    CHECK_STR("8344000100", r.frames.hex[0].c_str()); // GoogleTag 16 is the last target
    r.sendHex("03450101");
    CHECK_STR("8345000100", r.frames.hex[0].c_str()); // All 1 is the first
}

void stop_clear_acknowledge_and_the_two_settings()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 10);
    r.monitor.deliver(ReconTarget::Deauth, addr(2), 20);
    CHECK_INT(2, r.core.eventCount());
    r.sendHex("034602");
    CHECK_STR("8346000200", r.frames.hex[0].c_str());
    r.sendHex("034704");
    CHECK_STR("8347000400", r.frames.hex[0].c_str());
    CHECK_FALSE(r.core.reconState().alertPending);
    r.sendHex("034803");
    CHECK_STR("8348000300", r.frames.hex[0].c_str());
    CHECK_INT(0, r.core.eventCount());
    CHECK_TRUE(r.monitor.calls.back() == "resetDetectorState");
    r.sendHex("03490B01");
    CHECK_STR("8349000B00", r.frames.hex[0].c_str());
    CHECK_TRUE(r.core.settings().sleepModeEnabled);
    r.server.refresh();
    CHECK_INT(kFlagSleepMode, r.status().flags & kFlagSleepMode);
    r.sendHex("034A0C00");
    CHECK_STR("834A000C00", r.frames.hex[0].c_str());
    CHECK_FALSE(r.core.settings().earlyWarningEnabled);
    r.sendHex("034B0C02");
    CHECK_STR("834B000C02", r.frames.hex[0].c_str()); // 2 is not a boolean
    r.sendHex("034C0B02");
    CHECK_STR("834C000B02", r.frames.hex[0].c_str());
    CHECK_TRUE(r.core.settings().sleepModeEnabled); // unchanged
}

void command_lengths_and_unknown_commands()
{
    Rig r;
    r.sendHex("0350");
    CHECK_STR("8F5002", r.frames.hex[0].c_str());     // shorter than 3
    r.sendHex("035101");
    CHECK_STR("8F5102", r.frames.hex[0].c_str());     // ReconStart without its target
    r.sendHex("03520200");
    CHECK_STR("8F5202", r.frames.hex[0].c_str());     // ReconStop with an argument
    r.sendHex("03530C");
    CHECK_STR("8F5302", r.frames.hex[0].c_str());     // SetEarlyWarning without its value
    r.sendHex("035405");
    CHECK_STR("8354000501", r.frames.hex[0].c_str()); // MeshSendText: Unsupported here
    r.sendHex("0355090102030405060708090A0B0C0D0E0F1011");
    CHECK_STR("8355000901", r.frames.hex[0].c_str()); // 20 bytes, unknown: Unsupported
    r.sendHex("035600");
    CHECK_STR("8356000001", r.frames.hex[0].c_str()); // None is not a command either
    r.sendHex("0357090102030405060708090A0B0C0D0E0F101112");
    CHECK_STR("8F5702", r.frames.hex[0].c_str());     // 21 bytes: no frame is that long
    CHECK_TRUE(r.monitor.calls.empty());
}

void hello_and_ping_still_go_to_dispatch()
{
    Rig r;
    r.sendHex("01110001");
    CHECK_STR("8111000001A23F1F0014", r.frames.hex[0].c_str());
    r.sendHex("0221EFBEADDE");
    CHECK_STR("822100EFBEADDE2A", r.frames.hex[0].c_str());
    r.sendHex("7E05");
    CHECK_STR("8F0501", r.frames.hex[0].c_str());
    r.sendHex("");
    CHECK_STR("8F0002", r.frames.hex[0].c_str());
    r.sendHex("04");
    CHECK_STR("8F0002", r.frames.hex[0].c_str());
}

// --- GET_CHANGED --------------------------------------------------------------

void get_changed_from_zero_returns_every_event_oldest_first_then_end()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 1000, -67);
    r.monitor.deliver(ReconTarget::AirTag, addr(2), 2000, -90, Confidence::Medium, "");
    r.getChanged(0, 76000);
    CHECK_INT(3, r.frames.raw.size());
    auto s = r.summaries();
    CHECK_INT(2, s.size());
    CHECK_INT(1, static_cast<long>(s[0].eventId));
    CHECK_INT(static_cast<int>(ReconTarget::Deauth), s[0].detector);
    CHECK_INT(-67, s[0].rssi);
    CHECK_INT(1, s[0].count);
    CHECK_INT(75, s[0].ageSeconds);
    CHECK_INT(kSummaryHasSourceId | kSummaryHasDetail, s[0].flags);
    CHECK_INT(2, static_cast<long>(s[1].eventId));
    CHECK_INT(-90, s[1].rssi);
    CHECK_INT(74, s[1].ageSeconds);
    CHECK_INT(kSummaryHasSourceId, s[1].flags);  // empty detail
    End e = r.lastEnd();
    CHECK_INT(9, e.reqId);
    CHECK_INT(2, e.count);
    CHECK_INT(0, e.gap);
    CHECK_INT(2, static_cast<long>(e.changeSeq));
    // Byte-exact check of the first summary.
    CHECK_STR("840900010000000502010106BD01004B0003", r.frames.hex[0].c_str());
}

void get_changed_since_returns_only_what_changed()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 1000);
    r.monitor.deliver(ReconTarget::Deauth, addr(2), 1000);
    r.getChanged(0, 1000);
    const uint32_t synced = r.lastEnd().changeSeq;
    r.getChanged(synced, 1000);
    CHECK_INT(1, r.frames.raw.size());                // END only
    CHECK_INT(0, r.lastEnd().count);
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 5000, -40); // repeat sighting of event 1
    r.getChanged(synced, 6000);
    auto s = r.summaries();
    CHECK_INT(1, s.size());
    CHECK_INT(1, static_cast<long>(s[0].eventId));
    CHECK_INT(2, s[0].count);
    CHECK_INT(-40, s[0].rssi);
    CHECK_INT(1, s[0].ageSeconds);
    CHECK_INT(0, r.lastEnd().gap);
    CHECK_TRUE(r.lastEnd().changeSeq > synced);
}

void get_changed_reports_a_gap_across_the_forty_event_wrap()
{
    Rig r;
    for (int i = 0; i < 40; ++i) r.monitor.deliver(ReconTarget::Deauth, addr(i), 100);
    r.getChanged(0, 100);
    CHECK_INT(41, r.frames.raw.size());
    const uint32_t synced = r.lastEnd().changeSeq;
    CHECK_INT(0, r.lastEnd().gap);
    // Two new events push out events 1 and 2, which the interface had seen:
    // no gap, and the interface prunes them by keeping the 40 highest ids.
    r.monitor.deliver(ReconTarget::Deauth, addr(100), 200);
    r.monitor.deliver(ReconTarget::Deauth, addr(101), 200);
    r.getChanged(synced, 200);
    auto s = r.summaries();
    CHECK_INT(2, s.size());
    CHECK_INT(41, static_cast<long>(s[0].eventId));
    CHECK_INT(42, static_cast<long>(s[1].eventId));
    CHECK_INT(0, r.lastEnd().gap);
    const uint32_t synced2 = r.lastEnd().changeSeq;
    r.server.refresh();
    CHECK_INT(40, r.status().eventCount);
    // Event 3 changes, then is pushed out before the interface asks: that
    // change was missed, which END reports.
    r.monitor.deliver(ReconTarget::Deauth, addr(2), 300); // event 3 (address 2), a repeat
    r.server.refresh();
    r.monitor.deliver(ReconTarget::Deauth, addr(102), 300); // drops event 3
    r.getChanged(synced2, 300);
    s = r.summaries();
    CHECK_INT(1, s.size());
    CHECK_INT(43, static_cast<long>(s[0].eventId));
    CHECK_INT(1, r.lastEnd().gap);
    CHECK_INT(1, static_cast<long>(r.server.counters().gaps));
    // From the new END onward there is no gap.
    r.getChanged(r.lastEnd().changeSeq, 300);
    CHECK_INT(0, r.lastEnd().gap);
}

void get_changed_after_clear_is_empty_and_status_count_is_zero()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 100);
    r.getChanged(0, 100);
    const uint32_t synced = r.lastEnd().changeSeq;
    r.sendHex("036003");
    r.getChanged(synced, 100);
    CHECK_INT(0, r.lastEnd().count);
    CHECK_TRUE(r.lastEnd().changeSeq > synced);
    r.server.refresh();
    CHECK_INT(0, r.status().eventCount);
    // New events after a clear keep counting up from the old ids.
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 200);
    r.getChanged(r.lastEnd().changeSeq, 200);
    auto s = r.summaries();
    CHECK_INT(1, s.size());
    CHECK_INT(2, static_cast<long>(s[0].eventId));
}

void get_changed_saturates_count_and_age()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 0);
    for (int i = 0; i < 70000; ++i) r.monitor.deliver(ReconTarget::Deauth, addr(1), 0);
    r.getChanged(0, 0xFFFFFFF0u);
    auto s = r.summaries();
    CHECK_INT(1, s.size());
    CHECK_INT(65535, s[0].count);
    CHECK_INT(65535, s[0].ageSeconds);
    // lastSeen stamped after the request's clock read: age 0, not a wrap.
    r.monitor.deliver(ReconTarget::Deauth, addr(2), 5000);
    r.getChanged(0, 4000);
    s = r.summaries();
    CHECK_INT(0, s[1].ageSeconds);
}

void get_changed_wrong_length_is_bad_length()
{
    Rig r;
    r.sendHex("0461000000");
    CHECK_STR("8F6102", r.frames.hex[0].c_str());
    r.sendHex("046200000000FF");
    CHECK_STR("8F6202", r.frames.hex[0].c_str());
}

// --- GET_TEXT -----------------------------------------------------------------

void get_text_splits_into_fourteen_byte_fragments()
{
    Rig r;
    r.monitor.deliver(ReconTarget::MultiSSID, "AA:BB:CC:DD:EE:01", 100, -50, Confidence::High,
                      "Free Public WiFi and a much longer SSID"); // 39 characters
    r.sendHex("05700100000001");
    CHECK_INT(4, r.frames.raw.size()); // 14 + 14 + 11, then END
    Text t;
    CHECK_TRUE(decodeText(r.frames.raw[0].data(), r.frames.raw[0].size(), t));
    CHECK_INT(0, t.index);
    CHECK_INT(3, t.total);
    CHECK_INT(14, t.length);
    CHECK_INT(1, t.field);
    std::string all;
    for (int i = 0; i < 3; ++i) {
        CHECK_TRUE(decodeText(r.frames.raw[i].data(), r.frames.raw[i].size(), t));
        CHECK_INT(i, t.index);
        all.append(reinterpret_cast<const char *>(t.bytes), t.length);
    }
    CHECK_STR("Free Public WiFi and a much longer SSID", all.c_str());
    End e = r.lastEnd();
    CHECK_INT(3, e.count);
    CHECK_INT(0, e.gap);
    CHECK_STR("86700001000346726565205075626C6963205769", r.frames.hex[0].c_str());

    r.sendHex("05710100000000");
    CHECK_INT(3, r.frames.raw.size()); // 17-byte address: 14 + 3, then END
    CHECK_TRUE(decodeText(r.frames.raw[1].data(), r.frames.raw[1].size(), t));
    CHECK_INT(3, t.length);
    CHECK_INT(0, t.field);
}

void get_text_unknown_event_field_or_empty_is_end_zero()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Deauth, addr(1), 100, -50, Confidence::High, "");
    r.sendHex("05720900000001");                 // no event 9
    CHECK_INT(1, r.frames.raw.size());
    CHECK_INT(0, r.lastEnd().count);
    r.sendHex("05730100000007");                 // no field 7
    CHECK_INT(1, r.frames.raw.size());
    CHECK_INT(0, r.lastEnd().count);
    r.sendHex("05740100000001");                 // empty detail
    CHECK_INT(1, r.frames.raw.size());
    CHECK_INT(0, r.lastEnd().count);
    r.sendHex("057501000000");                   // 6 bytes
    CHECK_STR("8F7502", r.frames.hex[0].c_str());
}

// --- LinkPipe -----------------------------------------------------------------

void pipe_allows_one_request_until_its_reply_is_sent()
{
    LinkPipe p;
    const uint8_t a[] = {0x04, 0x01, 0, 0, 0, 0};
    const uint8_t b[] = {0x02, 0x02, 1, 2, 3, 4};
    CHECK_FALSE(p.busy());
    CHECK_TRUE(p.offer(a, sizeof(a)));
    CHECK_TRUE(p.busy());
    CHECK_FALSE(p.offer(b, sizeof(b)));          // waiting: Busy
    uint8_t buf[kMaxFrame];
    CHECK_INT(6, static_cast<long>(p.take(buf)));
    CHECK_INT(0x04, buf[0]);
    CHECK_INT(0, static_cast<long>(p.take(buf)));
    CHECK_FALSE(p.offer(b, sizeof(b)));          // being answered: Busy
    const uint8_t f1[] = {0x84, 1, 0};
    const uint8_t f2[] = {0x85, 1, 0, 1};
    CHECK_TRUE(p.push(f1, sizeof(f1)));
    CHECK_TRUE(p.push(f2, sizeof(f2)));
    p.finish();
    CHECK_FALSE(p.offer(b, sizeof(b)));          // frames still queued: Busy
    CHECK_INT(3, static_cast<long>(p.front(buf)));
    CHECK_INT(0x84, buf[0]);
    p.pop();
    CHECK_INT(4, static_cast<long>(p.front(buf)));
    p.pop();
    CHECK_INT(0, static_cast<long>(p.front(buf)));
    CHECK_FALSE(p.busy());
    CHECK_TRUE(p.offer(b, sizeof(b)));
    p.clear();
    CHECK_FALSE(p.busy());
    CHECK_FALSE(p.offer(b, 0));
    uint8_t big[kMaxFrame + 1] = {};
    CHECK_FALSE(p.offer(big, sizeof(big)));
}

void pipe_holds_a_full_get_changed_reply_and_counts_overflow()
{
    LinkPipe p;
    uint8_t f[kEventSummarySize] = {0x84};
    for (int i = 0; i < 41; ++i) CHECK_TRUE(p.push(f, sizeof(f)));
    CHECK_INT(41, p.queued());
    for (int i = 41; i < LinkPipe::kCapacity; ++i) CHECK_TRUE(p.push(f, sizeof(f)));
    CHECK_FALSE(p.push(f, sizeof(f)));
    CHECK_INT(1, static_cast<long>(p.dropped()));
    // Ring order survives wrapping.
    uint8_t buf[kMaxFrame];
    for (int i = 0; i < 10; ++i) p.pop();
    uint8_t g[3] = {0x85, 7, 0};
    for (int i = 0; i < 10; ++i) CHECK_TRUE(p.push(g, sizeof(g)));
    for (int i = 0; i < LinkPipe::kCapacity - 10; ++i) p.pop();
    CHECK_INT(3, static_cast<long>(p.front(buf)));
    CHECK_INT(7, buf[1]);
}

void a_full_reply_through_the_pipe_matches_the_server()
{
    Rig r;
    for (int i = 0; i < 40; ++i) r.monitor.deliver(ReconTarget::Deauth, addr(i), 100);
    LinkPipe p;
    uint8_t req[kMaxFrame];
    GetChanged g;
    g.reqId = 3;
    const size_t n = encodeGetChanged(g, req);
    CHECK_TRUE(p.offer(req, n));
    uint8_t taken[kMaxFrame];
    const size_t tn = p.take(taken);
    r.server.handle(r.node, 0, taken, tn, 100,
                    [](const uint8_t *f, size_t len, void *ctx) { static_cast<LinkPipe *>(ctx)->push(f, len); }, &p);
    p.finish();
    CHECK_INT(41, p.queued());
    CHECK_INT(0, static_cast<long>(p.dropped()));
    uint8_t out[kMaxFrame];
    size_t last = 0;
    while (p.queued()) { last = p.front(out); p.pop(); }
    End e;
    CHECK_TRUE(decodeEnd(out, last, e));
    CHECK_INT(40, e.count);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(tracker_starts_at_one_and_stays_while_nothing_changes);
    CASE(tracker_bumps_once_per_pass_and_stamps_new_and_changed_events);
    CASE(tracker_records_dropped_events_for_the_gap);
    CASE(status_reports_the_core_state_and_moves_changeseq);
    CASE(memory_only_sets_the_no_sd_log_flag_and_moves_changeseq);
    CASE(recon_start_reaches_the_monitor_and_answers_ok);
    CASE(recon_start_rejects_none_early_warning_and_beyond);
    CASE(stop_clear_acknowledge_and_the_two_settings);
    CASE(command_lengths_and_unknown_commands);
    CASE(hello_and_ping_still_go_to_dispatch);
    CASE(get_changed_from_zero_returns_every_event_oldest_first_then_end);
    CASE(get_changed_since_returns_only_what_changed);
    CASE(get_changed_reports_a_gap_across_the_forty_event_wrap);
    CASE(get_changed_after_clear_is_empty_and_status_count_is_zero);
    CASE(get_changed_saturates_count_and_age);
    CASE(get_changed_wrong_length_is_bad_length);
    CASE(get_text_splits_into_fourteen_byte_fragments);
    CASE(get_text_unknown_event_field_or_empty_is_end_zero);
    CASE(pipe_allows_one_request_until_its_reply_is_sent);
    CASE(pipe_holds_a_full_get_changed_reply_and_counts_overflow);
    CASE(a_full_reply_through_the_pipe_matches_the_server);
    CHECK_SUMMARY();
}
