// Characterization tests for mesh radio exclusivity: MeshCore and
// Meshtastic share one SX1262, so only one may own it at a time. Deferred
// from Phase 0 Step 2 as uncharacterized; written in Step 5a.
//
// Two pieces of existing code keep them apart, and both run for real here:
//   * SettingsScreen's MESHCORE and MESHTASTIC rows turn the other flag off
//     when one is turned on.
//   * WatchApp::settingsChanged() then switches radios off before it
//     switches any on, so both drivers never hold the radio at once.
// The test drives the real Settings rows and reads back, in order, every
// setRadioEnabled() call WatchApp makes.
//
// Link seams: every other class WatchApp owns is replaced by the test-only
// fakes below. The two mesh services record their setRadioEnabled() calls.
// WatchFace::setSettingsRequestedCallback records the hook that opens
// Settings, which is how the test gets there the way the wearer does.

#include "check.h"

#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_wifi.h>

#include "app/WatchApp.h"

// ---------------------------------------------------------------- link seams (test only)

namespace fake_app {
inline std::vector<std::string> g_radio;
inline AppSettings g_settings;
inline WatchFace::SettingsRequestedCallback g_openSettings = nullptr;
inline void *g_openSettingsUser = nullptr;
}

void ClockService::update(WatchState &) {}
void ClockService::setDateTime(int, int, int, int, int, int) {}
void BatteryService::update(WatchState &) {}
void GpsService::begin(bool) {}
void GpsService::poll(WatchState &) {}
void GpsService::setEnabled(bool) {}
void MeshService::begin() {}
void MeshService::poll() {}
bool MeshService::sendPublicMessage(const char *) { return false; }
void MeshService::setAdvertisingEnabled(bool) {}
bool MeshService::setRadioEnabled(bool on)
{
    fake_app::g_radio.push_back(on ? "meshcore:on" : "meshcore:off");
    return on;
}
void MeshtasticService::begin() {}
void MeshtasticService::poll() {}
bool MeshtasticService::sendChannelMessage(uint8_t, const char *) { return false; }
bool MeshtasticService::sendDirectMessage(uint32_t, const char *) { return false; }
bool MeshtasticService::setChannel(uint8_t, const char *, const char *) { return false; }
bool MeshtasticService::removeChannel(uint8_t) { return false; }
void MeshtasticService::setAdvertisingEnabled(bool) {}
void MeshtasticService::setIdentity(const char *) {}
void MeshtasticService::setOwnBattery(uint8_t) {}
void MeshtasticService::setOwnPosition(bool, double, double, int32_t) {}
bool MeshtasticService::setRadioEnabled(bool on)
{
    fake_app::g_radio.push_back(on ? "meshtastic:on" : "meshtastic:off");
    return on;
}
void SdCardService::begin() {}
bool SdCardService::refresh() { return false; }
bool SdCardService::formatAndMount() { return false; }
uint64_t SdCardService::totalBytes() const { return 0; }
uint64_t SdCardService::usedBytes() const { return 0; }
bool SdCardService::appendCsvRow(const char *, const char *, const char *) { return true; }
void SettingsService::load(AppSettings &s) { s = fake_app::g_settings; }
void SettingsService::apply(const AppSettings &) {}
void SettingsService::save(const AppSettings &) {}
void WatchFace::create() {}
void WatchFace::render(const WatchState &, const AppSettings &, const layertime::ApplicationSettings &,
                       const layertime::ReconState &) {}
void WatchFace::setSettingsRequestedCallback(SettingsRequestedCallback cb, void *u)
{
    fake_app::g_openSettings = cb;
    fake_app::g_openSettingsUser = u;
}
void WatchFace::setGpsRequestedCallback(GpsRequestedCallback, void *) {}
void WatchFace::setMeshRequestedCallback(MeshRequestedCallback, void *) {}
void WatchFace::setMeshtasticRequestedCallback(MeshtasticRequestedCallback, void *) {}
void WatchFace::setReconRequestedCallback(ReconRequestedCallback, void *) {}
void WatchFace::setThreatsRequestedCallback(ThreatsRequestedCallback, void *) {}
void WatchFace::setMappingRequestedCallback(MappingRequestedCallback, void *) {}
void GpsScreen::create(BackCallback, void *) {}
void GpsScreen::show(const WatchState &, const layertime::ApplicationSettings &) {}
void GpsScreen::render(const WatchState &, const layertime::ApplicationSettings &) {}
void MappingScreen::create(BackCallback, void *) {}
void MappingScreen::show(const WatchState &, const AppSettings &) {}
void MappingScreen::render(const WatchState &, const AppSettings &) {}
void MeshScreen::create(layertime::LayerTimeCore *, const MeshService *, BackCallback, void *) {}
void MeshScreen::show(const MeshStatus &) {}
void MeshScreen::render(const MeshStatus &) {}
void MeshtasticScreen::create(layertime::LayerTimeCore *, const MeshtasticService *, BackCallback, void *) {}
void MeshtasticScreen::show(const MeshtasticStatus &) {}
void MeshtasticScreen::render(const MeshtasticStatus &) {}
void ReconScreen::create(layertime::LayerTimeCore *, BackCallback, void *) {}
void ReconScreen::show(ReconDetector) {}
void ReconScreen::render() {}

