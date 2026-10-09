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

#include "UbxParser.h"

namespace {

constexpr uint32_t kInvalidAccuracy = 0xFFFFFFFFUL;

uint16_t readU16(const uint8_t *p, size_t offset)
{
    return static_cast<uint16_t>(p[offset]) |
           (static_cast<uint16_t>(p[offset + 1]) << 8);
}

uint32_t readU32(const uint8_t *p, size_t offset)
{
    return static_cast<uint32_t>(p[offset]) |
           (static_cast<uint32_t>(p[offset + 1]) << 8) |
           (static_cast<uint32_t>(p[offset + 2]) << 16) |
           (static_cast<uint32_t>(p[offset + 3]) << 24);
}

int32_t readI32(const uint8_t *p, size_t offset)
{
    return static_cast<int32_t>(readU32(p, offset));
}

}

namespace Ubx {

void Parser::reset()
{
    _state = State::Sync1;
    _class = 0;
    _id = 0;
    _length = 0;
    _filled = 0;
    _ckA = 0;
    _ckB = 0;
    _frameCkA = 0;
}

void Parser::startFrame()
{
    _length = 0;
    _filled = 0;
    _ckA = 0;
    _ckB = 0;
}

// 8-bit Fletcher over class, id, length, and payload. Not over the sync chars.
void Parser::accumulate(uint8_t byte)
{
    _ckA = static_cast<uint8_t>(_ckA + byte);
    _ckB = static_cast<uint8_t>(_ckB + _ckA);
}

bool Parser::feed(uint8_t byte)
{
    switch (_state) {
    case State::Sync1:
        if (byte == kSyncChar1) _state = State::Sync2;
        return false;

    case State::Sync2:
        // Not a sync pair. This byte could itself start one, so retry it here
        // rather than swallowing it.
        if (byte == kSyncChar2) {
            startFrame();
            _state = State::Class;
        } else {
            _state = (byte == kSyncChar1) ? State::Sync2 : State::Sync1;
        }
        return false;

    case State::Class:
        _class = byte;
        accumulate(byte);
        _state = State::Id;
        return false;

    case State::Id:
        _id = byte;
        accumulate(byte);
        _state = State::LengthLow;
        return false;

    case State::LengthLow:
        _length = byte;
        accumulate(byte);
        _state = State::LengthHigh;
        return false;

    case State::LengthHigh:
        _length = static_cast<uint16_t>(_length | (static_cast<uint16_t>(byte) << 8));
        accumulate(byte);
        // Too big to hold. Drop the whole frame rather than keep a truncated
        // one: a partial payload that still passed a checksum would be a lie
        // the rest of the firmware could not detect.
        if (_length > kMaxPayload) {
            ++_rejected;
            _state = State::Sync1;
            return false;
        }
        _state = (_length == 0) ? State::CkA : State::Payload;
        return false;

    case State::Payload:
        _payload[_filled++] = byte;
        accumulate(byte);
        if (_filled >= _length) _state = State::CkA;
        return false;

    case State::CkA:
        _frameCkA = byte;
        _state = State::CkB;
        return false;

    case State::CkB: {
        const bool ok = (_frameCkA == _ckA) && (byte == _ckB);
        _state = State::Sync1;
        if (ok) {
            ++_accepted;
        } else {
            // Almost always a false sync on NMEA traffic that happened to
            // contain 0xB5 0x62. Counted so a real problem is visible.
            ++_rejected;
        }
        return ok;
    }
    }

    _state = State::Sync1;
    return false;
}

// UBX-NAV-PVT, 92 bytes. Offsets verified against live payloads from this
// receiver rather than transcribed and trusted.
bool decodeNavPvt(const uint8_t *payload, uint16_t length, NavPvt &out)
{
    constexpr uint16_t kNavPvtLength = 92;
    if (payload == nullptr || length < kNavPvtLength) return false;

    out.iTowMs = readU32(payload, 0);
    out.fixType = payload[20];
    out.fixOk = (payload[21] & 0x01) != 0;
    out.satellites = payload[23];
    out.longitude = static_cast<double>(readI32(payload, 24)) * 1e-7;
    out.latitude = static_cast<double>(readI32(payload, 28)) * 1e-7;
    out.altitudeMslM = static_cast<float>(readI32(payload, 36)) / 1000.0f;

    const uint32_t hAcc = readU32(payload, 40);
    out.horizontalAccuracyValid = (hAcc != kInvalidAccuracy);
    out.horizontalAccuracyM = out.horizontalAccuracyValid
                                  ? static_cast<float>(hAcc) / 1000.0f
                                  : 0.0f;

    const uint32_t vAcc = readU32(payload, 44);
    out.verticalAccuracyValid = (vAcc != kInvalidAccuracy);
    out.verticalAccuracyM = out.verticalAccuracyValid
                                ? static_cast<float>(vAcc) / 1000.0f
                                : 0.0f;

    out.pdop = static_cast<float>(readU16(payload, 76)) * 0.01f;
    return true;
}

size_t buildPoll(uint8_t messageClass, uint8_t messageId, uint8_t *out, size_t outSize)
{
    constexpr size_t kFrameSize = 8;
    if (out == nullptr || outSize < kFrameSize) return 0;

    const uint8_t body[4] = {messageClass, messageId, 0x00, 0x00};
    uint8_t ckA = 0;
    uint8_t ckB = 0;
    for (uint8_t b : body) {
        ckA = static_cast<uint8_t>(ckA + b);
        ckB = static_cast<uint8_t>(ckB + ckA);
    }

    out[0] = kSyncChar1;
    out[1] = kSyncChar2;
    out[2] = body[0];
    out[3] = body[1];
    out[4] = body[2];
    out[5] = body[3];
    out[6] = ckA;
    out[7] = ckB;
    return kFrameSize;
}

}
