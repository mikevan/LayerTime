#include "ReconService.h"

#include <Arduino.h>
#include <LilyGoLib.h>
#include <lvgl.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

ReconService *ReconService::_activeInstance = nullptr;

namespace {
constexpr uint32_t kChannelHopMs = 650;
constexpr uint32_t kBleCycleMs = 12000;
constexpr uint32_t kBleScanMs = 1800;

// Deauth burst thresholds. Values follow SquachWatch-CYD's tuning (6 frames
// / 3s / 15s cooldown), which is applied globally there; here the same
// numbers are applied per transmitter, so crossing the threshold is a
// stronger signal than it is in the original.
// Group membership. Single source of truth for both the radio scheduling
// below and the Recon menu's sub-pages - add a new detector here and it is
// picked up by the sweep and the UI at once.
constexpr ReconDetector kTrackerMembers[] = {ReconDetector::AirTag};
constexpr ReconDetector kCounterSurveilMembers[] = {ReconDetector::Flock, ReconDetector::Meta};
constexpr ReconDetector kCounterIntrusionMembers[] = {
    ReconDetector::Deauth, ReconDetector::Pwnagotchi, ReconDetector::MultiSSID,
    ReconDetector::Pineapple, ReconDetector::Flipper};

bool isBleDetector(ReconDetector d)
{
    return d == ReconDetector::Flock || d == ReconDetector::AirTag ||
           d == ReconDetector::Flipper || d == ReconDetector::Meta;
}

constexpr uint32_t kDeauthWindowMs = 3000;
constexpr uint16_t kDeauthBurstFrames = 6;
constexpr uint32_t kDeauthCooldownMs = 15000;

void formatMac(char *out, size_t outSize, const uint8_t *mac)
{
    if (!out || outSize < 18 || !mac) return;
    snprintf(out, outSize, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool containsBytes(const uint8_t *data, size_t length, const uint8_t *needle, size_t needleLength)
{
    if (!data || !needle || !needleLength || length < needleLength) return false;
    for (size_t i = 0; i <= length - needleLength; ++i)
        if (memcmp(data + i, needle, needleLength) == 0) return true;
    return false;
}

bool allDigits(const std::string &value)
{
    if (value.empty()) return false;
    for (char c : value) if (c < '0' || c > '9') return false;
    return true;
}

uint16_t hashSsid(const uint8_t *ssid, size_t length)
{
    uint16_t hash = 5381;
    for (size_t i = 0; i < length; ++i)
        hash = static_cast<uint16_t>(((hash << 5) + hash) + ssid[i]);
    return hash;
}

bool isPineappleOui(const uint8_t *mac, bool openNetwork)
{
    const uint32_t oui = (static_cast<uint32_t>(mac[0]) << 16) |
                         (static_cast<uint32_t>(mac[1]) << 8) | mac[2];
    switch (oui) {
    case 0x001337: case 0x02C0CA: case 0x021337: case 0x000A00:
    case 0x000C43: case 0x000CE7: case 0x0017A5: case 0x9CEFD5:
    case 0x9CE5D5: case 0xDEADBE: return true;
    case 0x00C0CA: case 0x1CBFCE: case 0x0CEFAF: return openNetwork;
    default: return false;
    }
}

bool isFlockOui(const uint8_t *mac)
{
    static const uint8_t ouis[][3] = {
        {0x58,0x8E,0x81},{0xEC,0x1B,0xBD},{0x90,0x35,0xEA},{0x04,0x0D,0x84},
        {0xF0,0x82,0xC0},{0x1C,0x34,0xF1},{0x38,0x5B,0x44},{0x94,0x34,0x69}
    };
    for (const auto &oui : ouis) if (memcmp(mac, oui, 3) == 0) return true;
    return false;
}

// The single ReconService instance currently owning an in-flight async BLE
// scan. Set right before NimBLEScan::start(), read from the NimBLE host
// task's onResult() callback (mirrors the existing WiFi promiscuous callback
// pattern below, which has the same cross-task-write-into-_status shape).
ReconService *gBleActiveInstance = nullptr;

class ReconBleScanCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice *device) override
    {
        if (gBleActiveInstance != nullptr) {
            gBleActiveInstance->handleBleAdvertisement(device);
        }
    }
};

ReconBleScanCallbacks gBleScanCallbacks;
}

