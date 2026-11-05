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

#include "LinkEvidence.h"

#include "LinkCodec.h"
#include "LinkFrames.h"
#include "../model/LayerTimeCommand.h"

namespace layertime {
namespace link {
namespace evidence {

namespace ev = layertime::evidence;

namespace {

void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i)); }
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t *p) { return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24); }

class Locked {
public:
    explicit Locked(const Access &a) : _a(a) { if (_a.lock) _a.lock(_a.lockContext); }
    ~Locked() { if (_a.unlock) _a.unlock(_a.lockContext); }
private:
    const Access &_a;
};

bool sendError(uint8_t reqId, LinkStatus s, Send send, void *ctx)
{
    uint8_t out[kMaxFrame];
    Error e;
    e.reqId = reqId;
    e.status = s;
    return send(out, encodeError(e, out), ctx);
}
bool sendResult(uint8_t reqId, uint8_t op, CommandResult r, Send send, void *ctx)
{
    uint8_t out[kMaxFrame];
    Result res;
    res.reqId = reqId;
    res.commandType = op;
    res.commandResult = static_cast<uint8_t>(r);
    return send(out, encodeResult(res, out), ctx);
}
bool sendEnd(uint8_t reqId, uint8_t count, Send send, void *ctx)
{
    uint8_t out[kMaxFrame];
    End e;
    e.reqId = reqId;
    e.count = count;
    return send(out, encodeEnd(e, out), ctx);
}

// Shared checks: length, the event still in the log, and a window for it.
bool precheck(const Access &a, const uint8_t *req, size_t len, size_t want, bool eventKnown, uint32_t nowMs,
              ev::WindowInfo &info, Send send, void *ctx)
{
    const uint8_t reqId = len >= 2 ? req[1] : 0;
    if (len != want) { sendError(reqId, LinkStatus::BadLength, send, ctx); return false; }
    if (!eventKnown) { sendResult(reqId, req[0], CommandResult::InvalidArgument, send, ctx); return false; }
    bool have = false;
    {
        Locked l(a);
        have = a.store != nullptr && a.store->info(get32(req + 2), nowMs, info);
    }
    if (!have) { sendResult(reqId, req[0], CommandResult::NotReady, send, ctx); return false; }
    return true;
}

} // namespace

size_t frameSizeForMtu(uint16_t mtu)
{
    if (mtu < 23) return kMaxFrame;
    size_t n = static_cast<size_t>(mtu) - 3;
    if (n > kMaxNotify) n = kMaxNotify;
    if (n < kMaxFrame) n = kMaxFrame;
    return n;
}

uint32_t requestEventId(const uint8_t *req, size_t len) { return len >= 6 ? get32(req + 2) : 0; }

void answerGetFrames(const Access &a, const uint8_t *req, size_t len, bool eventKnown, uint8_t sourceKind,
                     uint32_t nowMs, Send send, void *ctx)
{
    ev::WindowInfo info;
    if (!precheck(a, req, len, kGetFramesSize, eventKnown, nowMs, info, send, ctx)) return;
    const uint8_t reqId = req[1];
    const uint32_t eventId = get32(req + 2);
    uint8_t out[kFrameIndexSize];
    out[0] = kTypeFrameWindow;
    out[1] = reqId;
    put32(out + 2, eventId);
    put16(out + 6, info.seen);
    out[8] = info.retained;
    put16(out + 9, info.dropped);
    out[11] = info.truncated;
    out[12] = sourceKind;
    out[13] = info.collecting ? 1 : 0;
    if (!send(out, kFrameWindowSize, ctx)) return;
    uint8_t sent = 0;
    for (uint8_t i = 0; i < info.retained; ++i) {
        ev::FrameMeta m;
        int32_t off = 0;
        bool ok;
        {
            Locked l(a);
            ok = a.store->frameMeta(eventId, i, m, off);
        }
        if (!ok) break; // the window was reused under us: stop, the count says how far we got
        out[0] = kTypeFrameIndex;
        out[1] = reqId;
        out[2] = i;
        out[3] = m.channel;
        out[4] = static_cast<uint8_t>(m.rssi);
        put16(out + 5, m.onAir);
        put16(out + 7, m.captured);
        out[9] = m.flags;
        put32(out + 10, static_cast<uint32_t>(off));
        if (!send(out, kFrameIndexSize, ctx)) return;
        ++sent;
    }
    sendEnd(reqId, sent, send, ctx);
}

void answerGetFrameBytes(const Access &a, const uint8_t *req, size_t len, bool eventKnown, size_t frameSize,
                         Send send, void *ctx)
{
    ev::WindowInfo info;
    if (!precheck(a, req, len, kGetFrameBytesSize, eventKnown, 0, info, send, ctx)) return;
    const uint8_t reqId = req[1];
    const uint32_t eventId = get32(req + 2);
    uint8_t index = req[6];
    uint16_t offset = get16(req + 7);
    if (frameSize > kMaxNotify) frameSize = kMaxNotify;
    if (frameSize <= kFrameBytesHeader) frameSize = kMaxFrame;
    const size_t chunk = frameSize - kFrameBytesHeader;
    uint8_t out[kMaxNotify];
    uint8_t frames = 0;
    while (index < info.retained) {
        size_t n;
        {
            Locked l(a);
            n = a.store->copyBytes(eventId, index, offset, out + kFrameBytesHeader, chunk);
        }
        if (n == 0) { ++index; offset = 0; continue; }
        out[0] = kTypeFrameBytes;
        out[1] = reqId;
        out[2] = index;
        put16(out + 3, offset);
        if (!send(out, kFrameBytesHeader + n, ctx)) return;
        if (frames < 0xFF) ++frames;
        offset = static_cast<uint16_t>(offset + n);
    }
    sendEnd(reqId, frames, send, ctx);
}

size_t encodeGetFrames(uint8_t reqId, uint32_t eventId, uint8_t *out)
{
    out[0] = kOpGetFrames;
    out[1] = reqId;
    put32(out + 2, eventId);
    return kGetFramesSize;
}

size_t encodeGetFrameBytes(uint8_t reqId, uint32_t eventId, uint8_t index, uint16_t offset, uint8_t *out)
{
    out[0] = kOpGetFrameBytes;
    out[1] = reqId;
    put32(out + 2, eventId);
    out[6] = index;
    put16(out + 7, offset);
    return kGetFrameBytesSize;
}

bool decodeFrameWindow(const uint8_t *in, size_t len, FrameWindow &out)
{
    if (len != kFrameWindowSize || in[0] != kTypeFrameWindow) return false;
    out.reqId = in[1];
    out.eventId = get32(in + 2);
    out.seen = get16(in + 6);
    out.retained = in[8];
    out.dropped = get16(in + 9);
    out.truncated = in[11];
    out.sourceKind = in[12];
    out.collecting = (in[13] & 1) != 0;
    return true;
}

bool decodeFrameIndex(const uint8_t *in, size_t len, FrameIndex &out)
{
    if (len != kFrameIndexSize || in[0] != kTypeFrameIndex) return false;
    out.reqId = in[1];
    out.index = in[2];
    out.channel = in[3];
    out.rssi = static_cast<int8_t>(in[4]);
    out.onAir = get16(in + 5);
    out.captured = get16(in + 7);
    out.flags = in[9];
    out.offsetMs = static_cast<int32_t>(get32(in + 10));
    return true;
}

} // namespace evidence
} // namespace link
} // namespace layertime
