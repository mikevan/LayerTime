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

#include "LinkCard.h"

#include "LinkCodec.h"
#include "LinkFrames.h"

#include <stdio.h>
#include <string.h>

namespace layertime {
namespace link {
namespace card {

namespace {

void put16(uint8_t *p, uint16_t v) { p[0] = static_cast<uint8_t>(v); p[1] = static_cast<uint8_t>(v >> 8); }
void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(v >> (8 * i)); }
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

bool digitsAt(const char *p, size_t n, uint32_t &value)
{
    uint64_t v = 0;
    for (size_t i = 0; i < n; ++i) {
        if (p[i] < '0' || p[i] > '9') return false;
        v = v * 10 + static_cast<uint64_t>(p[i] - '0');
    }
    if (v > 0xFFFFFFFFull) return false;
    value = static_cast<uint32_t>(v);
    return true;
}

} // namespace

bool validFileId(const FileId &id)
{
    if (id.serial < 1 || id.serial > kMaxSerial) return false;
    if (id.kind == kKindLog) return id.eventId == 0;
    if (id.kind == kKindPcap) return id.eventId >= 1;
    return false;
}

bool fileName(const FileId &id, char *out, size_t size)
{
    if (out == nullptr || size == 0) return false;
    out[0] = '\0';
    if (!validFileId(id)) return false;
    int n;
    if (id.kind == kKindLog) n = snprintf(out, size, "hunter_%04u.log", static_cast<unsigned>(id.serial));
    else
        n = snprintf(out, size, "ev_%04u/event_%05lu.pcap", static_cast<unsigned>(id.serial),
                     static_cast<unsigned long>(id.eventId));
    if (n < 0 || static_cast<size_t>(n) >= size) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool fileIdFromName(const char *name, FileId &out)
{
    if (name == nullptr) return false;
    const size_t len = strlen(name);
    uint32_t v = 0, e = 0;
    if (len == 15 && strncmp(name, "hunter_", 7) == 0 && strcmp(name + 11, ".log") == 0 && digitsAt(name + 7, 4, v)) {
        FileId id;
        id.kind = kKindLog;
        id.serial = static_cast<uint16_t>(v);
        if (!validFileId(id)) return false;
        out = id;
        return true;
    }
    if (len < 24 || len > 29) return false;
    if (strncmp(name, "ev_", 3) != 0 || !digitsAt(name + 3, 4, v) || name[7] != '/') return false;
    if (strncmp(name + 8, "event_", 6) != 0) return false;
    const size_t idLen = len - 8 - 6 - 5;
    if (!digitsAt(name + 14, idLen, e) || strcmp(name + 14 + idLen, ".pcap") != 0) return false;
    FileId id;
    id.kind = kKindPcap;
    id.serial = static_cast<uint16_t>(v);
    id.eventId = e;
    if (!validFileId(id)) return false;
    // Only the spelling the Hunter writes: 5 digits, more only when the id needs them.
    char again[kCardNameSize];
    if (!fileName(id, again, sizeof(again)) || strcmp(again, name) != 0) return false;
    out = id;
    return true;
}

size_t encodeCardList(uint8_t reqId, uint8_t *out)
{
    out[0] = kOpCardList;
    out[1] = reqId;
    return kCardListSize;
}

size_t encodeCardClear(uint8_t reqId, uint8_t *out)
{
    out[0] = kOpCardClear;
    out[1] = reqId;
    return kCardClearSize;
}

size_t encodeCardRead(uint8_t reqId, const FileId &id, uint32_t offset, uint8_t *out)
{
    out[0] = kOpCardRead;
    out[1] = reqId;
    out[2] = id.kind;
    put16(out + 3, id.serial);
    put32(out + 5, id.eventId);
    put32(out + 9, offset);
    return kCardReadSize;
}

size_t encodeCardEntry(uint8_t reqId, const FileId &id, uint32_t size, uint8_t *out)
{
    out[0] = kTypeCardEntry;
    out[1] = reqId;
    out[2] = id.kind;
    put16(out + 3, id.serial);
    put32(out + 5, id.eventId);
    put32(out + 9, size);
    return kCardEntrySize;
}

size_t encodeCardBytesHeader(uint8_t reqId, uint32_t offset, uint8_t *out)
{
    out[0] = kTypeCardBytes;
    out[1] = reqId;
    put32(out + 2, offset);
    return kCardBytesHeader;
}

size_t encodeCardDone(uint8_t reqId, uint32_t size, uint32_t crc32, uint8_t *out)
{
    out[0] = kTypeCardDone;
    out[1] = reqId;
    put32(out + 2, size);
    put32(out + 6, crc32);
    return kCardDoneSize;
}

ReadCheck decodeCardRead(const uint8_t *req, size_t len, uint8_t &reqId, FileId &id, uint32_t &offset)
{
    reqId = len >= 2 ? req[1] : 0;
    if (req == nullptr || len != kCardReadSize || req[0] != kOpCardRead) return ReadCheck::BadLength;
    id.kind = req[2];
    id.serial = get16(req + 3);
    id.eventId = get32(req + 5);
    offset = get32(req + 9);
    return validFileId(id) ? ReadCheck::Ok : ReadCheck::BadFile;
}

bool decodeCardEntry(const uint8_t *in, size_t len, uint8_t &reqId, FileId &id, uint32_t &size)
{
    if (in == nullptr || len != kCardEntrySize || in[0] != kTypeCardEntry) return false;
    reqId = in[1];
    id.kind = in[2];
    id.serial = get16(in + 3);
    id.eventId = get32(in + 5);
    size = get32(in + 9);
    return true;
}

bool decodeCardDone(const uint8_t *in, size_t len, uint8_t &reqId, uint32_t &size, uint32_t &crc32)
{
    if (in == nullptr || len != kCardDoneSize || in[0] != kTypeCardDone) return false;
    reqId = in[1];
    size = get32(in + 2);
    crc32 = get32(in + 6);
    return true;
}

uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
    crc = ~crc;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

void Streamer::start(uint8_t reqId, uint32_t fileSize, uint32_t offset, size_t frameSize)
{
    _state = State::Running;
    _reqId = reqId;
    _size = fileSize;
    _offset = offset > fileSize ? fileSize : offset;
    _pos = 0;
    _crc = 0;
    if (frameSize < kCardBytesHeader + 1) frameSize = kCardBytesHeader + 1;
    if (frameSize > 244) frameSize = 244;
    _frameSize = frameSize;
}

Streamer::State Streamer::step(Read read, void *readContext, Send send, void *sendContext, uint8_t maxFrames)
{
    if (_state != State::Running) return _state;
    uint8_t frame[244];
    // The bytes before the offset: read and checksummed, not sent, so CARD_DONE's
    // crc32 covers the whole file even on a resumed pull.
    while (_pos < _offset) {
        uint8_t skip[128];
        const uint32_t want = _offset - _pos < sizeof(skip) ? _offset - _pos : static_cast<uint32_t>(sizeof(skip));
        const size_t n = read(skip, want, readContext);
        if (n == 0) { _state = State::Failed; return _state; }
        _crc = crc32Update(_crc, skip, n);
        _pos += static_cast<uint32_t>(n);
    }
    for (uint8_t i = 0; i < maxFrames && _pos < _size; ++i) {
        const size_t room = _frameSize - kCardBytesHeader;
        const uint32_t left = _size - _pos;
        const size_t want = left < room ? left : room;
        encodeCardBytesHeader(_reqId, _pos, frame);
        const size_t n = read(frame + kCardBytesHeader, want, readContext);
        if (n == 0) { _state = State::Failed; return _state; }
        _crc = crc32Update(_crc, frame + kCardBytesHeader, n);
        if (!send(frame, kCardBytesHeader + n, sendContext)) { _state = State::Failed; return _state; }
        _pos += static_cast<uint32_t>(n);
    }
    if (_pos < _size) return _state;
    const size_t d = encodeCardDone(_reqId, _size, _crc, frame);
    if (!send(frame, d, sendContext)) { _state = State::Failed; return _state; }
    End e;
    e.reqId = _reqId;
    e.count = 0;
    _state = send(frame, encodeEnd(e, frame), sendContext) ? State::Done : State::Failed;
    return _state;
}

} // namespace card
} // namespace link
} // namespace layertime
