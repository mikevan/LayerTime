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

// This file belongs to the tdongle_c5 build environments only
// (devices/lilygo-layerwand/platformio.ini). The guard below dates from when
// every environment compiled all of src/, and is kept as a safety net.
#if defined(LAYERTIME_TARGET_TDONGLE_C5)

#include "C5Link.h"
#include "BringUpLogic.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <esp_mac.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

#include "core/link/LinkCodec.h"

namespace layertime {
namespace tdongle_c5 {

namespace {

constexpr uint32_t kHeartbeatMs = 1000;
// contracts/link.md: Status is notified within 200 ms of a change.
constexpr uint32_t kChangeNotifyMs = 200;

C5Link *gLink = nullptr;
NimBLEServer *gServer = nullptr;
NimBLECharacteristic *gControl = nullptr;
NimBLECharacteristic *gStatus = nullptr;
NimBLECharacteristic *gData = nullptr;
#if defined(LAYERTIME_LINK_TEST)
NimBLECharacteristic *gProbe = nullptr;
#endif

class ServerCallbacks : public NimBLEServerCallbacks {
public:
    void onConnect(NimBLEServer *, NimBLEConnInfo &info) override
    {
        if (gLink) gLink->onConnected(info.getAddress().toString().c_str(), info.getMTU());
    }
    void onDisconnect(NimBLEServer *, NimBLEConnInfo &, int) override
    {
        if (gLink) gLink->onDisconnected();
        // advertiseOnDisconnect(true) restarts advertising; nothing to do here.
    }
};

class ControlCallbacks : public NimBLECharacteristicCallbacks {
public:
    void onWrite(NimBLECharacteristic *c, NimBLEConnInfo &) override
    {
        const NimBLEAttValue v = c->getValue();
        if (gLink) gLink->onControlWritten(v.data(), v.size());
    }
};

ServerCallbacks gServerCallbacks;
ControlCallbacks gControlCallbacks;

uint16_t nonZeroSessionId()
{
    uint16_t id = 0;
    while (id == 0) id = static_cast<uint16_t>(esp_random() & 0xFFFF);
    return id;
}

} // namespace

void C5Link::lockIo() const
{
    if (_io) xSemaphoreTake(static_cast<SemaphoreHandle_t>(_io), portMAX_DELAY);
}

void C5Link::unlockIo() const
{
    if (_io) xSemaphoreGive(static_cast<SemaphoreHandle_t>(_io));
}

bool C5Link::begin(uint16_t capabilities)
{
    gLink = this;
    _io = xSemaphoreCreateMutex();
    _identity.sessionId = nonZeroSessionId();
    _identity.capabilities = capabilities;
#if defined(LAYERTIME_LINK_TEST)
    _identity.testBuild = true;
#endif
    _status.sessionId = _identity.sessionId;
    _status.changeSeq = 1;

    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_BT);
    formatAdvertisedName(mac, _name);

    if (!NimBLEDevice::init(_name)) return false;
    // The frame ceiling is 20 bytes by contract; a larger MTU only matters
    // for the Probe measurement, which the central decides by negotiating.
    NimBLEDevice::setMTU(247);

    gServer = NimBLEDevice::createServer();
    gServer->setCallbacks(&gServerCallbacks);
    gServer->advertiseOnDisconnect(true);

