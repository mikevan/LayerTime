// Characterization tests for src/platform/twatch_ultra/ui/ReconScreen. Written in Phase 0 Step 4a
// against the screen as it stood, before the detection log and the Recon
// commands moved into core.
//
// The screen is compiled against the test-only LVGL fake, driven by its own
// buttons, and read back from its labels. Detections come in through the
// real ReconService entry points. Everything the tests need from the app
// goes through the Harness below. Step 4 rewired the screen onto core; only
// the Harness changed, and every case is as written in 4a.

#include "check.h"

#include <string>
#include <vector>

#include <Arduino.h>
#include <LilyGoLib.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "core/app/LayerTimeCore.h"
#include "platform/twatch_ultra/TUltraAlertSink.h"
#include "platform/twatch_ultra/TUltraMonitorSource.h"
#include "platform/twatch_ultra/services/ReconService.h"
#include "platform/twatch_ultra/ui/ReconScreen.h"

namespace {

// The core wired to the T-Ultra adapters, as WatchApp wires it.
struct Harness {
    ReconService svc;
    layertime::twatch_ultra::TUltraMonitorSource monitor{svc};
    layertime::twatch_ultra::TUltraAlertSink alerts;
    layertime::LayerTimeCore core;
    ReconScreen screen;
    int backs = 0;

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 1000;
        fake_wifi::g_rxCallback = nullptr;
        fake_nimble::g_scan = NimBLEScan{};
        instance = FakeLilyGoInstance{};
        layertime::CorePorts p;
        p.monitor = &monitor;
        p.alerts = &alerts;
        core.attach(p);
        screen.create(&core, [](void *u) { ++static_cast<Harness *>(u)->backs; }, this);
    }
    ~Harness()
    {
        svc.stop();
        fake_lv::reset();
    }

    // ---- app state, as the screen's owner sees it
    bool monitoring() const { return core.reconState().monitoring; }
    ReconDetector selected() const { return core.reconState().selected; }
    bool alertPending() const { return core.reconState().alertPending; }
    size_t eventCount() const { return core.reconState().eventCount; }
    void tick() { core.tick(millis()); }

    // ---- the screen
    void open(ReconDetector d = ReconDetector::None) { screen.show(d); }
    void render() { screen.render(); }
    void tap(const char *label) { fake_lv::click(fake_lv::findVisibleLabel(label)); }
    bool shows(const char *label) { return fake_lv::findVisibleLabel(label) != nullptr; }
    std::string title()
    {
        // The title is the first label on the screen object itself after BACK.
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->parent == fake_lv::g_active && l->text != "LAYERTIME") return l->text;
        return "<none>";
    }
    std::string statusLine()
    {
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->text.find("  x") != std::string::npos && l->text.find('\n') == std::string::npos)
                return l->text;
        return "<none>";
    }
    std::string results()
    {
        lv_obj_t *l = fake_lv::findVisibleLabelStartingWith("No activity");
        if (l) return l->text;
        for (lv_obj_t *x : fake_lv::visibleLabels())
            if (x->text.find(" dBm") != std::string::npos && x->parent != fake_lv::g_active &&
                !fake_lv::within(x, fake_lv::g_top))
                return x->text;
        return "<none>";
    }
    std::string alertText()
    {
        if (!fake_lv::g_top) return "<hidden>";
        for (lv_obj_t *x : fake_lv::visibleLabels())
            if (fake_lv::within(x, fake_lv::g_top) && x->text.find(" dBm") != std::string::npos)
                return x->text;
        return "<hidden>";
    }
    std::vector<std::string> menuRows()
    {
        std::vector<std::string> out;
        for (lv_obj_t *l : fake_lv::visibleLabels())
            if (l->parent && l->parent->kind == FakeKind::Button && l->text != "BACK" &&
                l->text != "CLEAR LOG" && !fake_lv::within(l, fake_lv::g_top))
                out.push_back(l->text);
        return out;
    }

    // ---- detections, through the real radio entry points
    void pwnagotchiBeacon(int8_t rssi, uint8_t channel)
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
        pkt->rx_ctrl.rssi = rssi;
        pkt->rx_ctrl.channel = channel;
        pkt->rx_ctrl.sig_len = static_cast<unsigned>(f.size());
        memcpy(pkt->payload, f.data(), f.size());
        if (fake_wifi::g_rxCallback) fake_wifi::g_rxCallback(pkt, 0);
    }
    void bleAdvert(const uint8_t printed[6], int rssi, std::initializer_list<uint16_t> uuids)
    {
        NimBLEAdvertisedDevice d;
        d.address = NimBLEAddress::fromPrinted(printed);
        d.rssi = rssi;
        for (uint16_t u : uuids) d.serviceUuids.push_back(NimBLEUUID(u));
        svc.handleBleAdvertisement(&d);
    }
};

