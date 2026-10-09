// Characterization tests for devices/lilygo-tultra/src/services/ReconService.cpp, as it stands.
//
// The production file is compiled into this test unchanged, by including it,
// so its file-local classifiers and signature tables are reachable. Radio
// libraries are replaced by test-only stubs in test/stubs/: frames and
// advertisements are handed to the real code through the same entry points
// the Wi-Fi driver and NimBLE use on the watch.
//
// These tests record CURRENT behaviour. Where current behaviour looks wrong,
// the test still pins it, and its name says KNOWN_DEFECT, so a later fix is a
// deliberate, visible change rather than an accident.
//
// Since Phase 0 Step 4 the detection log, the alert decision and its
// actuation live in core (LayerTimeCore, MonitorEventLog) behind the T-Ultra
// adapters, and ReconService only acquires. The Session below wires those
// together exactly as WatchApp does, and presents them to the cases under the
// old ReconService names, so every case body is unchanged from Step 3.

#include "check.h"

#include <string>
#include <vector>

#include "services/ReconService.cpp"

#include "core/app/LayerTimeCore.h"
#include "TUltraAlertSink.h"
#include "TUltraMonitorSource.h"

// Since Phase 0 Step 3 the classifiers and tables checked below live in
// src/core/logic. The expectations are unchanged; only the namespace moved.
using namespace layertime::recon;

namespace {

// ---------------------------------------------------------------- harness

void resetFakes()
{
    fake_arduino::g_millis = 1000;
    fake_wifi::g_rxCallback = nullptr;
    fake_wifi::g_promiscuous = false;
    fake_nimble::g_scan = NimBLEScan{};
    fake_nimble::g_initialized = false;
    instance = FakeLilyGoInstance{};
    gBleActiveInstance = nullptr;
}

// The detection record and status as the cases read them. These were
// ReconService's own types before Step 4; the view below rebuilds them from
// core on every read, field for field.
struct ReconDetection {
    char category[14] = {0};
    char detail[40] = {0};
    char address[19] = {0};
    int8_t rssi = 0;
    uint8_t channel = 0;
    uint32_t lastSeenMs = 0;
    uint32_t encounterCount = 1;
    SignalConfidence confidence = SignalConfidence::High;
};

struct LegacyStatus {
    static constexpr size_t MAX_DETECTIONS = 40;
    ReconDetector detector = ReconDetector::None;
    ReconDetector activeDetector = ReconDetector::None;
    ReconDetection detections[MAX_DETECTIONS];
    size_t detectionCount = 0;
    uint32_t eventSerial = 0;
    bool monitoring = false;
    bool alertPending = false;
    bool earlyWarningEnabled = false;
    bool earlyWarningResting = false;
};

ReconDetection legacy(const layertime::MonitorEvent &e)
{
    ReconDetection d;
    snprintf(d.category, sizeof(d.category), "%s", ReconService::detectorName(e.detector));
    snprintf(d.detail, sizeof(d.detail), "%s", e.detail);
    snprintf(d.address, sizeof(d.address), "%s", e.sourceId);
    d.rssi = e.rssi;
    d.channel = e.channel;
    d.lastSeenMs = e.lastSeen.uptimeMs;
    d.encounterCount = e.count;
    d.confidence = e.confidence;
    return d;
}

struct Recorder : layertime::EventLog {
    std::vector<ReconDetection> *rows = nullptr;
    void append(const layertime::MonitorEvent &e) override { rows->push_back(legacy(e)); }
};

// The old ReconService surface, routed the way WatchApp routes it now: Recon
// commands and sleep mode to core, radio entry points and early warning to
// the service, poll() to core's tick.
struct Facade {
    ReconService &radio;
    layertime::LayerTimeCore &core;

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
    void poll() { core.tick(millis()); }
    void stop() { radio.stop(); }
    void setEarlyWarningEnabled(bool on) { radio.setEarlyWarningEnabled(on); }
    void setSleepModeEnabled(bool on) { core.setSleepMode(on); }
    void handleBleAdvertisement(const NimBLEAdvertisedDevice *d) { radio.handleBleAdvertisement(d); }
};

// Owns one service for a test case and always tears its radios down, so no
// static pointer inside ReconService outlives the object.
struct Session {
    ReconService radio;
    layertime::twatch_ultra::TUltraMonitorSource monitor{radio};
    layertime::twatch_ultra::TUltraAlertSink alerts;
    Recorder recorder;
    layertime::LayerTimeCore core;
    Facade svc{radio, core};
    std::vector<ReconDetection> sunk;
    mutable LegacyStatus view;
    Session()
    {
        resetFakes();
        recorder.rows = &sunk;
        layertime::CorePorts p;
        p.monitor = &monitor;
        p.alerts = &alerts;
        p.eventLog = &recorder;
        core.attach(p);
    }
    ~Session() { svc.stop(); }
    const LegacyStatus &s() const
    {
        const layertime::ReconState r = core.reconState();
        view = LegacyStatus{};
        view.detector = r.selected;
        view.activeDetector = r.active;
        view.monitoring = r.monitoring;
        view.earlyWarningEnabled = r.earlyWarningEnabled;
        view.earlyWarningResting = r.earlyWarningResting;
        view.alertPending = r.alertPending;
        view.eventSerial = r.lastEventId;
        view.detectionCount = r.eventCount;
        for (uint8_t i = 0; i < r.eventCount; ++i) view.detections[i] = legacy(core.event(i));
        return view;
    }
};

// ---- Wi-Fi frames, delivered through the captured promiscuous callback.

void deliver(const std::vector<uint8_t> &payload, int8_t rssi, uint8_t channel,
             uint8_t rxState = 0)
{
    std::vector<uint8_t> buf(sizeof(wifi_promiscuous_pkt_t) + payload.size());
    auto *pkt = reinterpret_cast<wifi_promiscuous_pkt_t *>(buf.data());
    pkt->rx_ctrl.rx_state = rxState;
    pkt->rx_ctrl.rssi = rssi;
    pkt->rx_ctrl.channel = channel;
    pkt->rx_ctrl.sig_len = static_cast<unsigned>(payload.size());
    memcpy(pkt->payload, payload.data(), payload.size());
    if (fake_wifi::g_rxCallback) fake_wifi::g_rxCallback(pkt, 0);
}

std::vector<uint8_t> mgmtFrame(uint8_t subtype, const uint8_t tx[6], size_t length = 24)
{
    std::vector<uint8_t> f(length, 0);
    f[0] = static_cast<uint8_t>(subtype << 4);  // type 0: management
    memcpy(&f[10], tx, 6);
    return f;
}

// Beacon: 24-byte header, 12 bytes of fixed fields (capabilities at 34),
// then the SSID element at 36.
std::vector<uint8_t> beacon(const uint8_t bssid[6], const std::string &ssid, bool privacy)
{
    std::vector<uint8_t> f = mgmtFrame(0x08, bssid, 36);
    f[34] = privacy ? 0x10 : 0x00;
    f.push_back(0x00);
    f.push_back(static_cast<uint8_t>(ssid.size()));
    f.insert(f.end(), ssid.begin(), ssid.end());
    return f;
}

// ---- BLE advertisements, handed to the public NimBLE callback target.

NimBLEAdvertisedDevice bleDevice(const uint8_t printed[6], int rssi = -60)
{
    NimBLEAdvertisedDevice d;
    d.address = NimBLEAddress::fromPrinted(printed);
    d.rssi = rssi;
    return d;
}

std::string mfg(std::initializer_list<uint8_t> bytes) { return std::string(bytes.begin(), bytes.end()); }

// Starts a BLE-only scan for `detector` the way the Recon screen does.
void startBle(Session &x, ReconDetector detector) { x.svc.startDetector(detector); }

const uint8_t kPlainMac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

} // namespace

