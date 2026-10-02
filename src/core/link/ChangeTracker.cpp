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

#include "ChangeTracker.h"

namespace layertime {
namespace link {

void ChangeTracker::reset()
{
    _count = 0;
    _nextCount = 0;
    _cursor = 0;
    _changed = false;
    _seq = 1;
    _droppedMaxSeq = 0;
    _state = TrackedState{};
}

void ChangeTracker::begin()
{
    _nextCount = 0;
    _cursor = 0;
    _changed = false;
}

void ChangeTracker::event(const MonitorEvent &e)
{
    if (_nextCount >= kCapacity) return;
    // Previous-pass entries older than this event are gone: dropped or
    // cleared.
    while (_cursor < _count && _held[_cursor].eventId < e.eventId) {
        if (_held[_cursor].seq > _droppedMaxSeq) _droppedMaxSeq = _held[_cursor].seq;
        _changed = true;
        ++_cursor;
    }
    Entry n;
    n.eventId = e.eventId;
    n.count = e.count;
    n.lastSeenMs = e.lastSeen.uptimeMs;
    n.confidence = static_cast<uint8_t>(e.confidence);
    n.rssi = e.rssi;
    n.channel = e.channel;
    n.seq = 0; // 0 marks created or changed in this pass
    if (_cursor < _count && _held[_cursor].eventId == e.eventId) {
        const Entry &p = _held[_cursor];
        if (p.count == n.count && p.lastSeenMs == n.lastSeenMs && p.confidence == n.confidence &&
            p.rssi == n.rssi && p.channel == n.channel) {
            n.seq = p.seq;
        }
        ++_cursor;
    }
    if (n.seq == 0) _changed = true;
    _next[_nextCount++] = n;
}

void ChangeTracker::end(const TrackedState &state)
{
    while (_cursor < _count) {
        if (_held[_cursor].seq > _droppedMaxSeq) _droppedMaxSeq = _held[_cursor].seq;
        _changed = true;
        ++_cursor;
    }
    if (state != _state) _changed = true;
    _state = state;
    if (_changed) ++_seq;
    for (uint8_t i = 0; i < _nextCount; ++i) {
        if (_next[i].seq == 0) _next[i].seq = _seq;
        _held[i] = _next[i];
    }
    _count = _nextCount;
    _changed = false;
}

uint32_t ChangeTracker::seqOf(uint32_t eventId) const
{
    for (uint8_t i = 0; i < _count; ++i) {
        if (_held[i].eventId == eventId) return _held[i].seq;
    }
    return 0;
}

} // namespace link
} // namespace layertime
