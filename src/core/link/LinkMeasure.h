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

// Link 0.2 Phase 0 measurement helpers (contracts/link_0_2.md, and the
// Phase 0 plan). Test builds only use these: the measurement firmware
// (tdongle_c5_measure, LAYERTIME_MEASURE) binds the test op BULK, and the
// Wi-Fi capture checks every frame's FCS. Nothing here is part of the Link
// 0.1 contract. The Node dispatcher never binds BULK, so a release Node
// answers it ERROR UnknownOp like every other test-range op
// (test/test_link_measure proves that).
//
// Pure C++: no stack, no clock, no hardware. Host-tested.

#include <stddef.h>
#include <stdint.h>

#include "LinkFrames.h"

namespace layertime {
namespace link {
namespace measure {

// BULK: `0xF2 | reqId | count u16 | size u8`. The Node answers with `count`
// Data notifications of `size` bytes (capped at the connection's MTU minus
// 3), then one BULK_DONE frame. Used to measure Link throughput at 20 bytes
// and at a larger MTU.
constexpr uint8_t kOpBulk = 0xF2;
static_assert(kOpBulk >= static_cast<uint8_t>(Op::TestFirst) && kOpBulk <= static_cast<uint8_t>(Op::TestLast),
              "BULK must stay in the test-only op range");
constexpr size_t kBulkRequestSize = 5;
constexpr uint16_t kBulkMaxCount = 1000;
constexpr size_t kBulkHeaderSize = 6;      // 0xF2, reqId, seq u16, count u16
constexpr size_t kBulkMinSize = kBulkHeaderSize;
constexpr size_t kBulkMaxSize = 244;       // MTU 247 minus the 3-byte ATT header
constexpr uint16_t kBulkDoneSeq = 0xFFFF;  // seq of the closing BULK_DONE frame
constexpr size_t kBulkDoneSize = 17;

struct BulkRequest {
    uint8_t reqId = 0;
    uint16_t count = 0;
    uint8_t size = 0;
};

// False for a wrong length, a count of 0 or above kBulkMaxCount, or a size
// below kBulkMinSize.
bool decodeBulk(const uint8_t *request, size_t len, BulkRequest &out);
size_t encodeBulk(const BulkRequest &in, uint8_t *out);

// The frame size actually used: the smallest of the request, MTU minus 3,
// and kBulkMaxSize. 0 when that is below kBulkMinSize.
size_t bulkFrameSize(uint8_t requested, uint16_t mtu);

// One BULK data frame: 0xF2, reqId, seq u16, count u16, then the byte
// pattern (i & 0xFF) for i from 6, so a receiver can check every byte.
size_t fillBulkFrame(uint8_t reqId, uint16_t seq, uint16_t count, size_t size, uint8_t *out);
// True when frame is a BULK data frame whose pattern is intact.
bool bulkFrameIntact(const uint8_t *frame, size_t len);

// BULK_DONE: 0xF2, reqId, 0xFF 0xFF, sent u16, size u8, mtu u16,
// refused u32 (notify calls NimBLE turned down and the Node retried),
// elapsedMs u32. 17 bytes, under the 20-byte ceiling.
struct BulkDone {
    uint8_t reqId = 0;
    uint16_t sent = 0;
    uint8_t size = 0;
    uint16_t mtu = 0;
    uint32_t refused = 0;
    uint32_t elapsedMs = 0;
};
size_t encodeBulkDone(const BulkDone &in, uint8_t *out);
bool decodeBulkDone(const uint8_t *frame, size_t len, BulkDone &out);

// IEEE 802.3 CRC-32, the 802.11 FCS: reflected polynomial 0xEDB88320,
// initial value and final XOR 0xFFFFFFFF.
uint32_t crc32(const uint8_t *data, size_t len);

// True when the last 4 bytes of frame are the little-endian CRC-32 of the
// bytes before them: the captured frame ends with its FCS. False when
// len < 8.
bool endsWithFcs(const uint8_t *frame, size_t len);

} // namespace measure
} // namespace link
} // namespace layertime
