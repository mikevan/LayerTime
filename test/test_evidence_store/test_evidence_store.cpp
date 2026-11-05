// EvidenceStore (Link 0.2 evidence, slice 2, 2026-11-04): the frames behind
// each Wi-Fi detection. Builds 802.11 frames byte by byte, so every case knows
// exactly what the Hunter would have heard.

#include "check.h"

#include "core/logic/EvidenceStore.h"

#include <string.h>
#include <vector>

using namespace layertime::evidence;

namespace {

const uint8_t AP[6] = {0xaa, 0xbb, 0xcc, 0x00, 0x00, 0x01};
const uint8_t CLIENT[6] = {0x10, 0x20, 0x30, 0x40, 0x50, 0x61};
const uint8_t TWIN[6] = {0xde, 0xad, 0xbe, 0xef, 0x00, 0x01};
const uint8_t OTHER[6] = {0x22, 0x22, 0x22, 0x22, 0x22, 0x22};
const uint8_t BCAST[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

std::vector<uint8_t> hdr(uint8_t fc0, uint8_t fc1, const uint8_t *a1, const uint8_t *a2, const uint8_t *a3, uint16_t seq)
{
    std::vector<uint8_t> f = {fc0, fc1, 0, 0};
    f.insert(f.end(), a1, a1 + 6);
    f.insert(f.end(), a2, a2 + 6);
    f.insert(f.end(), a3, a3 + 6);
    const uint16_t sc = static_cast<uint16_t>(seq << 4);
    f.push_back(static_cast<uint8_t>(sc & 0xFF));
    f.push_back(static_cast<uint8_t>(sc >> 8));
    return f;
}
std::vector<uint8_t> beacon(const uint8_t *bssid, const char *ssid, uint16_t seq)
{
    std::vector<uint8_t> f = hdr(0x80, 0, BCAST, bssid, bssid, seq);
    for (int i = 0; i < 12; ++i) f.push_back(0);
    f.push_back(0);
    f.push_back(static_cast<uint8_t>(strlen(ssid)));
    f.insert(f.end(), ssid, ssid + strlen(ssid));
    return f;
}
std::vector<uint8_t> deauth(const uint8_t *from, const uint8_t *to, uint16_t seq)
{
    std::vector<uint8_t> f = hdr(0xC0, 0, to, from, AP, seq);
    f.push_back(7);
    f.push_back(0);
    return f;
}
// Data from a station to the AP (ToDS).
std::vector<uint8_t> dataToAp(const uint8_t *station, const uint8_t *bssid, uint16_t seq)
{
    std::vector<uint8_t> f = hdr(0x08, 0x01, bssid, station, bssid, seq);
    for (int i = 0; i < 16; ++i) f.push_back(0x5A);
    return f;
}

struct Rig {
    std::vector<uint8_t> pool;
    EvidenceStore store;
    uint32_t now = 100000;
    Rig() : pool(EvidenceStore::poolBytes()) { CHECK_TRUE(store.attach(pool.data(), pool.size())); }
    void hear(const std::vector<uint8_t> &f, int8_t rssi = -50, uint32_t stepMs = 0)
    {
        now += stepMs;
        store.noteFrame(f.data(), static_cast<uint16_t>(f.size()), rssi, 6, now);
    }
    std::vector<uint8_t> bytes(uint32_t ev, uint8_t i)
    {
        std::vector<uint8_t> out;
        uint8_t chunk[17];
        uint16_t off = 0;
        for (;;) {
            const size_t n = store.copyBytes(ev, i, off, chunk, sizeof(chunk));
            if (n == 0) break;
            out.insert(out.end(), chunk, chunk + n);
            off = static_cast<uint16_t>(off + n);
        }
        return out;
    }
};

// One second of the AP's beacons (10), plus the client talking, then the flood's first frame.
void prelude(Rig &r)
{
    for (int i = 0; i < 10; ++i) {
        r.hear(beacon(AP, "HomeNet", static_cast<uint16_t>(1000 + i)), -48, 100);
        if (i == 5) r.hear(dataToAp(CLIENT, AP, 40), -58, 1);
    }
    r.hear(beacon(OTHER, "Neighbor", 7), -80, 10); // unrelated
}

} // namespace

void pool_size_and_attach()
{
    CHECK_TRUE(EvidenceStore::poolBytes() > 2300000 && EvidenceStore::poolBytes() < 2500000);
    std::vector<uint8_t> small(1000);
    EvidenceStore s;
    CHECK_FALSE(s.attach(small.data(), small.size()));
    CHECK_FALSE(s.attached());
    const uint8_t f[30] = {0};
    s.noteFrame(f, sizeof(f), -50, 6, 1); // no memory: ignored, no crash
    CHECK_FALSE(s.openWindow(1, AP, 1));
    WindowInfo w;
    CHECK_FALSE(s.info(1, 1, w));
}

void a_window_starts_with_recent_beacons_then_the_trigger()
{
    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now));
    WindowInfo w;
    CHECK_TRUE(r.store.info(7, r.now, w));
    // The four newest AP beacons, the client's data frame, then the trigger.
    CHECK_INT(6, w.retained);
    CHECK_TRUE(w.collecting);
    FrameMeta m;
    int32_t off = 0;
    CHECK_TRUE(r.store.frameMeta(7, 5, m, off));
    CHECK_INT(kFlagTrigger, m.flags & kFlagTrigger);
    CHECK_INT(0, off);
    CHECK_INT(-72, m.rssi);
    CHECK_TRUE(r.store.frameMeta(7, 0, m, off));
    CHECK_TRUE(off < 0);
    CHECK_INT(0, m.flags & kFlagTrigger);
    // The neighbor's beacon is not in it.
    for (uint8_t i = 0; i < w.retained; ++i) {
        const std::vector<uint8_t> b = r.bytes(7, i);
        CHECK_TRUE(memcmp(b.data() + 10, OTHER, 6) != 0);
    }
}

