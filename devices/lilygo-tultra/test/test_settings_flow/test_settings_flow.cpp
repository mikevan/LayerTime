// Characterization tests for the Settings flow as it stood in Phase 0 Step
// 6a, before the settings split: the real Settings screen, the real
// WatchApp::settingsChanged() and begin(), and the real SettingsService on
// the test-only in-memory NVS in test/stubs/Preferences.h. After the split
// the core and the T-Ultra settings store run for real here too; only the
// seams below changed, and every case is as written in 6a.
//
// What is pinned: what each row shows, what each tap saves, what
// settingsChanged() asks of the hardware and in what order, and what the
// watch does with a saved sleep mode or early-warning setting.
//
// Link seams: the other services and screens WatchApp owns are test-only
// fakes below. The GPS and mesh services record what they are asked to do,
// in order. WatchFace and GpsScreen record the settings they are drawn with.
// ReconScreen::create records the core, to read Recon state. Everything the
// tests need goes through the Harness, so that when the settings split
// lands only the Harness changes and every case stays as written.

#include "check.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <Arduino.h>
#include <LilyGoLib.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <esp_wifi.h>

#include "app/WatchApp.h"

// ---------------------------------------------------------------- link seams (test only)

namespace fake_app {
inline std::vector<std::string> g_log;
struct Face { bool drawn = false; bool use24Hour = false; bool metricUnits = false; bool squachify = false; };
inline Face g_face;
inline bool g_gpsMetric = false;
inline WatchFace::SettingsRequestedCallback g_openSettings = nullptr;
inline void *g_openSettingsUser = nullptr;
inline layertime::LayerTimeCore *g_core = nullptr;
void log(const std::string &s) { g_log.push_back(s); }
}

void ClockService::update(WatchState &) {}
void ClockService::setDateTime(int, int, int, int, int, int) {}
void BatteryService::update(WatchState &) {}
void GpsService::begin(bool on) { fake_app::log(on ? "gps:begin:on" : "gps:begin:off"); }
void GpsService::poll(WatchState &) {}
void GpsService::setEnabled(bool on) { fake_app::log(on ? "gps:on" : "gps:off"); }
void MeshService::begin() {}
void MeshService::poll() {}
bool MeshService::sendPublicMessage(const char *) { return false; }
void MeshService::setAdvertisingEnabled(bool on) { fake_app::log(on ? "meshcore:adv:on" : "meshcore:adv:off"); }
bool MeshService::setRadioEnabled(bool on)
{
    fake_app::log(on ? "meshcore:on" : "meshcore:off");
    return on;
}
void MeshtasticService::begin() {}
void MeshtasticService::poll() {}
bool MeshtasticService::sendChannelMessage(uint8_t, const char *) { return false; }
bool MeshtasticService::sendDirectMessage(uint32_t, const char *) { return false; }
bool MeshtasticService::setChannel(uint8_t, const char *, const char *) { return false; }
bool MeshtasticService::removeChannel(uint8_t) { return false; }
void MeshtasticService::setAdvertisingEnabled(bool on) { fake_app::log(on ? "meshtastic:adv:on" : "meshtastic:adv:off"); }
void MeshtasticService::setIdentity(const char *name) { fake_app::log(std::string("meshtastic:name:") + (name ? name : "")); }
void MeshtasticService::setOwnBattery(uint8_t) {}
void MeshtasticService::setOwnPosition(bool, double, double, int32_t) {}
bool MeshtasticService::setRadioEnabled(bool on)
{
    fake_app::log(on ? "meshtastic:on" : "meshtastic:off");
    return on;
}
void SdCardService::begin() {}
bool SdCardService::refresh() { return false; }
bool SdCardService::formatAndMount() { return false; }
uint64_t SdCardService::totalBytes() const { return 0; }
uint64_t SdCardService::usedBytes() const { return 0; }
bool SdCardService::appendCsvRow(const char *, const char *, const char *) { return true; }
void WatchFace::create() {}
void WatchFace::render(const WatchState &, const TUltraSettings &s, const layertime::ApplicationSettings &app,
                       const layertime::ReconState &)
{
    fake_app::g_face.drawn = true;
    fake_app::g_face.use24Hour = app.use24Hour;
    fake_app::g_face.metricUnits = app.metricUnits;
    fake_app::g_face.squachify = s.squachify;
}
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
void GpsScreen::render(const WatchState &, const layertime::ApplicationSettings &s) { fake_app::g_gpsMetric = s.metricUnits; }
void MappingScreen::create(BackCallback, void *) {}
void MappingScreen::show(const WatchState &, const TUltraSettings &) {}
void MappingScreen::render(const WatchState &, const TUltraSettings &) {}
void MeshScreen::create(layertime::LayerTimeCore *, const MeshService *, BackCallback, void *) {}
void MeshScreen::show(const MeshStatus &) {}
void MeshScreen::render(const MeshStatus &) {}
void MeshtasticScreen::create(layertime::LayerTimeCore *, const MeshtasticService *, BackCallback, void *) {}
void MeshtasticScreen::show(const MeshtasticStatus &) {}
void MeshtasticScreen::render(const MeshtasticStatus &) {}
void ReconScreen::create(layertime::LayerTimeCore *core, BackCallback, void *) { fake_app::g_core = core; }
void ReconScreen::show(ReconDetector) {}
void ReconScreen::render() {}

