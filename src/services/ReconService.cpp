// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch Ultra.
//
// Copyright (C) 2026 Michael Van Geertruy
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
constexpr ReconDetector kTrackerMembers[] = {
    ReconDetector::AirTag, ReconDetector::Tile, ReconDetector::SamsungTag,
    ReconDetector::GoogleTag};
constexpr ReconDetector kCounterSurveilMembers[] = {
    ReconDetector::Flock, ReconDetector::Axon, ReconDetector::Meta};
constexpr ReconDetector kCounterIntrusionMembers[] = {
    ReconDetector::Deauth, ReconDetector::Pwnagotchi, ReconDetector::MultiSSID,
    ReconDetector::Pineapple, ReconDetector::Flipper};

// Which radios a single detector can be found on. Deliberately two
// independent predicates rather than one either/or: Flock is genuinely
// both - a BLE manufacturer/OUI match AND a Wi-Fi beacon BSSID match - and
// deriving Wi-Fi as "not BLE" would have silently disabled its beacon path
// whenever FLOCK was selected on its own.
bool isBleDetector(ReconDetector d)
{
    return d == ReconDetector::Flock || d == ReconDetector::AirTag ||
           d == ReconDetector::Flipper || d == ReconDetector::Meta ||
           d == ReconDetector::Tile || d == ReconDetector::SamsungTag ||
           d == ReconDetector::GoogleTag;
}

bool isWifiDetector(ReconDetector d)
{
    return d == ReconDetector::Deauth || d == ReconDetector::Pwnagotchi ||
           d == ReconDetector::MultiSSID || d == ReconDetector::Pineapple ||
           d == ReconDetector::Flock || d == ReconDetector::Axon;
}

// ---- BLE signature tables ----------------------------------------------
// Matching is done against PARSED advertisement fields - the manufacturer
// record's company ID, and the 16-bit service UUIDs - never by searching
// the raw payload for byte pairs. A two-byte pattern scanned across a
// 31-byte advertisement matches by coincidence often enough to make a
// wrist alert useless, which is what the previous implementation did.

constexpr uint16_t kAppleCompanyId = 0x004C;
constexpr uint16_t kXuntongCompanyId = 0x09C8;  // Flock's BLE radio supplier

// Apple Find My offline-finding subtypes: 0x12 near owner, 0x1E separated.
// SquachWatch-CYD additionally matches 0x07 ("proximity pairing"), which
// catches a tag earlier but also fires on AirPods and other Apple
// accessories. Off here on purpose: this drives a buzz on the wrist, and a
// detector that trips on the owner's own earbuds gets ignored. Flip to
// true to trade precision for earlier warning.
constexpr bool kMatchProximityPairing = false;

struct BleUuidSignature {
    uint16_t uuid;
    ReconDetector detector;
    const char *label;
    SignalConfidence confidence;
};