std::string join(const std::vector<std::string> &v)
{
    std::string s;
    for (const std::string &x : v) s += (s.empty() ? "" : " | ") + x;
    return s;
}

const uint8_t kMac[6] = {0x0A, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F};

} // namespace

// ---------------------------------------------------------------- menu

void top_menu_lists_all_then_the_three_groups()
{
    Harness h;
    h.open();
    CHECK_STR("RECON", h.title().c_str());
    CHECK_STR("ALL | TRACKERS | COUNTER-SURVEIL | COUNTER-INTRUSION", join(h.menuRows()).c_str());
}

void a_group_page_lists_all_then_its_members()
{
    Harness h;
    h.open();
    h.tap("COUNTER-INTRUSION");
    CHECK_STR("COUNTER-INTRUSION", h.title().c_str());
    CHECK_STR("ALL | DEAUTH | PWNAGOTCHI | MULTISSID | PINEAPPLE | FLIPPER",
              join(h.menuRows()).c_str());
    CHECK_FALSE(h.monitoring());  // opening a group starts nothing
    h.tap("BACK");
    CHECK_STR("RECON", h.title().c_str());
    h.tap("BACK");
    CHECK_INT(1, h.backs);
}

void choosing_a_detector_starts_it_and_shows_the_monitor()
{
    Harness h;
    h.open();
    h.tap("TRACKERS");
    h.tap("TILE");
    CHECK_TRUE(h.monitoring());
    CHECK_INT(static_cast<int>(ReconDetector::Tile), static_cast<int>(h.selected()));
    CHECK_STR("TILE", h.title().c_str());
    CHECK_STR("TILE  x0", h.statusLine().c_str());
    CHECK_STR("No activity detected.", h.results().c_str());
}

void a_group_all_row_starts_the_group()
{
    Harness h;
    h.open();
    h.tap("TRACKERS");
    h.tap("ALL");
    CHECK_INT(static_cast<int>(ReconDetector::Trackers), static_cast<int>(h.selected()));
    CHECK_STR("TRACKERS", h.title().c_str());
}

void opening_straight_into_a_detector_skips_the_menu()
{
    Harness h;
    h.open(ReconDetector::All);
    CHECK_TRUE(h.monitoring());
    CHECK_STR("ALL", h.title().c_str());
    CHECK_STR("ALL  x0", h.statusLine().c_str());
}

void back_from_the_monitor_stops_and_returns_to_the_group()
{
    Harness h;
    h.open();
    h.tap("COUNTER-SURVEIL");
    h.tap("AXON");
    CHECK_TRUE(h.monitoring());
    h.tap("BACK");
    CHECK_FALSE(h.monitoring());
    CHECK_STR("COUNTER-SURVEIL", h.title().c_str());
    CHECK_INT(0, h.backs);
}

// ---------------------------------------------------------------- the log

void log_lines_show_channel_for_wifi_and_not_for_ble()
{
    Harness h;
    h.open(ReconDetector::All);
    h.pwnagotchiBeacon(-55, 11);
    fake_arduino::g_millis += 12000;  // All starts on Wi-Fi; wait for its BLE burst
    h.tick();
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    CHECK_STR("ALL  x3", h.statusLine().c_str());  // Pwnagotchi, Pineapple, Tile
    CHECK_STR("PWNAGOTCHI  [HIGH]\nPwnagotchi beacon\nDE:AD:BE:EF:DE:AD  -55 dBm  CH 11  x1\n\n"
              "PINEAPPLE  [MED]\nSuspicious Pineapple OUI\nDE:AD:BE:EF:DE:AD  -55 dBm  CH 11  x1\n\n"
              "TILE  [HIGH]\nTile tracker\n0a:1b:2c:3d:4e:5f  -71 dBm  x1\n\n",
              h.results().c_str());
}

void repeat_sightings_update_the_count_in_place()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.bleAdvert(kMac, -60, {0xFEED});
    h.render();
    CHECK_STR("TILE  x1", h.statusLine().c_str());
    CHECK_STR("TILE  [HIGH]\nTile tracker\n0a:1b:2c:3d:4e:5f  -60 dBm  x2\n\n", h.results().c_str());
}