// ---------------------------------------------------------------- harness

namespace {

struct Harness {
    std::unique_ptr<WatchApp> app;

    // `preload` runs against the empty store before boot, to put values
    // there as a watch already in the field would have them.
    explicit Harness(std::function<void()> preload = {})
    {
        fake_lv::reset();
        fake_lv::g_inactiveMs = 0;
        fake_arduino::g_millis = 1000;
        fake_wifi::g_rxCallback = nullptr;
        fake_nimble::g_scan = NimBLEScan{};
        fake_nvs::reset();
        instance = FakeLilyGoInstance{};
        fake_app::g_log.clear();
        fake_app::g_face = fake_app::Face{};
        fake_app::g_gpsMetric = false;
        fake_app::g_openSettings = nullptr;
        fake_app::g_core = nullptr;
        if (preload) preload();
        app.reset(new WatchApp());
        app->begin();
    }
    ~Harness()
    {
        // WatchApp keeps the backlight state in a file-level flag. Leave it
        // lit for the next case, and every radio off.
        fake_lv::g_inactiveMs = 0;
        if (app) app->tick();
        if (fake_app::g_core) {
            layertime::LayerTimeCommand stop;
            stop.type = layertime::CommandType::ReconStop;
            fake_app::g_core->execute(stop);
        }
        app.reset();
        fake_lv::reset();
    }

    // ---- NVS
    static void store(const char *key, bool v) { Preferences p; p.begin("layertime"); p.putBool(key, v); p.end(); }
    static void storeBrightness(uint8_t v) { Preferences p; p.begin("layertime"); p.putUChar("bright", v); p.end(); }
    static void storeName(const char *v) { Preferences p; p.begin("layertime"); p.putString("mtname", v); p.end(); }
    bool storedBool(const char *key) const { return fake_nvs::g_store["layertime"][key].number != 0; }
    long storedNumber(const char *key) const { return fake_nvs::g_store["layertime"][key].number; }
    std::string storedText(const char *key) const { return fake_nvs::g_store["layertime"][key].text; }
    size_t storedKeyCount() const
    {
        auto ns = fake_nvs::g_store.find("layertime");
        return ns == fake_nvs::g_store.end() ? 0 : ns->second.size();
    }
    bool stored(const char *key) const
    {
        auto ns = fake_nvs::g_store.find("layertime");
        return ns != fake_nvs::g_store.end() && ns->second.count(key) == 1;
    }

