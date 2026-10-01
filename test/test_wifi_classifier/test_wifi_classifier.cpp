// Unit tests for src/core/logic/WifiFrameClassifier, moved out of
// ReconService in Phase 0 Step 3e. test_recon still drives it through
// ReconService's promiscuous callback; this suite exercises it directly, with
// no stubs, including limits test_recon does not reach.

#include "check.h"

#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "core/logic/WifiFrameClassifier.h"

using namespace layertime;
using namespace layertime::recon;

namespace {

struct Seen { ReconTarget d; std::string detail, address; int rssi; Confidence c; int channel; };

struct Harness {
    WifiFrameClassifier k;
    std::set<ReconTarget> wanted;
    std::vector<Seen> seen;
    uint32_t now = 1000;

    static bool wantsFn(ReconTarget d, const void *ctx)
    {
        return static_cast<const Harness *>(ctx)->wanted.count(d) != 0;
    }
    static void sinkFn(const Candidate &c, void *ctx)
    {
        static_cast<Harness *>(ctx)->seen.push_back(
            {c.detector, c.detail, c.address, c.rssi, c.confidence, c.channel});
    }
    void feed(const std::vector<uint8_t> &f, int8_t rssi = -50, uint8_t ch = 1)
    {
        k.classify(f.data(), static_cast<uint16_t>(f.size()), rssi, ch, now, wantsFn, this,
                   sinkFn, this);
    }
};

std::vector<uint8_t> mgmt(uint8_t subtype, const uint8_t tx[6], size_t len = 24)
{
    std::vector<uint8_t> f(len, 0);
    f[0] = static_cast<uint8_t>(subtype << 4);
    memcpy(&f[10], tx, 6);
    return f;
}

std::vector<uint8_t> beacon(const uint8_t bssid[6], const std::string &ssid, bool privacy = true)
{
    auto f = mgmt(0x08, bssid, 36);
    f[34] = privacy ? 0x10 : 0;
    f.push_back(0);
    f.push_back(static_cast<uint8_t>(ssid.size()));
    f.insert(f.end(), ssid.begin(), ssid.end());
    return f;
}

const uint8_t kMac[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};
const uint8_t kPwn[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD};

} // namespace

void deauth_frames_that_are_not_wanted_are_not_counted()
{
    Harness h;
    for (int i = 0; i < 10; ++i) h.feed(mgmt(0x0C, kMac));
    h.wanted = {ReconTarget::Deauth};
    for (int i = 0; i < 5; ++i) h.feed(mgmt(0x0C, kMac));
    CHECK_INT(0, h.seen.size());
    h.feed(mgmt(0x0C, kMac));
    CHECK_INT(1, h.seen.size());
}

void only_management_deauth_and_disassoc_count()
{
    Harness h;
    h.wanted = {ReconTarget::Deauth};
    auto data = mgmt(0x0C, kMac);
    data[0] |= 0x08;  // type 2, data frame
    for (int i = 0; i < 10; ++i) h.feed(data);
    auto probe = mgmt(0x04, kMac);
    for (int i = 0; i < 10; ++i) h.feed(probe);
    CHECK_INT(0, h.seen.size());
}

void frame_length_limits()
{
    Harness h;
    h.wanted = {ReconTarget::Deauth, ReconTarget::Pwnagotchi};
    for (int i = 0; i < 6; ++i) h.feed(mgmt(0x0C, kMac, 23));
    CHECK_INT(0, h.seen.size());
    for (int i = 0; i < 6; ++i) h.feed(mgmt(0x0C, kMac, 24));
    CHECK_INT(1, h.seen.size());
    h.feed(mgmt(0x08, kPwn, 37));
    CHECK_INT(1, h.seen.size());
    h.feed(mgmt(0x08, kPwn, 38));
    CHECK_INT(2, h.seen.size());
}

void a_seventh_transmitter_evicts_the_longest_quiet()
{
    Harness h;
    h.wanted = {ReconTarget::Deauth};
    uint8_t macs[7][6];
    for (int m = 0; m < 7; ++m) {
        memcpy(macs[m], kMac, 6);
        macs[m][5] = static_cast<uint8_t>(m);
    }
    // Six transmitters, one frame each, oldest first.
    for (int m = 0; m < 6; ++m) { h.feed(mgmt(0x0C, macs[m])); h.now += 10; }
    // A seventh arrives; the first (quiet longest) loses its slot.
    h.feed(mgmt(0x0C, macs[6]));
    // Transmitter 1 still has its first frame counted: five more make six.
    for (int i = 0; i < 5; ++i) h.feed(mgmt(0x0C, macs[1]));
    CHECK_INT(1, h.seen.size());
    // Transmitter 0 was evicted: five more make only five, then a sixth fires.
    for (int i = 0; i < 5; ++i) h.feed(mgmt(0x0C, macs[0]));
    CHECK_INT(1, h.seen.size());
    h.feed(mgmt(0x0C, macs[0]));
    CHECK_INT(2, h.seen.size());
}