constexpr BleUuidSignature kBleUuidSignatures[] = {
    // Bluetooth SIG assigned, exclusive to one product line.
    {0xFD5F, ReconDetector::Meta, "Meta Ray-Ban glasses", SignalConfidence::High},
    {0xFEED, ReconDetector::Tile, "Tile tracker", SignalConfidence::High},
    {0xFEEC, ReconDetector::Tile, "Tile tracker", SignalConfidence::High},
    {0xFD5A, ReconDetector::SamsungTag, "Samsung SmartTag", SignalConfidence::High},
    // Flipper Zero case colours. Carried over from the previous byte
    // patterns {0x81,0x30}/{0x82,0x30}/{0x83,0x30}, read as little-endian
    // 16-bit UUIDs. UNVERIFIED against hardware - if Flipper detection
    // regresses, this reading is the first thing to re-check.
    {0x3081, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
    {0x3082, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
    {0x3083, ReconDetector::Flipper, "Flipper Zero", SignalConfidence::Medium},
    // 0xFEAA is Google's general Eddystone UUID - shared with retail and
    // asset beacons that are not trackers, so a match means "Google
    // beacon-class device", not "Find My Device tracker".
    {0xFEAA, ReconDetector::GoogleTag, "Google Find My Device", SignalConfidence::Medium},
    // Meta/Facebook service UUIDs carried over from the old byte patterns.
    // Unsourced, hence Low - the three remaining old patterns ({0xAB,0x01},
    // {0x8E,0x05}, {0x53,0x0D}) were dropped outright: two bytes each, no
    // provenance, and short enough to match noise constantly.
    {0xFEB7, ReconDetector::Meta, "Meta service", SignalConfidence::Low},
    {0xFEB8, ReconDetector::Meta, "Meta service", SignalConfidence::Low},
};

bool isFindMyBeacon(const uint8_t *mfg, size_t length)
{
    if (mfg == nullptr || length < 3) return false;
    const uint8_t subtype = mfg[2];
    if (subtype == 0x12 || subtype == 0x1E) return true;
    return kMatchProximityPairing && subtype == 0x07;
}

bool allDigits(const std::string &value)
{
    if (value.empty()) return false;
    for (char c : value) if (c < '0' || c > '9') return false;
    return true;
}

// Flock's BLE radio is a generic XUNTONG module, so the company ID alone is
// not enough - it is gated on a plausible device name. Kept from the
// previous implementation, which is stricter than SquachWatch's bare
// company-ID match and worth keeping.
bool isFlockName(const std::string &name)
{
    return name.empty() || name.rfind("Penguin-", 0) == 0 ||
           name == "FS Ext Battery" || (name.length() == 10 && allDigits(name));
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

struct OuiSignature {
    uint8_t oui[3];
    ReconDetector detector;
    SignalConfidence confidence;
    const char *label;
};

// Flock: LayerTime's original 8 prefixes plus SquachWatch-CYD's set, which
// were COMPLETELY DISJOINT from ours - zero overlap across 8 and 29.
//
// The split matters more than the merge. Over half of his entries are
// generic Espressif MA-L blocks, and his own source labels them that way
// (Flock-ESP32 / Flock-ESP-S3 / Flock-ESP-C6) even though his docs grade
// the whole set "High confidence". Those match any ESP32 dev board, ESP
// smart plug or hobby project in range - including, in principle, this
// watch. They are kept because coverage is coverage, but graded Low, which
// means they log without ever raising an alert (see addDetection).
//
// Deliberately NOT included: 82:6B:F2, which SquachWatch lists as
// "Flock-DeFlk". Its first octet has the locally-administered bit set
// (0x82 = 1000 0010), so it is a randomised address someone observed once,
// not a registered vendor prefix at all.
constexpr OuiSignature kOuiSignatures[] = {
    // --- Flock, registered prefixes (LayerTime originals) ---
    {{0x58,0x8E,0x81}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0xEC,0x1B,0xBD}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x90,0x35,0xEA}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x04,0x0D,0x84}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0xF0,0x82,0xC0}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x1C,0x34,0xF1}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x38,0x5B,0x44}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x94,0x34,0x69}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    // --- Flock, registered prefixes (from SquachWatch-CYD) ---
    {{0xB4,0x1E,0x52}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x24,0xB2,0xB9}, ReconDetector::Flock, SignalConfidence::High, "Flock Liteon"},
    {{0xD0,0x39,0x57}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x00,0xF4,0x8D}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x14,0x5A,0xFC}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x80,0x30,0x49}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0xE0,0x0A,0xF6}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x70,0xC9,0x4E}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x3C,0x91,0x80}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0xD8,0xF3,0xBC}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0xB8,0x35,0x32}, ReconDetector::Flock, SignalConfidence::High, "Flock"},
    {{0x00,0xA0,0xD8}, ReconDetector::Flock, SignalConfidence::High, "Flock Sierra"},
    // --- Axon / Taser body cameras ---
    {{0x00,0x25,0xDF}, ReconDetector::Axon, SignalConfidence::High, "Axon (Taser)"},
    {{0xE4,0x05,0x40}, ReconDetector::Axon, SignalConfidence::High, "Axon body cam"},
    {{0x28,0x24,0xFF}, ReconDetector::Axon, SignalConfidence::High, "Axon Signal"},
    // --- Flock, generic Espressif blocks: LOW, log-only, no alert ---
    {{0x24,0x0A,0xC4}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0x30,0xAE,0xA4}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0x24,0x6F,0x28}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xCC,0x50,0xE3}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xDC,0x54,0x75}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xE8,0x9F,0x6D}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0x8C,0xAA,0xB5}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
    {{0x34,0x85,0x18}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
    {{0xD4,0xAD,0xFC}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xAC,0x67,0xB2}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0x84,0xF3,0xEB}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S3)"},
    {{0xB4,0xE6,0x2D}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xCC,0xDB,0xA7}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0x94,0xB9,0x7E}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP32)"},
    {{0xA4,0xCF,0x12}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-S2)"},
    {{0xC0,0x49,0xEF}, ReconDetector::Flock, SignalConfidence::Low, "Flock? (ESP-C6)"},
};

const OuiSignature *lookupOui(const uint8_t *mac)
{
    if (mac == nullptr) return nullptr;
    for (const OuiSignature &sig : kOuiSignatures)
        if (memcmp(mac, sig.oui, 3) == 0) return &sig;
    return nullptr;
}