// ================================================================ classifiers

void find_my_subtypes_match_only_separated_and_near_owner()
{
    const uint8_t sep[] = {0x4C, 0x00, 0x12};
    const uint8_t near[] = {0x4C, 0x00, 0x1E};
    const uint8_t pairing[] = {0x4C, 0x00, 0x07};
    const uint8_t other[] = {0x4C, 0x00, 0x10};
    CHECK_TRUE(isFindMyBeacon(sep, 3));
    CHECK_TRUE(isFindMyBeacon(near, 3));
    CHECK_FALSE(isFindMyBeacon(pairing, 3));  // kMatchProximityPairing is false
    CHECK_FALSE(isFindMyBeacon(other, 3));
    CHECK_FALSE(isFindMyBeacon(sep, 2));
    CHECK_FALSE(isFindMyBeacon(nullptr, 3));
}

void flock_names_are_empty_penguin_battery_or_ten_digits()
{
    CHECK_TRUE(isFlockName(""));
    CHECK_TRUE(isFlockName("Penguin-"));
    CHECK_TRUE(isFlockName("Penguin-A7"));
    CHECK_FALSE(isFlockName("penguin-A7"));  // case matters
    CHECK_FALSE(isFlockName("xPenguin-A7"));
    CHECK_TRUE(isFlockName("FS Ext Battery"));
    CHECK_FALSE(isFlockName("FS Ext Battery "));
    CHECK_TRUE(isFlockName("0123456789"));
    CHECK_FALSE(isFlockName("012345678"));
    CHECK_FALSE(isFlockName("01234567890"));
    CHECK_FALSE(isFlockName("01234x6789"));
    CHECK_FALSE(allDigits(""));
}

void ssid_hash_is_16_bit_djb2()
{
    const uint8_t a[] = {'a'};
    const uint8_t hello[] = {'h', 'e', 'l', 'l', 'o'};
    CHECK_INT(5381, hashSsid(a, 0));
    CHECK_INT(46598, hashSsid(a, 1));        // (5381 * 33 + 97) mod 65536
    CHECK_INT(12441, hashSsid(hello, 5));    // djb2 over "hello", 16-bit arithmetic
}

void ssid_prefix_is_case_insensitive_and_length_bound()
{
    const uint8_t ssid[] = {'a', 'b', '3', '-', 'Z'};
    CHECK_TRUE(ssidHasPrefix(ssid, 5, "AB3-"));
    CHECK_TRUE(ssidHasPrefix(ssid, 4, "ab3-"));
    CHECK_FALSE(ssidHasPrefix(ssid, 3, "AB3-"));
    CHECK_FALSE(ssidHasPrefix(ssid, 5, "AB2-"));
    const uint8_t axon[] = {'A', 'x', 'O', 'n', '-', '1'};
    CHECK_TRUE(ssidHasPrefix(axon, 6, "AXON-"));
}

void mac_text_is_uppercase_with_colons()
{
    char out[18] = "unchanged";
    const uint8_t mac[] = {0xde, 0xad, 0xbe, 0xef, 0x0a, 0x01};
    formatMac(out, sizeof(out), mac);
    CHECK_STR("DE:AD:BE:EF:0A:01", out);
    char small[17] = "unchanged";
    formatMac(small, sizeof(small), mac);
    CHECK_STR("unchanged", small);  // buffer too small: left untouched
}

void pineapple_oui_list_and_open_network_rule()
{
    auto mac = [](uint32_t oui) {
        static uint8_t m[6];
        m[0] = static_cast<uint8_t>(oui >> 16);
        m[1] = static_cast<uint8_t>(oui >> 8);
        m[2] = static_cast<uint8_t>(oui);
        m[3] = m[4] = m[5] = 0x01;
        return m;
    };
    const uint32_t always[] = {0x001337, 0x02C0CA, 0x021337, 0x000A00, 0x000C43,
                               0x000CE7, 0x0017A5, 0x9CEFD5, 0x9CE5D5, 0xDEADBE};
    for (uint32_t o : always) {
        CHECK_TRUE(isPineappleOui(mac(o), true));
        CHECK_TRUE(isPineappleOui(mac(o), false));
    }
    const uint32_t openOnly[] = {0x00C0CA, 0x1CBFCE, 0x0CEFAF};
    for (uint32_t o : openOnly) {
        CHECK_TRUE(isPineappleOui(mac(o), true));
        CHECK_FALSE(isPineappleOui(mac(o), false));
    }
    CHECK_FALSE(isPineappleOui(mac(0x588E81), true));
}

