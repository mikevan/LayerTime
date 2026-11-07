// LinkCard (Link 0.2 card ops, 2026-11-04): the phone pulls the Hunter's SD
// card files over the Link. Codes, frames, the CRC, and the streamer that sends
// a file a few frames per loop pass.

#include "check.h"

#include "core/link/LinkCard.h"
#include "core/link/LinkCodec.h"
#include "core/link/LinkFrames.h"

#include <string.h>
#include <string>
#include <vector>

using namespace layertime::link;
using namespace layertime::link::card;

namespace {

struct File {
    std::vector<uint8_t> bytes;
    size_t pos = 0;
    size_t shortAt = static_cast<size_t>(-1); // simulate the card stopping
};
size_t readFile(uint8_t *out, size_t max, void *ctx)
{
    File *f = static_cast<File *>(ctx);
    size_t n = f->bytes.size() - f->pos;
    if (n > max) n = max;
    if (f->pos + n > f->shortAt) n = f->pos >= f->shortAt ? 0 : f->shortAt - f->pos;
    memcpy(out, f->bytes.data() + f->pos, n);
    f->pos += n;
    return n;
}

struct Sink {
    std::vector<std::vector<uint8_t>> frames;
    int failAfter = -1;
};
bool sendFrame(const uint8_t *frame, size_t len, void *ctx)
{
    Sink *s = static_cast<Sink *>(ctx);
    if (s->failAfter >= 0 && static_cast<int>(s->frames.size()) >= s->failAfter) return false;
    s->frames.emplace_back(frame, frame + len);
    return true;
}

uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

File makeFile(size_t n)
{
    File f;
    for (size_t i = 0; i < n; ++i) f.bytes.push_back(static_cast<uint8_t>((i * 7 + 3) & 0xFF));
    return f;
}

} // namespace

void names_and_codes_round_trip_strictly()
{
    char name[kCardNameSize];
    FileId log;
    log.kind = kKindLog;
    log.serial = 7;
    CHECK_TRUE(fileName(log, name, sizeof(name)));
    CHECK_STR("hunter_0007.log", name);
    FileId pcap;
    pcap.kind = kKindPcap;
    pcap.serial = 7;
    pcap.eventId = 42;
    CHECK_TRUE(fileName(pcap, name, sizeof(name)));
    CHECK_STR("ev_0007/event_00042.pcap", name);
    pcap.eventId = 4294967295u;
    CHECK_TRUE(fileName(pcap, name, sizeof(name)));
    CHECK_STR("ev_0007/event_4294967295.pcap", name);

    FileId back;
    CHECK_TRUE(fileIdFromName("ev_0007/event_00042.pcap", back));
    CHECK_INT(kKindPcap, back.kind);
    CHECK_INT(7, back.serial);
    CHECK_INT(42, back.eventId);
    CHECK_TRUE(fileIdFromName("hunter_9999.log", back));
    CHECK_INT(kKindLog, back.kind);
    CHECK_INT(9999, back.serial);
    CHECK_FALSE(fileIdFromName("hunter_0000.log", back));
    CHECK_FALSE(fileIdFromName("ev_0007/event_0042.pcap", back));    // 4 digits: never written
    CHECK_FALSE(fileIdFromName("ev_0007/event_000042.pcap", back));  // padded past 5: not the Hunter's spelling
    CHECK_FALSE(fileIdFromName("ev_0007/event_00000.pcap", back));   // event 0 does not exist
    CHECK_FALSE(fileIdFromName("ev_0007/event_9999999999.pcap", back)); // past u32
    CHECK_FALSE(fileIdFromName("../hunter_0001.log", back));
    CHECK_FALSE(fileIdFromName("HUNTER_0001.LOG", back));
    CHECK_FALSE(fileIdFromName(nullptr, back));

    FileId bad;
    bad.kind = 2;
    bad.serial = 1;
    CHECK_FALSE(validFileId(bad));
    bad.kind = kKindLog;
    bad.eventId = 3; // a log has no event id
    CHECK_FALSE(validFileId(bad));
    bad.eventId = 0;
    bad.serial = 10000;
    CHECK_FALSE(validFileId(bad));
    CHECK_FALSE(fileName(bad, name, sizeof(name)));
    CHECK_STR("", name);
}

