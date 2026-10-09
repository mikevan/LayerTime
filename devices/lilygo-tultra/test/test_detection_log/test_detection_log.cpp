// Characterization tests for the detection log CSV row. Until Phase 0 Step 4
// it was written by WatchApp::logReconDetection(); since then by the T-Ultra
// EventLog adapter (devices/lilygo-tultra/src/TUltraEventLog), which the core
// calls for each new event. Output is unchanged, and so is every case below.
//
// The adapter is only reachable through the core WatchApp builds in begin().
// So this test runs the real WatchApp::begin(), the real core and adapters,
// and the real ReconService, compiled against the test-only stubs in
// test/stubs/, and triggers genuine detections through the same radio entry
// points the watch uses.
//
// Link seams: every other class WatchApp owns is replaced by the test-only
// fakes below. Three of them do real work for the test:
//   * ClockService::update   supplies the date and time the row is stamped with.
//   * SettingsService::load  supplies the settings begin() starts from.
//   * SdCardService::appendCsvRow records what would have been written.
// ReconScreen::create also records the core WatchApp hands it, which is how
// a test starts a manual detector the way the Recon screen would: with a
// ReconStart command. SettingsScreen::create records the settings-changed
// hook, which teardown uses to switch early warning off the way the Settings
// screen would. Everything else is an empty body.

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
struct CsvWrite { std::string path, header, row; };
inline std::vector<CsvWrite> g_writes;
inline TUltraSettings g_settings;
inline WatchState g_clock;

// What the cases call to act as the Recon and Settings screens would.
struct Recon {
    layertime::LayerTimeCore *core = nullptr;
    TUltraSettings *settings = nullptr;
    SettingsScreen::SettingsChangedCallback settingsChanged = nullptr;
    void *app = nullptr;

    void run(layertime::CommandType t, ReconDetector d = ReconDetector::None)
    {
        layertime::LayerTimeCommand c;
        c.type = t;
        c.reconTarget.target = d;
        core->execute(c);
    }
    void startDetector(ReconDetector d) { run(layertime::CommandType::ReconStart, d); }
    // A BLE result, delivered through the scan callbacks the service
    // registered with NimBLE.
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *d)
    {
        if (fake_nimble::g_scan.callbacks) fake_nimble::g_scan.callbacks->onResult(d);
    }
    // Every radio off, so no static pointer inside ReconService outlives the
    // app: early warning off through Settings, then leave manual mode.
    void stop()
    {
        if (core && settingsChanged) {
            // As the EARLY WARNING row does since Phase 0 Step 6: the
            // command, then the settings-changed hook.
            layertime::LayerTimeCommand off;
            off.type = layertime::CommandType::SetEarlyWarning;
            off.setting.enabled = false;
            core->execute(off);
            settingsChanged(app);
        }
        run(layertime::CommandType::ReconStop);
    }
};
inline Recon g_reconSeam;
inline Recon *g_recon = nullptr;
}

void ClockService::update(WatchState &s)
{
    s.year = fake_app::g_clock.year;
    s.month = fake_app::g_clock.month;
    s.day = fake_app::g_clock.day;
    s.hour = fake_app::g_clock.hour;
    s.minute = fake_app::g_clock.minute;
    s.second = fake_app::g_clock.second;
}
void ClockService::setDateTime(int, int, int, int, int, int) {}
void BatteryService::update(WatchState &) {}
void GpsService::begin(bool) {}
void GpsService::poll(WatchState &) {}
void GpsService::setEnabled(bool) {}
void MeshService::begin() {}
void MeshService::poll() {}
bool MeshService::sendPublicMessage(const char *) { return false; }
void MeshService::setAdvertisingEnabled(bool) {}
bool MeshService::setRadioEnabled(bool) { return true; }
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
bool MeshtasticService::setRadioEnabled(bool) { return true; }
void SdCardService::begin() {}
bool SdCardService::appendCsvRow(const char *path, const char *header, const char *row)
{
    fake_app::g_writes.push_back({path, header, row});
    return true;
}
void SettingsService::load(TUltraSettings &s) { s = fake_app::g_settings; }
void SettingsService::apply(const TUltraSettings &) {}
void SettingsService::save(const TUltraSettings &) {}
void WatchFace::create() {}
void WatchFace::render(const WatchState &, const TUltraSettings &, const layertime::ApplicationSettings &,
                       const layertime::ReconState &) {}
