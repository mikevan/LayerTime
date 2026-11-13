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

#include "LinkMeasure.h"

namespace layertime {
namespace link {
namespace measure {

namespace {
void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i));
}
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

// Built at compile time (1 KB, read-only), so the per-frame check in the
// Wi-Fi callback is cheap and nothing is initialised at run time.
struct CrcTable {
    uint32_t v[256];
    constexpr CrcTable() : v()
    {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            v[i] = c;
        }
    }
};
constexpr CrcTable kCrcTable{};
} // namespace

bool decodeBulk(const uint8_t *request, size_t len, BulkRequest &out)
{
    if (request == nullptr || len != kBulkRequestSize || request[0] != kOpBulk) return false;
    BulkRequest r;
    r.reqId = request[1];
    r.count = get16(request + 2);
    r.size = request[4];
    if (r.count == 0 || r.count > kBulkMaxCount || r.size < kBulkMinSize) return false;
    out = r;
    return true;
}

size_t encodeBulk(const BulkRequest &in, uint8_t *out)
{
    out[0] = kOpBulk;
    out[1] = in.reqId;
    put16(out + 2, in.count);
    out[4] = in.size;
    return kBulkRequestSize;
}

size_t bulkFrameSize(uint8_t requested, uint16_t mtu)
{
    size_t size = requested;
    const size_t byMtu = mtu > 3 ? static_cast<size_t>(mtu) - 3 : 0;
    if (size > byMtu) size = byMtu;
    if (size > kBulkMaxSize) size = kBulkMaxSize;
    return size < kBulkMinSize ? 0 : size;
}

size_t fillBulkFrame(uint8_t reqId, uint16_t seq, uint16_t count, size_t size, uint8_t *out)
{
    if (size < kBulkMinSize || size > kBulkMaxSize) return 0;
    out[0] = kOpBulk;
    out[1] = reqId;
    put16(out + 2, seq);
    put16(out + 4, count);
    for (size_t i = kBulkHeaderSize; i < size; ++i) out[i] = static_cast<uint8_t>(i & 0xFF);
    return size;
}

bool bulkFrameIntact(const uint8_t *frame, size_t len)
{
    if (frame == nullptr || len < kBulkMinSize || len > kBulkMaxSize || frame[0] != kOpBulk) return false;
    if (get16(frame + 2) == kBulkDoneSeq) return false;
    for (size_t i = kBulkHeaderSize; i < len; ++i)
        if (frame[i] != static_cast<uint8_t>(i & 0xFF)) return false;
    return true;
}

size_t encodeBulkDone(const BulkDone &in, uint8_t *out)
{
    out[0] = kOpBulk;
    out[1] = in.reqId;
    put16(out + 2, kBulkDoneSeq);
    put16(out + 4, in.sent);
    out[6] = in.size;
    put16(out + 7, in.mtu);
    put32(out + 9, in.refused);
    put32(out + 13, in.elapsedMs);
    return kBulkDoneSize;
}

bool decodeBulkDone(const uint8_t *frame, size_t len, BulkDone &out)
{
    if (frame == nullptr || len != kBulkDoneSize || frame[0] != kOpBulk || get16(frame + 2) != kBulkDoneSeq)
        return false;
    out.reqId = frame[1];
    out.sent = get16(frame + 4);
    out.size = frame[6];
    out.mtu = get16(frame + 7);
    out.refused = get32(frame + 9);
    out.elapsedMs = get32(frame + 13);
    return true;
}

uint32_t crc32(const uint8_t *data, size_t len)
{
    const uint32_t *t = kCrcTable.v;
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) c = t[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

bool endsWithFcs(const uint8_t *frame, size_t len)
{
    if (frame == nullptr || len < 8) return false;
    return crc32(frame, len - 4) == get32(frame + len - 4);
}

} // namespace measure
} // namespace link
} // namespace layertime