// Axon body cameras advertise these SSID prefixes while in pairing mode.
// Source: Axon's own public device-management documentation.
struct SsidPrefixSignature {
    const char *prefix;
    ReconDetector detector;
    SignalConfidence confidence;
};

constexpr SsidPrefixSignature kSsidPrefixes[] = {
    {"AB2-", ReconDetector::Axon, SignalConfidence::High},
    {"AB3-", ReconDetector::Axon, SignalConfidence::High},
    {"AB4-", ReconDetector::Axon, SignalConfidence::High},
    {"AXON-", ReconDetector::Axon, SignalConfidence::High},
};

// The SSID in a beacon is not null-terminated, so this compares against the
// raw bytes and length rather than reaching for strncasecmp.
bool ssidHasPrefix(const uint8_t *ssid, uint8_t ssidLength, const char *prefix)
{
    const size_t n = strlen(prefix);
    if (ssidLength < n) return false;
    for (size_t i = 0; i < n; ++i) {
        char a = static_cast<char>(ssid[i]);
        char b = prefix[i];
        if (a >= 'a' && a <= 'z') a = static_cast<char>(a - 32);
        if (b >= 'a' && b <= 'z') b = static_cast<char>(b - 32);
        if (a != b) return false;
    }
    return true;
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
    case ReconDetector::Axon: return "AXON";
    case ReconDetector::Tile: return "TILE";
    case ReconDetector::SamsungTag: return "SMARTTAG";
    case ReconDetector::GoogleTag: return "GOOGLE TAG";
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
    if (members == nullptr) return isWifiDetector(detector);
    for (size_t i = 0; i < count; ++i)
        if (isWifiDetector(members[i])) return true;
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

const char *ReconService::confidenceLabel(SignalConfidence confidence)
{
    switch (confidence) {
    case SignalConfidence::High: return "HIGH";
    case SignalConfidence::Medium: return "MED";
    default: return "LOW";
    }
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
                                int8_t rssi, SignalConfidence confidence, uint8_t channel)
{
    const char *category = detectorName(detector);
    for (size_t i = 0; i < _status.detectionCount; ++i) {
        ReconDetection &existing = _status.detections[i];
        if (strcmp(existing.category, category) == 0 && strcmp(existing.address, address) == 0) {
            existing.rssi = rssi;
            existing.channel = channel;
            existing.lastSeenMs = millis();
            // Keep the strongest grade this device has ever matched at: a
            // Low-confidence hit later shouldn't downgrade a High one.
            if (confidence > existing.confidence) existing.confidence = confidence;
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
    entry.confidence = confidence;
    entry.lastSeenMs = millis();
    ++_status.eventSerial;
    // Sleep mode still logs and counts the detection above - it just never
    // arms the popup/vibration alert for it (see setSleepModeEnabled()).
    // Low-confidence matches are logged and counted like anything else but
    // deliberately never raise the alert: the generic-Espressif Flock
    // prefixes below would otherwise buzz the wrist for every ESP32 in
    // range, which is how a detector gets ignored. Same mechanism sleep
    // mode uses - nothing is missed, only the interruption is suppressed.
    if (!_sleepModeEnabled && confidence != SignalConfidence::Low) {
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
        // Medium: a real burst pattern, but the threshold and window are
        // judgement calls rather than a signature match.
        addDetection(ReconDetector::Deauth,
                     subtype == 0x0C ? "Deauth flood" : "Disassoc flood",
                     mac, packet->rx_ctrl.rssi, SignalConfidence::Medium,
                     packet->rx_ctrl.channel);
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
        // High: an exact match on a fixed, published BSSID.
        addDetection(ReconDetector::Pwnagotchi, "Pwnagotchi beacon", mac, rssi,
                     SignalConfidence::High, channel);

    // The SSID is the first information element after the 24-byte header and
    // the 12 bytes of fixed parameters: tag ID at [36], length at [37], data
    // from [38]. 802.11 puts the SSID first, but verify the tag rather than
    // assume it - otherwise a frame whose first element is something else
    // has an unrelated byte read as an SSID length.
    const bool haveSsid = payload[36] == 0x00 && payload[37] <= 32 &&
                          38U + payload[37] <= length;
    const uint8_t ssidLength = haveSsid ? payload[37] : 0;

    // Vendor OUI on the beacon's BSSID. The Flock and Axon prefixes are
    // Wi-Fi MAC blocks, so this is their natural home - the BLE-address
    // check in handleBleAdvertisement is the secondary path, not the main one.
    const OuiSignature *ouiMatch = lookupOui(bssid);
    if (ouiMatch != nullptr && wants(ouiMatch->detector))
        addDetection(ouiMatch->detector, ouiMatch->label, mac, rssi,
                     ouiMatch->confidence, channel);

    if (haveSsid) {
        for (const SsidPrefixSignature &sig : kSsidPrefixes) {
            if (!ssidHasPrefix(payload + 38, ssidLength, sig.prefix)) continue;
            if (!wants(sig.detector)) break;
            char detail[40];
            const int copied = ssidLength < 31 ? ssidLength : 31;
            snprintf(detail, sizeof(detail), "SSID %.*s", copied,
                     reinterpret_cast<const char *>(payload + 38));
            addDetection(sig.detector, detail, mac, rssi, sig.confidence, channel);
            break;
        }
    }
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
                // Medium: some legitimate APs also serve several SSIDs from
                // one BSSID, so this is a pattern rather than proof.
                addDetection(ReconDetector::MultiSSID, "Multiple SSIDs from BSSID", mac, rssi,
                             SignalConfidence::Medium, channel);
        }
    }
    if (wants(ReconDetector::Pineapple)) {
        const uint16_t capabilities = static_cast<uint16_t>(payload[34] | (payload[35] << 8));
        if (isPineappleOui(bssid, (capabilities & 0x10) == 0))
            // Medium: an OUI list built from older Hak5 hardware, and some
            // of those prefixes are shared with legitimate vendors.
            addDetection(ReconDetector::Pineapple, "Suspicious Pineapple OUI", mac, rssi,
                         SignalConfidence::Medium, channel);
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
    const ReconDetector scan = _currentBleScanDetector;
    const std::string name = device->haveName() ? device->getName() : std::string();
    const std::string address = device->getAddress().toString();
    const uint8_t *mac = device->getAddress().getVal();
    const int8_t rssi = device->getRSSI();
    const char *label = name.empty() ? nullptr : name.c_str();

    // ---- manufacturer data ----
    // An advertisement may carry more than one manufacturer record, so walk
    // them all rather than assuming index 0.
    const uint8_t mfgCount = device->haveManufacturerData() ? device->getManufacturerDataCount() : 0;
    for (uint8_t i = 0; i < mfgCount; ++i) {
        const std::string mfg = device->getManufacturerData(i);
        if (mfg.size() < 2) continue;
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(mfg.data());
        const uint16_t company = static_cast<uint16_t>(bytes[0]) |
                                 static_cast<uint16_t>(static_cast<uint16_t>(bytes[1]) << 8);

        if (company == kAppleCompanyId && bleScanWants(scan, ReconDetector::AirTag) &&
            isFindMyBeacon(bytes, mfg.size())) {
            // High: Apple company ID plus a Find My offline-finding subtype.
            addDetection(ReconDetector::AirTag, "Find My tracker beacon", address.c_str(),
                         rssi, SignalConfidence::High);
        }
        if (company == kXuntongCompanyId && bleScanWants(scan, ReconDetector::Flock) &&
            isFlockName(name)) {
            // High: the XUNTONG company ID alone would be weak, but it is
            // gated on a Flock-shaped device name.
            addDetection(ReconDetector::Flock, label ? label : "Flock BLE signature",
                         address.c_str(), rssi, SignalConfidence::High);
        }
    }

    // Vendor OUI on the BLE address, independent of any advertised data.
    const OuiSignature *ouiMatch = lookupOui(mac);
    if (ouiMatch != nullptr && bleScanWants(scan, ouiMatch->detector)) {
        addDetection(ouiMatch->detector, label ? label : ouiMatch->label,
                     address.c_str(), rssi, ouiMatch->confidence);
    }

    // ---- 16-bit service UUIDs ----
    const uint8_t uuidCount = device->haveServiceUUID() ? device->getServiceUUIDCount() : 0;
    for (uint8_t i = 0; i < uuidCount; ++i) {
        const NimBLEUUID uuid = device->getServiceUUID(i);
        // Compare through NimBLEUUID::equals() rather than reaching into
        // ble_uuid_t, whose layout has changed between NimBLE versions.
        if (uuid.bitSize() != 16) continue;
        for (const BleUuidSignature &sig : kBleUuidSignatures) {
            if (!uuid.equals(NimBLEUUID(sig.uuid))) continue;
            if (!bleScanWants(scan, sig.detector)) break;
            addDetection(sig.detector, label ? label : sig.label, address.c_str(), rssi,
                         sig.confidence);
            break;
        }
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
