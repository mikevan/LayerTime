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

#include "LinkCodec.h"

namespace layertime {
namespace link {

namespace {

void put16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>(v >> 8);
}

void put32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint16_t get16(const uint8_t *p)
{
    return static_cast<uint16_t>(p[0] | (p[1] << 8));
}

uint32_t get32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

size_t error(uint8_t reqId, LinkStatus status, uint8_t *reply)
{
    Error e;
    e.reqId = reqId;
    e.status = status;
    return encodeError(e, reply);
}

} // namespace

size_t encodeStatus(const StatusSnapshot &s, uint8_t *out)
{
    out[0] = s.linkVersion;
    put16(out + 1, s.sessionId);
    put32(out + 3, s.changeSeq);
    out[7] = s.flags;
    out[8] = s.selected;
    out[9] = s.active;
    out[10] = s.eventCount;
    put32(out + 11, s.lastAlertEventId);
    out[15] = s.heartbeat;
    out[16] = static_cast<uint8_t>(s.schedule);
    out[17] = s.nextReportS;
    return kStatusSize;
}

size_t encodeHello(const Hello &h, uint8_t *out)
{
    out[0] = static_cast<uint8_t>(Op::Hello);
    out[1] = h.reqId;
    out[2] = h.clientMajor;
    out[3] = h.clientMinor;
    return kHelloSize;
}

size_t encodePing(const Ping &p, uint8_t *out)
{
    out[0] = static_cast<uint8_t>(Op::Ping);
    out[1] = p.reqId;
    put32(out + 2, p.token);
    return kPingSize;
}

size_t encodeHelloAck(const HelloAck &a, uint8_t *out)
{
    out[0] = static_cast<uint8_t>(FrameType::HelloAck);
    out[1] = a.reqId;
    out[2] = static_cast<uint8_t>(a.status);
    out[3] = a.serverMajor;
    out[4] = a.serverMinor;
    put16(out + 5, a.sessionId);
    put16(out + 7, a.capabilities);
    out[9] = a.maxFrame;
    return kHelloAckSize;
}

size_t encodeAck(const Ack &a, uint8_t *out)
{
    out[0] = static_cast<uint8_t>(FrameType::Ack);
    out[1] = a.reqId;
    out[2] = static_cast<uint8_t>(a.status);
    put32(out + 3, a.token);
    out[7] = a.heartbeat;
    return kAckSize;
}

size_t encodeError(const Error &e, uint8_t *out)
{
    out[0] = static_cast<uint8_t>(FrameType::Error);
    out[1] = e.reqId;
    out[2] = static_cast<uint8_t>(e.status);
    return kErrorSize;
}

bool decodeStatus(const uint8_t *in, size_t len, StatusSnapshot &out)
{
    if (len != kStatusSize) return false;
    out.linkVersion = in[0];
    out.sessionId = get16(in + 1);
    out.changeSeq = get32(in + 3);
    out.flags = in[7];
    out.selected = in[8];
    out.active = in[9];
    out.eventCount = in[10];
    out.lastAlertEventId = get32(in + 11);
    out.heartbeat = in[15];
    out.schedule = static_cast<Schedule>(in[16]);
    out.nextReportS = in[17];
    return true;
}

bool decodeHello(const uint8_t *in, size_t len, Hello &out)
{
    if (len != kHelloSize || in[0] != static_cast<uint8_t>(Op::Hello)) return false;
    out.reqId = in[1];
    out.clientMajor = in[2];
    out.clientMinor = in[3];
    return true;
}

bool decodePing(const uint8_t *in, size_t len, Ping &out)
{
    if (len != kPingSize || in[0] != static_cast<uint8_t>(Op::Ping)) return false;
    out.reqId = in[1];
    out.token = get32(in + 2);
    return true;
}

bool decodeHelloAck(const uint8_t *in, size_t len, HelloAck &out)
{
    if (len != kHelloAckSize || in[0] != static_cast<uint8_t>(FrameType::HelloAck)) return false;
    out.reqId = in[1];
    out.status = static_cast<LinkStatus>(in[2]);
    out.serverMajor = in[3];
    out.serverMinor = in[4];
    out.sessionId = get16(in + 5);
    out.capabilities = get16(in + 7);
    out.maxFrame = in[9];
    return true;
}

bool decodeAck(const uint8_t *in, size_t len, Ack &out)
{
    if (len != kAckSize || in[0] != static_cast<uint8_t>(FrameType::Ack)) return false;
    out.reqId = in[1];
    out.status = static_cast<LinkStatus>(in[2]);
    out.token = get32(in + 3);
    out.heartbeat = in[7];
    return true;
}

bool decodeError(const uint8_t *in, size_t len, Error &out)
{
    if (len != kErrorSize || in[0] != static_cast<uint8_t>(FrameType::Error)) return false;
    out.reqId = in[1];
    out.status = static_cast<LinkStatus>(in[2]);
    return true;
}

size_t dispatch(const NodeIdentity &node, uint8_t heartbeat, const uint8_t *request, size_t len,
                uint8_t *reply)
{
    const uint8_t reqId = len >= 2 ? request[1] : 0;
    if (len < 2) return error(reqId, LinkStatus::BadLength, reply);

    const uint8_t op = request[0];
    switch (op) {
    case static_cast<uint8_t>(Op::Hello): {
        Hello h;
        if (!decodeHello(request, len, h)) return error(reqId, LinkStatus::BadLength, reply);
        HelloAck a;
        a.reqId = h.reqId;
        a.status = h.clientMajor == kMajor ? LinkStatus::Ok : LinkStatus::VersionMismatch;
        a.sessionId = node.sessionId;
        a.capabilities = node.capabilities;
        return encodeHelloAck(a, reply);
    }
    case static_cast<uint8_t>(Op::Ping): {
        Ping p;
        if (!decodePing(request, len, p)) return error(reqId, LinkStatus::BadLength, reply);
        Ack a;
        a.reqId = p.reqId;
        a.token = p.token;
        a.heartbeat = heartbeat;
        return encodeAck(a, reply);
    }
    default:
        // Command, GetChanged, GetText: named, not bound yet. Test-range ops:
        // bound in Increment 2B and only on test builds. Everything else:
        // never defined.
        return error(reqId, LinkStatus::UnknownOp, reply);
    }
}

size_t fillProbe(size_t size, uint8_t *out)
{
    bool known = false;
    for (size_t i = 0; i < kProbeSizeCount; ++i) known = known || kProbeSizes[i] == size;
    if (!known) return 0;
    for (size_t i = 0; i < size; ++i) out[i] = static_cast<uint8_t>(i & 0xFF);
    out[0] = static_cast<uint8_t>(size & 0xFF);
    return size;
}

} // namespace link
} // namespace layertime
