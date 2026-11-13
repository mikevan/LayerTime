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

// Link 0.2 card ops (Michael, 2026-11-04): the phone pulls the Hunter's SD card
// files over the Link, in the field, with no computer. Read only.
//
//   CARD_LIST  0x0B  op reqId                                          2 bytes
//     -> CARD_ENTRY 0x8B per file, then END (count = entries, at most 255)
//   CARD_READ  0x0C  op reqId kind(u8) serial(u16) eventId(u32) offset(u32)  13 bytes
//     -> CARD_BYTES 0x8C from offset to the END OF THE FILE at the connection's
//        size, then CARD_DONE 0x8D, then END. A dropped connection resumes from
//        the offset it last received.
//
//   CARD_ENTRY: type reqId kind serial(u16) eventId(u32) size(u32)     13 bytes
//   CARD_BYTES: type reqId offset(u32) bytes...                        6 + n
//   CARD_DONE:  type reqId size(u32) crc32(u32)                        10 bytes
//               crc32 (IEEE) covers the WHOLE file, from byte 0, so a resumed
//               pull checks the joined file.
//
// A file is named by code, not by text, so every frame but CARD_BYTES fits 20
// bytes and a request can only ever name the Hunter's own files:
//   kind 0 (log):  serial N          -> hunter_NNNN.log   (eventId 0)
//   kind 1 (pcap): serial N, event E -> ev_NNNN/event_EEEEE.pcap
// No card: RESULT NotReady. No such file, or a bad kind or serial: RESULT
// InvalidArgument. Wrong length: ERROR BadLength. RESULT carries the op as its
// commandType. HELLO_ACK capability bit 9 (0x0200): the Node serves its card.
//
// Clear Card (Michael, 2026-11-06). Not read only: it erases the card
// COMPLETELY, every file and folder, so a card full of tracker logs can be
// cleaned out in the field without a computer.
//   CARD_CLEAR 0x0D  op reqId                                          2 bytes
//     -> END (count = files erased, at most 255) once the card is empty and a
//        fresh hunter_0001.log is open. No card, a card read or another clear
//        running, or something that would not erase: RESULT NotReady. Wrong
//        length: ERROR BadLength. LayerScope asks the operator to confirm before
//        it sends this, and lists the card again afterwards to show the result.
// HELLO_ACK capability bit 10 (0x0400): the Node clears its card.

#include <stddef.h>
#include <stdint.h>

namespace layertime {
namespace link {
namespace card {

constexpr uint8_t kOpCardList = 0x0B;
constexpr uint8_t kOpCardRead = 0x0C;
constexpr uint8_t kOpCardClear = 0x0D;
constexpr uint8_t kTypeCardEntry = 0x8B;
constexpr uint8_t kTypeCardBytes = 0x8C;
constexpr uint8_t kTypeCardDone = 0x8D;
constexpr size_t kCardListSize = 2;
constexpr size_t kCardReadSize = 13;
constexpr size_t kCardClearSize = 2;
constexpr size_t kCardEntrySize = 13;
constexpr size_t kCardBytesHeader = 6;
constexpr size_t kCardDoneSize = 10;
constexpr uint16_t kCapCardRead = 0x0200;
constexpr uint16_t kCapCardClear = 0x0400;
constexpr uint8_t kKindLog = 0;
constexpr uint8_t kKindPcap = 1;
constexpr uint16_t kMaxSerial = 9999;
constexpr size_t kCardNameSize = sizeof("ev_0000/event_4294967295.pcap");

inline bool isCardOp(uint8_t op) { return op == kOpCardList || op == kOpCardRead || op == kOpCardClear; }

struct FileId {
    uint8_t kind = kKindLog;
    uint16_t serial = 0;
    uint32_t eventId = 0;
};

// A valid code: a log with serial 1 to 9999 and eventId 0, or a pcap with
// serial 1 to 9999 and eventId 1 or more.
bool validFileId(const FileId &id);
// "hunter_0007.log" or "ev_0007/event_00042.pcap". False if invalid or too small.
bool fileName(const FileId &id, char *out, size_t size);
// The reverse, strictly: exactly the names the Hunter writes (lower case, no
// leading '/', 4-digit serial, 5 to 10 digit event id). False for anything else.
bool fileIdFromName(const char *name, FileId &out);

size_t encodeCardList(uint8_t reqId, uint8_t *out);
size_t encodeCardClear(uint8_t reqId, uint8_t *out);
size_t encodeCardRead(uint8_t reqId, const FileId &id, uint32_t offset, uint8_t *out);
size_t encodeCardEntry(uint8_t reqId, const FileId &id, uint32_t size, uint8_t *out);
size_t encodeCardBytesHeader(uint8_t reqId, uint32_t offset, uint8_t *out);
size_t encodeCardDone(uint8_t reqId, uint32_t size, uint32_t crc32, uint8_t *out);

enum class ReadCheck : uint8_t { Ok = 0, BadLength = 1, BadFile = 2 };
ReadCheck decodeCardRead(const uint8_t *req, size_t len, uint8_t &reqId, FileId &id, uint32_t &offset);
bool decodeCardEntry(const uint8_t *in, size_t len, uint8_t &reqId, FileId &id, uint32_t &size);
bool decodeCardDone(const uint8_t *in, size_t len, uint8_t &reqId, uint32_t &size, uint32_t &crc32);

// CRC-32 (IEEE 802.3, zlib's). Start at 0 and feed every chunk in order.
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length);

// Streams one CARD_READ answer a few frames at a time, so the platform's loop
// never stalls on a large file. Pure: it reads through a callback and sends
// through another. The platform opens the file at byte 0 and calls step()
// once per loop pass until it returns Done or Failed.
class Streamer {
public:
    using Read = size_t (*)(uint8_t *out, size_t max, void *context);   // 0 at the end
    using Send = bool (*)(const uint8_t *frame, size_t len, void *context); // false: the connection is gone
    enum class State : uint8_t { Idle = 0, Running = 1, Done = 2, Failed = 3 };

    // frameSize: the connection's notification size (20 to 244).
    void start(uint8_t reqId, uint32_t fileSize, uint32_t offset, size_t frameSize);
    // Skips (and checksums) the bytes before the offset, then sends up to
    // maxFrames CARD_BYTES; after the last byte, CARD_DONE and END.
    State step(Read read, void *readContext, Send send, void *sendContext, uint8_t maxFrames);
    State state() const { return _state; }
    uint32_t sent() const { return _pos; }

private:
    State _state = State::Idle;
    uint8_t _reqId = 0;
    uint32_t _size = 0;
    uint32_t _offset = 0;
    uint32_t _pos = 0; // bytes read from the file so far
    uint32_t _crc = 0;
    size_t _frameSize = 20;
};

} // namespace card
} // namespace link
} // namespace layertime