// ---------------------------------------------------------------- harness

namespace {

struct Harness {
    std::unique_ptr<WatchApp> app;

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 1000;
        fake_wifi::g_rxCallback = nullptr;
        fake_nimble::g_scan = NimBLEScan{};
        fake_app::g_radio.clear();
        fake_app::g_settings = AppSettings{};
        // Keep Recon's radios out of it. Early warning is an application
        // setting since Phase 0 Step 6, loaded by the core from NVS.
        fake_nvs::reset();
        Preferences prefs;
        prefs.begin("layertime");
        prefs.putBool("reconew", false);
        prefs.end();
        fake_app::g_openSettings = nullptr;
        app.reset(new WatchApp());
        app->begin();
        if (fake_app::g_openSettings) fake_app::g_openSettings(fake_app::g_openSettingsUser);
    }
    ~Harness()
    {
        app.reset();
        fake_lv::reset();
    }

    const std::vector<std::string> &radio() const { return fake_app::g_radio; }
    void clearLog() { fake_app::g_radio.clear(); }
    void tap(const char *row) { fake_lv::click(fake_lv::findVisibleLabel(row)); }

    // The ON/OFF value shown beside a Settings row.
    std::string value(const char *row)
    {
        bool seen = false;
        for (lv_obj_t *o : fake_lv::g_objects) {
            if (o->kind != FakeKind::Label) continue;
            if (!seen) { seen = (o->text == row); continue; }
            if (o->text == "ON" || o->text == "OFF") return o->text;
        }
        return "<none>";
    }
};

std::string join(const std::vector<std::string> &v)
{
    std::string s;
    for (const std::string &x : v) s += (s.empty() ? "" : " ") + x;
    return s;
}

// Replays the radio log and reports whether both were ever on together.
bool everBothOn(const std::vector<std::string> &log)
{
    bool core = false, tastic = false;
    for (const std::string &e : log) {
        if (e == "meshcore:on") core = true;
        if (e == "meshcore:off") core = false;
        if (e == "meshtastic:on") tastic = true;
        if (e == "meshtastic:off") tastic = false;
        if (core && tastic) return true;
    }
    return false;
}

} // namespace

void boot_leaves_both_radios_untouched_and_off()
{
    Harness h;
    CHECK_STR("", join(h.radio()).c_str());
    CHECK_STR("OFF", h.value("MESHCORE").c_str());
    CHECK_STR("OFF", h.value("MESHTASTIC").c_str());
}

void meshcore_on_switches_meshtastic_off_first()
{
    Harness h;
    h.tap("MESHCORE");
    CHECK_STR("meshtastic:off meshcore:on", join(h.radio()).c_str());
    CHECK_STR("ON", h.value("MESHCORE").c_str());
    CHECK_STR("OFF", h.value("MESHTASTIC").c_str());
}

void switching_to_meshtastic_turns_meshcore_off_before_meshtastic_on()
{
    Harness h;
    h.tap("MESHCORE");
    h.clearLog();
    h.tap("MESHTASTIC");
    CHECK_STR("meshcore:off meshtastic:on", join(h.radio()).c_str());
    CHECK_STR("OFF", h.value("MESHCORE").c_str());
    CHECK_STR("ON", h.value("MESHTASTIC").c_str());
}

void switching_back_to_meshcore_turns_meshtastic_off_first()
{
    Harness h;
    h.tap("MESHTASTIC");
    h.clearLog();
    h.tap("MESHCORE");
    CHECK_STR("meshtastic:off meshcore:on", join(h.radio()).c_str());
}

void turning_the_active_one_off_leaves_both_off()
{
    Harness h;
    h.tap("MESHTASTIC");
    h.clearLog();
    h.tap("MESHTASTIC");
    CHECK_STR("meshcore:off meshtastic:off", join(h.radio()).c_str());
    CHECK_STR("OFF", h.value("MESHTASTIC").c_str());
}

void any_settings_change_reapplies_the_radio_state()
{
    // settingsChanged() runs the whole radio sequence for every change,
    // including ones that have nothing to do with the radios.
    Harness h;
    h.tap("MESHCORE");
    h.clearLog();
    h.tap("MESHCORE ADVERTISE");
    CHECK_STR("meshtastic:off meshcore:on", join(h.radio()).c_str());
}

void no_sequence_of_rows_ever_has_both_on()
{
    Harness h;
    for (const char *row : {"MESHCORE", "MESHTASTIC", "MESHCORE", "MESHCORE", "MESHTASTIC",
                            "MESHTASTIC ADVERTISE", "MESHCORE", "MESHTASTIC", "MESHTASTIC"})
        h.tap(row);
    CHECK_FALSE(everBothOn(h.radio()));
    CHECK_TRUE(h.radio().size() >= 18);  // every tap ran the sequence
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(boot_leaves_both_radios_untouched_and_off);
    CASE(meshcore_on_switches_meshtastic_off_first);
    CASE(switching_to_meshtastic_turns_meshcore_off_before_meshtastic_on);
    CASE(switching_back_to_meshcore_turns_meshtastic_off_first);
    CASE(turning_the_active_one_off_leaves_both_off);
    CASE(any_settings_change_reapplies_the_radio_state);
    CASE(no_sequence_of_rows_ever_has_both_on);
    CHECK_SUMMARY();
}