void oui_table_is_exactly_the_current_39_entries()
{
    struct Row { uint8_t oui[3]; ReconDetector d; SignalConfidence c; const char *label; };
    const Row expected[] = {
        {{0x58, 0x8E, 0x81}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xEC, 0x1B, 0xBD}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x90, 0x35, 0xEA}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x04, 0x0D, 0x84}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xF0, 0x82, 0xC0}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x1C, 0x34, 0xF1}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x38, 0x5B, 0x44}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x94, 0x34, 0x69}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xB4, 0x1E, 0x52}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x24, 0xB2, 0xB9}, ReconDetector::Flock, SignalConfidence::High, "Flock Liteon"},
        {{0xD0, 0x39, 0x57}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x00, 0xF4, 0x8D}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x14, 0x5A, 0xFC}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x80, 0x30, 0x49}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xE0, 0x0A, 0xF6}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x70, 0xC9, 0x4E}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x3C, 0x91, 0x80}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xD8, 0xF3, 0xBC}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0xB8, 0x35, 0x32}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
        {{0x00, 0xA0, 0xD8}, ReconDetector::Flock, SignalConfidence::High, "Flock Sierra"},
        {{0x00, 0x25, 0xDF}, ReconDetector::Axon, SignalConfidence::High, "Axon (Taser)"},
        {{0xE4, 0x05, 0x40}, ReconDetector::Axon, SignalConfidence::High, "Axon body cam"},
        {{0x28, 0x24, 0xFF}, ReconDetector::Axon, SignalConfidence::High, "Axon Signal"},
        {{0x24, 0x0A, 0xC4}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0x30, 0xAE, 0xA4}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0x24, 0x6F, 0x28}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xCC, 0x50, 0xE3}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xDC, 0x54, 0x75}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xE8, 0x9F, 0x6D}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0x8C, 0xAA, 0xB5}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
        {{0x34, 0x85, 0x18}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
        {{0xD4, 0xAD, 0xFC}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xAC, 0x67, 0xB2}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0x84, 0xF3, 0xEB}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
        {{0xB4, 0xE6, 0x2D}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xCC, 0xDB, 0xA7}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0x94, 0xB9, 0x7E}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
        {{0xA4, 0xCF, 0x12}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S2)"},
        {{0xC0, 0x49, 0xEF}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-C6)"},
    };
    CHECK_INT(39, sizeof(kOuiSignatures) / sizeof(kOuiSignatures[0]));
    for (const Row &r : expected) {
        const uint8_t mac[6] = {r.oui[0], r.oui[1], r.oui[2], 0xAA, 0xBB, 0xCC};
        const OuiSignature *sig = lookupOui(mac);
        CHECK_TRUE(sig != nullptr);
        if (!sig) continue;
        CHECK_INT(static_cast<int>(r.d), static_cast<int>(sig->detector));
        CHECK_INT(static_cast<int>(r.c), static_cast<int>(sig->confidence));
        CHECK_STR(r.label, sig->label);
    }
    CHECK_TRUE(lookupOui(kPlainMac) == nullptr);
    CHECK_TRUE(lookupOui(nullptr) == nullptr);
    // Deliberately absent: 82:6B:F2, a locally administered address.
    const uint8_t la[6] = {0x82, 0x6B, 0xF2, 0, 0, 0};
    CHECK_TRUE(lookupOui(la) == nullptr);
}