    // ---- the Settings screen
    void openSettings()
    {
        if (fake_app::g_openSettings) fake_app::g_openSettings(fake_app::g_openSettingsUser);
    }
    void clearLog() { fake_app::g_log.clear(); }
    const std::vector<std::string> &log() const { return fake_app::g_log; }
    void tap(const char *row) { fake_lv::click(fake_lv::findVisibleLabel(row)); }
    std::string value(const char *row)
    {
        bool seen = false;
        for (lv_obj_t *o : fake_lv::g_objects) {
            if (o->kind != FakeKind::Label) continue;
            if (!seen) { seen = (o->text == row); continue; }
            return o->text;
        }
        return "<none>";
    }
    std::string brightnessLabel() { return value("BRIGHTNESS"); }
    lv_obj_t *slider()
    {
        for (lv_obj_t *o : fake_lv::g_objects)
            if (o->rangeMin == 20 && o->rangeMax == 255) return o;
        return nullptr;
    }
    void slide(int32_t v)
    {
        lv_obj_t *s = slider();
        if (!s) return;
        s->value = v;
        fake_lv::fire(s, LV_EVENT_VALUE_CHANGED);
    }
    lv_obj_t *nameField()
    {
        for (lv_obj_t *o : fake_lv::g_objects)
            if (o->kind == FakeKind::Textarea && fake_lv::visible(o) && fake_lv::within(o, fake_lv::g_active)) return o;
        return nullptr;
    }
    void enterName(const char *text, const char *button)
    {
        tap("MESHTASTIC NAME");
        lv_obj_t *f = nameField();
        if (f) lv_textarea_set_text(f, text);
        tap(button);
    }

    // ---- what the rest of the watch sees
    fake_app::Face face() const { return fake_app::g_face; }
    bool gpsScreenMetric() const { return fake_app::g_gpsMetric; }
    bool earlyWarningRunning() const
    {
        return fake_app::g_core && fake_app::g_core->reconState().earlyWarningEnabled;
    }
    uint8_t backlight() const { return instance.brightness; }
    uint32_t vibrations() const { return instance.vibrations; }
    void tick(uint32_t inactiveMs)
    {
        fake_lv::g_inactiveMs = inactiveMs;
        fake_arduino::g_millis += 300;
        app->tick();
    }
    // A Pwnagotchi beacon, which the early-warning sweep catches at High.
    void pwnagotchi()
    {
        std::vector<uint8_t> f(36, 0);
        f[0] = 0x80;
        const uint8_t bssid[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
        memcpy(&f[10], bssid, 6);
        f.push_back(0);
        f.push_back(3);
        f.insert(f.end(), {'p', 'w', 'n'});
        std::vector<uint8_t> buf(sizeof(wifi_promiscuous_pkt_t) + f.size());
        auto *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buf.data());
        pkt->rx_ctrl.rssi = -50;
        pkt->rx_ctrl.channel = 6;
        pkt->rx_ctrl.sig_len = static_cast<unsigned>(f.size());
        memcpy(pkt->payload, f.data(), f.size());
        if (fake_wifi::g_rxCallback) fake_wifi::g_rxCallback(pkt, 0);
    }
};

std::string join(const std::vector<std::string> &v)
{
    std::string s;
    for (const std::string &x : v) s += (s.empty() ? "" : " ") + x;
    return s;
}

} // namespace

// ---------------------------------------------------------------- boot

void boot_applies_the_saved_settings_in_this_order()
{
    Harness h([] {
        Harness::store("gps", false);
        Harness::store("meshadv", true);
        Harness::storeName("Hawk");
        Harness::store("mtadv", true);
    });
    CHECK_STR("gps:begin:off meshcore:adv:on meshtastic:name:Hawk meshtastic:adv:on", join(h.log()).c_str());
}

void a_fresh_watch_boots_lit_with_early_warning_on()
{
    Harness h;
    CHECK_INT(80, h.backlight());
    CHECK_TRUE(h.earlyWarningRunning());
    CHECK_STR("gps:begin:on meshcore:adv:off meshtastic:name: meshtastic:adv:off", join(h.log()).c_str());
}

