// Unit tests for the application settings in src/core/app/LayerTimeCore,
// added in Phase 0 Step 6: the six settings commands, their results
// (contracts/commands.md), loading and saving through the SettingsStore
// port, and sleep mode reaching the alert policy. Fake ports; no platform.

#include "check.h"

#include <cstring>
#include <string>

#include "core/app/LayerTimeCore.h"

using namespace layertime;

namespace {

struct FakeMonitor : MonitorSource {
    recon::CandidateSink sink = nullptr;
    void *context = nullptr;
    void start(ReconTarget) override {}
    void stopManual() override {}
    void resetDetectorState() override {}
    void setEarlyWarningEnabled(bool) override {}
    void poll() override {}
    AcquisitionStatus acquisition() const override { return {}; }
    void setCandidateSink(recon::CandidateSink s, void *c) override { sink = s; context = c; }
    void high(const char *addr)
    {
        recon::Candidate c;
        c.detector = ReconTarget::Tile;
        c.address = addr;
        c.confidence = Confidence::High;
        if (sink) sink(c, context);
    }
};

struct FakeTransport : MeshTransport {
    MeshNetwork net;
    explicit FakeTransport(MeshNetwork n) : net(n) {}
    MeshNetwork network() const override { return net; }
    void observeStatus(MeshNetworkStatus &) const override {}
    CommandResult sendText(const MeshDestination &, const char *) override { return CommandResult::Ok; }
};

struct FakeStore : SettingsStore {
    ApplicationSettings held;
    int loads = 0;
    int saves = 0;
    void load(ApplicationSettings &out) override { ++loads; out = held; }
    void save(const ApplicationSettings &s) override { ++saves; held = s; }
};

struct Rig {
    FakeMonitor monitor;
    FakeTransport tastic{MeshNetwork::Meshtastic};
    FakeTransport meshcore{MeshNetwork::MeshCore};
    FakeStore store;
    LayerTimeCore core;
    Rig()
    {
        CorePorts p;
        p.monitor = &monitor;
        p.mesh[static_cast<uint8_t>(MeshNetwork::Meshtastic)] = &tastic;
        p.mesh[static_cast<uint8_t>(MeshNetwork::MeshCore)] = &meshcore;
        p.settings = &store;
        core.attach(p);
    }
    CommandResult set(CommandType t, bool on, MeshNetwork n = MeshNetwork::Meshtastic)
    {
        LayerTimeCommand c;
        c.type = t;
        c.setting.enabled = on;
        c.setting.network = n;
        return core.execute(c);
    }
    CommandResult name(MeshNetwork n, const char *text)
    {
        LayerTimeCommand c;
        c.type = CommandType::MeshSetOwnName;
        c.setting.network = n;
        snprintf(c.setting.name, sizeof(c.setting.name), "%s", text);
        return core.execute(c);
    }
};

int r(CommandResult c) { return static_cast<int>(c); }
const int kOk = static_cast<int>(CommandResult::Ok);
const int kUnsupported = static_cast<int>(CommandResult::Unsupported);
const int kInvalid = static_cast<int>(CommandResult::InvalidArgument);

} // namespace

void the_defaults_are_the_first_boot_values()
{
    LayerTimeCore core;
    const ApplicationSettings &s = core.settings();
    CHECK_FALSE(s.use24Hour);
    CHECK_FALSE(s.metricUnits);
    CHECK_FALSE(s.sleepModeEnabled);
    CHECK_TRUE(s.earlyWarningEnabled);
    CHECK_FALSE(s.advertisingOn(MeshNetwork::MeshCore));
    CHECK_FALSE(s.advertisingOn(MeshNetwork::Meshtastic));
    CHECK_STR("", s.meshtasticName);
}

void each_toggle_command_sets_its_setting()
{
    Rig g;
    CHECK_INT(kOk, r(g.set(CommandType::SetClockFormat, true)));
    CHECK_TRUE(g.core.settings().use24Hour);
    CHECK_INT(kOk, r(g.set(CommandType::SetUnits, true)));
    CHECK_TRUE(g.core.settings().metricUnits);
    CHECK_INT(kOk, r(g.set(CommandType::SetSleepMode, true)));
    CHECK_TRUE(g.core.settings().sleepModeEnabled);
    CHECK_INT(kOk, r(g.set(CommandType::SetEarlyWarning, false)));
    CHECK_FALSE(g.core.settings().earlyWarningEnabled);
    CHECK_INT(kOk, r(g.set(CommandType::MeshSetAdvertising, true, MeshNetwork::MeshCore)));
    CHECK_TRUE(g.core.settings().advertisingOn(MeshNetwork::MeshCore));
    CHECK_FALSE(g.core.settings().advertisingOn(MeshNetwork::Meshtastic));
    CHECK_INT(kOk, r(g.set(CommandType::MeshSetAdvertising, true, MeshNetwork::Meshtastic)));
    CHECK_TRUE(g.core.settings().advertisingOn(MeshNetwork::Meshtastic));
    // and back
    g.set(CommandType::SetClockFormat, false);
    g.set(CommandType::SetUnits, false);
    g.set(CommandType::MeshSetAdvertising, false, MeshNetwork::MeshCore);
    CHECK_FALSE(g.core.settings().use24Hour);
    CHECK_FALSE(g.core.settings().metricUnits);
    CHECK_FALSE(g.core.settings().advertisingOn(MeshNetwork::MeshCore));
}