void ble_uuid_table_is_exactly_the_current_10_entries()
{
    struct Row { uint16_t uuid; ReconDetector d; const char *label; SignalConfidence c; };
    const Row expected[] = {
        {0xFD5F, ReconDetector::Meta, "Meta Ray-Ban glasses", SignalConfidence::High},
        {0xFEED, ReconDetector::Tile, "Tile tracker", SignalConfidence::High},
        {0xFEEC, ReconDetector::Tile, "Tile tracker", SignalConfidence::High},
        {0xFD5A, ReconDetector::SamsungTag, "Samsung SmartTag", SignalConfidence::High},
        {0x3081, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
        {0x3082, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
        {0x3083, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
        {0xFEAA, ReconDetector::GoogleTag, "Google Find My Device", SignalConfidence::Medium},
        {0xFEB7, ReconDetector::Meta, "Meta service", SignalConfidence::Low},
        {0xFEB8, ReconDetector::Meta, "Meta service", SignalConfidence::Low},
    };
    const size_t n = sizeof(kBleUuidSignatures) / sizeof(kBleUuidSignatures[0]);
    CHECK_INT(10, n);
    for (size_t i = 0; i < n && i < 10; ++i) {
        CHECK_INT(expected[i].uuid, kBleUuidSignatures[i].uuid);
        CHECK_INT(static_cast<int>(expected[i].d), static_cast<int>(kBleUuidSignatures[i].detector));
        CHECK_STR(expected[i].label, kBleUuidSignatures[i].label);
        CHECK_INT(static_cast<int>(expected[i].c), static_cast<int>(kBleUuidSignatures[i].confidence));
    }
}

void group_membership_is_fixed()
{
    size_t n = 0;
    const ReconDetector *m = ReconService::groupMembers(ReconDetector::Trackers, n);
    CHECK_INT(4, n);
    CHECK_INT(static_cast<int>(ReconDetector::AirTag), static_cast<int>(m[0]));
    CHECK_INT(static_cast<int>(ReconDetector::Tile), static_cast<int>(m[1]));
    CHECK_INT(static_cast<int>(ReconDetector::SamsungTag), static_cast<int>(m[2]));
    CHECK_INT(static_cast<int>(ReconDetector::GoogleTag), static_cast<int>(m[3]));
    m = ReconService::groupMembers(ReconDetector::CounterSurveil, n);
    CHECK_INT(3, n);
    CHECK_INT(static_cast<int>(ReconDetector::Flock), static_cast<int>(m[0]));
    CHECK_INT(static_cast<int>(ReconDetector::Axon), static_cast<int>(m[1]));
    CHECK_INT(static_cast<int>(ReconDetector::Meta), static_cast<int>(m[2]));
    m = ReconService::groupMembers(ReconDetector::CounterIntrusion, n);
    CHECK_INT(5, n);
    CHECK_INT(static_cast<int>(ReconDetector::Deauth), static_cast<int>(m[0]));
    CHECK_INT(static_cast<int>(ReconDetector::Pwnagotchi), static_cast<int>(m[1]));
    CHECK_INT(static_cast<int>(ReconDetector::MultiSSID), static_cast<int>(m[2]));
    CHECK_INT(static_cast<int>(ReconDetector::Pineapple), static_cast<int>(m[3]));
    CHECK_INT(static_cast<int>(ReconDetector::Flipper), static_cast<int>(m[4]));
    CHECK_TRUE(ReconService::groupMembers(ReconDetector::Deauth, n) == nullptr);
    CHECK_INT(0, n);
}

void detector_and_confidence_names_are_fixed()
{
    struct Row { ReconDetector d; const char *name; const char *shortName; };
    const Row rows[] = {
        {ReconDetector::None, "STOPPED", "STOPPED"},
        {ReconDetector::All, "ALL", "ALL"},
        {ReconDetector::Trackers, "TRACKERS", "TRACKERS"},
        {ReconDetector::CounterSurveil, "COUNTER-SURVEIL", "SURVEIL"},
        {ReconDetector::CounterIntrusion, "COUNTER-INTRUSION", "INTRUSION"},
        {ReconDetector::Deauth, "DEAUTH", "DEAUTH"},
        {ReconDetector::Pwnagotchi, "PWNAGOTCHI", "PWNAGOTCHI"},
        {ReconDetector::MultiSSID, "MULTISSID", "MULTISSID"},
        {ReconDetector::Flock, "FLOCK", "FLOCK"},
        {ReconDetector::Pineapple, "PINEAPPLE", "PINEAPPLE"},
        {ReconDetector::AirTag, "AIRTAG", "AIRTAG"},
        {ReconDetector::Flipper, "FLIPPER", "FLIPPER"},
        {ReconDetector::Meta, "META", "META"},
        {ReconDetector::Axon, "AXON", "AXON"},
        {ReconDetector::Tile, "TILE", "TILE"},
        {ReconDetector::SamsungTag, "SMARTTAG", "SMARTTAG"},
        {ReconDetector::GoogleTag, "GOOGLE TAG", "GOOGLE TAG"},
        {ReconDetector::EarlyWarning, "EARLY WARNING", "EARLY WARN"},
    };
    for (const Row &r : rows) {
        CHECK_STR(r.name, ReconService::detectorName(r.d));
        CHECK_STR(r.shortName, ReconService::detectorShortName(r.d));
    }
    CHECK_STR("HIGH", ReconService::confidenceLabel(SignalConfidence::High));
    CHECK_STR("MED", ReconService::confidenceLabel(SignalConfidence::Medium));
    CHECK_STR("LOW", ReconService::confidenceLabel(SignalConfidence::Low));
}

// ================================================================ BLE path

void airtag_find_my_beacon_is_a_high_detection()
{
    Session x;
    startBle(x, ReconDetector::AirTag);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac, -71);
    d.manufacturerData = {mfg({0x4C, 0x00, 0x12, 0x19})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(1, x.s().detectionCount);
    const ReconDetection &e = x.s().detections[0];
    CHECK_STR("AIRTAG", e.category);
    CHECK_STR("Find My tracker beacon", e.detail);
    CHECK_STR("02:11:22:33:44:55", e.address);  // NimBLE prints lowercase
    CHECK_INT(-71, e.rssi);
    CHECK_INT(0, e.channel);
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(e.confidence));
    CHECK_INT(1, e.encounterCount);
}

void airtag_proximity_pairing_and_other_vendors_are_ignored()
{
    Session x;
    startBle(x, ReconDetector::AirTag);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.manufacturerData = {mfg({0x4C, 0x00, 0x07, 0x19}), mfg({0x06, 0x00, 0x12}), mfg({0x4C})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(0, x.s().detectionCount);
}

void every_manufacturer_record_is_examined()
{
    Session x;
    startBle(x, ReconDetector::AirTag);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.manufacturerData = {mfg({0x06, 0x00, 0x01}), mfg({0x4C, 0x00, 0x1E})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(1, x.s().detectionCount);
}

void flock_ble_needs_xuntong_id_and_a_flock_shaped_name()
{
    Session x;
    x.svc.startDetector(ReconDetector::All);
    fake_arduino::g_millis += 12000;  // All starts on Wi-Fi; wait for its BLE burst
    x.svc.poll();
    CHECK_TRUE(fake_nimble::g_scan.isScanning());

    NimBLEAdvertisedDevice named = bleDevice(kPlainMac);
    named.hasName = true;
    named.name = "Penguin-42";
    named.manufacturerData = {mfg({0xC8, 0x09, 0x00})};
    x.svc.handleBleAdvertisement(&named);

    const uint8_t mac2[6] = {0x02, 0, 0, 0, 0, 2};
    NimBLEAdvertisedDevice unnamed = bleDevice(mac2);
    unnamed.manufacturerData = {mfg({0xC8, 0x09})};
    x.svc.handleBleAdvertisement(&unnamed);

    const uint8_t mac3[6] = {0x02, 0, 0, 0, 0, 3};
    NimBLEAdvertisedDevice wrongName = bleDevice(mac3);
    wrongName.hasName = true;
    wrongName.name = "Headphones";
    wrongName.manufacturerData = {mfg({0xC8, 0x09})};
    x.svc.handleBleAdvertisement(&wrongName);

    CHECK_INT(2, x.s().detectionCount);
    CHECK_STR("FLOCK", x.s().detections[0].category);
    CHECK_STR("Penguin-42", x.s().detections[0].detail);  // name wins over the label
    CHECK_STR("Flock BLE signature", x.s().detections[1].detail);
}

void service_uuid_matches_use_name_when_present_else_label()
{
    Session x;
    startBle(x, ReconDetector::Tile);
    NimBLEAdvertisedDevice a = bleDevice(kPlainMac);
    a.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};
    x.svc.handleBleAdvertisement(&a);

    const uint8_t mac2[6] = {0x02, 0, 0, 0, 0, 2};
    NimBLEAdvertisedDevice b = bleDevice(mac2);
    b.hasName = true;
    b.name = "Keys";
    b.serviceUuids = {NimBLEUUID(uint16_t{0xFEEC})};
    x.svc.handleBleAdvertisement(&b);

    CHECK_INT(2, x.s().detectionCount);
    CHECK_STR("Tile tracker", x.s().detections[0].detail);
    CHECK_STR("Keys", x.s().detections[1].detail);
}

void only_16_bit_uuids_are_compared()
{
    Session x;
    startBle(x, ReconDetector::Tile);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.serviceUuids = {NimBLEUUID::with128Bits()};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(0, x.s().detectionCount);
}

void a_ble_scan_only_reports_what_it_was_started_for()
{
    Session x;
    startBle(x, ReconDetector::AirTag);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};  // a Tile
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(0, x.s().detectionCount);
}

void group_scan_reports_its_members()
{
    Session x;
    startBle(x, ReconDetector::Trackers);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.serviceUuids = {NimBLEUUID(uint16_t{0xFD5A})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("SMARTTAG", x.s().detections[0].category);
}

void ble_oui_path_compares_the_low_three_bytes_KNOWN_DEFECT()
{
    // NimBLE keeps addresses least-significant byte first, and ReconService
    // hands that raw array to lookupOui(), which reads bytes 0..2 as the OUI.
    // So the BLE-address OUI check tests the LAST three octets of the printed
    // address, not the vendor prefix. Pinned as-is; not fixed in Step 2.
    Session x;
    x.svc.startDetector(ReconDetector::All);
    fake_arduino::g_millis += 12000;
    x.svc.poll();

    const uint8_t flockFirst[6] = {0x58, 0x8E, 0x81, 0x12, 0x34, 0x56};
    NimBLEAdvertisedDevice real = bleDevice(flockFirst);
    x.svc.handleBleAdvertisement(&real);
    CHECK_INT(0, x.s().detectionCount);  // a real Flock prefix is missed

    const uint8_t flockLast[6] = {0x56, 0x34, 0x12, 0x81, 0x8E, 0x58};
    NimBLEAdvertisedDevice reversed = bleDevice(flockLast);
    x.svc.handleBleAdvertisement(&reversed);
    CHECK_INT(1, x.s().detectionCount);  // and a reversed one matches
    CHECK_STR("FLOCK", x.s().detections[0].category);
    CHECK_STR("56:34:12:81:8e:58", x.s().detections[0].address);
}

void early_warning_ble_burst_reports_only_flipper_and_meta()
{
    Session x;
    x.svc.setEarlyWarningEnabled(true);  // arms the Wi-Fi sweep
    fake_arduino::g_millis += 10000;     // sweep window ends, BLE burst starts
    x.svc.poll();
    CHECK_TRUE(fake_nimble::g_scan.isScanning());

    NimBLEAdvertisedDevice tile = bleDevice(kPlainMac);
    tile.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};
    x.svc.handleBleAdvertisement(&tile);
    CHECK_INT(0, x.s().detectionCount);

    NimBLEAdvertisedDevice flipper = bleDevice(kPlainMac);
    flipper.serviceUuids = {NimBLEUUID(uint16_t{0x3082})};
    x.svc.handleBleAdvertisement(&flipper);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("FLIPPER", x.s().detections[0].category);
    x.svc.setEarlyWarningEnabled(false);
}

// ================================================================ Wi-Fi path

void deauth_fires_on_the_sixth_frame_within_three_seconds()
{
    Session x;
    x.svc.startDetector(ReconDetector::Deauth);
    const auto f = mgmtFrame(0x0C, kPlainMac);
    for (int i = 0; i < 5; ++i) {
        deliver(f, -40, 6);
        fake_arduino::g_millis += 500;
    }
    CHECK_INT(0, x.s().detectionCount);
    deliver(f, -41, 6);
    CHECK_INT(1, x.s().detectionCount);
    const ReconDetection &e = x.s().detections[0];
    CHECK_STR("DEAUTH", e.category);
    CHECK_STR("Deauth flood", e.detail);
    CHECK_STR("02:11:22:33:44:55", e.address);  // Wi-Fi path prints uppercase
    CHECK_INT(-41, e.rssi);
    CHECK_INT(6, e.channel);
    CHECK_INT(static_cast<int>(SignalConfidence::Medium), static_cast<int>(e.confidence));
}

void deauth_window_restarts_after_a_gap_longer_than_three_seconds()
{
    Session x;
    x.svc.startDetector(ReconDetector::Deauth);
    const auto f = mgmtFrame(0x0C, kPlainMac);
    for (int i = 0; i < 5; ++i) deliver(f, -40, 1);
    fake_arduino::g_millis += 3001;
    for (int i = 0; i < 5; ++i) deliver(f, -40, 1);
    CHECK_INT(0, x.s().detectionCount);
    deliver(f, -40, 1);
    CHECK_INT(1, x.s().detectionCount);
}

void deauth_cooldown_limits_repeat_counts_to_one_per_fifteen_seconds()
{
    Session x;
    x.svc.startDetector(ReconDetector::Deauth);
    const auto f = mgmtFrame(0x0C, kPlainMac);
    for (int i = 0; i < 6; ++i) deliver(f, -40, 1);
    for (int i = 0; i < 20; ++i) deliver(f, -40, 1);  // same burst, inside cooldown
    CHECK_INT(1, x.s().detections[0].encounterCount);
    fake_arduino::g_millis += 15000;
    // 15 s later the old window has expired, so a fresh burst is needed.
    for (int i = 0; i < 5; ++i) deliver(f, -40, 1);
    CHECK_INT(1, x.s().detections[0].encounterCount);
    deliver(f, -40, 1);
    CHECK_INT(2, x.s().detections[0].encounterCount);
    CHECK_INT(1, x.s().detectionCount);
}

void disassoc_bursts_are_labelled_disassoc()
{
    Session x;
    x.svc.startDetector(ReconDetector::Deauth);
    const uint8_t other[6] = {0x02, 0x99, 0x88, 0x77, 0x66, 0x55};
    const auto dis = mgmtFrame(0x0A, other);
    for (int i = 0; i < 6; ++i) deliver(dis, -50, 3);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("Disassoc flood", x.s().detections[0].detail);
}

void deauth_frames_are_ignored_by_other_detectors_and_when_short()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const auto f = mgmtFrame(0x0C, kPlainMac);
    for (int i = 0; i < 10; ++i) deliver(f, -40, 1);
    CHECK_INT(0, x.s().detectionCount);
    x.svc.startDetector(ReconDetector::Deauth);
    const auto shortFrame = mgmtFrame(0x0C, kPlainMac, 23);
    for (int i = 0; i < 10; ++i) deliver(shortFrame, -40, 1);
    CHECK_INT(0, x.s().detectionCount);
}

void pwnagotchi_beacon_bssid_is_a_high_detection()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 11);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("PWNAGOTCHI", x.s().detections[0].category);
    CHECK_STR("Pwnagotchi beacon", x.s().detections[0].detail);
    CHECK_STR("DE:AD:BE:EF:DE:AD", x.s().detections[0].address);
    CHECK_INT(11, x.s().detections[0].channel);
}

