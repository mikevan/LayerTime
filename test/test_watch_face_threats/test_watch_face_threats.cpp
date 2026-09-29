// Characterization tests for the THREATS block on src/ui/WatchFace. Written
// in Phase 0 Step 4a against the face as it stood, before the detection log
// moved into core.
//
// The face is compiled against the test-only LVGL fake. The owl and Squachy
// logos are replaced by the no-op fakes below, since they draw images and
// read the SD card and play no part in the THREATS block. Recon state comes
// from the real ReconService, driven through its public entry points, and
// the core, wired as WatchApp wires it. Everything the tests need from the
// app goes through the Harness. Step 4 changed where the face reads Recon
// state from; only the Harness changed, and every case is as written in 4a.

#include "check.h"

#include <string>

#include <Arduino.h>
#include <LilyGoLib.h>
#include <NimBLEDevice.h>
#include <esp_wifi.h>

#include "core/app/LayerTimeCore.h"
#include "platform/twatch_ultra/TUltraAlertSink.h"
#include "platform/twatch_ultra/TUltraMonitorSource.h"
#include "services/ReconService.h"
#include "ui/WatchFace.h"

// ---- link fakes for the two logo widgets
void OwlLogo::create(lv_obj_t *, int, int, int, int) {}
void OwlLogo::setHidden(bool) {}
bool SquachLogo::create(lv_obj_t *, int, int, int, int) { return false; }
void SquachLogo::setHidden(bool) {}

namespace {

constexpr uint32_t kGreen = 0x63E06B;
constexpr uint32_t kDanger = 0xE0524A;

// The Recon calls the face's owner makes, routed as WatchApp routes them.
struct Recon {
    ReconService radio;
    layertime::twatch_ultra::TUltraMonitorSource monitor{radio};
    layertime::LayerTimeCore core;

    Recon()
    {
        layertime::CorePorts p;
        p.monitor = &monitor;
        core.attach(p);
    }
    void run(layertime::CommandType t, ReconDetector d = ReconDetector::None)
    {
        layertime::LayerTimeCommand c;
        c.type = t;
        c.reconTarget.target = d;
        core.execute(c);
    }
    void startDetector(ReconDetector d) { run(layertime::CommandType::ReconStart, d); }
    void exitManualMode() { run(layertime::CommandType::ReconStop); }
    void clearDetections() { run(layertime::CommandType::ReconClearEvents); }
    void acknowledgeAlert() { run(layertime::CommandType::ReconAcknowledgeAlert); }
    void setEarlyWarningEnabled(bool on) { radio.setEarlyWarningEnabled(on); }
    void stop() { radio.stop(); }
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *d) { radio.handleBleAdvertisement(d); }
};

struct Harness {
    Recon svc;
    WatchFace face;
    WatchState state;
    AppSettings settings;

    Harness()
    {
        fake_lv::reset();
        fake_arduino::g_millis = 1000;
        fake_wifi::g_rxCallback = nullptr;
        fake_nimble::g_scan = NimBLEScan{};
        instance = FakeLilyGoInstance{};
        // The face builds on whatever screen is active at boot.
        lv_screen_load(lv_obj_create(nullptr));
        face.create();
    }
    ~Harness()
    {
        svc.stop();
        fake_lv::reset();
    }

    // ---- app actions
    void start(ReconDetector d) { svc.startDetector(d); }
    void stop() { svc.exitManualMode(); }
    void clearLog() { svc.clearDetections(); }
    void earlyWarning(bool on) { svc.setEarlyWarningEnabled(on); }

    // ---- the face
    void render() { face.render(state, settings, svc.core.reconState()); }
    lv_obj_t *threats() { return fake_lv::findVisibleLabelStartingWith("THREATS"); }
    std::string text()
    {
        lv_obj_t *l = threats();
        return l ? l->text : std::string("<missing>");
    }
    uint32_t color()
    {
        lv_obj_t *l = threats();
        return l ? l->textColor : 0;
    }

    // ---- a detection, through the real BLE entry point
    void tileAdvert()
    {
        const uint8_t mac[6] = {0x0A, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F};
        NimBLEAdvertisedDevice d;
        d.address = NimBLEAddress::fromPrinted(mac);
        d.rssi = -70;
        d.serviceUuids.push_back(NimBLEUUID(static_cast<uint16_t>(0xFEED)));
        svc.handleBleAdvertisement(&d);
    }
};

} // namespace

void nothing_running_reads_off_in_green()
{
    Harness h;
    h.render();
    CHECK_STR("THREATS\nOFF", h.text().c_str());
    CHECK_INT(kGreen, h.color());
}

void early_warning_alone_reads_early_warn()
{
    Harness h;
    h.earlyWarning(true);
    h.render();
    CHECK_STR("THREATS\nEARLY WARN", h.text().c_str());
    CHECK_INT(kGreen, h.color());
}

void a_running_detector_shows_its_short_name()
{
    Harness h;
    h.start(ReconDetector::All);
    h.render();
    CHECK_STR("THREATS\nALL", h.text().c_str());
    h.stop();
    h.start(ReconDetector::CounterSurveil);
    h.render();
    CHECK_STR("THREATS\nSURVEIL", h.text().c_str());
    h.stop();
    h.start(ReconDetector::Tile);
    h.render();
    CHECK_STR("THREATS\nTILE", h.text().c_str());
}

void a_manual_detector_wins_over_early_warning()
{
    Harness h;
    h.earlyWarning(true);
    h.start(ReconDetector::Tile);
    h.render();
    CHECK_STR("THREATS\nTILE", h.text().c_str());
    h.stop();
    h.render();
    CHECK_STR("THREATS\nEARLY WARN", h.text().c_str());
}

void any_logged_event_turns_it_red()
{
    Harness h;
    h.start(ReconDetector::Tile);
    h.tileAdvert();
    h.render();
    CHECK_STR("THREATS\nTILE", h.text().c_str());
    CHECK_INT(kDanger, h.color());
}

void red_outlasts_monitoring_until_the_log_is_cleared()
{
    Harness h;
    h.start(ReconDetector::Tile);
    h.tileAdvert();
    h.stop();
    h.render();
    CHECK_STR("THREATS\nOFF", h.text().c_str());
    CHECK_INT(kDanger, h.color());
    h.clearLog();
    h.render();
    CHECK_INT(kGreen, h.color());
}

void acknowledging_the_alert_does_not_clear_red()
{
    Harness h;
    h.start(ReconDetector::Tile);
    h.tileAdvert();
    h.svc.acknowledgeAlert();
    h.render();
    CHECK_INT(kDanger, h.color());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(nothing_running_reads_off_in_green);
    CASE(early_warning_alone_reads_early_warn);
    CASE(a_running_detector_shows_its_short_name);
    CASE(a_manual_detector_wins_over_early_warning);
    CASE(any_logged_event_turns_it_red);
    CASE(red_outlasts_monitoring_until_the_log_is_cleared);
    CASE(acknowledging_the_alert_does_not_clear_red);
    CHECK_SUMMARY();
}