void request_and_reply_frames_have_their_documented_bytes()
{
    uint8_t f[32];
    CHECK_INT(2, encodeCardList(0x41, f));
    CHECK_INT(0x0B, f[0]);
    FileId id;
    id.kind = kKindPcap;
    id.serial = 0x0102;
    id.eventId = 0x0A0B0C0D;
    CHECK_INT(13, encodeCardRead(0x42, id, 0x11223344, f));
    const uint8_t read[13] = {0x0C, 0x42, 1, 0x02, 0x01, 0x0D, 0x0C, 0x0B, 0x0A, 0x44, 0x33, 0x22, 0x11};
    CHECK_TRUE(memcmp(f, read, 13) == 0);
    uint8_t reqId = 0;
    FileId got;
    uint32_t offset = 0;
    CHECK_INT(static_cast<int>(ReadCheck::Ok), static_cast<int>(decodeCardRead(f, 13, reqId, got, offset)));
    CHECK_INT(0x42, reqId);
    CHECK_INT(0x0A0B0C0D, got.eventId);
    CHECK_INT(0x11223344, offset);
    CHECK_INT(static_cast<int>(ReadCheck::BadLength), static_cast<int>(decodeCardRead(f, 12, reqId, got, offset)));
    f[2] = 9; // no such kind
    CHECK_INT(static_cast<int>(ReadCheck::BadFile), static_cast<int>(decodeCardRead(f, 13, reqId, got, offset)));

    CHECK_INT(13, encodeCardEntry(0x43, id, 67608, f));
    CHECK_INT(0x8B, f[0]);
    uint32_t size = 0;
    CHECK_TRUE(decodeCardEntry(f, 13, reqId, got, size));
    CHECK_INT(67608, size);
    CHECK_INT(0x0102, got.serial);
    CHECK_FALSE(decodeCardEntry(f, 12, reqId, got, size));
    CHECK_INT(6, encodeCardBytesHeader(0x44, 500, f));
    CHECK_INT(0x8C, f[0]);
    CHECK_INT(500, rd32(f + 2));
    CHECK_INT(10, encodeCardDone(0x45, 1000, 0xCBF43926u, f));
    uint32_t crc = 0;
    CHECK_TRUE(decodeCardDone(f, 10, reqId, size, crc));
    CHECK_INT(1000, size);
    CHECK_INT(static_cast<int>(0xCBF43926u), static_cast<int>(crc));
    CHECK_TRUE(isCardOp(0x0B) && isCardOp(0x0C) && !isCardOp(0x07));
    // Clear Card (2026-11-06): op 0x0D, two bytes, capability bit 10, and no
    // clash with any other op or capability.
    CHECK_TRUE(isCardOp(0x0D) && !isCardOp(0x0E));
    uint8_t c[4] = {0};
    CHECK_INT(2, encodeCardClear(0x44, c));
    CHECK_INT(0x0D, c[0]);
    CHECK_INT(0x44, c[1]);
    CHECK_INT(0x0400, kCapCardClear);
    CHECK_TRUE((kCapCardClear & (kCapCardRead | 0x01FF)) == 0);
    CHECK_INT(0x0200, kCapCardRead);
}

