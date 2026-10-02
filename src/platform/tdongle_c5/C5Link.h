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

#pragma once

// The LayerTime Link server on the T-Dongle-C5: a NimBLE GATT service with
// the Control, Status and Data characteristics of contracts/link.md, plus
// the Probe characteristic on test builds (LAYERTIME_LINK_TEST).
//
// All protocol logic lives in src/core/link (LinkCodec, LinkServer,
// LinkPipe); this class only moves bytes between NimBLE and that code, keeps
// the heartbeat, and re-advertises when the central goes away.
//
// Two Nodes use it. The Link-only Node (Increment 1, C5BringUp.cpp) answers
// every request at once in the BLE callback with dispatch(). The Recon Node
// (Increment 2B, C5App.cpp) attaches a LinkServer: HELLO and PING are still
// answered at once, COMMAND, GET_CHANGED and GET_TEXT wait for the loop
// (serviceRequests), and a request that arrives while another is being
// answered gets ERROR Busy (contracts/link.md rule 2).

#include <stddef.h>
#include <stdint.h>

#include "core/link/LinkCodec.h"
#include "core/link/LinkPipe.h"
#include "core/link/LinkServer.h"

namespace layertime {
namespace tdongle_c5 {

class C5Link {
public:
    // Starts NimBLE, builds the service, and begins advertising as
    // "LT-C5-xxxx". Returns false if NimBLE could not initialise.
    bool begin(uint16_t capabilities);

    // Runs the one-second heartbeat, and on a Recon Node the change
    // notification (within 200 ms of a change). Call from loop().
    void service(uint32_t nowMs);

    // Recon Node only. Call before begin().
    void attachServer(link::LinkServer *server) { _server = server; }
    // Recon Node only, from loop(): answers the waiting request, if any, and
    // hands up to kFramesPerService queued reply frames to NimBLE.
    void serviceRequests(uint32_t nowMs);
    // Recon Node only, from loop(): takes the Recon fields of Status from the
    // server's last pass. A change is notified by service().
    void updateStatus();
    static constexpr uint8_t kFramesPerService = 4;

    // Test builds only: sends the next probe payload (20, 21, 64, 180 bytes)
    // on the Probe characteristic. Returns the size sent, 0 when nothing
    // was sent (release build, or no subscriber).
    size_t sendNextProbe();

    bool connected() const { return _connected; }
    uint16_t sessionId() const { return _identity.sessionId; }
    uint8_t heartbeat() const { return _status.heartbeat; }
    uint32_t pingsAnswered() const { return _pings; }
    // Test builds only (LAYERTIME_LINK_TEST): Status notifications actually
    // handed to NimBLE's notify while a central was connected. Evidence for
    // the tactix 10-minute Status measurement; always 0 on release builds.
    uint32_t statusNotifies() const { return _statusNotifies; }
    uint32_t lastPingToken() const { return _lastToken; }
    // "aa:bb:cc:dd:ee:ff" of the connected central, or "" when none.
    const char *peerAddress() const { return _peer; }
    const char *advertisedName() const { return _name; }
    // Recon Node counters: requests answered by the loop, ERROR Busy
    // replies, reply frames sent, notify calls NimBLE refused (retried on the
    // next pass), reply frames dropped because the queue was full, and Status
    // notifications caused by a change rather than the heartbeat.
    uint32_t requestsAnswered() const { return _requestsAnswered; }
    uint32_t busyReplies() const { return _busyReplies; }
    uint32_t framesSent() const { return _framesSent; }
    uint32_t notifyRefused() const { return _notifyRefused; }
    uint32_t framesDropped() const { return _pipe.dropped(); }
    uint32_t changeNotifies() const { return _changeNotifies; }
    uint32_t changeSeq() const { return _status.changeSeq; }

    // NimBLE callbacks route here.
    void onConnected(const char *peer, uint16_t mtu);
    void onDisconnected();
    void onControlWritten(const uint8_t *data, size_t len);

private:
    void notifyStatus(uint32_t nowMs);
    void lockIo() const;
    void unlockIo() const;
    static void pushFrame(const uint8_t *frame, size_t len, void *self);

    link::NodeIdentity _identity;
    link::StatusSnapshot _status;
    link::LinkServer *_server = nullptr;
    // Guards _status and _pipe, which the BLE task and the loop share.
    void *_io = nullptr;
    link::LinkPipe _pipe;
    // Counts disconnects; a reply being built for an earlier connection is
    // not queued (pushFrame).
    uint32_t _connection = 0;
    uint32_t _answeringConnection = 0;
    bool _statusDirty = false;
    uint32_t _lastStatusNotifyMs = 0;
    uint32_t _requestsAnswered = 0;
    uint32_t _busyReplies = 0;
    uint32_t _framesSent = 0;
    uint32_t _notifyRefused = 0;
    uint32_t _changeNotifies = 0;
    bool _connected = false;
    char _peer[18] = "";
    char _name[link::kMaxFrame] = "";
    uint32_t _lastBeatMs = 0;
    uint32_t _pings = 0;
    uint32_t _lastToken = 0;
    uint32_t _statusNotifies = 0;
    size_t _probeIndex = 0;
};

} // namespace tdongle_c5
} // namespace layertime