void all_reports_pwnagotchi_and_pineapple_for_the_same_bssid()
{
    Session x;
    x.svc.startDetector(ReconDetector::All);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", true), -55, 1);
    CHECK_INT(2, x.s().detectionCount);
    CHECK_STR("PWNAGOTCHI", x.s().detections[0].category);
    CHECK_STR("PINEAPPLE", x.s().detections[1].category);
    CHECK_STR("Suspicious Pineapple OUI", x.s().detections[1].detail);
}

void pineapple_open_only_prefixes_need_an_open_network()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pineapple);
    const uint8_t mac[6] = {0x00, 0xC0, 0xCA, 1, 2, 3};
    deliver(beacon(mac, "x", true), -50, 1);
    CHECK_INT(0, x.s().detectionCount);
    deliver(beacon(mac, "x", false), -50, 1);
    CHECK_INT(1, x.s().detectionCount);
}

void wifi_oui_on_the_bssid_uses_the_printed_order()
{
    Session x;
    x.svc.startDetector(ReconDetector::Flock);
    const uint8_t flock[6] = {0x58, 0x8E, 0x81, 1, 2, 3};
    deliver(beacon(flock, "cam", true), -60, 6);
    const uint8_t esp[6] = {0x24, 0x0A, 0xC4, 1, 2, 3};
    deliver(beacon(esp, "plug", true), -60, 6);
    CHECK_INT(2, x.s().detectionCount);
    CHECK_STR("Flock", x.s().detections[0].detail);
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(x.s().detections[0].confidence));
    CHECK_STR("Flock? (ESP32)", x.s().detections[1].detail);
    CHECK_INT(static_cast<int>(SignalConfidence::Low), static_cast<int>(x.s().detections[1].confidence));
}