void WatchFace::setSettingsRequestedCallback(SettingsRequestedCallback, void *) {}
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
void MappingScreen::show(const WatchState &, const TUltraSettings &) {}
void MappingScreen::render(const WatchState &, const TUltraSettings &) {}
void MeshScreen::create(layertime::LayerTimeCore *, const MeshService *, BackCallback, void *) {}
void MeshScreen::show(const MeshStatus &) {}
void MeshScreen::render(const MeshStatus &) {}
void MeshtasticScreen::create(layertime::LayerTimeCore *, const MeshtasticService *, BackCallback, void *) {}
void MeshtasticScreen::show(const MeshtasticStatus &) {}
void MeshtasticScreen::render(const MeshtasticStatus &) {}
void ReconScreen::create(layertime::LayerTimeCore *core, BackCallback, void *)
{
    fake_app::g_reconSeam.core = core;
    fake_app::g_recon = &fake_app::g_reconSeam;
}
void ReconScreen::show(ReconDetector) {}
void ReconScreen::render() {}
void SettingsScreen::create(TUltraSettings &settings, layertime::LayerTimeCore *, const WatchState &, SdCardService &, BackCallback,
                            SettingsChangedCallback changed, DateTimeSaveCallback, void *app)
{
    fake_app::g_reconSeam.settings = &settings;
    fake_app::g_reconSeam.settingsChanged = changed;
    fake_app::g_reconSeam.app = app;
}
void SettingsScreen::show() {}

// ---------------------------------------------------------------- harness

namespace {

const char *kPath = "/recon_log.csv";
const char *kHeader = "timestamp,category,detail,address,rssi,channel,confidence";

struct Harness {
    std::unique_ptr<WatchApp> app;

    explicit Harness(bool logging, bool sleepMode = false)
    {
        fake_arduino::g_millis = 1000;
        fake_wifi::g_rxCallback = nullptr;
        fake_nimble::g_scan = NimBLEScan{};
        fake_app::g_writes.clear();
        fake_app::g_recon = nullptr;
        fake_app::g_reconSeam = fake_app::Recon{};
        fake_app::g_settings = TUltraSettings{};
        fake_app::g_settings.reconSdLoggingEnabled = logging;
        // Since Phase 0 Step 6 early warning and sleep mode are application
        // settings, loaded by the core from NVS (test/stubs/Preferences.h).
        fake_nvs::reset();
        Preferences prefs;
        prefs.begin("layertime");
        prefs.putBool("reconew", true);  // the shipped default
        prefs.putBool("sleepmode", sleepMode);
        prefs.end();
        fake_app::g_clock = WatchState{};
        fake_app::g_clock.year = 2026;
        fake_app::g_clock.month = 9;
        fake_app::g_clock.day = 8;
        fake_app::g_clock.hour = 7;
        fake_app::g_clock.minute = 5;
        fake_app::g_clock.second = 3;
        app.reset(new WatchApp());
        app->begin();
    }
    ~Harness()
    {
        if (fake_app::g_recon) fake_app::g_recon->stop();
        app.reset();
    }
};

void deliver(const std::vector<uint8_t> &payload, int8_t rssi, uint8_t channel)
{
    std::vector<uint8_t> buf(sizeof(wifi_promiscuous_pkt_t) + payload.size());
    auto *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buf.data());
    pkt->rx_ctrl.rssi = rssi;
    pkt->rx_ctrl.channel = channel;
    pkt->rx_ctrl.sig_len = static_cast<unsigned>(payload.size());
    memcpy(pkt->payload, payload.data(), payload.size());
    if (fake_wifi::g_rxCallback) fake_wifi::g_rxCallback(pkt, 0);
}

std::vector<uint8_t> beacon(const uint8_t bssid[6], const std::string &ssid, bool privacy)
{
    std::vector<uint8_t> f(36, 0);
    f[0] = 0x80;
    memcpy(&f[10], bssid, 6);
    f[34] = privacy ? 0x10 : 0x00;
    f.push_back(0x00);
    f.push_back(static_cast<uint8_t>(ssid.size()));
    f.insert(f.end(), ssid.begin(), ssid.end());
    return f;
}

const uint8_t kPwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};

} // namespace

void row_format_path_and_header()
{
    Harness h(true);
    deliver(beacon(kPwn, "pwn", true), -55, 11);  // caught by the early-warning sweep
    CHECK_INT(2, fake_app::g_writes.size());      // Pwnagotchi, then Pineapple (DE:AD:BE)
    if (fake_app::g_writes.size() < 2) return;
    CHECK_STR(kPath, fake_app::g_writes[0].path.c_str());
    CHECK_STR(kHeader, fake_app::g_writes[0].header.c_str());
    CHECK_STR("2026-09-08 07:05:03,PWNAGOTCHI,Pwnagotchi beacon,DE:AD:BE:EF:DE:AD,-55,11,HIGH",
              fake_app::g_writes[0].row.c_str());
    CHECK_STR("2026-09-08 07:05:03,PINEAPPLE,Suspicious Pineapple OUI,DE:AD:BE:EF:DE:AD,-55,11,MED",
              fake_app::g_writes[1].row.c_str());
}