void beacon_candidates_come_out_in_a_fixed_order()
{
    Harness h;
    // Confirm "a" and two beacons of "b" first, with only Multi-SSID wanted.
    h.wanted = {ReconTarget::MultiSSID};
    for (int i = 0; i < 3; ++i) h.feed(beacon(kPwn, "a", false), -61, 9);
    for (int i = 0; i < 2; ++i) h.feed(beacon(kPwn, "b", false), -61, 9);
    CHECK_INT(0, h.seen.size());
    // The third "b" confirms it: Pwnagotchi, then Multi-SSID, then Pineapple.
    h.wanted = {ReconTarget::Pwnagotchi, ReconTarget::Pineapple, ReconTarget::MultiSSID};
    h.feed(beacon(kPwn, "b", false), -61, 9);
    CHECK_INT(3, h.seen.size());
    const ReconTarget order[] = {ReconTarget::Pwnagotchi, ReconTarget::MultiSSID,
                                 ReconTarget::Pineapple};
    for (size_t i = 0; i < 3 && i < h.seen.size(); ++i)
        CHECK_INT(static_cast<int>(order[i]), static_cast<int>(h.seen[i].d));
    CHECK_STR("DE:AD:BE:EF:DE:AD", h.seen[0].address.c_str());
    CHECK_INT(-61, h.seen[0].rssi);
    CHECK_INT(9, h.seen[0].channel);
}

void ssid_element_must_fit_and_be_32_bytes_or_fewer()
{
    Harness h;
    h.wanted = {ReconTarget::Axon};
    auto tooLong = beacon(kMac, std::string("AXON-") + std::string(28, 'x'));  // 33 bytes
    h.feed(tooLong);
    auto cut = beacon(kMac, "AXON-1");
    cut.pop_back();  // declared length runs past the frame
    h.feed(cut);
    CHECK_INT(0, h.seen.size());
    h.feed(beacon(kMac, std::string("AXON-") + std::string(27, 'x')));  // exactly 32
    CHECK_INT(1, h.seen.size());
}

void ssid_detail_is_cut_at_31_characters()
{
    Harness h;
    h.wanted = {ReconTarget::Axon};
    h.feed(beacon(kMac, "AB2-0123456789012345678901234567"));  // 32 bytes
    CHECK_INT(1, h.seen.size());
    if (!h.seen.empty()) CHECK_STR("SSID AB2-012345678901234567890123456", h.seen[0].detail.c_str());
}

void unwanted_ssid_prefix_stops_the_prefix_search()
{
    Harness h;
    h.wanted = {ReconTarget::Pwnagotchi};
    h.feed(beacon(kMac, "AXON-1"));
    CHECK_INT(0, h.seen.size());
}

// ---- several SSIDs from one BSSID --------------------------------------------

namespace {
void feedN(Harness &h, const uint8_t bssid[6], const std::string &ssid, int n)
{
    for (int i = 0; i < n; ++i) h.feed(beacon(bssid, ssid));
}
} // namespace

void multissid_one_stray_frame_never_flags()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, "home", 10);
    feedN(h, kMac, "hone", 1);  // one garbled or odd beacon
    feedN(h, kMac, "home", 10);
    CHECK_INT(0, h.seen.size());
}

void multissid_flags_when_a_second_name_is_confirmed()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, "home", 3);
    feedN(h, kMac, "guest", 2);
    CHECK_INT(0, h.seen.size());
    h.feed(beacon(kMac, "guest"), -48, 6);
    CHECK_INT(1, h.seen.size());
    if (h.seen.empty()) return;
    CHECK_INT(static_cast<int>(ReconTarget::MultiSSID), static_cast<int>(h.seen[0].d));
    CHECK_STR("SSIDs: home | guest", h.seen[0].detail.c_str());
    CHECK_STR("02:11:22:33:44:55", h.seen[0].address.c_str());
    CHECK_INT(static_cast<int>(Confidence::Medium), static_cast<int>(h.seen[0].c));
    CHECK_INT(-48, h.seen[0].rssi);
    CHECK_INT(6, h.seen[0].channel);
}

void multissid_beacons_alternating_count_toward_each_name()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    for (int i = 0; i < 3; ++i) {
        h.feed(beacon(kMac, "home"));
        h.feed(beacon(kMac, "guest"));
    }
    CHECK_INT(1, h.seen.size());
}

void multissid_reports_once_per_confirmed_name_not_per_beacon()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, "home", 3);
    feedN(h, kMac, "guest", 3);
    CHECK_INT(1, h.seen.size());
    for (int i = 0; i < 100; ++i) {
        h.feed(beacon(kMac, "home"));
        h.feed(beacon(kMac, "guest"));
    }
    CHECK_INT(1, h.seen.size());
    feedN(h, kMac, "third", 3);
    CHECK_INT(2, h.seen.size());
    if (h.seen.size() == 2) CHECK_STR("SSIDs: home | guest | third", h.seen[1].detail.c_str());
}