void the_crc_is_the_standard_one()
{
    const uint8_t nine[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK_INT(static_cast<int>(0xCBF43926u), static_cast<int>(crc32Update(0, nine, 9)));
    CHECK_INT(static_cast<int>(0xCBF43926u), static_cast<int>(crc32Update(crc32Update(0, nine, 3), nine + 3, 6)));
}

void a_file_streams_a_few_frames_per_pass_then_done_and_end()
{
    File f = makeFile(1000);
    Sink s;
    Streamer st;
    st.start(0x50, 1000, 0, 20); // a 20-byte connection: 14 data bytes per frame
    int passes = 0;
    while (st.step(readFile, &f, sendFrame, &s, 8) == Streamer::State::Running) ++passes;
    CHECK_INT(static_cast<int>(Streamer::State::Done), static_cast<int>(st.state()));
    CHECK_INT(8, passes);                    // 72 data frames at 8 a pass: 9 passes, the last also sends DONE
    CHECK_INT(72 + 2, s.frames.size());      // 1000 / 14 = 72 frames, then CARD_DONE and END
    std::vector<uint8_t> joined;
    uint32_t expectOffset = 0;
    for (size_t i = 0; i < 72; ++i) {
        CHECK_INT(0x8C, s.frames[i][0]);
        CHECK_INT(0x50, s.frames[i][1]);
        CHECK_INT(expectOffset, rd32(s.frames[i].data() + 2));
        CHECK_TRUE(s.frames[i].size() <= 20);
        joined.insert(joined.end(), s.frames[i].begin() + 6, s.frames[i].end());
        expectOffset += static_cast<uint32_t>(s.frames[i].size() - 6);
    }
    CHECK_TRUE(joined == f.bytes);
    uint8_t reqId = 0;
    uint32_t size = 0, crc = 0;
    CHECK_TRUE(decodeCardDone(s.frames[72].data(), s.frames[72].size(), reqId, size, crc));
    CHECK_INT(1000, size);
    CHECK_INT(static_cast<int>(crc32Update(0, f.bytes.data(), 1000)), static_cast<int>(crc));
    uint8_t end[kMaxFrame];
    End e;
    e.reqId = 0x50;
    const size_t endLen = encodeEnd(e, end);
    CHECK_INT(endLen, s.frames[73].size());
    CHECK_TRUE(memcmp(end, s.frames[73].data(), endLen) == 0);
}

void a_resumed_pull_starts_at_its_offset_and_the_crc_covers_the_whole_file()
{
    File f = makeFile(3000);
    Sink s;
    Streamer st;
    st.start(0x51, 3000, 1234, 244);
    while (st.step(readFile, &f, sendFrame, &s, 4) == Streamer::State::Running) {}
    CHECK_INT(static_cast<int>(Streamer::State::Done), static_cast<int>(st.state()));
    CHECK_INT(1234, rd32(s.frames[0].data() + 2));
    CHECK_INT(244, s.frames[0].size());
    size_t data = 0;
    for (size_t i = 0; i + 2 < s.frames.size(); ++i) data += s.frames[i].size() - 6;
    CHECK_INT(3000 - 1234, data);
    uint8_t reqId = 0;
    uint32_t size = 0, crc = 0;
    CHECK_TRUE(decodeCardDone(s.frames[s.frames.size() - 2].data(), 10, reqId, size, crc));
    CHECK_INT(static_cast<int>(crc32Update(0, f.bytes.data(), 3000)), static_cast<int>(crc));
}

void an_empty_file_is_done_at_once_and_failures_stop_the_stream()
{
    File empty;
    Sink s;
    Streamer st;
    st.start(0x52, 0, 0, 244);
    CHECK_INT(static_cast<int>(Streamer::State::Done), static_cast<int>(st.step(readFile, &empty, sendFrame, &s, 4)));
    CHECK_INT(2, s.frames.size()); // DONE and END
    // The card stops returning data: Failed, never a short DONE.
    File cut = makeFile(500);
    cut.shortAt = 100;
    Sink s2;
    st.start(0x53, 500, 0, 64);
    while (st.step(readFile, &cut, sendFrame, &s2, 4) == Streamer::State::Running) {}
    CHECK_INT(static_cast<int>(Streamer::State::Failed), static_cast<int>(st.state()));
    for (const auto &fr : s2.frames) CHECK_INT(0x8C, fr[0]);
    // The connection goes: Failed.
    File f = makeFile(500);
    Sink s3;
    s3.failAfter = 2;
    st.start(0x54, 500, 0, 64);
    while (st.step(readFile, &f, sendFrame, &s3, 4) == Streamer::State::Running) {}
    CHECK_INT(static_cast<int>(Streamer::State::Failed), static_cast<int>(st.state()));
    CHECK_INT(2, s3.frames.size());
    // An offset past the end is clamped: just DONE and END.
    File g = makeFile(10);
    Sink s4;
    st.start(0x55, 10, 99, 64);
    while (st.step(readFile, &g, sendFrame, &s4, 4) == Streamer::State::Running) {}
    CHECK_INT(static_cast<int>(Streamer::State::Done), static_cast<int>(st.state()));
    CHECK_INT(2, s4.frames.size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(names_and_codes_round_trip_strictly);
    CASE(request_and_reply_frames_have_their_documented_bytes);
    CASE(the_crc_is_the_standard_one);
    CASE(a_file_streams_a_few_frames_per_pass_then_done_and_end);
    CASE(a_resumed_pull_starts_at_its_offset_and_the_crc_covers_the_whole_file);
    CASE(an_empty_file_is_done_at_once_and_failures_stop_the_stream);
    CHECK_SUMMARY();
}
