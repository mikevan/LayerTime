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


#include "LinkHunt.h"

#include "LinkCodec.h"

namespace layertime {
namespace link {
namespace hunt {

namespace {
void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }

size_t encodeResultFor(uint8_t reqId, uint8_t op, uint8_t commandResult, uint8_t *out)
{
    Result r;
    r.reqId = reqId;
    r.status = LinkStatus::Ok;
    r.commandType = op;
    r.commandResult = commandResult;
    return encodeResult(r, out);
}

size_t encodeBadLength(uint8_t reqId, uint8_t *out)
{
    Error e;
    e.reqId = reqId;
    e.status = LinkStatus::BadLength;
    return encodeError(e, out);
}
} // namespace

size_t encodeHuntStart(const HuntStart &in, uint8_t *out)
{
    out[0] = kOpHuntStart;
    out[1] = in.reqId;
    out[2] = in.targetKind;
    for (int i = 0; i < 6; ++i) out[3 + i] = in.address[i];
    out[9] = in.channel;
    return kHuntStartSize;
}

bool decodeHuntStart(const uint8_t *in, size_t len, HuntStart &out)
{
    if (in == nullptr || len != kHuntStartSize || in[0] != kOpHuntStart) return false;
    HuntStart s;
    s.reqId = in[1];
    s.targetKind = in[2];
    for (int i = 0; i < 6; ++i) s.address[i] = in[3 + i];
    s.channel = in[9];
    out = s;
    return true;
}

size_t encodeHuntStop(uint8_t reqId, uint8_t *out)
{
    out[0] = kOpHuntStop;
    out[1] = reqId;
    return kHuntStopSize;
}

size_t encodeGetHunt(uint8_t reqId, uint8_t *out)
{
    out[0] = kOpGetHunt;
    out[1] = reqId;
    return kGetHuntSize;
}

size_t encodeHuntReading(const HuntReading &in, uint8_t *out)
{
    out[0] = kTypeHuntReading;
    out[1] = in.reqId;
    out[2] = static_cast<uint8_t>(in.status);
    put16(out + 3, in.readings);
    out[5] = static_cast<uint8_t>(in.latest);
    out[6] = static_cast<uint8_t>(in.strongest);
    out[7] = static_cast<uint8_t>(in.weakest);
    out[8] = static_cast<uint8_t>(in.mean);
    put16(out + 9, in.msSinceHeard);
    out[11] = in.channel;
    out[12] = in.flags;
    out[13] = 0; // reserved
    return kHuntReadingSize;
}

bool decodeHuntReading(const uint8_t *in, size_t len, HuntReading &out)
{
    if (in == nullptr || len != kHuntReadingSize || in[0] != kTypeHuntReading) return false;
    HuntReading r;
    r.reqId = in[1];
    r.status = static_cast<LinkStatus>(in[2]);
    r.readings = get16(in + 3);
    r.latest = static_cast<int8_t>(in[5]);
    r.strongest = static_cast<int8_t>(in[6]);
    r.weakest = static_cast<int8_t>(in[7]);
    r.mean = static_cast<int8_t>(in[8]);
    r.msSinceHeard = get16(in + 9);
    r.channel = in[11];
    r.flags = in[12];
    out = r;
    return true;
}

bool isHuntOp(uint8_t op)
{
    return op == kOpHuntStart || op == kOpHuntStop || op == kOpGetHunt;
}

void HuntTracker::start(uint32_t nowMs)
{
    *this = HuntTracker{};
    _active = true;
    _startMs = nowMs;
}

void HuntTracker::stop()
{
    *this = HuntTracker{};
}

void HuntTracker::sighting(int rssi, uint32_t nowMs)
{
    if (!_active) return;
    if (rssi < -127) rssi = -127;
    if (rssi > 20) rssi = 20;
    const int8_t v = static_cast<int8_t>(rssi);
    if (_count == 0 || v > _strongest) _strongest = v;
    if (_count == 0 || v < _weakest) _weakest = v;
    _latest = v;
    _sum += v;
    if (_count < 0xFFFF) ++_count;
    ++_total;
    _heard = true;
    _lastHeardMs = nowMs;
}

HuntReading HuntTracker::take(uint8_t reqId, uint32_t nowMs)
{
    HuntReading r;
    r.reqId = reqId;
    if (!_active) return r; // flags 0: not hunting
    r.flags = kReadingHunting;
    r.readings = _count;
    if (_count > 0) {
        r.latest = _latest;
        r.strongest = _strongest;
        r.weakest = _weakest;
        // Rounded to the nearest dBm, half away from zero.
        const int32_t n = _count;
        r.mean = static_cast<int8_t>(_sum >= 0 ? (_sum + n / 2) / n : -((-_sum + n / 2) / n));
    }
    const uint32_t since = nowMs - (_heard ? _lastHeardMs : _startMs);
    if (_heard) r.msSinceHeard = since >= 0xFFFF ? 0xFFFF : static_cast<uint16_t>(since);
    if (since >= kLostAfterMs) r.flags |= kReadingLost;
    _count = 0;
    _sum = 0;
    _latest = _strongest = _weakest = kNoReading;
    return r;
}

size_t answer(HuntTracker &tracker, const uint8_t *request, size_t len, uint32_t nowMs, uint8_t *reply)
{
    const uint8_t reqId = len >= 2 ? request[1] : 0;
    const uint8_t op = len >= 1 ? request[0] : 0;
    if (op == kOpHuntStart) {
        HuntStart s;
        if (!decodeHuntStart(request, len, s)) return encodeBadLength(reqId, reply);
        if (s.targetKind != kTargetAnyVixen) return encodeResultFor(reqId, op, kUnsupportedTarget, reply);
        // A second HUNT_START restarts the hunt: a fresh window.
        tracker.start(nowMs);
        return encodeResultFor(reqId, op, kStarted, reply);
    }
    if (op == kOpHuntStop) {
        if (len != kHuntStopSize) return encodeBadLength(reqId, reply);
        tracker.stop();
        return encodeResultFor(reqId, op, 0, reply);
    }
    if (op == kOpGetHunt) {
        if (len != kGetHuntSize) return encodeBadLength(reqId, reply);
        return encodeHuntReading(tracker.take(reqId, nowMs), reply);
    }
    Error e;
    e.reqId = reqId;
    e.status = LinkStatus::UnknownOp;
    return encodeError(e, reply);
}

} // namespace hunt
} // namespace link
} // namespace layertime