const char *ReconService::detectorName(ReconDetector detector)
{
    switch (detector) {
    case ReconDetector::All: return "ALL";
    case ReconDetector::Trackers: return "TRACKERS";
    case ReconDetector::CounterSurveil: return "COUNTER-SURVEIL";
    case ReconDetector::CounterIntrusion: return "COUNTER-INTRUSION";
    case ReconDetector::Deauth: return "DEAUTH";
    case ReconDetector::Pwnagotchi: return "PWNAGOTCHI";
    case ReconDetector::MultiSSID: return "MULTISSID";
    case ReconDetector::Flock: return "FLOCK";
    case ReconDetector::Pineapple: return "PINEAPPLE";
    case ReconDetector::AirTag: return "AIRTAG";
    case ReconDetector::Flipper: return "FLIPPER";
    case ReconDetector::Meta: return "META";
    case ReconDetector::EarlyWarning: return "EARLY WARNING";
    default: return "STOPPED";
    }
}

const char *ReconService::detectorShortName(ReconDetector detector)
{
    switch (detector) {
    case ReconDetector::CounterSurveil: return "SURVEIL";
    case ReconDetector::CounterIntrusion: return "INTRUSION";
    case ReconDetector::EarlyWarning: return "EARLY WARN";
    default: return detectorName(detector);
    }
}

const ReconDetector *ReconService::groupMembers(ReconDetector group, size_t &count)
{
    switch (group) {
    case ReconDetector::Trackers:
        count = sizeof(kTrackerMembers) / sizeof(kTrackerMembers[0]);
        return kTrackerMembers;
    case ReconDetector::CounterSurveil:
        count = sizeof(kCounterSurveilMembers) / sizeof(kCounterSurveilMembers[0]);
        return kCounterSurveilMembers;
    case ReconDetector::CounterIntrusion:
        count = sizeof(kCounterIntrusionMembers) / sizeof(kCounterIntrusionMembers[0]);
        return kCounterIntrusionMembers;
    default:
        count = 0;
        return nullptr;
    }
}

bool ReconService::groupContains(ReconDetector group, ReconDetector detector)
{
    size_t count = 0;
    const ReconDetector *members = groupMembers(group, count);
    for (size_t i = 0; i < count; ++i)
        if (members[i] == detector) return true;
    return false;
}

bool ReconService::needsBle(ReconDetector detector)
{
    if (detector == ReconDetector::All) return true;
    size_t count = 0;
    const ReconDetector *members = groupMembers(detector, count);
    if (members == nullptr) return isBleDetector(detector);
    for (size_t i = 0; i < count; ++i)
        if (isBleDetector(members[i])) return true;
    return false;
}

bool ReconService::needsWifi(ReconDetector detector)
{
    if (detector == ReconDetector::All) return true;
    size_t count = 0;
    const ReconDetector *members = groupMembers(detector, count);
    if (members == nullptr) return !isBleDetector(detector);
    for (size_t i = 0; i < count; ++i)
        if (!isBleDetector(members[i])) return true;
    return false;
}

bool ReconService::bleScanWants(ReconDetector scanDetector, ReconDetector target)
{
    if (scanDetector == ReconDetector::All) return true;
    if (scanDetector == ReconDetector::EarlyWarning)
        return target == ReconDetector::Flipper || target == ReconDetector::Meta;
    if (scanDetector == target) return true;
    return groupContains(scanDetector, target);
}

void ReconService::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
}