void the_flood_the_clone_and_the_handshake_follow_into_the_window()
{
    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now));
    for (int i = 1; i < 40; ++i) r.hear(deauth(AP, BCAST, static_cast<uint16_t>(3000 + i)), -72, 50);
    // 2 s of AP beacons: thinned to one a second.
    for (int i = 0; i < 20; ++i) r.hear(beacon(AP, "HomeNet", static_cast<uint16_t>(1040 + i)), -48, 100);
    // The clone: same name, new BSSID.
    r.hear(beacon(TWIN, "HomeNet", 10), -70, 100);
    // The client hands its handshake to the twin (ToDS data from the client).
    r.hear(dataToAp(CLIENT, TWIN, 41), -60, 100);
    r.hear(beacon(OTHER, "Neighbor", 8), -80, 10);
    WindowInfo w;
    CHECK_TRUE(r.store.info(7, r.now, w));
    // 6 seeded/trigger + 39 deauths + 2 thinned AP beacons + clone + client frame.
    CHECK_INT(49, w.retained);
    CHECK_INT(0, w.dropped);
    bool sawTwin = false, sawClientToTwin = false;
    for (uint8_t i = 0; i < w.retained; ++i) {
        const std::vector<uint8_t> b = r.bytes(7, i);
        if (b[0] == 0x80 && memcmp(b.data() + 10, TWIN, 6) == 0) sawTwin = true;
        if (b[0] == 0x08 && memcmp(b.data() + 10, CLIENT, 6) == 0 && memcmp(b.data() + 4, TWIN, 6) == 0) sawClientToTwin = true;
        CHECK_TRUE(memcmp(b.data() + 10, OTHER, 6) != 0);
    }
    CHECK_TRUE(sawTwin);
    CHECK_TRUE(sawClientToTwin);
}

void the_window_stops_collecting_after_ten_seconds()
{
    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now));
    WindowInfo w;
    r.store.info(7, r.now, w);
    const uint8_t before = w.retained;
    r.hear(deauth(AP, BCAST, 3001), -72, kFollowMs + 1);
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_FALSE(w.collecting);
    CHECK_INT(before, w.retained);
}