void nothing_is_written_when_logging_is_off()
{
    Harness h(false);
    deliver(beacon(kPwn, "pwn", true), -55, 11);
    CHECK_INT(0, fake_app::g_writes.size());
}

void a_repeat_sighting_writes_no_new_row()
{
    Harness h(true);
    deliver(beacon(kPwn, "pwn", false), -55, 11);
    const size_t first = fake_app::g_writes.size();
    deliver(beacon(kPwn, "pwn", false), -40, 6);
    CHECK_INT(first, fake_app::g_writes.size());
}

void sleep_mode_still_logs()
{
    Harness h(true, true);
    deliver(beacon(kPwn, "pwn", false), -55, 11);
    CHECK_TRUE(fake_app::g_writes.size() >= 1);
}

void ble_rows_carry_lowercase_address_and_channel_zero()
{
    Harness h(true);
    CHECK_TRUE(fake_app::g_recon != nullptr);
    if (!fake_app::g_recon) return;
    fake_app::g_recon->startDetector(ReconDetector::AirTag);
    NimBLEAdvertisedDevice d;
    const uint8_t printed[6] = {0x0A, 0x1B, 0x2C, 0x3D, 0x4E, 0x5F};
    d.address = NimBLEAddress::fromPrinted(printed);
    d.rssi = -71;
    const uint8_t apple[] = {0x4C, 0x00, 0x12};
    d.manufacturerData = {std::string(apple, apple + 3)};
    fake_app::g_recon->handleBleAdvertisement(&d);
    CHECK_INT(1, fake_app::g_writes.size());
    if (fake_app::g_writes.empty()) return;
    CHECK_STR("2026-09-08 07:05:03,AIRTAG,Find My tracker beacon,0a:1b:2c:3d:4e:5f,-71,0,HIGH",
              fake_app::g_writes[0].row.c_str());
}

void low_confidence_detections_are_logged_too()
{
    Harness h(true);
    fake_app::g_recon->startDetector(ReconDetector::Flock);
    const uint8_t esp[6] = {0x24, 0x0A, 0xC4, 0x01, 0x02, 0x03};
    deliver(beacon(esp, "plug", true), -60, 6);
    CHECK_INT(1, fake_app::g_writes.size());
    if (fake_app::g_writes.empty()) return;
    CHECK_STR("2026-09-08 07:05:03,FLOCK,Flock? (ESP32),24:0A:C4:01:02:03,-60,6,LOW",
              fake_app::g_writes[0].row.c_str());
}

void fields_are_not_quoted_or_escaped_KNOWN_DEFECT()
{
    // Detail text goes into the row as-is. An SSID containing a comma or a
    // quote therefore shifts or breaks the columns. Pinned as-is.
    Harness h(true);
    fake_app::g_recon->startDetector(ReconDetector::Axon);
    const uint8_t mac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
    deliver(beacon(mac, "AB2-a,\"b\"", true), -50, 1);
    CHECK_INT(1, fake_app::g_writes.size());
    if (fake_app::g_writes.empty()) return;
    CHECK_STR("2026-09-08 07:05:03,AXON,SSID AB2-a,\"b\",02:11:22:33:44:55,-50,1,HIGH",
              fake_app::g_writes[0].row.c_str());
}

void timestamp_comes_from_the_last_state_refresh()
{
    // The row uses WatchApp's cached state, refreshed every 250 ms by tick(),
    // not the clock at the moment of detection.
    Harness h(true);
    fake_app::g_clock.hour = 23;  // clock moves, no tick has run yet
    deliver(beacon(kPwn, "pwn", false), -55, 11);
    CHECK_TRUE(!fake_app::g_writes.empty());
    if (fake_app::g_writes.empty()) return;
    CHECK_STR("2026-09-08 07:05:03", fake_app::g_writes[0].row.substr(0, 19).c_str());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(row_format_path_and_header);
    CASE(nothing_is_written_when_logging_is_off);
    CASE(a_repeat_sighting_writes_no_new_row);
    CASE(sleep_mode_still_logs);
    CASE(ble_rows_carry_lowercase_address_and_channel_zero);
    CASE(low_confidence_detections_are_logged_too);
    CASE(fields_are_not_quoted_or_escaped_KNOWN_DEFECT);
    CASE(timestamp_comes_from_the_last_state_refresh);
    CHECK_SUMMARY();
}
