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

// Evidence hold (2026-11-04): a 30 s hold collects for 30 s; Off and 5 s
// still collect for kFollowMs, because collecting costs no radio time.
void the_window_collects_for_the_evidence_hold_but_never_less_than_ten_seconds()
{
    CHECK_INT(kFollowMs, EvidenceStore::followMsFor(0));
    CHECK_INT(kFollowMs, EvidenceStore::followMsFor(5));
    CHECK_INT(10000, EvidenceStore::followMsFor(10));
    CHECK_INT(20000, EvidenceStore::followMsFor(20));
    CHECK_INT(30000, EvidenceStore::followMsFor(30));
    CHECK_INT(255000, EvidenceStore::followMsFor(255)); // no overflow at the type's limit

    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now, EvidenceStore::followMsFor(30)));
    WindowInfo w;
    r.hear(deauth(AP, BCAST, 3001), -72, 25000);   // 25 s in: still collecting
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_TRUE(w.collecting);
    const uint8_t at25 = w.retained;
    r.hear(deauth(AP, BCAST, 3002), -72, 5001);    // just past 30 s: stopped
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_FALSE(w.collecting);
    CHECK_INT(at25, w.retained);
}

// A repeat sighting pushes the deadline out, and revives a window whose
// deadline had passed, so a sustained flood stays in one window.
void a_repeat_sighting_extends_or_reopens_the_window()
{
    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now, kFollowMs));
    WindowInfo w;
    r.hear(deauth(AP, BCAST, 3001), -72, 8000);
    CHECK_TRUE(r.store.extendWindow(7, r.now, kFollowMs)); // 8 s in: until 18 s
    r.hear(deauth(AP, BCAST, 3002), -72, 9000);            // 17 s: kept
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_TRUE(w.collecting);
    const uint8_t at17 = w.retained;
    // A shorter extension never pulls the deadline in.
    CHECK_TRUE(r.store.extendWindow(7, r.now, 100));
    r.hear(deauth(AP, BCAST, 3003), -72, 500);             // 17.5 s: still kept
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_INT(at17 + 1, w.retained);
    // Let it lapse, then a repeat 15 s later (the deauth cooldown) revives it.
    r.hear(deauth(AP, BCAST, 3004), -72, 15000);
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_FALSE(w.collecting);
    const uint8_t lapsed = w.retained;
    CHECK_TRUE(r.store.extendWindow(7, r.now, kFollowMs));
    r.hear(deauth(AP, BCAST, 3005), -72, 1000);
    CHECK_TRUE(r.store.info(7, r.now, w));
    CHECK_TRUE(w.collecting);
    CHECK_INT(lapsed + 1, w.retained);
    // No such window: refused, nothing opened.
    CHECK_FALSE(r.store.extendWindow(99, r.now, kFollowMs));
    CHECK_FALSE(r.store.info(99, r.now, w));
    CHECK_FALSE(r.store.extendWindow(0, r.now, kFollowMs));
}

// Saving to the card (2026-11-04): a window is due once it stops collecting,
// again only if a revival heard more, and a window reused unsaved is counted.
void a_closed_window_is_due_for_saving_once_and_again_after_it_hears_more()
{
    Rig r;
    prelude(r);
    r.hear(deauth(AP, BCAST, 3000), -72, 50);
    CHECK_TRUE(r.store.openWindow(7, AP, r.now, kFollowMs));
    uint32_t ev = 0;
    CHECK_FALSE(r.store.nextToSave(r.now, ev));          // still collecting
    r.hear(deauth(AP, BCAST, 3001), -72, kFollowMs + 1);  // closed
    CHECK_TRUE(r.store.nextToSave(r.now, ev));
    CHECK_INT(7, ev);
    WindowInfo w;
    CHECK_TRUE(r.store.info(7, r.now, w));
    r.store.markSaved(7, w.seen);
    CHECK_FALSE(r.store.nextToSave(r.now, ev));          // saved as it stands
    // Marked with an older count (a frame came in after the copy): due again.
    r.store.markSaved(7, static_cast<uint16_t>(w.seen - 1));
    CHECK_TRUE(r.store.nextToSave(r.now, ev));
    r.store.markSaved(7, w.seen);
    // Revived, but nothing new heard: no rewrite.
    CHECK_TRUE(r.store.extendWindow(7, r.now, kFollowMs));
    r.hear(beacon(OTHER, "Neighbor", 9), -80, kFollowMs + 1);
    CHECK_FALSE(r.store.nextToSave(r.now, ev));
    // Revived and heard more: due again once it closes.
    CHECK_TRUE(r.store.extendWindow(7, r.now, kFollowMs));
    r.hear(deauth(AP, BCAST, 3002), -72, 100);
    CHECK_FALSE(r.store.nextToSave(r.now, ev));          // collecting
    r.hear(beacon(OTHER, "Neighbor", 10), -80, kFollowMs);
    CHECK_TRUE(r.store.nextToSave(r.now, ev));
    CHECK_INT(7, ev);
    r.store.markSaved(99, 0);                            // no such window: harmless
    CHECK_INT(0, r.store.lostUnsaved());
}

void the_oldest_closed_window_saves_first_and_an_unsaved_reuse_is_counted()
{
    Rig r;
    r.hear(deauth(AP, BCAST, 1), -72, 10);
    CHECK_TRUE(r.store.openWindow(1, AP, r.now, kFollowMs));
    r.hear(deauth(AP, BCAST, 2), -72, 10);
    CHECK_TRUE(r.store.openWindow(2, AP, r.now, kFollowMs));
    r.hear(deauth(AP, BCAST, 3), -72, kFollowMs + 100);
    uint32_t ev = 0;
    CHECK_TRUE(r.store.nextToSave(r.now, ev));
    CHECK_INT(1, ev);
    WindowInfo w1;
    CHECK_TRUE(r.store.info(1, r.now, w1));
    r.store.markSaved(1, w1.seen);
    CHECK_TRUE(r.store.nextToSave(r.now, ev));
    CHECK_INT(2, ev);
    // Fill every window; the reuse of saved window 1 is not a loss, of unsaved 2 is.
    for (uint32_t id = 10; id < 10 + kWindows; ++id) {
        r.hear(deauth(AP, BCAST, static_cast<uint16_t>(id)), -72, 1);
        CHECK_TRUE(r.store.openWindow(id, AP, r.now, kFollowMs));
    }
    CHECK_INT(1, r.store.lostUnsaved());
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
    CASE(the_window_collects_for_the_evidence_hold_but_never_less_than_ten_seconds);
    CASE(a_repeat_sighting_extends_or_reopens_the_window);
    CASE(a_closed_window_is_due_for_saving_once_and_again_after_it_hears_more);
    CASE(the_oldest_closed_window_saves_first_and_an_unsaved_reuse_is_counted);
    CASE(a_full_window_counts_what_it_dropped);
    CASE(a_long_frame_is_cut_and_flagged);
    CASE(bytes_read_back_exactly_in_any_chunk_size);
    CASE(the_oldest_window_is_reused_when_all_are_taken);
    CASE(mac_text_parses_strictly);
    CHECK_SUMMARY();
}