void a_full_window_counts_what_it_dropped()
{
    Rig r;
    r.hear(deauth(AP, BCAST, 1), -72, 10);
    CHECK_TRUE(r.store.openWindow(9, AP, r.now));
    for (int i = 0; i < 100; ++i) r.hear(deauth(AP, BCAST, static_cast<uint16_t>(2 + i)), -72, 10);
    WindowInfo w;
    CHECK_TRUE(r.store.info(9, r.now, w));
    CHECK_INT(kWindowFrames, w.retained);
    CHECK_INT(101, w.seen);
    CHECK_INT(101 - kWindowFrames, w.dropped);
}

void a_long_frame_is_cut_and_flagged()
{
    Rig r;
    std::vector<uint8_t> big = deauth(AP, BCAST, 5);
    big.resize(1500, 0xEE);
    r.hear(big, -60, 10);
    CHECK_TRUE(r.store.openWindow(3, AP, r.now));
    WindowInfo w;
    r.store.info(3, r.now, w);
    CHECK_INT(1, w.truncated);
    FrameMeta m;
    int32_t off;
    CHECK_TRUE(r.store.frameMeta(3, 0, m, off));
    CHECK_INT(1500, m.onAir);
    CHECK_INT(kMaxFrameBytes, m.captured);
    CHECK_INT(kFlagTruncated, m.flags & kFlagTruncated);
    const std::vector<uint8_t> b = r.bytes(3, 0);
    CHECK_INT(kMaxFrameBytes, b.size());
    CHECK_TRUE(memcmp(b.data(), big.data(), kMaxFrameBytes) == 0);
}

void bytes_read_back_exactly_in_any_chunk_size()
{
    Rig r;
    const std::vector<uint8_t> f = beacon(AP, "HomeNet", 77);
    r.hear(f, -50, 10);
    CHECK_TRUE(r.store.openWindow(4, AP, r.now));
    CHECK_TRUE(r.bytes(4, 0) == f);
    uint8_t one[1];
    CHECK_INT(0, r.store.copyBytes(4, 0, static_cast<uint16_t>(f.size()), one, 1)); // past the end
    CHECK_INT(0, r.store.copyBytes(4, 1, 0, one, 1));                              // no such frame
    CHECK_INT(0, r.store.copyBytes(99, 0, 0, one, 1));                             // no such window
}

void the_oldest_window_is_reused_when_all_are_taken()
{
    Rig r;
    for (uint32_t ev = 1; ev <= kWindows + 1; ++ev) {
        r.hear(deauth(AP, BCAST, static_cast<uint16_t>(ev)), -72, 20);
        CHECK_TRUE(r.store.openWindow(ev, AP, r.now));
    }
    WindowInfo w;
    CHECK_FALSE(r.store.info(1, r.now, w));
    CHECK_TRUE(r.store.info(2, r.now, w));
    CHECK_TRUE(r.store.info(kWindows + 1, r.now, w));
}

void mac_text_parses_strictly()
{
    uint8_t m[6];
    CHECK_TRUE(EvidenceStore::parseMac("aa:BB:cc:00:00:01", m));
    CHECK_TRUE(memcmp(m, AP, 6) == 0);
    CHECK_FALSE(EvidenceStore::parseMac("aa:bb:cc:00:00", m));
    CHECK_FALSE(EvidenceStore::parseMac("aa-bb-cc-00-00-01", m));
    CHECK_FALSE(EvidenceStore::parseMac("aa:bb:cc:00:00:01x", m));
    CHECK_FALSE(EvidenceStore::parseMac(nullptr, m));
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(pool_size_and_attach);
    CASE(a_window_starts_with_recent_beacons_then_the_trigger);
    CASE(the_flood_the_clone_and_the_handshake_follow_into_the_window);
    CASE(the_window_stops_collecting_after_ten_seconds);
    CASE(a_full_window_counts_what_it_dropped);
    CASE(a_long_frame_is_cut_and_flagged);
    CASE(bytes_read_back_exactly_in_any_chunk_size);
    CASE(the_oldest_window_is_reused_when_all_are_taken);
    CASE(mac_text_parses_strictly);
    CHECK_SUMMARY();
}
