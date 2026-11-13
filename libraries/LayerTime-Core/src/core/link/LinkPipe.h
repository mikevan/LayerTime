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

// LayerTime Link 0.1: the Node's side of rule 2, one outstanding request
// (contracts/link.md). Pure C++, no stack and no lock: the platform holds
// its own lock around every call, because requests arrive on the BLE task
// and replies are produced and sent from the application loop.
//
//   offer()        a request arrived. False means answer ERROR Busy: a
//                  request is waiting, being answered, or its reply frames
//                  are still queued.
//   take()         the loop takes the waiting request to answer it.
//   push()         the loop queues each reply frame, in order.
//   finish()       the loop has queued the whole reply.
//   front()/pop()  the loop sends queued frames, oldest first.
//   clear()        the central went away: everything is dropped.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "LinkFrames.h"

namespace layertime {
namespace link {

class LinkPipe {
public:
    // A full GET_CHANGED reply is 40 EVENT_SUMMARY frames and END.
    static constexpr uint8_t kCapacity = 48;

    bool busy() const { return _requestLen != 0 || _answering || _count != 0; }

    bool offer(const uint8_t *request, size_t len)
    {
        if (busy() || len == 0 || len > kMaxFrame) return false;
        memcpy(_request, request, len);
        _requestLen = static_cast<uint8_t>(len);
        return true;
    }

    // Copies the waiting request into out (kMaxFrame bytes) and returns its
    // length, or 0 when none is waiting.
    size_t take(uint8_t *out)
    {
        if (_requestLen == 0) return 0;
        const size_t n = _requestLen;
        memcpy(out, _request, n);
        _requestLen = 0;
        _answering = true;
        return n;
    }

    // False when the frame does not fit (the queue is full or the frame is
    // longer than kMaxFrame); the frame is then dropped and counted.
    bool push(const uint8_t *frame, size_t len)
    {
        if (len == 0 || len > kMaxFrame || _count >= kCapacity) {
            ++_dropped;
            return false;
        }
        Slot &s = _slots[(_head + _count) % kCapacity];
        memcpy(s.bytes, frame, len);
        s.len = static_cast<uint8_t>(len);
        ++_count;
        return true;
    }

    void finish() { _answering = false; }

    // The oldest queued frame, copied into out; 0 when the queue is empty.
    size_t front(uint8_t *out) const
    {
        if (_count == 0) return 0;
        const Slot &s = _slots[_head];
        memcpy(out, s.bytes, s.len);
        return s.len;
    }

    void pop()
    {
        if (_count == 0) return;
        _head = static_cast<uint8_t>((_head + 1) % kCapacity);
        --_count;
    }

    void clear()
    {
        _requestLen = 0;
        _answering = false;
        _head = 0;
        _count = 0;
    }

    uint8_t queued() const { return _count; }
    uint32_t dropped() const { return _dropped; }

private:
    struct Slot {
        uint8_t bytes[kMaxFrame];
        uint8_t len;
    };

    uint8_t _request[kMaxFrame] = {};
    uint8_t _requestLen = 0;
    bool _answering = false;
    Slot _slots[kCapacity] = {};
    uint8_t _head = 0;
    uint8_t _count = 0;
    uint32_t _dropped = 0;
};

} // namespace link
} // namespace layertime