void ReconService::startDetector(ReconDetector detector)
{
    stop();
    // Note: does NOT clearDetections() here. The threat log is persistent
    // across start/stop cycles by design - only an explicit user action
    // (ReconScreen's CLEAR LOG button) clears it, so the user always knows
    // whether anything has ever been seen, not just this session.
    _status.detector = detector;
    _status.activeDetector = detector;
    _status.monitoring = detector != ReconDetector::None;
    if (!_status.monitoring) return;

    _allBleBursting = false;
    if (needsWifi(detector)) {
        // Both radios needed (ALL, or a group mixing Wi-Fi and BLE members):
        // start on Wi-Fi and let pollManual alternate into BLE bursts.
        startWifiMonitoring();
        _lastBleCycleMs = millis();
        if (detector == ReconDetector::All) _status.activeDetector = ReconDetector::Deauth;
    } else {
        startBleScan(detector, kBleScanMs);
        _lastBleCycleMs = millis();
    }
}

void ReconService::poll()
{
    const uint32_t now = millis();

    // Hardware and LVGL calls stay on the application loop, never the Wi-Fi
    // or BLE callbacks.
    if (_status.alertPending && _alertActuatedSerial != _status.eventSerial) {
        _alertActuatedSerial = _status.eventSerial;
        lv_display_trigger_activity(nullptr);
        instance.vibrator();
    }

    if (_status.monitoring) {
        pollManual(now);
        return;
    }

    pollEarlyWarning(now);
}

void ReconService::pollManual(uint32_t now)
{
    const bool mixedRadios = needsWifi(_status.detector) && needsBle(_status.detector);
    if (mixedRadios) {
        NimBLEScan *scan = _bleInitialized ? NimBLEDevice::getScan() : nullptr;
        const bool bleBusy = scan && scan->isScanning();

        if (_allBleBursting) {
            if (bleBusy) return;
            // BLE burst finished - resume the Wi-Fi sweep.
            startWifiMonitoring();
            if (_status.detector == ReconDetector::All)
                _status.activeDetector = ReconDetector::Deauth;
            _allBleBursting = false;
            _lastBleCycleMs = now;
            return;
        }

        if (now - _lastBleCycleMs >= kBleCycleMs) {
            stopWifiMonitoring();
            if (_status.detector == ReconDetector::All)
                _status.activeDetector = ReconDetector::Flock;
            startBleScan(_status.detector, kBleScanMs);
            _allBleBursting = true;
            return;
        }

        if (now - _lastChannelHopMs >= kChannelHopMs) {
            _lastChannelHopMs = now;
            _wifiChannel = _wifiChannel >= 11 ? 1 : static_cast<uint8_t>(_wifiChannel + 1);
            esp_wifi_set_channel(_wifiChannel, WIFI_SECOND_CHAN_NONE);
        }
        return;
    }

    if (needsBle(_status.detector)) {
        NimBLEScan *scan = _bleInitialized ? NimBLEDevice::getScan() : nullptr;
        const bool bleBusy = scan && scan->isScanning();
        if (!bleBusy && now - _lastBleCycleMs >= 5000) {
            startBleScan(_status.detector, kBleScanMs);
            _lastBleCycleMs = now;
        }
        return;
    }

    if (now - _lastChannelHopMs >= kChannelHopMs) {
        _lastChannelHopMs = now;
        _wifiChannel = _wifiChannel >= 11 ? 1 : static_cast<uint8_t>(_wifiChannel + 1);
        esp_wifi_set_channel(_wifiChannel, WIFI_SECOND_CHAN_NONE);
    }
}

void ReconService::setEarlyWarningEnabled(bool enabled)
{
    if (enabled == _earlyWarningEnabled) {
        return;
    }
    _earlyWarningEnabled = enabled;
    _status.earlyWarningEnabled = enabled;

    if (!enabled) {
        if (!_status.monitoring) {
            // No manual session running, so any radio activity right now
            // belongs to the background scheduler - tear it down.
            stopWifiMonitoring();
            if (_bleInitialized && NimBLEDevice::isInitialized()) NimBLEDevice::getScan()->stop();
        }
        _earlyWarningSweeping = false;
        _status.earlyWarningResting = false;
        return;
    }

    if (!_status.monitoring) {
        armEarlyWarningSweep(millis());
    }
    // If a manual session is active, exitManualMode() will arm the sweep
    // once that session ends.
}

