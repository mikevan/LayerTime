// LayerTime - passive early-warning firmware for the LILYGO T-Dongle-C5.
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

// This file belongs to the tdongle_c5 build environments only; the T-Watch
// Ultra build sees it as an empty translation unit.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5ReconRadio.h"

#include "C5StageLog.h"

#include "../../core/link/LinkFrames.h"
#include "../../core/logic/BleAdvertClassifier.h"
#include "../../core/logic/ReconSelection.h"
#include "../../core/logic/ReconSignatures.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_timer.h>
#include <esp_wifi.h>
#include <freertos/FreeRTOS.h>
#include <soc/soc_caps.h>

#include <string>

namespace layertime {
namespace tdongle_c5 {

C5ReconRadio *C5ReconRadio::_activeInstance = nullptr;

namespace {

// Pulls one advertisement's manufacturer records and 16-bit UUIDs out of
// NimBLE for the core classifier, exactly as the T-Ultra's ReconService does.
void bleManufacturerThunk(uint8_t index, std::string &out, const void *context)
{
    out = static_cast<const NimBLEAdvertisedDevice *>(context)->getManufacturerData(index);
}

bool bleUuid16Thunk(uint8_t index, uint16_t &out, const void *context)
{
    const NimBLEUUID uuid = static_cast<const NimBLEAdvertisedDevice *>(context)->getServiceUUID(index);
    if (uuid.bitSize() != 16) return false;
    for (const recon::BleUuidSignature &sig : recon::kBleUuidSignatures) {
        if (!uuid.equals(NimBLEUUID(sig.uuid))) continue;
        out = sig.uuid;
        return true;
    }
    return false;
}

// The single radio owning an in-flight async BLE scan, read from the NimBLE
// host task's callbacks.
C5ReconRadio *gBleActiveInstance = nullptr;

class C5BleScanCallbacks : public NimBLEScanCallbacks {
public:
    void onResult(const NimBLEAdvertisedDevice *device) override
    {
        if (gBleActiveInstance != nullptr) gBleActiveInstance->handleBleAdvertisement(device);
    }
    void onScanEnd(const NimBLEScanResults &results, int reason) override
    {
        (void)results;
        (void)reason;
        if (gBleActiveInstance != nullptr) gBleActiveInstance->handleBleScanEnd();
    }
};

C5BleScanCallbacks gBleScanCallbacks;

// Guards the peer sightings: written on the NimBLE host task, read on the
// loop task.
portMUX_TYPE gPeerLock = portMUX_INITIALIZER_UNLOCKED;

bool channelAccepted(uint8_t channel, void *)
{
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
    uint8_t primary = 0;
    wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
    if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return false;
    return primary == channel;
}

Band bandOfChannel(uint8_t channel)
{
    if (channel >= 1 && channel <= 14) return Band::Band2_4GHz;
    if (channel >= 32) return Band::Band5GHz;
    return Band::Unknown;
}

} // namespace

void C5ReconRadio::begin(bool dualBand)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
#if SOC_WIFI_SUPPORT_5G
    // The 2A baseline keeps the 2.4 GHz channel plan 1 to 11, pinned there so
    // esp_wifi_set_channel means what it means on the T-Ultra. The LayerWand
    // listens on both bands (Michael, 2026-10-02).
    esp_wifi_set_band_mode(dualBand ? WIFI_BAND_MODE_AUTO : WIFI_BAND_MODE_2G_ONLY);
#else
    (void)dualBand;
#endif
}

uint8_t C5ReconRadio::probeChannelPlan(uint8_t *out, uint8_t capacity)
{
    // Probed with promiscuous receive on, the mode Recon uses, and no frame
    // callback attached.
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    esp_wifi_set_promiscuous(true);
    const uint8_t n = buildChannelPlan(channelAccepted, nullptr, out, capacity);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    return n;
}

void C5ReconRadio::setCandidateSink(recon::CandidateSink sink, void *context)
{
    _candidateSink = sink;
    _candidateSinkContext = context;
}

void C5ReconRadio::resetDetectorState()
{
    // Not _wifiClassifier.reset() here: that raced the Wi-Fi task's
    // classify(). The Wi-Fi task resets before its next frame.
    _resetRequested.store(true);
}

// ---- ReconRadio ---------------------------------------------------------------

void C5ReconRadio::startWifiMonitoring(uint8_t channel)
{
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);
    _activeInstance = this;
    esp_wifi_set_promiscuous(false);
    esp_wifi_set_promiscuous_rx_cb(reinterpret_cast<wifi_promiscuous_cb_t>(promiscuousThunk));
    // The scheduler's hop cursor restarts at its plan's first channel.
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) ++_counters.channelRefusals;
    _channel = channel;
    _firstFrameOnChannel = true;
    if (esp_wifi_set_promiscuous(true) != ESP_OK) _activeInstance = nullptr;
    ++_counters.wifiStarts;
    if (_stage) _stage->record(recon::Stage::WifiStart, channel, 0, _activeInstance != nullptr);
}

