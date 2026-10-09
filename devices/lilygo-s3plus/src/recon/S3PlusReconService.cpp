// LayerTime - counter-intrusion and resilient-communications firmware
// for the LilyGo T-Watch S3 Plus.
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

#include "S3PlusReconService.h"

#include "core/logic/ReconClassification.h"
#include "core/logic/ReconSelection.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_wifi.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

namespace layertime {
namespace twatch_s3plus {

S3PlusReconService *S3PlusReconService::_activeInstance = nullptr;

namespace {
constexpr uint32_t kChannelHopMs = 650;
constexpr uint32_t kBleCycleMs = 12000;
constexpr uint32_t kBleScanMs = 1800;

// Pulls one advertisement's manufacturer records and 16-bit UUIDs out of
// NimBLE for core's classification entry point (src/core/logic/ReconClassification).
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

// The single S3PlusReconService instance currently owning an in-flight async BLE
// scan. Set right before NimBLEScan::start(), read from the NimBLE host
// task's onResult() callback (mirrors the existing WiFi promiscuous callback
// pattern below, which has the same cross-task-write-into-_status shape).
S3PlusReconService *gBleActiveInstance = nullptr;

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

// Selection rules live in core (src/core/logic/ReconSelection). These are
// the ones the scheduling below asks.

bool S3PlusReconService::needsBle(ReconTarget detector)
{
    return layertime::recon::needsBle(detector);
}

bool S3PlusReconService::needsWifi(ReconTarget detector)
{
    return layertime::recon::needsWifi(detector);
}

void S3PlusReconService::begin()
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
}

void S3PlusReconService::startDetector(ReconTarget detector)
{
    stop();
    // Note: does NOT clear the event log here. The threat log is persistent
    // across start/stop cycles by design - only an explicit user action
    // (ReconScreen's CLEAR LOG button) clears it, so the user always knows
    // whether anything has ever been seen, not just this session.
    _status.detector = detector;
    _status.activeDetector = detector;
    _status.monitoring = detector != ReconTarget::None;
    if (!_status.monitoring) return;

    _allBleBursting = false;
    if (needsWifi(detector)) {
        // Both radios needed (ALL, or a group mixing Wi-Fi and BLE members):
        // start on Wi-Fi and let pollManual alternate into BLE bursts.
        startWifiMonitoring();
        _lastBleCycleMs = millis();
        if (detector == ReconTarget::All) _status.activeDetector = ReconTarget::Deauth;
    } else {
        startBleScan(detector, kBleScanMs);
        _lastBleCycleMs = millis();
    }
}

void S3PlusReconService::poll()
{
    const uint32_t now = millis();

    // Alert actuation (wake + vibrate) moved to core's tick() and the
    // T-Ultra AlertSink in Phase 0 Step 4, and still runs just before this.

    if (_status.monitoring) {
        pollManual(now);
        return;
    }

    pollEarlyWarning(now);
}

void S3PlusReconService::pollManual(uint32_t now)
{
    const bool mixedRadios = needsWifi(_status.detector) && needsBle(_status.detector);
    if (mixedRadios) {
        NimBLEScan *scan = _bleInitialized ? NimBLEDevice::getScan() : nullptr;
        const bool bleBusy = scan && scan->isScanning();

        if (_allBleBursting) {
            if (bleBusy) return;
            // BLE burst finished - resume the Wi-Fi sweep.
            startWifiMonitoring();
            if (_status.detector == ReconTarget::All)
                _status.activeDetector = ReconTarget::Deauth;
            _allBleBursting = false;
            _lastBleCycleMs = now;
            return;
        }

        if (now - _lastBleCycleMs >= kBleCycleMs) {
            stopWifiMonitoring();
            if (_status.detector == ReconTarget::All)
                _status.activeDetector = ReconTarget::Flock;
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

void S3PlusReconService::setEarlyWarningEnabled(bool enabled)
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

void S3PlusReconService::armEarlyWarningSweep(uint32_t now)
{
    startWifiMonitoring();
    _earlyWarningSweeping = true;
    _status.earlyWarningResting = false;
    _earlyWarningPhaseStartMs = now;
}

void S3PlusReconService::pollEarlyWarning(uint32_t now)
{
    if (!_earlyWarningEnabled) return;

    if (_earlyWarningSweeping) {
        if (now - _earlyWarningPhaseStartMs >= kEarlyWarningActiveMs) {
            // Sweep window done - a short BLE burst before resting.
            stopWifiMonitoring();
            startBleScan(ReconTarget::EarlyWarning, kBleScanMs);
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

void S3PlusReconService::startWifiMonitoring()
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

void S3PlusReconService::stopWifiMonitoring()
{
    if (_activeInstance == this) _activeInstance = nullptr;
    esp_wifi_set_promiscuous(false);
    delay(60);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    delay(60);
}

void S3PlusReconService::stop()
{
    stopWifiMonitoring();
    if (_bleInitialized && NimBLEDevice::isInitialized()) NimBLEDevice::getScan()->stop();
    _status.monitoring = false;
    _status.detector = ReconTarget::None;
    _status.activeDetector = ReconTarget::None;
    _allBleBursting = false;
    _earlyWarningSweeping = false;
    _status.earlyWarningResting = false;
}

void S3PlusReconService::exitManualMode()
{
    stop();
    if (_earlyWarningEnabled) {
        armEarlyWarningSweep(millis());
    }
}

// Called only with the event-log lock held (the ReconClearEvents command),
// because the Wi-Fi callback runs the same classifier under that lock.
void S3PlusReconService::resetDetectorState()
{
    _wifiClassifier.reset();
}

bool S3PlusReconService::wants(ReconTarget detector) const
{
    return layertime::recon::wants(_status.monitoring, _status.detector,
                                   _earlyWarningEnabled && _earlyWarningSweeping, detector);
}

void S3PlusReconService::setCandidateSink(layertime::recon::CandidateSink sink, void *context)
{
    _candidateSink = sink;
    _candidateSinkContext = context;
}

void S3PlusReconService::deliver(const layertime::recon::Candidate &candidate, layertime::SourceKind kind,
                           layertime::Band band)
{
    // The event log itself is core's (src/core/logic/MonitorEventLog).
    if (_candidateSink == nullptr) return;
    layertime::recon::Candidate stamped = candidate;
    stamped.sourceKind = kind;
    stamped.band = band;
    stamped.atMs = millis();
    _candidateSink(stamped, _candidateSinkContext);
}

void S3PlusReconService::promiscuousThunk(void *buf, int type)
{
    if (_activeInstance) _activeInstance->onPromiscuousPacket(buf, type);
}

void S3PlusReconService::onPromiscuousPacket(void *buf, int type)
{
    (void)type;
    if (!buf) return;
    auto *packet = static_cast<wifi_promiscuous_pkt_t *>(buf);
    // The receiver marks a frame it did not get cleanly with a nonzero
    // rx_state (ESP-IDF: "0: no error; others: error numbers which are not
    // public"). No detector reads such a frame.
    if (packet->rx_ctrl.rx_state != 0) return;
    // S3 Plus: the classifier state and the core's event history change
    // together, under the event-log lock (EventLock.h).
    EventLockGuard guard(*_lock);
    // Frame classification goes through core (src/core/logic/ReconClassification).
    // This side only unpacks what the Wi-Fi driver handed over.
    _wifiClassifier.classify(packet->payload, packet->rx_ctrl.sig_len, packet->rx_ctrl.rssi,
                             packet->rx_ctrl.channel, millis(), wantsThunk, this,
                             wifiCandidateThunk, this);
}

bool S3PlusReconService::wantsThunk(ReconTarget detector, const void *self)
{
    return static_cast<const S3PlusReconService *>(self)->wants(detector);
}

// The ESP32-S3's Wi-Fi radio is 2.4 GHz only.
void S3PlusReconService::wifiCandidateThunk(const layertime::recon::Candidate &c, void *self)
{
    static_cast<S3PlusReconService *>(self)->deliver(c, layertime::SourceKind::Wifi,
                                               layertime::Band::Band2_4GHz);
}

void S3PlusReconService::bleCandidateThunk(const layertime::recon::Candidate &c, void *self)
{
    static_cast<S3PlusReconService *>(self)->deliver(c, layertime::SourceKind::Ble,
                                               layertime::Band::Unknown);
}

void S3PlusReconService::startBleScan(ReconTarget detector, uint32_t durationMs)
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

void S3PlusReconService::handleBleAdvertisement(const NimBLEAdvertisedDevice *device)
{
    if (!device) return;
    // S3 Plus: see onPromiscuousPacket().
    EventLockGuard guard(*_lock);
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

    layertime::recon::classifyBleAdvert(advert, _currentBleScanDetector, bleCandidateThunk, this);
}

} // namespace twatch_s3plus
} // namespace layertime
