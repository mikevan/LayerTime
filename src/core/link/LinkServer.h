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

#pragma once

// LayerTime Link 0.1 server for a Node that runs Recon (contracts/link.md):
// COMMAND, GET_CHANGED and GET_TEXT against LayerTimeCore, and the live
// Status fields. HELLO, PING and every other op go to dispatch() unchanged.
// Pure C++: no stack, no clock, no hardware.
//
// Threading. On a Node whose radio callbacks write the event log from
// another task, the platform supplies a lock. The server holds it around
// every read of the event log and alert state and around the commands that
// write them (ReconClearEvents, ReconAcknowledgeAlert, SetSleepMode). It
// never holds it around ReconStart, ReconStop or SetEarlyWarning, which
// reach the radios. Frames go to the sink while the lock may be held, so the
// sink must not take it.

#include <stddef.h>
#include <stdint.h>

#include "../app/LayerTimeCore.h"
#include "ChangeTracker.h"
#include "LinkCodec.h"
#include "LinkFrames.h"

namespace layertime {
namespace link {

class LinkServer {
public:
    using FrameSink = void (*)(const uint8_t *frame, size_t len, void *context);
    using LockFn = void (*)(void *context);

    explicit LinkServer(LayerTimeCore &core) : _core(core) {}

    void setLock(LockFn lock, LockFn unlock, void *context);

    // A new session: changeSeq back to 1.
    void reset() { _tracker.reset(); }

    // One pass over the core's event log and state. Returns true when
    // changeSeq moved.
    bool refresh();

    // Fills changeSeq, flags, selected, active, eventCount and
    // lastAlertEventId from the last pass. The rest of the snapshot is the
    // transport's.
    void fillStatus(StatusSnapshot &s) const;
    uint32_t changeSeq() const { return _tracker.changeSeq(); }

    // Answers one Control request. Every reply frame goes to sink, in
    // order: one frame, or for GET_CHANGED and GET_TEXT a run ending with
    // END. nowMs is the Node's uptime, for EVENT_SUMMARY's age.
    void handle(const NodeIdentity &node, uint8_t heartbeat, const uint8_t *request, size_t len,
                uint32_t nowMs, FrameSink sink, void *context);

    // How many requests of each kind were answered (diagnostics).
    struct Counters {
        uint32_t commands = 0;
        uint32_t getChanged = 0;
        uint32_t getText = 0;
        uint32_t summaries = 0;
        uint32_t gaps = 0;
        // The most recent COMMAND answered with a RESULT.
        uint8_t lastCommandType = 0;
        uint8_t lastCommandArgument = 0;
        uint8_t lastCommandResult = 0;
    };
    const Counters &counters() const { return _counters; }

private:
    void lock() const { if (_lock) _lock(_lockContext); }
    void unlock() const { if (_unlock) _unlock(_lockContext); }
    void refreshLocked();
    void handleCommand(const uint8_t *request, size_t len, FrameSink sink, void *context);
    void handleGetChanged(const uint8_t *request, size_t len, uint32_t nowMs, FrameSink sink, void *context);
    void handleGetText(const uint8_t *request, size_t len, FrameSink sink, void *context);

    LayerTimeCore &_core;
    ChangeTracker _tracker;
    LockFn _lock = nullptr;
    LockFn _unlock = nullptr;
    void *_lockContext = nullptr;
    Counters _counters;
};

// Status.flags from the core's Recon state and sleep setting.
uint8_t statusFlags(const ReconState &r, bool sleepMode);

} // namespace link
} // namespace layertime
