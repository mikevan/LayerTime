// Link 0.2 Phase 0 measurement helpers: the test-only BULK op and the FCS
// check (src/core/link/LinkMeasure). Also proves the release Node
// dispatcher still answers BULK with ERROR UnknownOp: BULK lives only in
// the measurement firmware.
//
// Build command: see test/README.md, "Link 0.2 Phase 0 measurement".

#include "check.h"

#include <cstdint>
#include <cstring>
#include <vector>

#include "core/link/LinkCodec.h"
#include "core/link/LinkFrames.h"
#include "core/link/LinkMeasure.h"

using namespace layertime::link;
using namespace layertime::link::measure;

void bulk_request_round_trips()
{
    BulkRequest r;
    r.reqId = 7; r.count = 200; r.size = 244;
    uint8_t b[kBulkRequestSize];
    CHECK_INT(5, encodeBulk(r, b));
    const uint8_t expected[] = {0xF2, 7, 200, 0, 244};
    CHECK_TRUE(std::memcmp(b, expected, 5) == 0);
    BulkRequest d;
    CHECK_TRUE(decodeBulk(b, 5, d));
    CHECK_INT(7, d.reqId);
    CHECK_INT(200, d.count);
    CHECK_INT(244, d.size);
}

void bulk_request_rejects_bad_input()
{
    BulkRequest d;
    const uint8_t ok[] = {0xF2, 1, 10, 0, 20};
    CHECK_FALSE(decodeBulk(ok, 4, d));
    CHECK_FALSE(decodeBulk(ok, 6, d));
    CHECK_FALSE(decodeBulk(nullptr, 5, d));
    const uint8_t zero[] = {0xF2, 1, 0, 0, 20};
    CHECK_FALSE(decodeBulk(zero, 5, d));
    const uint8_t many[] = {0xF2, 1, 0xE9, 0x03, 20};  // 1001
    CHECK_FALSE(decodeBulk(many, 5, d));
    const uint8_t tiny[] = {0xF2, 1, 10, 0, 5};
    CHECK_FALSE(decodeBulk(tiny, 5, d));
    const uint8_t wrongOp[] = {0xF1, 1, 10, 0, 20};
    CHECK_FALSE(decodeBulk(wrongOp, 5, d));
    const uint8_t max[] = {0xF2, 1, 0xE8, 0x03, 6};  // 1000, minimum size
    CHECK_TRUE(decodeBulk(max, 5, d));
}

void frame_size_is_capped_by_mtu_and_maximum()
{
    CHECK_INT(20, bulkFrameSize(20, 23));    // default MTU 23: 20 bytes
    CHECK_INT(20, bulkFrameSize(244, 23));
    CHECK_INT(244, bulkFrameSize(244, 247));
    CHECK_INT(244, bulkFrameSize(255, 517));
    CHECK_INT(100, bulkFrameSize(100, 247));
    CHECK_INT(0, bulkFrameSize(5, 247));     // below the header
    CHECK_INT(0, bulkFrameSize(20, 8));      // MTU too small for a header
    CHECK_INT(0, bulkFrameSize(20, 0));
}

void data_frames_carry_header_and_checkable_pattern()
{
    uint8_t f[kBulkMaxSize];
    CHECK_INT(244, fillBulkFrame(9, 0x0102, 300, 244, f));
    CHECK_INT(0xF2, f[0]);
    CHECK_INT(9, f[1]);
    CHECK_INT(0x02, f[2]);
    CHECK_INT(0x01, f[3]);
    CHECK_INT(44, f[4]);   // 300 = 0x012C
    CHECK_INT(1, f[5]);
    CHECK_INT(6, f[6]);
    CHECK_INT(243, f[243]);
    CHECK_TRUE(bulkFrameIntact(f, 244));
    f[100] ^= 0x01;
    CHECK_FALSE(bulkFrameIntact(f, 244));
    CHECK_INT(0, fillBulkFrame(9, 0, 1, 5, f));
    CHECK_INT(0, fillBulkFrame(9, 0, 1, 245, f));
    CHECK_INT(20, fillBulkFrame(9, 0, 1, 20, f));
    CHECK_TRUE(bulkFrameIntact(f, 20));
}

