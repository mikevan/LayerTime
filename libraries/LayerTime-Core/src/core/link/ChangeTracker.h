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

// LayerTime Link 0.1: the Node's change sequence (contracts/link.md, Status
// changeSeq and GET_CHANGED). Pure C++: no stack, no clock, no lock.
//
// The Node calls one pass per loop, with the event log it holds now, oldest
// first, and the Status fields that describe it:
//
//     begin(); for each event: event(e); end(state);
//
// A pass that finds anything new bumps changeSeq by one, and every event
// that was created or changed in that pass takes the new value as its own
// change sequence. An event that was held in the previous pass and is gone
// now (dropped when the log was full, or cleared) records its change
// sequence as dropped, which is what END's gap reports.

#include <stddef.h>
#include <stdint.h>

#include "../model/MonitorEvent.h"
#include "../model/ReconState.h"

namespace layertime {
namespace link {

// The Status fields other than version, session, heartbeat and schedule.
struct TrackedState {
    uint8_t flags = 0;
    uint8_t selected = 0;
    uint8_t active = 0;
    uint8_t eventCount = 0;
    uint32_t lastAlertEventId = 0;

    bool operator==(const TrackedState &o) const
    {
        return flags == o.flags && selected == o.selected && active == o.active &&
               eventCount == o.eventCount && lastAlertEventId == o.lastAlertEventId;
    }
    bool operator!=(const TrackedState &o) const { return !(*this == o); }
};

class ChangeTracker {
public:
    static constexpr uint8_t kCapacity = ReconState::kMaxEvents;

    ChangeTracker() { reset(); }

    // A new session: changeSeq 1, nothing held, nothing dropped.
    void reset();

    void begin();
    // Each event the Node holds now, oldest first. Events beyond kCapacity
    // are ignored (the log never holds more).
    void event(const MonitorEvent &e);
    void end(const TrackedState &state);

    uint32_t changeSeq() const { return _seq; }
    const TrackedState &state() const { return _state; }

    // The events of the last completed pass, oldest first.
    uint8_t heldCount() const { return _count; }
    uint32_t heldEventId(uint8_t i) const { return _held[i].eventId; }
    uint32_t heldSeq(uint8_t i) const { return _held[i].seq; }
    // The change sequence of a held event; 0 when it is not held.
    uint32_t seqOf(uint32_t eventId) const;

    // True when an event whose change sequence is above `since` has been
    // dropped: an interface synchronised to `since` missed that change.
    bool gapSince(uint32_t since) const { return _droppedMaxSeq > since; }

private:
    struct Entry {
        uint32_t eventId = 0;
        uint32_t count = 0;
        uint32_t lastSeenMs = 0;
        uint32_t seq = 0;
        uint8_t confidence = 0;
        int8_t rssi = 0;
        uint8_t channel = 0;
    };

    Entry _held[kCapacity];
    uint8_t _count = 0;
    Entry _next[kCapacity];
    uint8_t _nextCount = 0;
    // Index into _held of the next previous-pass entry to match; events and
    // entries are both in ascending eventId order.
    uint8_t _cursor = 0;
    bool _changed = false;
    uint32_t _seq = 1;
    uint32_t _droppedMaxSeq = 0;
    TrackedState _state;
};

} // namespace link
} // namespace layertime
