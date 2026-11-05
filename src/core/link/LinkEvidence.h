// LayerTime - counter-intrusion and resilient-communications firmware.
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

// Link 0.2 evidence ops (contracts/link_0_2.md, slice 2, 2026-11-04): how the
// phone pulls the frames behind a detection out of the Hunter's EvidenceStore.
//
//   GET_FRAMES       0x06  op reqId eventId(u32)                       6 bytes
//     -> FRAME_WINDOW 0x87, one FRAME_INDEX 0x88 per kept frame, END
//   GET_FRAME_BYTES  0x07  op reqId eventId(u32) index(u8) offset(u16) 9 bytes
//     -> FRAME_BYTES 0x89 from (index, offset) through the END OF THE WINDOW,
//        then END. One request moves the whole window (measured: 56 KB/s at
//        MTU 247 against a 60 ms round trip, so one request per frame would be
//        about ten times slower). A reconnecting client resumes from the
//        (index, offset) it last received.
//
//   FRAME_WINDOW: type reqId eventId(u32) seen(u16) retained(u8) dropped(u16)
//                 truncated(u8) sourceKind(u8) flags(u8, bit 0 collecting) 14 bytes
//   FRAME_INDEX:  type reqId index channel rssi(i8) onAir(u16) captured(u16)
//                 flags(u8) offsetMs(i32, from the triggering frame)          14 bytes
//   FRAME_BYTES:  type reqId index offset(u16) bytes...                   5 + n
//
// Frames up to 20 bytes except FRAME_BYTES, which uses the connection's size
// (MTU minus 3, at most 244); a Garmin-sized connection still works at 20.
// An event the log no longer holds: RESULT InvalidArgument. An event with no
// window: RESULT NotReady. RESULT carries the op as its commandType. All
// multi-byte fields little-endian, as the rest of the Link.

#include <stddef.h>
#include <stdint.h>

#include "../logic/EvidenceStore.h"

namespace layertime {
namespace link {
namespace evidence {

constexpr uint8_t kOpGetFrames = 0x06;
constexpr uint8_t kOpGetFrameBytes = 0x07;
constexpr uint8_t kTypeFrameWindow = 0x87;
constexpr uint8_t kTypeFrameIndex = 0x88;
constexpr uint8_t kTypeFrameBytes = 0x89;
constexpr size_t kGetFramesSize = 6;
constexpr size_t kGetFrameBytesSize = 9;
constexpr size_t kFrameWindowSize = 14;
constexpr size_t kFrameIndexSize = 14;
constexpr size_t kFrameBytesHeader = 5;
constexpr size_t kMaxNotify = 244;
// HELLO_ACK capability bit 5 (Link 0.2): the Node answers the evidence ops.
constexpr uint16_t kCapRawCapture = 0x0020;

inline bool isEvidenceOp(uint8_t op) { return op == kOpGetFrames || op == kOpGetFrameBytes; }

// The notification size for FRAME_BYTES on a connection with this ATT MTU.
size_t frameSizeForMtu(uint16_t mtu);

// Sends one frame. False means the connection is gone and the answer stops.
using Send = bool (*)(const uint8_t *frame, size_t len, void *context);
using LockFn = void (*)(void *context);

// What the answer needs from the platform: the store, the lock that guards it
// (the Wi-Fi task writes it), and whether the event log still holds the event.
struct Access {
    const layertime::evidence::EvidenceStore *store = nullptr;
    LockFn lock = nullptr;
    LockFn unlock = nullptr;
    void *lockContext = nullptr;
};

void answerGetFrames(const Access &a, const uint8_t *req, size_t len, bool eventKnown, uint8_t sourceKind,
                     uint32_t nowMs, Send send, void *ctx);
void answerGetFrameBytes(const Access &a, const uint8_t *req, size_t len, bool eventKnown, size_t frameSize,
                         Send send, void *ctx);

// The eventId a request asks about (0 when the request is too short).
uint32_t requestEventId(const uint8_t *req, size_t len);

// Client side (and tests): encoders for the requests, decoders for the replies.
size_t encodeGetFrames(uint8_t reqId, uint32_t eventId, uint8_t *out);
size_t encodeGetFrameBytes(uint8_t reqId, uint32_t eventId, uint8_t index, uint16_t offset, uint8_t *out);

struct FrameWindow {
    uint8_t reqId = 0;
    uint32_t eventId = 0;
    uint16_t seen = 0;
    uint8_t retained = 0;
    uint16_t dropped = 0;
    uint8_t truncated = 0;
    uint8_t sourceKind = 0;
    bool collecting = false;
};
struct FrameIndex {
    uint8_t reqId = 0;
    uint8_t index = 0;
    uint8_t channel = 0;
    int8_t rssi = 0;
    uint16_t onAir = 0;
    uint16_t captured = 0;
    uint8_t flags = 0;
    int32_t offsetMs = 0;
};
bool decodeFrameWindow(const uint8_t *in, size_t len, FrameWindow &out);
bool decodeFrameIndex(const uint8_t *in, size_t len, FrameIndex &out);

} // namespace evidence
} // namespace link
} // namespace layertime