void axon_ssid_prefix_detail_quotes_the_ssid()
{
    Session x;
    x.svc.startDetector(ReconDetector::Axon);
    deliver(beacon(kPlainMac, "ab3-X81234", true), -60, 6);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("AXON", x.s().detections[0].category);
    CHECK_STR("SSID ab3-X81234", x.s().detections[0].detail);
}

void ssid_is_only_read_when_the_first_element_is_an_ssid()
{
    Session x;
    x.svc.startDetector(ReconDetector::Axon);
    auto f = beacon(kPlainMac, "AXON-1", true);
    f[36] = 0x01;  // first element is not the SSID
    deliver(f, -60, 6);
    CHECK_INT(0, x.s().detectionCount);
}

void multissid_needs_two_confirmed_ssids_from_one_bssid()
{
    // Behaviour changed deliberately (Multi-SSID rules rewritten in core): a
    // name counts after three beacons, and the event counts confirmed names,
    // not beacons. Was: any second SSID flagged, then +1 on every beacon.
    Session x;
    x.svc.startDetector(ReconDetector::MultiSSID);
    for (int i = 0; i < 3; ++i) deliver(beacon(kPlainMac, "home", true), -60, 1);
    deliver(beacon(kPlainMac, "guest", true), -60, 1);
    deliver(beacon(kPlainMac, "guest", true), -60, 1);
    CHECK_INT(0, x.s().detectionCount);
    deliver(beacon(kPlainMac, "guest", true), -60, 1);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_STR("SSIDs: home | guest", x.s().detections[0].detail);
    for (int i = 0; i < 20; ++i) deliver(beacon(kPlainMac, "home", true), -60, 1);
    CHECK_INT(1, x.s().detections[0].encounterCount);  // beacons no longer count
}

void frames_the_receiver_marked_as_errors_are_not_classified()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "x", true), -60, 1, 1);  // rx_state 1
    CHECK_INT(0, x.s().detectionCount);
    deliver(beacon(pwn, "x", true), -60, 1);
    CHECK_INT(1, x.s().detectionCount);
}

void beacons_shorter_than_38_bytes_are_ignored()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    auto f = mgmtFrame(0x08, pwn, 37);
    deliver(f, -50, 1);
    CHECK_INT(0, x.s().detectionCount);
}