    NimBLEService *svc = gServer->createService(link::kServiceUuid);
    gControl = svc->createCharacteristic(link::kControlUuid, NIMBLE_PROPERTY::WRITE);
    gControl->setCallbacks(&gControlCallbacks);
    gStatus = svc->createCharacteristic(link::kStatusUuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    gData = svc->createCharacteristic(link::kDataUuid, NIMBLE_PROPERTY::NOTIFY);
#if defined(LAYERTIME_LINK_TEST)
    gProbe = svc->createCharacteristic(link::kProbeUuid, NIMBLE_PROPERTY::NOTIFY);
#endif
    // No svc->start(): in NimBLE-Arduino 2.x it is a deprecated no-op; the
    // GATT server (and its services) starts inside adv->start() below.

    uint8_t frame[link::kMaxFrame];
    gStatus->setValue(frame, static_cast<uint16_t>(link::encodeStatus(_status, frame)));

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData primary;
    primary.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    primary.addServiceUUID(link::kServiceUuid);
    NimBLEAdvertisementData scanResponse;
    scanResponse.setName(_name);
    adv->setAdvertisementData(primary);
    adv->setScanResponseData(scanResponse);
    adv->enableScanResponse(true);
    return adv->start();
}

void C5Link::service(uint32_t nowMs)
{
    if (nowMs - _lastBeatMs >= kHeartbeatMs) {
        _lastBeatMs = nowMs;
        lockIo();
        ++_status.heartbeat;
        unlockIo();
        notifyStatus(nowMs);
        return;
    }
    if (_statusDirty && nowMs - _lastStatusNotifyMs >= kChangeNotifyMs) {
        ++_changeNotifies;
        notifyStatus(nowMs);
    }
}

void C5Link::updateStatus()
{
    if (_server == nullptr) return;
    link::StatusSnapshot next;
    _server->fillStatus(next);
    lockIo();
    const bool changed = next.changeSeq != _status.changeSeq || next.flags != _status.flags ||
                         next.selected != _status.selected || next.active != _status.active ||
                         next.eventCount != _status.eventCount ||
                         next.lastAlertEventId != _status.lastAlertEventId;
    if (changed) {
        _status.changeSeq = next.changeSeq;
        _status.flags = next.flags;
        _status.selected = next.selected;
        _status.active = next.active;
        _status.eventCount = next.eventCount;
        _status.lastAlertEventId = next.lastAlertEventId;
    }
    unlockIo();
    if (changed) _statusDirty = true;
}

void C5Link::pushFrame(const uint8_t *frame, size_t len, void *self)
{
    C5Link *me = static_cast<C5Link *>(self);
    me->lockIo();
    // A reply to a central that has since gone is dropped, so the next
    // central does not find the pipe busy with it.
    if (me->_answeringConnection == me->_connection) me->_pipe.push(frame, len);
    me->unlockIo();
}

void C5Link::serviceRequests(uint32_t nowMs)
{
    if (_server == nullptr) return;
    uint8_t request[link::kMaxFrame];
    lockIo();
    const size_t n = _pipe.take(request);
    const uint8_t heartbeat = _status.heartbeat;
    _answeringConnection = _connection;
    unlockIo();
    if (n > 0) {
        // The server takes the event-log lock where it needs it; the io lock
        // is not held here, and pushFrame takes it per frame.
        _server->handle(_identity, heartbeat, request, n, nowMs, pushFrame, this);
        lockIo();
        _pipe.finish();
        unlockIo();
        ++_requestsAnswered;
    }
    for (uint8_t i = 0; i < kFramesPerService; ++i) {
        uint8_t frame[link::kMaxFrame];
        lockIo();
        const size_t len = _pipe.front(frame);
        unlockIo();
        if (len == 0) break;
        if (!_connected) break;
        if (!gData->notify(frame, len)) {
            // NimBLE is out of buffers: keep the frame and try next pass.
            ++_notifyRefused;
            break;
        }
        lockIo();
        _pipe.pop();
        unlockIo();
        ++_framesSent;
    }
}

void C5Link::notifyStatus(uint32_t nowMs)
{
    uint8_t frame[link::kMaxFrame];
    lockIo();
    const size_t n = link::encodeStatus(_status, frame);
    _statusDirty = false;
    _lastStatusNotifyMs = nowMs;
    unlockIo();
    gStatus->setValue(frame, static_cast<uint16_t>(n));
    if (_connected) {
        gStatus->notify();
#if defined(LAYERTIME_LINK_TEST)
        ++_statusNotifies; // counted after the call; scheduling is unchanged
#endif
    }
}

void C5Link::onConnected(const char *peer, uint16_t)
{
    strncpy(_peer, peer, sizeof(_peer) - 1);
    _peer[sizeof(_peer) - 1] = '\0';
    _connected = true;
    notifyStatus(millis());
}

void C5Link::onDisconnected()
{
    _connected = false;
    _peer[0] = '\0';
    // The central is gone: a waiting request and unsent reply frames go with
    // it. A reply in progress in the loop finishes into an empty pipe.
    lockIo();
    _pipe.clear();
    ++_connection;
    unlockIo();
}

void C5Link::onControlWritten(const uint8_t *data, size_t len)
{
    if (_server != nullptr && len >= 1) {
        const uint8_t op = data[0];
        const bool deferred = op == static_cast<uint8_t>(link::Op::Command) ||
                              op == static_cast<uint8_t>(link::Op::GetChanged) ||
                              op == static_cast<uint8_t>(link::Op::GetText);
        lockIo();
        const bool busy = _pipe.busy();
        const bool accepted = !busy && deferred && _pipe.offer(data, len);
        unlockIo();
        if (busy || (deferred && !accepted)) {
            // Rule 2: one outstanding request. (A deferred request longer
            // than a frame also lands here; the loop never sees it.)
            uint8_t reply[link::kMaxFrame];
            link::Error e;
            e.reqId = len >= 2 ? data[1] : 0;
            e.status = busy ? link::LinkStatus::Busy : link::LinkStatus::BadLength;
            if (busy) ++_busyReplies;
            gData->notify(reply, link::encodeError(e, reply));
            return;
        }
        if (accepted) return; // the loop answers it
    }
    uint8_t reply[link::kMaxFrame];
    const size_t n = link::dispatch(_identity, _status.heartbeat, data, len, reply);
    if (n >= 2 && len == link::kPingSize && data[0] == static_cast<uint8_t>(link::Op::Ping) &&
        reply[0] == static_cast<uint8_t>(link::FrameType::Ack)) {
        ++_pings;
        _lastToken = static_cast<uint32_t>(data[2]) | (static_cast<uint32_t>(data[3]) << 8) |
                     (static_cast<uint32_t>(data[4]) << 16) | (static_cast<uint32_t>(data[5]) << 24);
    }
    gData->notify(reply, n);
}

size_t C5Link::sendNextProbe()
{
#if defined(LAYERTIME_LINK_TEST)
    if (!_connected || gProbe == nullptr) return 0;
    uint8_t payload[link::kProbeMaxSize];
    const size_t size = link::fillProbe(link::kProbeSizes[_probeIndex], payload);
    _probeIndex = (_probeIndex + 1) % link::kProbeSizeCount;
    gProbe->notify(payload, size);
    return size;
#else
    return 0;
#endif
}

} // namespace tdongle_c5
} // namespace layertime

#endif
