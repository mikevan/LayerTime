// Link 0.2 evidence ops (slice 2, 2026-11-04): GET_FRAMES and GET_FRAME_BYTES
// answered from a real EvidenceStore, byte by byte, with the errors.

#include "check.h"

#include "core/link/LinkCodec.h"
#include "core/link/LinkEvidence.h"
#include "core/logic/EvidenceStore.h"
#include "core/model/LayerTimeCommand.h"

#include <string.h>
#include <vector>

using namespace layertime;
using namespace layertime::link;
using namespace layertime::link::evidence;
namespace ev = layertime::evidence;

namespace {

const uint8_t AP[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

std::vector<uint8_t> deauth(uint16_t seq, size_t pad = 0)
{
    std::vector<uint8_t> f = {0xC0, 0, 0, 0};
    f.insert(f.end(), BCAST, BCAST + 6);
    f.insert(f.end(), AP, AP + 6);
    f.insert(f.end(), AP, AP + 6);
    f.push_back(static_cast<uint8_t>((seq << 4) & 0xFF));
    f.push_back(static_cast<uint8_t>(seq >> 4));
    f.push_back(7);
    f.push_back(0);
    for (size_t i = 0; i < pad; ++i) f.push_back(static_cast<uint8_t>(i));
    return f;
}

struct Sent {
    std::vector<std::vector<uint8_t>> frames;
    int failAfter = -1;
    static bool send(const uint8_t *f, size_t n, void *c)
    {
        Sent *s = static_cast<Sent *>(c);
        if (s->failAfter >= 0 && static_cast<int>(s->frames.size()) >= s->failAfter) return false;
        s->frames.emplace_back(f, f + n);
        return true;
    }
};

int g_locks = 0, g_depth = 0;
void lockFn(void *) { ++g_locks; ++g_depth; }
void unlockFn(void *) { --g_depth; }

struct Rig {
    std::vector<uint8_t> pool;
    ev::EvidenceStore store;
    Access access;
    std::vector<std::vector<uint8_t>> heard;
    Rig() : pool(ev::EvidenceStore::poolBytes())
    {
        store.attach(pool.data(), pool.size());
        access.store = &store;
        access.lock = lockFn;
        access.unlock = unlockFn;
        g_locks = g_depth = 0;
        uint32_t t = 1000;
        heard.push_back(deauth(1, 300));
        store.noteFrame(heard.back().data(), static_cast<uint16_t>(heard.back().size()), -70, 6, t);
        store.openWindow(42, AP, t);
        for (uint16_t i = 2; i <= 3; ++i) {
            heard.push_back(deauth(i, i == 3 ? 900 : 10));
            store.noteFrame(heard.back().data(), static_cast<uint16_t>(heard.back().size()), -71, 6, t += 50);
        }
    }
};

} // namespace

void get_frames_lists_the_window_then_ends()
{
    Rig r;
    Sent s;
    uint8_t req[kGetFramesSize];
    encodeGetFrames(0x31, 42, req);
    answerGetFrames(r.access, req, sizeof(req), true, 1, 1100, Sent::send, &s);
    CHECK_INT(5, s.frames.size()); // window, 3 index, end
    FrameWindow w;
    CHECK_TRUE(decodeFrameWindow(s.frames[0].data(), s.frames[0].size(), w));
    CHECK_INT(0x31, w.reqId);
    CHECK_INT(42, w.eventId);
    CHECK_INT(3, w.retained);
    CHECK_INT(3, w.seen);
    CHECK_INT(0, w.dropped);
    CHECK_INT(1, w.sourceKind);
    CHECK_TRUE(w.collecting);
    // Exact sizes on the wire: the window and every index frame (pinned, so an
    // encoder and decoder that agree on a wrong size cannot both pass).
    CHECK_INT(14, s.frames[0].size());
    CHECK_INT(14, s.frames[1].size());
    CHECK_INT(14, s.frames[3].size());
    FrameIndex fi;
    CHECK_TRUE(decodeFrameIndex(s.frames[1].data(), s.frames[1].size(), fi));
    CHECK_INT(0, fi.index);
    CHECK_INT(6, fi.channel);
    CHECK_INT(-70, fi.rssi);
    CHECK_INT(r.heard[0].size(), fi.onAir);
    CHECK_INT(ev::kFlagTrigger, fi.flags & ev::kFlagTrigger);
    CHECK_INT(0, fi.offsetMs);
    CHECK_TRUE(decodeFrameIndex(s.frames[3].data(), s.frames[3].size(), fi));
    CHECK_INT(100, fi.offsetMs);
    End e;
    CHECK_TRUE(decodeEnd(s.frames[4].data(), s.frames[4].size(), e));
    CHECK_INT(3, e.count);
    for (auto &f : s.frames) CHECK_TRUE(f.size() <= kMaxFrame);
    CHECK_INT(0, g_depth); // every lock released
}

void get_frame_bytes_streams_the_whole_window_at_the_connection_size()
{
    Rig r;
    Sent s;
    uint8_t req[kGetFrameBytesSize];
    encodeGetFrameBytes(0x32, 42, 0, 0, req);
    const size_t size = frameSizeForMtu(247);
    CHECK_INT(244, size);
    answerGetFrameBytes(r.access, req, sizeof(req), true, size, Sent::send, &s);
    // Reassemble by (index, offset) and compare with what was heard.
    std::vector<std::vector<uint8_t>> got(3);
    for (size_t k = 0; k + 1 < s.frames.size(); ++k) {
        const auto &f = s.frames[k];
        CHECK_INT(kTypeFrameBytes, f[0]);
        CHECK_TRUE(f.size() <= size);
        const uint8_t idx = f[2];
        const uint16_t off = static_cast<uint16_t>(f[3] | (f[4] << 8));
        CHECK_INT(got[idx].size(), off); // in order, no gaps
        got[idx].insert(got[idx].end(), f.begin() + kFrameBytesHeader, f.end());
    }
    for (int i = 0; i < 3; ++i) CHECK_TRUE(got[i] == r.heard[i]);
    End e;
    CHECK_TRUE(decodeEnd(s.frames.back().data(), s.frames.back().size(), e));
    CHECK_INT(s.frames.size() - 1, e.count);
    CHECK_INT(0, g_depth);
}

void bytes_resume_from_any_index_and_offset_at_twenty_bytes()
{
    Rig r;
    Sent s;
    uint8_t req[kGetFrameBytesSize];
    encodeGetFrameBytes(0x33, 42, 2, 100, req);
    answerGetFrameBytes(r.access, req, sizeof(req), true, frameSizeForMtu(23), Sent::send, &s);
    std::vector<uint8_t> tail;
    for (size_t k = 0; k + 1 < s.frames.size(); ++k) {
        CHECK_TRUE(s.frames[k].size() <= kMaxFrame);
        CHECK_INT(2, s.frames[k][2]);
        tail.insert(tail.end(), s.frames[k].begin() + kFrameBytesHeader, s.frames[k].end());
    }
    CHECK_TRUE(tail == std::vector<uint8_t>(r.heard[2].begin() + 100, r.heard[2].end()));
}

void a_dropped_connection_stops_the_stream()
{
    Rig r;
    Sent s;
    s.failAfter = 2;
    uint8_t req[kGetFrameBytesSize];
    encodeGetFrameBytes(0x34, 42, 0, 0, req);
    answerGetFrameBytes(r.access, req, sizeof(req), true, 244, Sent::send, &s);
    CHECK_INT(2, s.frames.size());
    CHECK_INT(0, g_depth);
}

void errors_name_the_problem()
{
    Rig r;
    uint8_t req[kGetFramesSize];
    encodeGetFrames(0x40, 42, req);
    Sent bad;
    answerGetFrames(r.access, req, 5, true, 1, 0, Sent::send, &bad);
    Error err;
    CHECK_TRUE(decodeError(bad.frames[0].data(), bad.frames[0].size(), err));
    CHECK_INT(static_cast<int>(LinkStatus::BadLength), static_cast<int>(err.status));

    Sent gone;
    answerGetFrames(r.access, req, sizeof(req), false, 1, 0, Sent::send, &gone);
    Result res;
    CHECK_TRUE(decodeResult(gone.frames[0].data(), gone.frames[0].size(), res));
    CHECK_INT(kOpGetFrames, res.commandType);
    CHECK_INT(static_cast<int>(CommandResult::InvalidArgument), res.commandResult);

    Sent none;
    encodeGetFrames(0x41, 99, req);
    answerGetFrames(r.access, req, sizeof(req), true, 1, 0, Sent::send, &none);
    CHECK_TRUE(decodeResult(none.frames[0].data(), none.frames[0].size(), res));
    CHECK_INT(static_cast<int>(CommandResult::NotReady), res.commandResult);
    CHECK_INT(0, g_depth);
}

void frame_sizes_follow_the_mtu()
{
    CHECK_INT(20, frameSizeForMtu(0));
    CHECK_INT(20, frameSizeForMtu(23));
    CHECK_INT(97, frameSizeForMtu(100));
    CHECK_INT(244, frameSizeForMtu(247));
    CHECK_INT(244, frameSizeForMtu(517));
    CHECK_TRUE(isEvidenceOp(0x06) && isEvidenceOp(0x07) && !isEvidenceOp(0x08));
    CHECK_INT(0x20, kCapRawCapture);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(get_frames_lists_the_window_then_ends);
    CASE(get_frame_bytes_streams_the_whole_window_at_the_connection_size);
    CASE(bytes_resume_from_any_index_and_offset_at_twenty_bytes);
    CASE(a_dropped_connection_stops_the_stream);
    CASE(errors_name_the_problem);
    CASE(frame_sizes_follow_the_mtu);
    CHECK_SUMMARY();
}
