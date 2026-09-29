// Unit tests for src/core/app/LayerTimeCore against fake ports. Checks the
// command routing, the split between clearing event history and resetting
// acquisition, alert actuation once per new alerting event, and the order
// tick() runs things in.

#include "check.h"

#include <string>
#include <vector>

#include "core/app/LayerTimeCore.h"

using namespace layertime;

namespace {

std::vector<std::string> g_calls;

struct FakeMonitor : MonitorSource {
    AcquisitionStatus status;
    recon::CandidateSink sink = nullptr;
    void *context = nullptr;
    ReconTarget started = ReconTarget::None;

    void start(ReconTarget t) override { started = t; g_calls.push_back("start"); }
    void stopManual() override { g_calls.push_back("stopManual"); }
    void resetDetectorState() override { g_calls.push_back("resetDetectorState"); }
    void setEarlyWarningEnabled(bool) override { g_calls.push_back("setEarlyWarning"); }
    void poll() override { g_calls.push_back("poll"); }
    AcquisitionStatus acquisition() const override { return status; }
    void setCandidateSink(recon::CandidateSink s, void *c) override { sink = s; context = c; }

    void deliver(ReconTarget d, const char *addr, Confidence conf = Confidence::High)
    {
        recon::Candidate c;
        c.detector = d;
        c.address = addr;
        c.confidence = conf;
        c.detail = "x";
        if (sink) sink(c, context);
    }
};

struct FakeAlerts : AlertSink {
    std::vector<Alert> raised;
    void raise(const Alert &a) override { raised.push_back(a); g_calls.push_back("raise"); }
};

struct FakeLog : EventLog {
    std::vector<uint32_t> ids;
    void append(const MonitorEvent &e) override { ids.push_back(e.eventId); }
};

struct FakeNav : NavigationSource {
    NavigationState next;
    void read(NavigationState &out) override { out = next; }
};

struct Rig {
    FakeMonitor monitor;
    FakeAlerts alerts;
    FakeLog log;
    FakeNav nav;
    LayerTimeCore core;
    Rig()
    {
        g_calls.clear();
        CorePorts p;
        p.monitor = &monitor;
        p.alerts = &alerts;
        p.eventLog = &log;
        p.navigation = &nav;
        core.attach(p);
    }
    CommandResult run(CommandType t, ReconTarget target = ReconTarget::None)
    {
        LayerTimeCommand c;
        c.type = t;
        c.reconTarget.target = target;
        return core.execute(c);
    }
};

const char *kA = "AA:BB:CC:DD:EE:01";
const char *kB = "AA:BB:CC:DD:EE:02";

} // namespace

void recon_start_and_stop_go_to_the_monitor_source()
{
    Rig r;
    CHECK_INT(static_cast<int>(CommandResult::Ok), static_cast<int>(r.run(CommandType::ReconStart, ReconTarget::Tile)));
    CHECK_INT(static_cast<int>(ReconTarget::Tile), static_cast<int>(r.monitor.started));
    CHECK_INT(static_cast<int>(CommandResult::Ok), static_cast<int>(r.run(CommandType::ReconStop)));
    CHECK_INT(2, g_calls.size());
    CHECK_STR("stopManual", g_calls.back().c_str());
}

void clear_events_clears_history_and_separately_resets_acquisition()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.monitor.deliver(ReconTarget::Tile, kB);
    g_calls.clear();
    CHECK_INT(static_cast<int>(CommandResult::Ok), static_cast<int>(r.run(CommandType::ReconClearEvents)));
    CHECK_INT(0, r.core.eventCount());
    CHECK_FALSE(r.core.reconState().alertPending);
    // Only the detector-side reset. Monitoring is not stopped or restarted.
    CHECK_INT(1, g_calls.size());
    CHECK_STR("resetDetectorState", g_calls[0].c_str());
}

void acknowledge_clears_the_pending_alert_only()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    CHECK_TRUE(r.core.reconState().alertPending);
    CHECK_INT(static_cast<int>(CommandResult::Ok), static_cast<int>(r.run(CommandType::ReconAcknowledgeAlert)));
    CHECK_FALSE(r.core.reconState().alertPending);
    CHECK_INT(1, r.core.eventCount());
}

void mesh_commands_are_unsupported_for_now()
{
    Rig r;
    for (CommandType t : {CommandType::MeshSendText, CommandType::MeshSendQuickMessage,
                          CommandType::MeshSetChannel, CommandType::MeshRemoveChannel})
        CHECK_INT(static_cast<int>(CommandResult::Unsupported), static_cast<int>(r.run(t)));
    CHECK_INT(static_cast<int>(CommandResult::InvalidArgument), static_cast<int>(r.run(CommandType::None)));
}

void recon_commands_without_a_monitor_are_unsupported()
{
    LayerTimeCore core;
    LayerTimeCommand c;
    c.type = CommandType::ReconStart;
    CHECK_INT(static_cast<int>(CommandResult::Unsupported), static_cast<int>(core.execute(c)));
    c.type = CommandType::ReconStop;
    CHECK_INT(static_cast<int>(CommandResult::Unsupported), static_cast<int>(core.execute(c)));
    c.type = CommandType::ReconClearEvents;
    CHECK_INT(static_cast<int>(CommandResult::Ok), static_cast<int>(core.execute(c)));
    core.tick(0);  // no ports: nothing to do, nothing to crash on
    CHECK_INT(0, core.eventCount());
}