void clear_log_empties_the_list_and_keeps_monitoring()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    h.tap("CLEAR LOG");
    CHECK_STR("TILE  x0", h.statusLine().c_str());
    CHECK_STR("No activity detected.", h.results().c_str());
    CHECK_TRUE(h.monitoring());
    CHECK_FALSE(h.alertPending());
    CHECK_INT(0, h.eventCount());
}

void the_log_survives_leaving_and_reentering()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.tap("BACK");
    h.open(ReconDetector::Deauth);
    CHECK_STR("DEAUTH  x1", h.statusLine().c_str());
}

// ---------------------------------------------------------------- the alert

void a_new_alerting_detection_pops_the_alert_over_any_screen()
{
    Harness h;
    h.open(ReconDetector::Tile);
    lv_screen_load(nullptr);  // the wearer has gone elsewhere
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    CHECK_STR("TILE  [HIGH]\nTile tracker\n0a:1b:2c:3d:4e:5f\n-71 dBm", h.alertText().c_str());
    CHECK_TRUE(h.shows("RECON ALERT"));
}

void the_alert_shows_the_newest_record()
{
    Harness h;
    h.open(ReconDetector::All);
    h.pwnagotchiBeacon(-55, 11);  // two records: Pwnagotchi then Pineapple
    h.render();
    CHECK_STR("PINEAPPLE  [MED]\nSuspicious Pineapple OUI\nDE:AD:BE:EF:DE:AD\n-55 dBm",
              h.alertText().c_str());
}

void dismiss_acknowledges_and_hides()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    h.tap("DISMISS");
    CHECK_FALSE(h.alertPending());
    CHECK_STR("<hidden>", h.alertText().c_str());
    h.render();
    CHECK_STR("<hidden>", h.alertText().c_str());
}

void a_repeat_sighting_does_not_pop_the_alert_again()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    h.tap("DISMISS");
    h.bleAdvert(kMac, -50, {0xFEED});
    h.render();
    CHECK_STR("<hidden>", h.alertText().c_str());
}

// While an alert is still up, the popup is drawn once per new record. A
// repeat sighting updates the record but not the popup text.
void a_pending_alert_is_not_redrawn_for_a_repeat()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.render();
    h.bleAdvert(kMac, -50, {0xFEED});
    h.render();
    CHECK_TRUE(h.alertPending());
    CHECK_STR("TILE  [HIGH]\nTile tracker\n0a:1b:2c:3d:4e:5f\n-71 dBm", h.alertText().c_str());
    CHECK_STR("TILE  [HIGH]\nTile tracker\n0a:1b:2c:3d:4e:5f  -50 dBm  x2\n\n", h.results().c_str());
}

void low_confidence_never_pops_the_alert()
{
    Harness h;
    h.open(ReconDetector::Meta);
    h.bleAdvert(kMac, -71, {0xFEB7});
    h.render();
    CHECK_STR("<hidden>", h.alertText().c_str());
    CHECK_STR("META  x1", h.statusLine().c_str());
}

void the_watch_vibrates_once_per_new_alerting_record()
{
    Harness h;
    h.open(ReconDetector::Tile);
    h.bleAdvert(kMac, -71, {0xFEED});
    h.tick();
    h.tick();
    CHECK_INT(1, instance.vibrations);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(top_menu_lists_all_then_the_three_groups);
    CASE(a_group_page_lists_all_then_its_members);
    CASE(choosing_a_detector_starts_it_and_shows_the_monitor);
    CASE(a_group_all_row_starts_the_group);
    CASE(opening_straight_into_a_detector_skips_the_menu);
    CASE(back_from_the_monitor_stops_and_returns_to_the_group);
    CASE(log_lines_show_channel_for_wifi_and_not_for_ble);
    CASE(repeat_sightings_update_the_count_in_place);
    CASE(clear_log_empties_the_list_and_keeps_monitoring);
    CASE(the_log_survives_leaving_and_reentering);
    CASE(a_new_alerting_detection_pops_the_alert_over_any_screen);
    CASE(the_alert_shows_the_newest_record);
    CASE(dismiss_acknowledges_and_hides);
    CASE(a_repeat_sighting_does_not_pop_the_alert_again);
    CASE(a_pending_alert_is_not_redrawn_for_a_repeat);
    CASE(low_confidence_never_pops_the_alert);
    CASE(the_watch_vibrates_once_per_new_alerting_record);
    CHECK_SUMMARY();
}
