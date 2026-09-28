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

#include "../core/logic/AlertPolicy.h"
#include "../core/logic/BleAdvertClassifier.h"
#include "../core/logic/ReconSelection.h"
#include "../core/logic/ReconSignatures.h"
#include "../core/logic/WifiFrameClassifier.h"

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

// Pulls one advertisement's manufacturer records and 16-bit UUIDs out of
// NimBLE for the core classifier (src/core/logic/BleAdvertClassifier).
void bleManufacturerThunk(uint8_t index, std::string &out, const void *context)
{
    out = static_cast<const NimBLEAdvertisedDevice *>(context)->getManufacturerData(index);
}

bool bleUuid16Thunk(uint8_t index, uint16_t &out, const void *context)
{
    const NimBLEUUID uuid =
        static_cast<const NimBLEAdvertisedDevice *>(context)->getServiceUUID(index);
    // Compare through NimBLEUUID::equals() rather than reaching into
    // ble_uuid_t, whose layout has changed between NimBLE versions. Only the
    // known signature values can match, so those are the only values tried.
    if (uuid.bitSize() != 16) return false;
    for (const layertime::recon::BleUuidSignature &sig : layertime::recon::kBleUuidSignatures) {
        if (!uuid.equals(NimBLEUUID(sig.uuid))) continue;
        out = sig.uuid;
        return true;
    }
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

// Selection rules and names live in core (src/core/logic/ReconSelection).
// These keep ReconService's existing interface for the screens.

const char *ReconService::detectorName(ReconDetector detector)
{
    return layertime::recon::detectorName(detector);
}

const char *ReconService::detectorShortName(ReconDetector detector)
{
    return layertime::recon::detectorShortName(detector);
}

const ReconDetector *ReconService::groupMembers(ReconDetector group, size_t &count)
{
    return layertime::recon::groupMembers(group, count);
}

bool ReconService::groupContains(ReconDetector group, ReconDetector detector)
{
    return layertime::recon::groupContains(group, detector);
}

bool ReconService::needsBle(ReconDetector detector)
{
    return layertime::recon::needsBle(detector);
}

bool ReconService::needsWifi(ReconDetector detector)
{
    return layertime::recon::needsWifi(detector);
}

bool ReconService::bleScanWants(ReconDetector scanDetector, ReconDetector target)
{
    return layertime::recon::bleScanWants(scanDetector, target);
}

const char *ReconService::confidenceLabel(SignalConfidence confidence)
{
    return layertime::recon::confidenceLabel(confidence);
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
    return layertime::recon::isBackgroundWifiDetector(detector);
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
    _wifiClassifier.reset();
}

void ReconService::acknowledgeAlert() { _status.alertPending = false; }
bool ReconService::wants(ReconDetector detector) const
{
    return layertime::recon::wants(_status.monitoring, _status.detector,
                                   _earlyWarningEnabled && _earlyWarningSweeping, detector);
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
            // Alert policy lives in core (src/core/logic/AlertPolicy): keep
            // the strongest grade seen, and never alert on a repeat.
            const layertime::alert::RepeatOutcome outcome =
                layertime::alert::onRepeatSighting(existing.confidence, confidence);
            existing.confidence = outcome.confidence;
            if (outcome.raiseAlert) _status.alertPending = true;
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
    // The detection above is logged and counted either way. Whether it also
    // buzzes is the core alert policy (src/core/logic/AlertPolicy).
    if (layertime::alert::raisesOnNewRecord(confidence, _sleepModeEnabled)) {
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
    (void)type;
    if (!buf) return;
    auto *packet = static_cast<wifi_promiscuous_pkt_t *>(buf);
    // Frame classification lives in core (src/core/logic/WifiFrameClassifier).
    // This side only unpacks what the Wi-Fi driver handed over.
    _wifiClassifier.classify(packet->payload, packet->rx_ctrl.sig_len, packet->rx_ctrl.rssi,
                             packet->rx_ctrl.channel, millis(), wantsThunk, this,
                             candidateThunk, this);
}

bool ReconService::wantsThunk(ReconDetector detector, const void *self)
{
    return static_cast<const ReconService *>(self)->wants(detector);
}

void ReconService::candidateThunk(const layertime::recon::Candidate &c, void *self)
{
    static_cast<ReconService *>(self)->addDetection(c.detector, c.detail, c.address, c.rssi,
                                                    c.confidence, c.channel);
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
    const std::string name = device->haveName() ? device->getName() : std::string();
    const std::string address = device->getAddress().toString();

    layertime::recon::BleAdvertSource advert;
    advert.name = name.data();
    advert.nameLength = name.size();
    advert.printedAddress = address.c_str();
    // Handed over exactly as NimBLE stores it: least-significant byte first.
    // The core OUI check reads bytes 0 to 2 as the vendor prefix, so this
    // handoff is where the characterized BLE byte-order KNOWN_DEFECT lives
    // (test_recon: ble_oui_path_compares_the_low_three_bytes_KNOWN_DEFECT).
    // Deliberately not fixed in Phase 0.
    advert.addressBytes = device->getAddress().getVal();
    advert.rssi = device->getRSSI();
    advert.manufacturerCount = device->haveManufacturerData() ? device->getManufacturerDataCount() : 0;
    advert.manufacturer = bleManufacturerThunk;
    advert.uuidCount = device->haveServiceUUID() ? device->getServiceUUIDCount() : 0;
    advert.uuid16 = bleUuid16Thunk;
    advert.context = device;

    layertime::recon::classifyBleAdvert(advert, _currentBleScanDetector, candidateThunk, this);
}