void done_frame_round_trips_and_fits_twenty_bytes()
{
    BulkDone d;
    d.reqId = 3; d.sent = 200; d.size = 244; d.mtu = 247; d.refused = 70000; d.elapsedMs = 1234567;
    uint8_t f[kBulkDoneSize];
    CHECK_INT(17, encodeBulkDone(d, f));
    CHECK_TRUE(kBulkDoneSize <= kMaxFrame);
    CHECK_FALSE(bulkFrameIntact(f, 17));  // a done frame is not a data frame
    BulkDone r;
    CHECK_TRUE(decodeBulkDone(f, 17, r));
    CHECK_INT(3, r.reqId);
    CHECK_INT(200, r.sent);
    CHECK_INT(244, r.size);
    CHECK_INT(247, r.mtu);
    CHECK_INT(70000, r.refused);
    CHECK_INT(1234567, r.elapsedMs);
    CHECK_FALSE(decodeBulkDone(f, 16, r));
    uint8_t data[20];
    fillBulkFrame(3, 0, 1, 17, data);
    CHECK_FALSE(decodeBulkDone(data, 17, r));
}

// Reference values from zlib.crc32, the same CRC-32 802.11 uses for its FCS.
void crc32_matches_the_reference()
{
    const uint8_t text[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    CHECK_INT(0xCBF43926u, crc32(text, sizeof(text)));
    CHECK_INT(0u, crc32(text, 0));
}

// The deauth fixture (wireshark-spike/deauth_fixture.pcap) with its FCS
// appended: c0003a01 ffffffffffff 00112233aabb 00112233aabb 7000 0700, then
// 4949dd11, computed with zlib.crc32 and stored little-endian.
void fcs_is_found_on_a_frame_that_carries_one()
{
    const uint8_t withFcs[] = {0xC0, 0x00, 0x3A, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                               0x00, 0x11, 0x22, 0x33, 0xAA, 0xBB, 0x00, 0x11, 0x22, 0x33,
                               0xAA, 0xBB, 0x70, 0x00, 0x07, 0x00, 0x49, 0x49, 0xDD, 0x11};
    CHECK_TRUE(endsWithFcs(withFcs, sizeof(withFcs)));
    CHECK_FALSE(endsWithFcs(withFcs, sizeof(withFcs) - 4));   // the bare frame has none
    std::vector<uint8_t> corrupt(withFcs, withFcs + sizeof(withFcs));
    corrupt[12] ^= 0x40;
    CHECK_FALSE(endsWithFcs(corrupt.data(), corrupt.size()));
    CHECK_FALSE(endsWithFcs(withFcs, 7));
    CHECK_FALSE(endsWithFcs(nullptr, 30));
}

void release_dispatcher_still_rejects_bulk()
{
    NodeIdentity node;
    node.sessionId = 0x1234;
    BulkRequest r;
    r.reqId = 0x42; r.count = 10; r.size = 20;
    uint8_t req[kBulkRequestSize];
    encodeBulk(r, req);
    uint8_t reply[kMaxFrame];
    const size_t n = dispatch(node, 0, req, sizeof(req), reply);
    CHECK_INT(kErrorSize, n);
    CHECK_INT(static_cast<int>(FrameType::Error), reply[0]);
    CHECK_INT(0x42, reply[1]);
    CHECK_INT(static_cast<int>(LinkStatus::UnknownOp), reply[2]);
    // A test build's dispatcher does not bind it either; the measurement
    // firmware answers BULK in its own transport (C5Link).
    node.testBuild = true;
    CHECK_INT(kErrorSize, dispatch(node, 0, req, sizeof(req), reply));
    CHECK_INT(static_cast<int>(LinkStatus::UnknownOp), reply[2]);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(bulk_request_round_trips);
    CASE(bulk_request_rejects_bad_input);
    CASE(frame_size_is_capped_by_mtu_and_maximum);
    CASE(data_frames_carry_header_and_checkable_pattern);
    CASE(done_frame_round_trips_and_fits_twenty_bytes);
    CASE(crc32_matches_the_reference);
    CASE(fcs_is_found_on_a_frame_that_carries_one);
    CASE(release_dispatcher_still_rejects_bulk);
    CHECK_SUMMARY();
}