void C5ReconRadio::setWifiChannel(uint8_t channel)
{
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) ++_counters.channelRefusals;
    _channel = channel;
    _firstFrameOnChannel = true;
    ++_counters.hops;
    if (_stage) _stage->record(recon::Stage::WifiHop, channel);
}

void C5ReconRadio::stopWifiMonitoring()
{
    if (_activeInstance == this) _activeInstance = nullptr;
    esp_wifi_set_promiscuous(false);
    delay(60);
    esp_wifi_set_promiscuous_rx_cb(nullptr);
    delay(60);
    ++_counters.wifiStops;
    if (_stage) _stage->record(recon::Stage::WifiStop, _channel);
    _channel = 0;
}

void C5ReconRadio::startBleScan(ReconTarget detector, uint32_t durationMs)
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
    // Each device is delivered once via onResult() and not retained.
    scan->setMaxResults(0);
    _currentBleScanDetector = detector;
    _firstAdvertInScan = true;
    gBleActiveInstance = this;
    ++_counters.bleScanStarts;
    if (_stage) _stage->record(recon::Stage::BleScanStart, static_cast<uint8_t>(detector), 0, durationMs);
    scan->start(durationMs, false);
}

void C5ReconRadio::stopBleScan()
{
    if (_bleInitialized && NimBLEDevice::isInitialized()) NimBLEDevice::getScan()->stop();
}

bool C5ReconRadio::bleScanning() const
{
    NimBLEScan *scan = _bleInitialized ? NimBLEDevice::getScan() : nullptr;
    return scan && scan->isScanning();
}

// ---- Acquisition ----------------------------------------------------------------

void C5ReconRadio::handleBleScanEnd()
{
    ++_counters.bleScanEnds;
    if (_stage) _stage->record(recon::Stage::BleScanEnd, static_cast<uint8_t>(_currentBleScanDetector), 0, _counters.adverts);
}

void C5ReconRadio::promiscuousThunk(void *buf, int type)
{
    if (_activeInstance) _activeInstance->onPromiscuousPacket(buf, type);
}

void C5ReconRadio::onPromiscuousPacket(void *buf, int type)
{
    (void)type;
    if (!buf) return;
    auto *packet = static_cast<wifi_promiscuous_pkt_t *>(buf);
    const uint8_t channel = packet->rx_ctrl.channel;
    ++_counters.frames;
    if (channel <= kMaxWifiChannel) ++_counters.framesByChannel[channel];
    if (_firstFrameOnChannel) {
        _firstFrameOnChannel = false;
        if (_stage) _stage->record(recon::Stage::WifiFrame, channel, 0, _counters.frames);
    }
    // A reset asked for from another task is done here, on the only task
    // that classifies, before this frame is classified.
    if (_resetRequested.exchange(false)) _wifiClassifier.reset();
    // Frame classification lives in core; this side only unpacks what the
    // Wi-Fi driver handed over, exactly as the T-Ultra does.
    _wifiClassifier.classify(packet->payload, packet->rx_ctrl.sig_len, packet->rx_ctrl.rssi,
                             channel, millis(), wantsThunk, this, wifiCandidateThunk, this);
}

