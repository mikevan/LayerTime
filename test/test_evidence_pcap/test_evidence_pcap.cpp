// EvidencePcap (2026-11-04): an evidence window as the pcap file the Hunter
// writes to its SD card. The bytes match what LayerScope writes for a pulled
// window (LinkEvidence.toPcap, FrameFabricator.radiotap), time aside.

#include "check.h"

#include "core/logic/EvidencePcap.h"
#include "core/logic/EvidenceStore.h"

#include <string.h>
#include <vector>

using namespace layertime::evidence;

namespace {

const uint8_t AP[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

std::vector<uint8_t> deauth(uint16_t seq)
{
    std::vector<uint8_t> f = {0xC0, 0, 0, 0};
    f.insert(f.end(), BCAST, BCAST + 6);
    f.insert(f.end(), AP, AP + 6);
    f.insert(f.end(), AP, AP + 6);
    const uint16_t sc = static_cast<uint16_t>(seq << 4);
    f.push_back(static_cast<uint8_t>(sc & 0xFF));
    f.push_back(static_cast<uint8_t>(sc >> 8));
    f.push_back(7);
    f.push_back(0);
    return f;
}

uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

} // namespace

void the_file_header_is_classic_pcap_with_radiotap()
{
    uint8_t h[kPcapHeaderSize];
    CHECK_INT(24, pcapHeader(h));
    const uint8_t want[24] = {0xD4, 0xC3, 0xB2, 0xA1, 2, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 127, 0, 0, 0};
    CHECK_TRUE(memcmp(h, want, 24) == 0);
}

void radiotap_matches_layerscope_byte_for_byte()
{
    // FrameFabricator.radiotap(6, -48) and (36, -61).
    uint8_t rt[kRadiotapSize];
    CHECK_INT(16, radiotap(6, -48, rt));
    const uint8_t ch6[16] = {0, 0, 16, 0, 0x2A, 0, 0, 0, 0, 0, 0x85, 0x09, 0x80, 0x04, 0xD0, 0};
    CHECK_TRUE(memcmp(rt, ch6, 16) == 0);
    radiotap(36, -61, rt);
    const uint8_t ch36[16] = {0, 0, 16, 0, 0x2A, 0, 0, 0, 0, 0, 0x3C, 0x14, 0x40, 0x01, 0xC3, 0};
    CHECK_TRUE(memcmp(rt, ch36, 16) == 0);
    CHECK_INT(2484, channelMhz(14));
    CHECK_INT(2412, channelMhz(1));
    CHECK_INT(5745, channelMhz(149));
}

void a_window_becomes_one_record_per_kept_frame()
{
    std::vector<uint8_t> pool(EvidenceStore::poolBytes());
    EvidenceStore store;
    CHECK_TRUE(store.attach(pool.data(), pool.size()));
    uint32_t now = 123456;
    std::vector<std::vector<uint8_t>> sent;
    for (uint16_t i = 0; i < 3; ++i) {
        sent.push_back(deauth(static_cast<uint16_t>(100 + i)));
        now += 250;
        store.noteFrame(sent.back().data(), static_cast<uint16_t>(sent.back().size()), -48, 6, now);
        if (i == 0) CHECK_TRUE(store.openWindow(42, AP, now, kFollowMs));
    }
    std::vector<uint8_t> out(kMaxWindowPcapBytes);
    WindowInfo info;
    const size_t n = buildWindowPcap(store, 42, now, out.data(), out.size(), info);
    CHECK_INT(3, info.retained);
    CHECK_INT(24 + 3 * (16 + 16 + 26), n);
    size_t off = 24;
    uint32_t t = 123456;
    for (int i = 0; i < 3; ++i) {
        t += 250;
        const uint8_t *rec = out.data() + off;
        CHECK_INT(t / 1000, rd32(rec));
        CHECK_INT((t % 1000) * 1000, rd32(rec + 4));
        CHECK_INT(16 + 26, rd32(rec + 8));
        CHECK_INT(16 + 26, rd32(rec + 12));
        CHECK_INT(0xD0, rec[16 + 14]);                       // the signal, in the radiotap header
        CHECK_TRUE(memcmp(rec + 32, sent[i].data(), 26) == 0); // the frame, exactly as heard
        off += 16 + 16 + 26;
    }
    // Too small a buffer, or no such window: 0, never a short file.
    CHECK_INT(0, buildWindowPcap(store, 42, now, out.data(), n - 1, info));
    CHECK_INT(0, buildWindowPcap(store, 99, now, out.data(), out.size(), info));
    CHECK_INT(0, buildWindowPcap(store, 42, now, nullptr, out.size(), info));
}

void the_largest_window_fits_the_documented_size()
{
    CHECK_INT(24 + 64 * (16 + 16 + 1024), kMaxWindowPcapBytes);
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(the_file_header_is_classic_pcap_with_radiotap);
    CASE(radiotap_matches_layerscope_byte_for_byte);
    CASE(a_window_becomes_one_record_per_kept_frame);
    CASE(the_largest_window_fits_the_documented_size);
    CHECK_SUMMARY();
}