void ReconService::armEarlyWarningSweep(uint32_t now)
{
    startWifiMonitoring();
    _earlyWarningSweeping = true;
    _status.earlyWarningResting = false;
    _earlyWarningPhaseStartMs = now;
}

bool ReconService::isBackgroundWifiDetector(ReconDetector detector)
{
    switch (detector) {
    case ReconDetector::Deauth:
    case ReconDetector::Pwnagotchi:
    case ReconDetector::Pineapple:
    case ReconDetector::MultiSSID:
        return true;
    default:
        return false;
    }
}

void ReconService::pollEarlyWarning(uint32_t now)
{
    if (!_earlyWarningEnabled) return;

    if (_earlyWarningSweeping) {
        if (now - _earlyWarningPhaseStartMs >= kEarlyWarningActiveMs) {
            // Sweep window done - a short BLE burst before resting.
            stopWifiMonitoring();
            startBleScan(ReconDetector::EarlyWarning, kBleScanMs);
            _earlyWarningSweeping = false;
            _earlyWarningPhaseStartMs = now;
            return;
        }
        if (now - _lastChannelHopMs >= kChannelHopMs) {
            _lastChannelHopMs = now;
            _wifiChannel = _wifiChannel >= 11 ? 1 : static_cast<uint8_t>(_wifiChannel + 1);
            esp_wifi_set_channel(_wifiChannel, WIFI_SECOND_CHAN_NONE);
        }
        return;
    }

    NimBLEScan *scan = _bleInitialized ? NimBLEDevice::getScan() : nullptr;
    const bool bleBusy = scan && scan->isScanning();
    if (bleBusy) {
        return; // Let the BLE burst finish; handleBleAdvertisement() streams results in.
    }

    if (!_status.earlyWarningResting) {
        // BLE burst just finished - begin the rest window (both radios idle).
        _status.earlyWarningResting = true;
        _earlyWarningPhaseStartMs = now;
        return;
    }

    if (now - _earlyWarningPhaseStartMs >= kEarlyWarningRestMs) {
        armEarlyWarningSweep(now);
    }
}

void ReconService::startWifiMonitoring()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    _activeInstance = this;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(reinterpret_cast<wifi_promiscuous_cb_t>(promiscuousThunk));
    _wifiChannel = 1;
    esp_wifi_set_channel(_wifiChannel, WIFI_SECOND_CHAN_NONE);
    _lastChannelHopMs = millis();
    if (esp_wifi_set_promiscuous(true) != ESP_OK) _activeInstance = nullptr;
}

void ReconService::stopWifiMonitoring()
{
    if (_activeInstance == this) _activeInstance = nullptr;
    esp_wifi_set_promiscuous(false);
    delay(60);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    delay(60);
}

void ReconService::stop()
{
    stopWifiMonitoring();
    if (_bleInitialized && NimBLEDevice::isInitialized()) NimBLEDevice::getScan()->stop();
    _status.monitoring = false;
    _status.detector = ReconDetector::None;
    _status.activeDetector = ReconDetector::None;
    _allBleBursting = false;
    _earlyWarningSweeping = false;
    _status.earlyWarningResting = false;
}

void ReconService::exitManualMode()
{
    stop();
    if (_earlyWarningEnabled) {
        armEarlyWarningSweep(millis());
    }
}

void ReconService::clearDetections()
{
    _status.detectionCount = 0;
    _status.alertPending = false;
    _multiSsidCount = 0;
    memset(_multiSsid, 0, sizeof(_multiSsid));
    for (DeauthTracker &tracker : _deauth) tracker = DeauthTracker{};
}