void C5ReconRadio::handleBleAdvertisement(const NimBLEAdvertisedDevice *device)
{
    if (!device) return;
    ++_counters.adverts;
    if (_firstAdvertInScan) {
        _firstAdvertInScan = false;
        if (_stage) _stage->record(recon::Stage::BleAdvert, static_cast<uint8_t>(_currentBleScanDetector), 0, _counters.adverts);
    }
    // Another LayerWand (NODES on the screen): it advertises the LayerTime
    // service while no watch is connected to it. Never this dongle itself.
    static const NimBLEUUID kLayerTimeService(link::kServiceUuid);
    if (device->isAdvertisingService(kLayerTimeService) &&
        !device->getAddress().equals(NimBLEDevice::getAddress())) {
        const uint32_t now = millis();
        portENTER_CRITICAL(&gPeerLock);
        _peers.saw(device->getAddress().getVal(), now);
        portEXIT_CRITICAL(&gPeerLock);
    }
    const std::string name = device->haveName() ? device->getName() : std::string();
    const std::string address = device->getAddress().toString();

    recon::BleAdvertSource advert;
    advert.name = name.data();
    advert.nameLength = name.size();
    advert.printedAddress = address.c_str();
    // Handed over exactly as NimBLE stores it: least-significant byte first.
    // The characterized byte-order KNOWN_DEFECT lives here, as on the
    // T-Ultra. Deliberately not fixed in Increment 2A.
    advert.addressBytes = device->getAddress().getVal();
    advert.rssi = device->getRSSI();
    advert.manufacturerCount = device->haveManufacturerData() ? device->getManufacturerDataCount() : 0;
    advert.manufacturer = bleManufacturerThunk;
    advert.uuidCount = device->haveServiceUUID() ? device->getServiceUUIDCount() : 0;
    advert.uuid16 = bleUuid16Thunk;
    advert.context = device;

    recon::classifyBleAdvert(advert, _currentBleScanDetector, bleCandidateThunk, this);
}

uint8_t C5ReconRadio::peerCount(uint32_t nowMs) const
{
    portENTER_CRITICAL(&gPeerLock);
    const uint8_t n = _peers.count(nowMs);
    portEXIT_CRITICAL(&gPeerLock);
    return n;
}

bool C5ReconRadio::wants(ReconTarget detector) const
{
    if (_scheduler == nullptr) return false;
    const AcquisitionStatus &s = _scheduler->status();
    return recon::wants(s.monitoring, s.selected, _scheduler->sweepingForBackground(), detector);
}

bool C5ReconRadio::wantsThunk(ReconTarget detector, const void *self)
{
    return static_cast<const C5ReconRadio *>(self)->wants(detector);
}

void C5ReconRadio::wifiCandidateThunk(const recon::Candidate &c, void *self)
{
    static_cast<C5ReconRadio *>(self)->deliver(c, SourceKind::Wifi, bandOfChannel(c.channel));
}

void C5ReconRadio::bleCandidateThunk(const recon::Candidate &c, void *self)
{
    static_cast<C5ReconRadio *>(self)->deliver(c, SourceKind::Ble, Band::Unknown);
}

void C5ReconRadio::deliver(const recon::Candidate &candidate, SourceKind kind, Band band)
{
    ++_counters.candidates;
    const uint8_t d = static_cast<uint8_t>(candidate.detector);
    if (d < 18) ++_counters.candidatesByDetector[d];
    if (_stage) {
        _stage->record(recon::Stage::Candidate, d, static_cast<uint16_t>(static_cast<uint8_t>(candidate.rssi)),
                       static_cast<uint32_t>(kind));
    }
    // The event log itself is core's (src/core/logic/MonitorEventLog).
    if (_candidateSink == nullptr) return;
    recon::Candidate stamped = candidate;
    stamped.sourceKind = kind;
    stamped.band = band;
    stamped.atMs = millis();
    const int64_t before = esp_timer_get_time();
    _candidateSink(stamped, _candidateSinkContext);
    const uint32_t took = static_cast<uint32_t>(esp_timer_get_time() - before);
    if (took > _counters.sinkMaxUs) _counters.sinkMaxUs = took;
    if (_stage) _stage->record(recon::Stage::EventRecorded, d, 0, took);
}

} // namespace tdongle_c5
} // namespace layertime

#endif