void saved_brightness_and_early_warning_are_applied_at_boot()
{
    Harness h([] {
        Harness::storeBrightness(150);
        Harness::store("reconew", false);
    });
    CHECK_INT(150, h.backlight());
    CHECK_FALSE(h.earlyWarningRunning());
}

void a_saved_sleep_mode_boots_dark_and_silent()
{
    Harness h([] { Harness::store("sleepmode", true); });
    CHECK_INT(0, h.backlight());
    h.pwnagotchi();
    h.tick(60000);
    CHECK_INT(0, h.vibrations());
    CHECK_INT(0, h.backlight());
}

void without_sleep_mode_a_detection_buzzes()
{
    Harness h;
    h.pwnagotchi();
    h.tick(0);
    CHECK_INT(1, h.vibrations());
}

// ---------------------------------------------------------------- the rows

void every_row_opens_showing_the_saved_value()
{
    Harness h([] {
        Harness::storeBrightness(150);
        Harness::store("clock24", true);
        Harness::store("metric", true);
        Harness::store("gps", false);
        Harness::store("meshadv", true);
        Harness::store("mtadv", true);
        Harness::storeName("Hawk");
        Harness::store("reconew", false);
        Harness::store("reconsd", true);
        Harness::store("sleepmode", true);
        Harness::store("squach", true);
    });
    h.openSettings();
    CHECK_STR("150", h.brightnessLabel().c_str());
    CHECK_INT(150, h.slider() ? h.slider()->value : -1);
    CHECK_STR("24 H", h.value("CLOCK FORMAT").c_str());
    CHECK_STR("METRIC", h.value("UNITS").c_str());
    CHECK_STR("OFF", h.value("GPS").c_str());
    CHECK_STR("OFF", h.value("MESHCORE").c_str());
    CHECK_STR("ON", h.value("MESHCORE ADVERTISE").c_str());
    CHECK_STR("OFF", h.value("MESHTASTIC").c_str());
    CHECK_STR("ON", h.value("MESHTASTIC ADVERTISE").c_str());
    CHECK_STR("Hawk", h.value("MESHTASTIC NAME").c_str());
    CHECK_STR("OFF", h.value("EARLY WARNING").c_str());
    CHECK_STR("ON", h.value("SD LOGGING").c_str());
    CHECK_STR("ON", h.value("SLEEP MODE").c_str());
    CHECK_STR("NO FILE", h.value("SQUACHIFY?").c_str());  // no card in the test
}

void a_fresh_watch_opens_on_the_defaults()
{
    Harness h;
    h.openSettings();
    CHECK_STR("80", h.brightnessLabel().c_str());
    CHECK_STR("12 H", h.value("CLOCK FORMAT").c_str());
    CHECK_STR("IMPERIAL", h.value("UNITS").c_str());
    CHECK_STR("ON", h.value("GPS").c_str());
    CHECK_STR("AUTO", h.value("MESHTASTIC NAME").c_str());
    CHECK_STR("ON", h.value("EARLY WARNING").c_str());
    CHECK_STR("OFF", h.value("SLEEP MODE").c_str());
}

void each_toggle_flips_its_row_and_saves_it()
{
    Harness h;
    h.openSettings();
    struct Row { const char *row; const char *key; const char *on; const char *off; bool startsOn; };
    const Row rows[] = {
        {"CLOCK FORMAT", "clock24", "24 H", "12 H", false},
        {"UNITS", "metric", "METRIC", "IMPERIAL", false},
        {"GPS", "gps", "ON", "OFF", true},
        {"MESHCORE ADVERTISE", "meshadv", "ON", "OFF", false},
        {"MESHTASTIC ADVERTISE", "mtadv", "ON", "OFF", false},
        {"EARLY WARNING", "reconew", "ON", "OFF", true},
        {"SD LOGGING", "reconsd", "ON", "OFF", false},
        {"SLEEP MODE", "sleepmode", "ON", "OFF", false},
    };
    for (const Row &r : rows) {
        h.tap(r.row);
        CHECK_STR(r.startsOn ? r.off : r.on, h.value(r.row).c_str());
        CHECK_TRUE(h.storedBool(r.key) == !r.startsOn);
        h.tap(r.row);
        CHECK_STR(r.startsOn ? r.on : r.off, h.value(r.row).c_str());
        CHECK_TRUE(h.storedBool(r.key) == r.startsOn);
    }
}