void the_meshtastic_name_is_set_and_cleared()
{
    Rig g;
    CHECK_INT(kOk, r(g.name(MeshNetwork::Meshtastic, "Ranger 7")));
    CHECK_STR("Ranger 7", g.core.settings().meshtasticName);
    CHECK_INT(kOk, r(g.name(MeshNetwork::Meshtastic, "")));
    CHECK_STR("", g.core.settings().meshtasticName);
    const std::string nineteen(19, 'n');
    CHECK_INT(kOk, r(g.name(MeshNetwork::Meshtastic, nineteen.c_str())));
    CHECK_STR(nineteen.c_str(), g.core.settings().meshtasticName);
}

void meshcore_takes_no_chosen_name()
{
    Rig g;
    CHECK_INT(kUnsupported, r(g.name(MeshNetwork::MeshCore, "x")));
    CHECK_STR("", g.core.settings().meshtasticName);
}

void an_unterminated_name_is_rejected()
{
    Rig g;
    g.name(MeshNetwork::Meshtastic, "Hawk");
    LayerTimeCommand c;
    c.type = CommandType::MeshSetOwnName;
    c.setting.network = MeshNetwork::Meshtastic;
    memset(c.setting.name, 'z', sizeof(c.setting.name));
    CHECK_INT(kInvalid, r(g.core.execute(c)));
    CHECK_STR("Hawk", g.core.settings().meshtasticName);
}

void settings_for_missing_parts_are_unsupported_and_unchanged()
{
    LayerTimeCore bare;
    LayerTimeCommand c;
    c.type = CommandType::SetEarlyWarning;
    c.setting.enabled = false;
    CHECK_INT(kUnsupported, r(bare.execute(c)));
    CHECK_TRUE(bare.settings().earlyWarningEnabled);
    c.type = CommandType::MeshSetAdvertising;
    c.setting.enabled = true;
    CHECK_INT(kUnsupported, r(bare.execute(c)));
    CHECK_FALSE(bare.settings().advertisingOn(MeshNetwork::Meshtastic));
    c.type = CommandType::MeshSetOwnName;
    snprintf(c.setting.name, sizeof(c.setting.name), "x");
    CHECK_INT(kUnsupported, r(bare.execute(c)));
    // Presentation settings need nothing.
    c.type = CommandType::SetClockFormat;
    CHECK_INT(kOk, r(bare.execute(c)));
}

void sleep_mode_by_command_suppresses_alerts_but_not_events()
{
    Rig g;
    g.set(CommandType::SetSleepMode, true);
    g.monitor.high("AA:BB:CC:DD:EE:01");
    CHECK_FALSE(g.core.reconState().alertPending);
    CHECK_INT(1, g.core.eventCount());
    g.set(CommandType::SetSleepMode, false);
    g.monitor.high("AA:BB:CC:DD:EE:02");
    CHECK_TRUE(g.core.reconState().alertPending);
}

void set_sleep_mode_is_the_same_as_the_command()
{
    Rig g;
    g.core.setSleepMode(true);
    CHECK_TRUE(g.core.settings().sleepModeEnabled);
    g.monitor.high("AA:BB:CC:DD:EE:01");
    CHECK_FALSE(g.core.reconState().alertPending);
}

void load_takes_the_stored_settings_and_applies_sleep_mode()
{
    Rig g;
    g.store.held.use24Hour = true;
    g.store.held.sleepModeEnabled = true;
    snprintf(g.store.held.meshtasticName, sizeof(g.store.held.meshtasticName), "Hawk");
    g.core.loadSettings();
    CHECK_INT(1, g.store.loads);
    CHECK_TRUE(g.core.settings().use24Hour);
    CHECK_STR("Hawk", g.core.settings().meshtasticName);
    g.monitor.high("AA:BB:CC:DD:EE:01");
    CHECK_FALSE(g.core.reconState().alertPending);
}

void save_writes_the_current_settings_only_when_asked()
{
    Rig g;
    g.set(CommandType::SetUnits, true);
    CHECK_INT(0, g.store.saves);  // commands do not save
    g.core.saveSettings();
    CHECK_INT(1, g.store.saves);
    CHECK_TRUE(g.store.held.metricUnits);
}

void without_a_store_load_and_save_keep_the_defaults()
{
    LayerTimeCore core;
    core.loadSettings();
    core.saveSettings();
    CHECK_TRUE(core.settings().earlyWarningEnabled);
    CHECK_FALSE(core.settings().sleepModeEnabled);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_defaults_are_the_first_boot_values);
    CASE(each_toggle_command_sets_its_setting);
    CASE(the_meshtastic_name_is_set_and_cleared);
    CASE(meshcore_takes_no_chosen_name);
    CASE(an_unterminated_name_is_rejected);
    CASE(settings_for_missing_parts_are_unsupported_and_unchanged);
    CASE(sleep_mode_by_command_suppresses_alerts_but_not_events);
    CASE(set_sleep_mode_is_the_same_as_the_command);
    CASE(load_takes_the_stored_settings_and_applies_sleep_mode);
    CASE(save_writes_the_current_settings_only_when_asked);
    CASE(without_a_store_load_and_save_keep_the_defaults);
    CHECK_SUMMARY();
}