void early_warning_sweep_listens_for_the_four_background_wifi_detectors()
{
    Session x;
    x.svc.setEarlyWarningEnabled(true);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", true), -50, 1);
    const uint8_t flock[6] = {0x58, 0x8E, 0x81, 1, 2, 3};
    deliver(beacon(flock, "cam", true), -50, 1);  // Flock is not a background detector
    CHECK_INT(2, x.s().detectionCount);           // Pwnagotchi and Pineapple (DE:AD:BE)
    CHECK_STR("PWNAGOTCHI", x.s().detections[0].category);
    CHECK_STR("PINEAPPLE", x.s().detections[1].category);
    x.svc.setEarlyWarningEnabled(false);
}

// ================================================================ merging

void repeat_sighting_updates_rssi_channel_time_and_count_only()
{
    Session x;
    x.svc.startDetector(ReconDetector::Axon);
    deliver(beacon(kPlainMac, "AB2-first", true), -70, 1);
    fake_arduino::g_millis += 5000;
    deliver(beacon(kPlainMac, "AB4-second", true), -45, 9);
    CHECK_INT(1, x.s().detectionCount);
    const ReconDetection &e = x.s().detections[0];
    CHECK_STR("SSID AB2-first", e.detail);  // detail is from the first sighting
    CHECK_INT(-45, e.rssi);
    CHECK_INT(9, e.channel);
    CHECK_INT(6000, e.lastSeenMs);
    CHECK_INT(2, e.encounterCount);
    CHECK_INT(1, x.s().eventSerial);  // serial counts new records, not repeats
    CHECK_INT(1, x.sunk.size());      // the sink hears new records only
}

void same_address_different_detector_is_a_separate_record()
{
    Session x;
    x.svc.startDetector(ReconDetector::All);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 1);
    CHECK_INT(2, x.s().detectionCount);
    CHECK_STR(x.s().detections[0].address, x.s().detections[1].address);
}

void confidence_only_ever_rises()
{
    Session x;
    startBle(x, ReconDetector::Meta);
    NimBLEAdvertisedDevice low = bleDevice(kPlainMac);
    low.serviceUuids = {NimBLEUUID(uint16_t{0xFEB8})};
    NimBLEAdvertisedDevice high = bleDevice(kPlainMac);
    high.serviceUuids = {NimBLEUUID(uint16_t{0xFD5F})};

    x.svc.handleBleAdvertisement(&low);
    CHECK_INT(static_cast<int>(SignalConfidence::Low), static_cast<int>(x.s().detections[0].confidence));
    x.svc.handleBleAdvertisement(&high);
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(x.s().detections[0].confidence));
    x.svc.handleBleAdvertisement(&low);
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(x.s().detections[0].confidence));
    CHECK_INT(3, x.s().detections[0].encounterCount);
    CHECK_STR("Meta service", x.s().detections[0].detail);
}

void full_list_drops_the_oldest_record()
{
    Session x;
    startBle(x, ReconDetector::Tile);
    for (int i = 0; i < 41; ++i) {
        const uint8_t mac[6] = {0x02, 0, 0, 0, 0, static_cast<uint8_t>(i)};
        NimBLEAdvertisedDevice d = bleDevice(mac);
        d.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};
        x.svc.handleBleAdvertisement(&d);
    }
    CHECK_INT(40, x.s().detectionCount);
    CHECK_INT(41, x.s().eventSerial);
    CHECK_STR("02:00:00:00:00:01", x.s().detections[0].address);
    CHECK_STR("02:00:00:00:00:28", x.s().detections[39].address);
    CHECK_INT(41, x.sunk.size());
}

void detections_survive_stop_and_restart()
{
    Session x;
    startBle(x, ReconDetector::Tile);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};
    x.svc.handleBleAdvertisement(&d);
    x.svc.exitManualMode();
    x.svc.startDetector(ReconDetector::Deauth);
    CHECK_INT(1, x.s().detectionCount);
}

void clear_empties_the_list_and_the_alert()
{
    Session x;
    startBle(x, ReconDetector::Tile);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac);
    d.serviceUuids = {NimBLEUUID(uint16_t{0xFEED})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_TRUE(x.s().alertPending);
    x.svc.clearDetections();
    CHECK_INT(0, x.s().detectionCount);
    CHECK_FALSE(x.s().alertPending);
    CHECK_INT(1, x.s().eventSerial);  // serial is not reset
}

// ================================================================ alert suppression

void new_high_or_medium_record_raises_the_alert()
{
    Session x;
    x.svc.startDetector(ReconDetector::Deauth);
    const auto f = mgmtFrame(0x0C, kPlainMac);
    for (int i = 0; i < 6; ++i) deliver(f, -40, 1);  // Medium
    CHECK_TRUE(x.s().alertPending);
}

void low_confidence_logs_without_alert()
{
    Session x;
    x.svc.startDetector(ReconDetector::Flock);
    const uint8_t esp[6] = {0x24, 0x0A, 0xC4, 1, 2, 3};
    deliver(beacon(esp, "plug", true), -60, 6);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_FALSE(x.s().alertPending);
    CHECK_INT(1, x.sunk.size());  // still logged
}

void sleep_mode_logs_without_alert()
{
    Session x;
    x.svc.setSleepModeEnabled(true);
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 1);
    CHECK_INT(1, x.s().detectionCount);
    CHECK_FALSE(x.s().alertPending);
    CHECK_INT(1, x.sunk.size());
}

void a_repeat_sighting_never_raises_the_alert()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 1);
    x.svc.acknowledgeAlert();
    CHECK_FALSE(x.s().alertPending);
    deliver(beacon(pwn, "pwn", false), -55, 1);
    CHECK_FALSE(x.s().alertPending);
    CHECK_INT(2, x.s().detections[0].encounterCount);
}

void low_first_then_high_never_alerts_KNOWN_DEFECT()
{
    // An emitter first matched at Low is upgraded to High on a later match,
    // but the upgrade happens on the repeat path, which never raises the
    // alert. So it is logged as High and never buzzes. Pinned as-is.
    Session x;
    startBle(x, ReconDetector::Meta);
    NimBLEAdvertisedDevice low = bleDevice(kPlainMac);
    low.serviceUuids = {NimBLEUUID(uint16_t{0xFEB7})};
    NimBLEAdvertisedDevice high = bleDevice(kPlainMac);
    high.serviceUuids = {NimBLEUUID(uint16_t{0xFD5F})};
    x.svc.handleBleAdvertisement(&low);
    x.svc.handleBleAdvertisement(&high);
    CHECK_INT(static_cast<int>(SignalConfidence::High), static_cast<int>(x.s().detections[0].confidence));
    CHECK_FALSE(x.s().alertPending);
}