void candidates_become_events_and_reach_the_event_log()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.monitor.deliver(ReconTarget::Pwnagotchi, kA);
    CHECK_INT(2, r.core.eventCount());
    CHECK_INT(2, r.core.event(0).count);
    CHECK_INT(2, r.log.ids.size());
    CHECK_INT(2, r.core.reconState().lastEventId);
}

void tick_raises_the_alert_once_then_polls()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    g_calls.clear();
    r.core.tick(5000);
    r.core.tick(5250);
    CHECK_INT(1, r.alerts.raised.size());
    CHECK_INT(1, r.alerts.raised[0].eventId);
    CHECK_INT(5000, r.alerts.raised[0].raised.uptimeMs);
    CHECK_INT(static_cast<int>(AlertKind::ReconDetection), static_cast<int>(r.alerts.raised[0].kind));
    CHECK_INT(3, g_calls.size());
    CHECK_STR("raise", g_calls[0].c_str());
    CHECK_STR("poll", g_calls[1].c_str());
    CHECK_STR("poll", g_calls[2].c_str());
}

void each_new_alerting_event_raises_once()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.core.tick(1);
    r.monitor.deliver(ReconTarget::Tile, kB);
    r.core.tick(2);
    CHECK_INT(2, r.alerts.raised.size());
    CHECK_INT(2, r.alerts.raised[1].eventId);
}

void two_events_between_ticks_raise_once_for_the_newest()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.monitor.deliver(ReconTarget::Tile, kB);
    r.core.tick(1);
    CHECK_INT(1, r.alerts.raised.size());
    CHECK_INT(2, r.alerts.raised[0].eventId);
}

void acknowledged_or_quiet_events_raise_nothing()
{
    Rig r;
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.run(CommandType::ReconAcknowledgeAlert);
    r.core.tick(1);
    r.monitor.deliver(ReconTarget::Flock, kB, Confidence::Low);
    r.core.tick(2);
    r.monitor.deliver(ReconTarget::Tile, kA);  // repeat
    r.core.tick(3);
    CHECK_INT(0, r.alerts.raised.size());
}

void sleep_mode_suppresses_the_alert_not_the_event()
{
    Rig r;
    r.core.setSleepMode(true);
    r.monitor.deliver(ReconTarget::Tile, kA);
    r.core.tick(1);
    CHECK_INT(0, r.alerts.raised.size());
    CHECK_INT(1, r.core.eventCount());
    CHECK_INT(1, r.log.ids.size());
}

void recon_state_combines_acquisition_and_history()
{
    Rig r;
    r.monitor.status.selected = ReconTarget::All;
    r.monitor.status.active = ReconTarget::Flock;
    r.monitor.status.monitoring = true;
    r.monitor.status.earlyWarningEnabled = true;
    r.monitor.status.earlyWarningResting = true;
    r.monitor.deliver(ReconTarget::Tile, kA);
    const ReconState s = r.core.reconState();
    CHECK_INT(static_cast<int>(ReconTarget::All), static_cast<int>(s.selected));
    CHECK_INT(static_cast<int>(ReconTarget::Flock), static_cast<int>(s.active));
    CHECK_TRUE(s.monitoring);
    CHECK_TRUE(s.earlyWarningEnabled);
    CHECK_TRUE(s.earlyWarningResting);
    CHECK_TRUE(s.alertPending);
    CHECK_INT(1, s.lastEventId);
    CHECK_INT(1, s.eventCount);
}

void navigation_is_read_on_refresh_only()
{
    Rig r;
    r.nav.next.fixUsable = true;
    r.nav.next.latitudeDeg = 47.5;
    CHECK_FALSE(r.core.navigation().fixUsable);
    r.core.refreshNavigation();
    CHECK_TRUE(r.core.navigation().fixUsable);
    CHECK_NEAR(47.5, r.core.navigation().latitudeDeg, 0.0);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(recon_start_and_stop_go_to_the_monitor_source);
    CASE(clear_events_clears_history_and_separately_resets_acquisition);
    CASE(acknowledge_clears_the_pending_alert_only);
    CASE(mesh_commands_are_unsupported_for_now);
    CASE(recon_commands_without_a_monitor_are_unsupported);
    CASE(candidates_become_events_and_reach_the_event_log);
    CASE(tick_raises_the_alert_once_then_polls);
    CASE(each_new_alerting_event_raises_once);
    CASE(two_events_between_ticks_raise_once_for_the_newest);
    CASE(acknowledged_or_quiet_events_raise_nothing);
    CASE(sleep_mode_suppresses_the_alert_not_the_event);
    CASE(recon_state_combines_acquisition_and_history);
    CASE(navigation_is_read_on_refresh_only);
    CHECK_SUMMARY();
}