void squachify_saves_even_with_no_art_on_the_card()
{
    Harness h;
    h.openSettings();
    h.tap("SQUACHIFY?");
    CHECK_STR("NO FILE", h.value("SQUACHIFY?").c_str());
    CHECK_TRUE(h.storedBool("squach"));
    CHECK_TRUE(h.face().squachify);
}

void the_brightness_slider_sets_label_backlight_and_store()
{
    Harness h;
    h.openSettings();
    h.slide(200);
    CHECK_STR("200", h.brightnessLabel().c_str());
    CHECK_INT(200, h.backlight());
    CHECK_INT(200, h.storedNumber("bright"));
}

void the_meshtastic_name_saves_and_shows_auto_when_empty()
{
    Harness h;
    h.openSettings();
    h.enterName("Ranger 7", "SAVE");
    CHECK_STR("Ranger 7", h.value("MESHTASTIC NAME").c_str());
    CHECK_STR("Ranger 7", h.storedText("mtname").c_str());
    CHECK_TRUE(h.log().size() > 0 && std::string(join(h.log())).find("meshtastic:name:Ranger 7") != std::string::npos);
    h.enterName("", "SAVE");
    CHECK_STR("AUTO", h.value("MESHTASTIC NAME").c_str());
    CHECK_STR("", h.storedText("mtname").c_str());
}

void the_name_page_opens_on_the_current_name_and_cancel_keeps_it()
{
    Harness h([] { Harness::storeName("Hawk"); });
    h.openSettings();
    h.tap("MESHTASTIC NAME");
    CHECK_STR("Hawk", h.nameField() ? h.nameField()->text.c_str() : "<none>");
    CHECK_INT(19, h.nameField() ? h.nameField()->maxLength : 0);
    lv_textarea_set_text(h.nameField(), "Eagle");
    h.clearLog();
    h.tap("CANCEL");
    CHECK_STR("Hawk", h.value("MESHTASTIC NAME").c_str());
    CHECK_STR("Hawk", h.storedText("mtname").c_str());
    CHECK_INT(0, h.log().size());
}

void a_name_is_cut_to_19_characters()
{
    Harness h;
    h.openSettings();
    h.enterName("abcdefghijklmnopqrstuvwxyz", "SAVE");
    CHECK_STR("abcdefghijklmnopqrs", h.storedText("mtname").c_str());
}

void the_radio_switches_are_never_saved()
{
    Harness h;
    h.openSettings();
    h.tap("MESHCORE");
    h.tap("MESHTASTIC");
    CHECK_TRUE(h.stored("clock24"));  // the save did happen
    CHECK_INT(11, h.storedKeyCount()); // the eleven keys, and no radio switch
}

// ---------------------------------------------------------------- settingsChanged

void any_change_reapplies_everything_in_this_order()
{
    Harness h([] {
        Harness::store("meshadv", true);
        Harness::storeName("Hawk");
    });
    h.openSettings();
    h.clearLog();
    h.tap("UNITS");
    CHECK_STR("gps:on meshcore:off meshtastic:off meshcore:adv:on meshtastic:name:Hawk meshtastic:adv:off",
              join(h.log()).c_str());
}

void the_face_and_gps_screen_are_redrawn_with_the_new_clock_and_units()
{
    Harness h;
    h.openSettings();
    h.tap("CLOCK FORMAT");
    CHECK_TRUE(h.face().use24Hour);
    CHECK_FALSE(h.face().metricUnits);
    h.tap("UNITS");
    CHECK_TRUE(h.face().metricUnits);
    CHECK_TRUE(h.gpsScreenMetric());
}