void ReconService::acknowledgeAlert() { _status.alertPending = false; }
bool ReconService::wants(ReconDetector detector) const
{
    if (_status.monitoring) {
        return _status.detector == ReconDetector::All || _status.detector == detector ||
               groupContains(_status.detector, detector);
    }
    return _earlyWarningEnabled && _earlyWarningSweeping && isBackgroundWifiDetector(detector);
}

void ReconService::addDetection(ReconDetector detector, const char *detail, const char *address,
                                int8_t rssi, uint8_t channel)
{
    const char *category = detectorName(detector);
    for (size_t i = 0; i < _status.detectionCount; ++i) {
        ReconDetection &existing = _status.detections[i];
        if (strcmp(existing.category, category) == 0 && strcmp(existing.address, address) == 0) {
            existing.rssi = rssi;
            existing.channel = channel;
            existing.lastSeenMs = millis();
            ++existing.encounterCount;
            return;
        }
    }

    size_t index = _status.detectionCount;
    if (index >= ReconStatus::MAX_DETECTIONS) {
        memmove(&_status.detections[0], &_status.detections[1],
                sizeof(ReconDetection) * (ReconStatus::MAX_DETECTIONS - 1));
        index = ReconStatus::MAX_DETECTIONS - 1;
    } else ++_status.detectionCount;

    ReconDetection &entry = _status.detections[index];
    entry = ReconDetection{};
    snprintf(entry.category, sizeof(entry.category), "%s", category);
    snprintf(entry.detail, sizeof(entry.detail), "%s", detail ? detail : "Activity detected");
    snprintf(entry.address, sizeof(entry.address), "%s", address ? address : "");
    entry.rssi = rssi;
    entry.channel = channel;
    entry.lastSeenMs = millis();
    ++_status.eventSerial;
    // Sleep mode still logs and counts the detection above - it just never
    // arms the popup/vibration alert for it (see setSleepModeEnabled()).
    if (!_sleepModeEnabled) {
        _status.alertPending = true;
    }

    if (_detectionSink != nullptr) {
        _detectionSink(entry, _detectionSinkUserData);
    }
}

void ReconService::setDetectionSink(DetectionSink sink, void *userData)
{
    _detectionSink = sink;
    _detectionSinkUserData = userData;
}

void ReconService::promiscuousThunk(void *buf, int type)
{
    if (_activeInstance) _activeInstance->onPromiscuousPacket(buf, type);
}

void ReconService::onPromiscuousPacket(void *buf, int type)
{
    if (!buf) return;
    auto *packet = static_cast<wifi_promiscuous_pkt_t *>(buf);
    const uint16_t length = packet->rx_ctrl.sig_len;
    if (length < 24) return;
    const uint8_t frameType = (packet->payload[0] >> 2) & 0x03;
    const uint8_t subtype = (packet->payload[0] >> 4) & 0x0F;
    char mac[18];
    formatMac(mac, sizeof(mac), packet->payload + 10);

    // noteDeauthFrame() is last in the chain on purpose - it only runs for
    // frames that are actually deauth/disassoc while the detector is live,
    // and it returns true only on a genuine burst.
    if (wants(ReconDetector::Deauth) && frameType == 0 && (subtype == 0x0C || subtype == 0x0A) &&
        noteDeauthFrame(packet->payload + 10, millis()))
        addDetection(ReconDetector::Deauth,
                     subtype == 0x0C ? "Deauth flood" : "Disassoc flood",
                     mac, packet->rx_ctrl.rssi, packet->rx_ctrl.channel);
    if (frameType == 0 && subtype == 0x08)
        inspectBeacon(packet->payload, length, packet->rx_ctrl.rssi, packet->rx_ctrl.channel);
}