void multissid_names_age_out_after_sixty_seconds()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    // Heard 60 s ago exactly: still a live name.
    feedN(h, kMac, "home", 3);
    h.now += WifiFrameClassifier::kMultiSsidStaleMs;
    feedN(h, kMac, "guest", 3);
    CHECK_INT(1, h.seen.size());

    // One millisecond more: a renamed router, not two networks.
    const uint8_t other[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x66};
    feedN(h, other, "oldname", 3);
    h.now += WifiFrameClassifier::kMultiSsidStaleMs + 1;
    feedN(h, other, "newname", 3);
    CHECK_INT(1, h.seen.size());
}

void multissid_hidden_names_do_not_count()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, "", 5);
    feedN(h, kMac, std::string(3, '\0'), 5);
    feedN(h, kMac, "home", 5);
    CHECK_INT(0, h.seen.size());
}

void multissid_detail_is_safe_for_the_csv()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, "a,b", 3);
    feedN(h, kMac, std::string("q\"") + '\x01' + '\xC3', 3);
    CHECK_INT(1, h.seen.size());
    if (!h.seen.empty()) CHECK_STR("SSIDs: a?b | q???", h.seen[0].detail.c_str());
}

void multissid_detail_is_cut_to_the_event_record()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    feedN(h, kMac, std::string(32, 'A'), 3);
    feedN(h, kMac, std::string(32, 'B'), 3);
    CHECK_INT(1, h.seen.size());
    if (!h.seen.empty()) {
        CHECK_INT(39, h.seen[0].detail.size());  // MonitorEvent::kDetailSize - 1
        CHECK_STR((std::string("SSIDs: ") + std::string(32, 'A')).c_str(), h.seen[0].detail.c_str());
    }
}

void multissid_tracks_sixteen_bssids_and_gives_up_the_quietest()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    uint8_t b[17][6];
    for (int i = 0; i < 17; ++i) { memcpy(b[i], kMac, 6); b[i][5] = static_cast<uint8_t>(i); }
    for (int i = 0; i < 16; ++i) {
        feedN(h, b[i], "one", 3);
        h.now += 10;
    }
    // A seventeenth BSSID takes the slot of b[0], heard least recently.
    feedN(h, b[16], "one", 3);
    // b[0] comes back as a new BSSID ("one" is gone) and takes b[1]'s slot.
    feedN(h, b[0], "two", 3);
    CHECK_INT(0, h.seen.size());
    feedN(h, b[2], "two", 3);  // b[2] was kept, "one" and all
    CHECK_INT(1, h.seen.size());
}

void multissid_keeps_confirmed_names_when_slots_run_out()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    for (const char *s : {"n1", "n2", "n3", "n4"}) feedN(h, kMac, s, 3);
    CHECK_INT(3, h.seen.size());  // at the second, third, and fourth name
    feedN(h, kMac, "n5", 3);      // every slot confirmed: not tracked
    CHECK_INT(3, h.seen.size());
}

void multissid_unconfirmed_names_make_room()
{
    Harness h;
    h.wanted = {ReconTarget::MultiSSID};
    // Four one-beacon names fill every slot without confirming.
    for (const char *s : {"n1", "n2", "n3", "n4"}) {
        feedN(h, kMac, s, 1);
        h.now += 10;
    }
    feedN(h, kMac, "x", 3);
    feedN(h, kMac, "y", 3);
    CHECK_INT(1, h.seen.size());
}

void reset_forgets_trackers()
{
    Harness h;
    h.wanted = {ReconTarget::Deauth, ReconTarget::MultiSSID};
    for (int i = 0; i < 5; ++i) h.feed(mgmt(0x0C, kMac));
    feedN(h, kMac, "one", 3);
    h.k.reset();
    h.feed(mgmt(0x0C, kMac));
    feedN(h, kMac, "two", 3);
    CHECK_INT(0, h.seen.size());
}

int main(int argc, char **argv)
{
    CHECK_MAIN(argc, argv);
    CASE(deauth_frames_that_are_not_wanted_are_not_counted);
    CASE(only_management_deauth_and_disassoc_count);
    CASE(frame_length_limits);
    CASE(a_seventh_transmitter_evicts_the_longest_quiet);
    CASE(beacon_candidates_come_out_in_a_fixed_order);
    CASE(ssid_element_must_fit_and_be_32_bytes_or_fewer);
    CASE(ssid_detail_is_cut_at_31_characters);
    CASE(unwanted_ssid_prefix_stops_the_prefix_search);
    CASE(multissid_one_stray_frame_never_flags);
    CASE(multissid_flags_when_a_second_name_is_confirmed);
    CASE(multissid_beacons_alternating_count_toward_each_name);
    CASE(multissid_reports_once_per_confirmed_name_not_per_beacon);
    CASE(multissid_names_age_out_after_sixty_seconds);
    CASE(multissid_hidden_names_do_not_count);
    CASE(multissid_detail_is_safe_for_the_csv);
    CASE(multissid_detail_is_cut_to_the_event_record);
    CASE(multissid_tracks_sixteen_bssids_and_gives_up_the_quietest);
    CASE(multissid_keeps_confirmed_names_when_slots_run_out);
    CASE(multissid_unconfirmed_names_make_room);
    CASE(reset_forgets_trackers);
    CHECK_SUMMARY();
}