void the_early_warning_row_starts_and_stops_the_background_sweep()
{
    Harness h;
    h.openSettings();
    CHECK_TRUE(h.earlyWarningRunning());
    h.tap("EARLY WARNING");
    CHECK_FALSE(h.earlyWarningRunning());
    h.tap("EARLY WARNING");
    CHECK_TRUE(h.earlyWarningRunning());
}

void sleep_mode_on_goes_dark_at_once_and_silences_alerts()
{
    Harness h;
    h.openSettings();
    h.tap("SLEEP MODE");
    CHECK_INT(0, h.backlight());
    h.pwnagotchi();
    h.tick(60000);
    CHECK_INT(0, h.vibrations());
    CHECK_INT(0, h.backlight());
}

void sleep_mode_blank_is_undone_by_the_next_recent_activity_tick_KNOWN_DEFECT()
{
    // Turning sleep mode on blanks the backlight from inside the tap. The
    // tap itself is fresh activity, and tick() relights any blanked display
    // whose inactivity is under 15 s, so the very next pass turns it back on.
    // On the watch that is 2 ms later. The alert silencing is unaffected.
    Harness h;
    h.openSettings();
    h.tap("SLEEP MODE");
    CHECK_INT(0, h.backlight());
    h.tick(0);
    CHECK_INT(80, h.backlight());
}

void a_saved_sleep_mode_boot_blank_is_undone_by_recent_activity_KNOWN_DEFECT()
{
    // Same path as above, at boot: begin() blanks for a saved sleep mode, and
    // the first tick relights it unless LVGL already reports 15 s of
    // inactivity. The alert silencing is unaffected.
    Harness h([] { Harness::store("sleepmode", true); });
    CHECK_INT(0, h.backlight());
    h.tick(0);
    CHECK_INT(80, h.backlight());
    h.pwnagotchi();
    h.tick(0);
    CHECK_INT(0, h.vibrations());
}

void sleep_mode_off_lets_alerts_through_again()
{
    Harness h([] { Harness::store("sleepmode", true); });
    h.openSettings();
    h.tap("SLEEP MODE");
    h.pwnagotchi();
    h.tick(0);
    CHECK_INT(1, h.vibrations());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(boot_applies_the_saved_settings_in_this_order);
    CASE(a_fresh_watch_boots_lit_with_early_warning_on);
    CASE(saved_brightness_and_early_warning_are_applied_at_boot);
    CASE(a_saved_sleep_mode_boots_dark_and_silent);
    CASE(without_sleep_mode_a_detection_buzzes);
    CASE(every_row_opens_showing_the_saved_value);
    CASE(a_fresh_watch_opens_on_the_defaults);
    CASE(each_toggle_flips_its_row_and_saves_it);
    CASE(squachify_saves_even_with_no_art_on_the_card);
    CASE(the_brightness_slider_sets_label_backlight_and_store);
    CASE(the_meshtastic_name_saves_and_shows_auto_when_empty);
    CASE(the_name_page_opens_on_the_current_name_and_cancel_keeps_it);
    CASE(a_name_is_cut_to_19_characters);
    CASE(the_radio_switches_are_never_saved);
    CASE(any_change_reapplies_everything_in_this_order);
    CASE(the_face_and_gps_screen_are_redrawn_with_the_new_clock_and_units);
    CASE(the_early_warning_row_starts_and_stops_the_background_sweep);
    CASE(sleep_mode_on_goes_dark_at_once_and_silences_alerts);
    CASE(sleep_mode_blank_is_undone_by_the_next_recent_activity_tick_KNOWN_DEFECT);
    CASE(a_saved_sleep_mode_boot_blank_is_undone_by_recent_activity_KNOWN_DEFECT);
    CASE(sleep_mode_off_lets_alerts_through_again);
    CHECK_SUMMARY();
}