void ReconService::inspectBeacon(const uint8_t *payload, uint16_t length, int8_t rssi, uint8_t channel)
{
    if (length < 38) return;
    const uint8_t *bssid = payload + 10;
    char mac[18];
    formatMac(mac, sizeof(mac), bssid);
    static const uint8_t pwnMac[6] = {0xDE,0xAD,0xBE,0xEF,0xDE,0xAD};
    if (wants(ReconDetector::Pwnagotchi) && memcmp(bssid, pwnMac, 6) == 0)
        addDetection(ReconDetector::Pwnagotchi, "Pwnagotchi beacon", mac, rssi, channel);

    // The SSID is the first information element after the 24-byte header and
    // the 12 bytes of fixed parameters: tag ID at [36], length at [37], data
    // from [38]. 802.11 puts the SSID first, but verify the tag rather than
    // assume it - otherwise a frame whose first element is something else
    // has an unrelated byte read as an SSID length.
    const bool haveSsid = payload[36] == 0x00 && payload[37] <= 32 &&
                          38U + payload[37] <= length;
    const uint8_t ssidLength = haveSsid ? payload[37] : 0;
    if (wants(ReconDetector::MultiSSID) && haveSsid) {
        MultiSsidTracker *tracker = nullptr;
        for (size_t i = 0; i < _multiSsidCount; ++i)
            if (memcmp(_multiSsid[i].bssid, bssid, 6) == 0) { tracker = &_multiSsid[i]; break; }
        if (!tracker && _multiSsidCount < 8) {
            tracker = &_multiSsid[_multiSsidCount++];
            memcpy(tracker->bssid, bssid, 6);
        }
        if (tracker) {
            const uint16_t hash = ssidLength ? hashSsid(payload + 38, ssidLength) : 0xFFFF;
            bool known = false;
            for (uint8_t i = 0; i < tracker->count; ++i) if (tracker->hashes[i] == hash) known = true;
            if (!known && tracker->count < 4) tracker->hashes[tracker->count++] = hash;
            if (tracker->count >= 2)
                addDetection(ReconDetector::MultiSSID, "Multiple SSIDs from BSSID", mac, rssi, channel);
        }
    }
    if (wants(ReconDetector::Pineapple)) {
        const uint16_t capabilities = static_cast<uint16_t>(payload[34] | (payload[35] << 8));
        if (isPineappleOui(bssid, (capabilities & 0x10) == 0))
            addDetection(ReconDetector::Pineapple, "Suspicious Pineapple OUI", mac, rssi, channel);
    }
}

void ReconService::startBleScan(ReconDetector detector, uint32_t durationMs)
{
    stopWifiMonitoring();
    delay(120);
    if (!_bleInitialized) {
        if (!NimBLEDevice::isInitialized()) NimBLEDevice::init("LayerTime");
        _bleInitialized = true;
    }
    NimBLEScan *scan = NimBLEDevice::getScan();
    scan->stop();
    scan->clearResults();
    scan->setScanCallbacks(&gBleScanCallbacks);
    scan->setActiveScan(false);
    scan->setInterval(100);
    scan->setWindow(100);
    // 0 = don't cap distinct devices seen and don't retain them in NimBLE's
    // internal results vector - each device is delivered once via onResult()
    // below and then freed, which is exactly what an async/streaming scan
    // needs (unlike the old blocking getResults() call, this never batches).
    scan->setMaxResults(0);
    _currentBleScanDetector = detector;
    gBleActiveInstance = this;
    scan->start(durationMs, false);
}