void poll_vibrates_once_per_new_alerting_record()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    x.svc.poll();
    CHECK_INT(0, instance.vibrations);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 1);
    x.svc.poll();
    x.svc.poll();
    CHECK_INT(1, instance.vibrations);
    // A second, different emitter while the first alert is still pending.
    x.svc.startDetector(ReconDetector::Axon);
    deliver(beacon(kPlainMac, "AXON-2", true), -50, 1);
    x.svc.poll();
    CHECK_INT(2, instance.vibrations);
}

void acknowledged_alert_does_not_vibrate()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    deliver(beacon(pwn, "pwn", false), -55, 1);
    x.svc.acknowledgeAlert();
    x.svc.poll();
    CHECK_INT(0, instance.vibrations);
}

// Added in Phase 0 Step 4: the fields the core event carries that the old
// record did not. Which radio saw it and on what band come from the
// platform, and lastSeen is the uptime at delivery.
void events_record_the_radio_and_band_that_saw_them()
{
    Session x;
    x.svc.startDetector(ReconDetector::Pwnagotchi);
    const uint8_t pwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};
    fake_arduino::g_millis = 4321;
    deliver(beacon(pwn, "pwn", false), -55, 1);
    CHECK_INT(1, x.core.eventCount());
    CHECK_INT(static_cast<int>(layertime::SourceKind::Wifi), static_cast<int>(x.core.event(0).sourceKind));
    CHECK_INT(static_cast<int>(layertime::Band::Band2_4GHz), static_cast<int>(x.core.event(0).band));
    CHECK_INT(4321, x.core.event(0).lastSeen.uptimeMs);

    startBle(x, ReconDetector::AirTag);
    NimBLEAdvertisedDevice d = bleDevice(kPlainMac, -71);
    d.manufacturerData = {mfg({0x4C, 0x00, 0x12, 0x19})};
    x.svc.handleBleAdvertisement(&d);
    CHECK_INT(2, x.core.eventCount());
    CHECK_INT(static_cast<int>(layertime::SourceKind::Ble), static_cast<int>(x.core.event(1).sourceKind));
    CHECK_INT(static_cast<int>(layertime::Band::Unknown), static_cast<int>(x.core.event(1).band));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(find_my_subtypes_match_only_separated_and_near_owner);
    CASE(flock_names_are_empty_penguin_battery_or_ten_digits);
    CASE(ssid_hash_is_16_bit_djb2);
    CASE(ssid_prefix_is_case_insensitive_and_length_bound);
    CASE(mac_text_is_uppercase_with_colons);
    CASE(pineapple_oui_list_and_open_network_rule);
    CASE(oui_table_is_exactly_the_current_39_entries);
    CASE(ble_uuid_table_is_exactly_the_current_10_entries);
    CASE(group_membership_is_fixed);
    CASE(detector_and_confidence_names_are_fixed);
    CASE(airtag_find_my_beacon_is_a_high_detection);
    CASE(airtag_proximity_pairing_and_other_vendors_are_ignored);
    CASE(every_manufacturer_record_is_examined);
    CASE(flock_ble_needs_xuntong_id_and_a_flock_shaped_name);
    CASE(service_uuid_matches_use_name_when_present_else_label);
    CASE(only_16_bit_uuids_are_compared);
    CASE(a_ble_scan_only_reports_what_it_was_started_for);
    CASE(group_scan_reports_its_members);
    CASE(ble_oui_path_compares_the_low_three_bytes_KNOWN_DEFECT);
    CASE(early_warning_ble_burst_reports_only_flipper_and_meta);
    CASE(deauth_fires_on_the_sixth_frame_within_three_seconds);
    CASE(deauth_window_restarts_after_a_gap_longer_than_three_seconds);
    CASE(deauth_cooldown_limits_repeat_counts_to_one_per_fifteen_seconds);
    CASE(disassoc_bursts_are_labelled_disassoc);
    CASE(deauth_frames_are_ignored_by_other_detectors_and_when_short);
    CASE(pwnagotchi_beacon_bssid_is_a_high_detection);
    CASE(all_reports_pwnagotchi_and_pineapple_for_the_same_bssid);
    CASE(pineapple_open_only_prefixes_need_an_open_network);
    CASE(wifi_oui_on_the_bssid_uses_the_printed_order);
    CASE(axon_ssid_prefix_detail_quotes_the_ssid);
    CASE(ssid_is_only_read_when_the_first_element_is_an_ssid);
    CASE(multissid_needs_two_confirmed_ssids_from_one_bssid);
    CASE(frames_the_receiver_marked_as_errors_are_not_classified);
    CASE(beacons_shorter_than_38_bytes_are_ignored);
    CASE(early_warning_sweep_listens_for_the_four_background_wifi_detectors);
    CASE(repeat_sighting_updates_rssi_channel_time_and_count_only);
    CASE(same_address_different_detector_is_a_separate_record);
    CASE(confidence_only_ever_rises);
    CASE(full_list_drops_the_oldest_record);
    CASE(detections_survive_stop_and_restart);
    CASE(clear_empties_the_list_and_the_alert);
    CASE(new_high_or_medium_record_raises_the_alert);
    CASE(low_confidence_logs_without_alert);
    CASE(sleep_mode_logs_without_alert);
    CASE(a_repeat_sighting_never_raises_the_alert);
    CASE(low_first_then_high_never_alerts_KNOWN_DEFECT);
    CASE(poll_vibrates_once_per_new_alerting_record);
    CASE(acknowledged_alert_does_not_vibrate);
    CASE(events_record_the_radio_and_band_that_saw_them);
    CHECK_SUMMARY();
}