void ReconService::handleBleAdvertisement(const NimBLEAdvertisedDevice *device)
{
    if (!device) return;
    const ReconDetector detector = _currentBleScanDetector;

    const std::vector<uint8_t> &payload = device->getPayload();
    const uint8_t *data = payload.data();
    const size_t length = payload.size();
    const std::string name = device->haveName() ? device->getName() : std::string();
    const std::string address = device->getAddress().toString();
    const uint8_t *mac = device->getAddress().getVal();

    if (bleScanWants(detector, ReconDetector::Flock)) {
        const uint8_t xuntong[] = {0xFF,0xC8,0x09};
        const bool flockName = name.rfind("Penguin-", 0) == 0 || name == "FS Ext Battery" ||
                               (name.length() == 10 && allDigits(name));
        if ((containsBytes(data, length, xuntong, 3) && (flockName || name.empty())) || isFlockOui(mac))
            addDetection(ReconDetector::Flock, name.empty() ? "Flock BLE signature" : name.c_str(),
                         address.c_str(), device->getRSSI());
    }
    if (bleScanWants(detector, ReconDetector::AirTag)) {
        const uint8_t apple1[] = {0x1E,0xFF,0x4C,0x00};
        const uint8_t apple2[] = {0x4C,0x00,0x12};
        if (containsBytes(data, length, apple1, 4) || containsBytes(data, length, apple2, 3))
            addDetection(ReconDetector::AirTag, "Find My advertisement", address.c_str(), device->getRSSI());
    }
    if (bleScanWants(detector, ReconDetector::Flipper)) {
        const uint8_t black[] = {0x81,0x30}, white[] = {0x82,0x30}, clear[] = {0x83,0x30};
        if (containsBytes(data, length, black, 2) || containsBytes(data, length, white, 2) ||
            containsBytes(data, length, clear, 2))
            addDetection(ReconDetector::Flipper, name.empty() ? "Flipper BLE signature" : name.c_str(),
                         address.c_str(), device->getRSSI());
    }
    if (bleScanWants(detector, ReconDetector::Meta)) {
        static const uint8_t meta[][2] = {{0x5F,0xFD},{0xB7,0xFE},{0xB8,0xFE},{0xAB,0x01},{0x8E,0x05},{0x53,0x0D}};
        bool match = false;
        for (const auto &id : meta) if (containsBytes(data, length, id, 2)) match = true;
        if (match) addDetection(ReconDetector::Meta, name.empty() ? "Meta BLE identifier" : name.c_str(),
                                address.c_str(), device->getRSSI());
    }
}

bool ReconService::noteDeauthFrame(const uint8_t *mac, uint32_t now)
{
    DeauthTracker *tracker = nullptr;
    for (DeauthTracker &candidate : _deauth) {
        if (candidate.used && memcmp(candidate.mac, mac, 6) == 0) {
            tracker = &candidate;
            break;
        }
    }

    if (tracker == nullptr) {
        // Unseen transmitter: take a free slot, or evict whichever tracked
        // transmitter has been quiet longest. The table is deliberately small
        // - a real flood comes from one or two sources, and anything larger
        // would just be remembering background noise.
        tracker = &_deauth[0];
        for (DeauthTracker &candidate : _deauth) {
            if (!candidate.used) {
                tracker = &candidate;
                break;
            }
            if (now - candidate.windowStartMs > now - tracker->windowStartMs) tracker = &candidate;
        }
        *tracker = DeauthTracker{};
        memcpy(tracker->mac, mac, 6);
        tracker->used = true;
        tracker->windowStartMs = now;
    }

    // Rolling window that restarts after a gap longer than itself, rather
    // than a fixed repeating interval - so isolated frames minutes apart
    // never accumulate into a burst, and a flood straddling a boundary is
    // not split into two halves that each miss the threshold.
    if (now - tracker->windowStartMs > kDeauthWindowMs) {
        tracker->windowStartMs = now;
        tracker->count = 0;
    }
    if (tracker->count < 0xFFFF) ++tracker->count;

    if (tracker->count < kDeauthBurstFrames) return false;
    // Cooldown throttles how fast a sustained flood drives the detection's
    // encounterCount up. It does not gate the alert itself - addDetection()
    // only raises alertPending for a genuinely new (category, address).
    if (tracker->lastFiredMs != 0 && now - tracker->lastFiredMs < kDeauthCooldownMs) return false;
    tracker->lastFiredMs = now;
    return true;
}
